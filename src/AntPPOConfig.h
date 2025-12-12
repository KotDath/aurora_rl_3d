// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <rl_tools/rl_tools.h>
#include <rl_tools/devices/devices.h>
#include <rl_tools/math/operations_cpu.h>
#include <rl_tools/utils/assert/operations_cpu.h>
#include <rl_tools/math/operations_generic.h>
#include <rl_tools/random/operations_generic.h>
#include <rl_tools/random/operations_cpu.h>
#include <rl_tools/containers/matrix/matrix.h>
#include <rl_tools/containers/matrix/operations_cpu.h>
#include <rl_tools/containers/tensor/tensor.h>
#include <rl_tools/containers/tensor/operations_cpu.h>
#include <rl_tools/rl/environments/mujoco/ant/operations_cpu.h>
#include <rl_tools/rl/algorithms/ppo/ppo.h>
#include <rl_tools/rl/components/on_policy_runner/on_policy_runner.h>
#include <rl_tools/nn/layers/standardize/layer.h>
#include <rl_tools/nn_models/sequential/model.h>
#include <rl_tools/nn_models/mlp_unconditional_stddev/network.h>

namespace aurora::ant::ppo_config {
namespace rlt = RL_TOOLS_NAMESPACE_WRAPPER::rl_tools;

// Slimmed-down copy of RLtools ant PPO parameters tuned for our in-app training loop.
template <typename T, typename TI>
struct Environment
{
    struct ENVIRONMENT_PARAMETERS : rlt::rl::environments::mujoco::ant::DefaultParameters<T, TI>
    {
        static constexpr T HEALTY_Z_MIN = static_cast<T>(0.05);
        static constexpr T HEALTY_Z_MAX = static_cast<T>(3.5);
        static constexpr T HEALTHY_REWARD = static_cast<T>(1.0);
        static constexpr T CONTROL_COST_WEIGHT = static_cast<T>(0.1);
        static constexpr T RESET_NOISE_SCALE = static_cast<T>(0.1);
    };
    using ENVIRONMENT_SPEC = rlt::rl::environments::mujoco::ant::Specification<T, TI, ENVIRONMENT_PARAMETERS>;
    using ENVIRONMENT = rlt::rl::environments::mujoco::Ant<ENVIRONMENT_SPEC>;
};

template <typename T, typename TI, typename ENVIRONMENT>
struct RL
{
    // Aim for ~30-40 ms per training step on CPU: keep batch and networks small.
    static constexpr TI BATCH_SIZE = 32;

    template <typename CAPABILITY>
    struct Actor
    {
        using INPUT_SHAPE = rlt::tensor::Shape<TI, 1, BATCH_SIZE, ENVIRONMENT::Observation::DIM>;
        using STANDARDIZATION_LAYER_CONFIG = rlt::nn::layers::standardize::Configuration<T, TI>;
        using STANDARDIZATION_LAYER = rlt::nn::layers::standardize::BindConfiguration<STANDARDIZATION_LAYER_CONFIG>;
        using CONFIG = rlt::nn_models::mlp::Configuration<
            T,
            TI,
            ENVIRONMENT::ACTION_DIM,
            2,
            64,
            rlt::nn::activation_functions::ActivationFunction::RELU,
            rlt::nn::activation_functions::IDENTITY>;
        using TYPE = rlt::nn_models::mlp_unconditional_stddev::BindConfiguration<CONFIG>;

        template <typename T_CONTENT, typename T_NEXT_MODULE = rlt::nn_models::sequential::OutputModule>
        using Module = typename rlt::nn_models::sequential::Module<T_CONTENT, T_NEXT_MODULE>;

        using MODULE_CHAIN = Module<STANDARDIZATION_LAYER, Module<TYPE>>;
        using MODEL = rlt::nn_models::sequential::Build<CAPABILITY, MODULE_CHAIN, INPUT_SHAPE>;
    };

    template <typename CAPABILITY>
    struct Critic
    {
        using INPUT_SHAPE = rlt::tensor::Shape<TI, 1, BATCH_SIZE, ENVIRONMENT::Observation::DIM>;
        using STANDARDIZATION_LAYER_CONFIG = rlt::nn::layers::standardize::Configuration<T, TI>;
        using STANDARDIZATION_LAYER = rlt::nn::layers::standardize::BindConfiguration<STANDARDIZATION_LAYER_CONFIG>;
        using CONFIG = rlt::nn_models::mlp::Configuration<
            T,
            TI,
            1,
            2,
            64,
            rlt::nn::activation_functions::ActivationFunction::RELU,
            rlt::nn::activation_functions::IDENTITY>;
        using TYPE = rlt::nn_models::mlp_unconditional_stddev::BindConfiguration<CONFIG>;

        template <typename T_CONTENT, typename T_NEXT_MODULE = rlt::nn_models::sequential::OutputModule>
        using Module = typename rlt::nn_models::sequential::Module<T_CONTENT, T_NEXT_MODULE>;

        using MODULE_CHAIN = Module<STANDARDIZATION_LAYER, Module<TYPE>>;
        using MODEL = rlt::nn_models::sequential::Build<CAPABILITY, MODULE_CHAIN, INPUT_SHAPE>;
    };

    using ACTOR_OPTIMIZER_SPEC = rlt::nn::optimizers::adam::Specification<T, TI>;
    using CRITIC_OPTIMIZER_SPEC = rlt::nn::optimizers::adam::Specification<T, TI>;
    using ACTOR_OPTIMIZER = rlt::nn::optimizers::Adam<ACTOR_OPTIMIZER_SPEC>;
    using CRITIC_OPTIMIZER = rlt::nn::optimizers::Adam<CRITIC_OPTIMIZER_SPEC>;
    using CAPABILITY_ADAM = rlt::nn::capability::Gradient<rlt::nn::parameters::Adam>;
    using ACTOR_TYPE = typename Actor<CAPABILITY_ADAM>::MODEL;
    using CRITIC_TYPE = typename Critic<CAPABILITY_ADAM>::MODEL;

    struct PPO_PARAMETERS : rlt::rl::algorithms::ppo::DefaultParameters<T, TI, BATCH_SIZE>
    {
        // Single-epoch PPO to minimize per-step latency.
        static constexpr TI N_EPOCHS = 1;
        static constexpr bool LEARN_ACTION_STD = true;
        static constexpr T INITIAL_ACTION_STD = 0.6;
        static constexpr T ACTION_ENTROPY_COEFFICIENT = 0.0;
        static constexpr T LAMBDA = static_cast<T>(0.95);
        static constexpr T CLIP_EPSILON = static_cast<T>(0.2);
        static constexpr bool NORMALIZE_ADVANTAGE = true;
        static constexpr T GAMMA = 0.99;
        static constexpr bool ADAPTIVE_LEARNING_RATE = true;
        static constexpr T ADAPTIVE_LEARNING_RATE_POLICY_KL_THRESHOLD = 0.01;
        static constexpr bool NORMALIZE_OBSERVATIONS = true;
    };

    static constexpr TI OBSERVATION_NORMALIZATION_WARMUP_STEPS =
        PPO_PARAMETERS::NORMALIZE_OBSERVATIONS ? 2 : 0;

    using PPO_SPEC = rlt::rl::algorithms::ppo::Specification<T, TI, ENVIRONMENT, ACTOR_TYPE, CRITIC_TYPE, PPO_PARAMETERS>;
    using PPO_TYPE = rlt::rl::algorithms::PPO<PPO_SPEC>;
    using PPO_BUFFERS_TYPE = rlt::rl::algorithms::ppo::Buffers<rlt::rl::algorithms::ppo::BufferSpecification<PPO_SPEC>>;

    // Allow longer render episodes without adding per-update work (dataset still 8 steps/env).
    static constexpr TI ON_POLICY_RUNNER_STEP_LIMIT = 1024;
    static constexpr TI N_ENVIRONMENTS = 4;
    using ON_POLICY_RUNNER_SPEC = rlt::rl::components::on_policy_runner::Specification<
        T,
        TI,
        ENVIRONMENT,
        N_ENVIRONMENTS,
        ON_POLICY_RUNNER_STEP_LIMIT>;
    // 4 envs * 8 steps = 32 samples per PPO update; keeps latency within 30-40 ms.
    static constexpr TI ON_POLICY_RUNNER_STEPS_PER_ENV = 8;
    using ON_POLICY_RUNNER_DATASET_SPEC =
        rlt::rl::components::on_policy_runner::DatasetSpecification<ON_POLICY_RUNNER_SPEC, ON_POLICY_RUNNER_STEPS_PER_ENV>;
    using ON_POLICY_RUNNER_DATASET_TYPE = rlt::rl::components::on_policy_runner::Dataset<ON_POLICY_RUNNER_DATASET_SPEC>;
    using ON_POLICY_RUNNER_TYPE = rlt::rl::components::OnPolicyRunner<ON_POLICY_RUNNER_SPEC>;

    using ACTOR_EVAL_TYPE = typename ACTOR_TYPE::template CHANGE_BATCH_SIZE<TI, ON_POLICY_RUNNER_SPEC::N_ENVIRONMENTS>;
    using ACTOR_EVAL_BUFFERS = typename ACTOR_EVAL_TYPE::template Buffer<>;
    using ACTOR_BUFFERS = typename ACTOR_TYPE::template Buffer<>;
    using CRITIC_BUFFERS = typename CRITIC_TYPE::template Buffer<>;

    using CRITIC_GAE = typename CRITIC_TYPE::template CHANGE_BATCH_SIZE<
        TI,
        ON_POLICY_RUNNER_DATASET_SPEC::STEPS_TOTAL_ALL>;
    using CRITIC_BUFFERS_GAE = typename CRITIC_GAE::template Buffer<>;
};
} // namespace aurora::ant::ppo_config
