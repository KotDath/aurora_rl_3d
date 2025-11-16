// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#include "AntTrainingEngine.h"

#include <QtMath>
#include <QVector3D>
#include <QQuaternion>
#include <QDebug>

#if AURORA_RL3D_HAS_MUJOCO
#include <rl_tools/operations/cpu_mux.h>
#include <rl_tools/nn/optimizers/adam/instance/operations_generic.h>
#include <rl_tools/nn/operations_cpu_mux.h>
#include <rl_tools/nn/layers/standardize/operations_generic.h>
#include <rl_tools/nn_models/mlp_unconditional_stddev/operations_generic.h>
#include <rl_tools/nn_models/sequential/operations_generic.h>
#include <rl_tools/nn/optimizers/adam/operations_generic.h>
#include <rl_tools/rl/environments/mujoco/ant/operations_cpu.h>
#include <rl_tools/rl/environments/mujoco/ant/ppo/parameters.h>
#include <rl_tools/rl/components/on_policy_runner/operations_cpu.h>
#include <rl_tools/rl/algorithms/ppo/operations_generic.h>
#include <rl_tools/rl/components/running_normalizer/operations_generic.h>
#include <rl_tools/rl/utils/evaluation/operations_generic.h>
#include <rl_tools/containers/tensor/operations_cpu.h>
#include <rl_tools/containers/matrix/operations_generic.h>

namespace {
namespace rlt = rl_tools;
using DEVICE = rlt::devices::DefaultCPU;
using RNG = typename DEVICE::SPEC::RANDOM::ENGINE<>;
using TI = typename DEVICE::index_t;
using penv = parameters_0::environment<float, TI>;
using prl = parameters_0::rl<float, TI, typename penv::ENVIRONMENT>;
using NormalizerSpec = rlt::rl::components::running_normalizer::Specification<float, TI, penv::ENVIRONMENT::Observation::DIM>;
using Normalizer = rlt::rl::components::RunningNormalizer<NormalizerSpec>;
using ObservationMatrix = rlt::Matrix<rlt::matrix::Specification<float, TI, 1, penv::ENVIRONMENT::Observation::DIM>>;
using ActionMatrix = rlt::Matrix<rlt::matrix::Specification<float, TI, 1, penv::ENVIRONMENT::ACTION_DIM>>;
}
#endif // AURORA_RL3D_HAS_MUJOCO

AntTrainingEngine::AntTrainingEngine()
    : m_usingFallback(true)
    , m_timeAccumulator(0.0f)
    , m_averageReward(0.0f)
    , m_iteration(0)
{
#if AURORA_RL3D_HAS_MUJOCO
    try {
        m_mujoco = std::make_unique<MuJoCoContext>();
        m_usingFallback = false;
    } catch (const std::exception& e) {
        qWarning() << "MuJoCo PPO initialization failed:" << e.what();
        m_mujoco.reset();
        m_usingFallback = true;
    }
#endif
}

AntTrainingEngine::~AntTrainingEngine() = default;

void AntTrainingEngine::step(AntTrainingMetrics& metrics)
{
#if AURORA_RL3D_HAS_MUJOCO
    if (!m_usingFallback && m_mujoco) {
        stepMuJoCo(metrics);
        return;
    }
#endif
    stepFallback(metrics);
}

void AntTrainingEngine::stepFallback(AntTrainingMetrics& metrics)
{
    m_iteration++;
    m_timeAccumulator += 0.75f;

    const float learningGain = 1.0f - qExp(-0.0015f * static_cast<float>(m_iteration));
    const float torsoSway = qSin(m_timeAccumulator * 0.3f);

    m_pose.torsoPosition.setX(m_pose.torsoPosition.x() + 0.015f * learningGain);
    m_pose.torsoPosition.setY(0.6f + 0.05f * qSin(m_timeAccumulator * 0.45f));
    m_pose.torsoPosition.setZ(0.2f * qSin(m_timeAccumulator * 0.12f));
    m_pose.torsoRotation = QQuaternion::fromAxisAndAngle(QVector3D(0.0f, 1.0f, 0.0f),
                                                         qRadiansToDegrees(0.15f * torsoSway));

    constexpr float halfPi = 1.57079632679f;
    for (int leg = 0; leg < 4; ++leg) {
        const float phaseShift = float(leg) * halfPi * 0.5f;
        const float hipPhase = m_timeAccumulator * 0.5f + phaseShift;
        const float kneePhase = hipPhase + halfPi;
        m_pose.jointAngles[leg * 2] = 0.6f * learningGain * qSin(hipPhase);
        m_pose.jointAngles[leg * 2 + 1] = -0.3f + 0.8f * learningGain * qSin(kneePhase);
    }

    const float rewardSample = 40.0f * learningGain + 10.0f * qSin(m_timeAccumulator * 0.2f);
    m_averageReward = 0.94f * m_averageReward + 0.06f * rewardSample;

    metrics.reward = rewardSample;
    metrics.averageReward = m_averageReward;
    metrics.episodeProgress = static_cast<float>((m_iteration % 240)) / 240.0f;
    metrics.iteration = m_iteration;
    metrics.fallbackActive = true;
}

#if AURORA_RL3D_HAS_MUJOCO
struct AntTrainingEngine::MuJoCoContext
{
    MuJoCoContext();
    ~MuJoCoContext();

    void trainStep(AntTrainingMetrics& metrics, PoseSnapshot& pose);

    DEVICE device;
    typename DEVICE::SPEC::LOGGING logger;
    RNG rng;
    RNG evaluation_rng;
    prl::PPO_TYPE ppo;
    prl::PPO_BUFFERS_TYPE ppo_buffers;
    prl::ON_POLICY_RUNNER_TYPE runner;
    prl::ON_POLICY_RUNNER_DATASET_TYPE dataset;
    prl::ACTOR_OPTIMIZER actor_optimizer;
    prl::CRITIC_OPTIMIZER critic_optimizer;
    prl::ACTOR_EVAL_BUFFERS actor_eval_buffers;
    prl::ACTOR_BUFFERS actor_buffers;
    prl::CRITIC_BUFFERS critic_buffers;
    prl::CRITIC_BUFFERS_GAE critic_buffers_gae;
    Normalizer observation_normalizer;
    penv::ENVIRONMENT envs[prl::N_ENVIRONMENTS];
    penv::ENVIRONMENT::Parameters env_parameters[prl::N_ENVIRONMENTS];
    penv::ENVIRONMENT evaluation_env;
    penv::ENVIRONMENT::Parameters evaluation_parameters;
    typename prl::PPO_TYPE::SPEC::ACTOR_TYPE::template Buffer<1> actor_deterministic_buffers;
    penv::ENVIRONMENT::State display_state;
    penv::ENVIRONMENT::State next_state;
    ActionMatrix display_action;

    TI seed = 600;
    TI ppo_step = 0;
    float smoothedReturn = 0.0f;
};

AntTrainingEngine::MuJoCoContext::MuJoCoContext()
{
    rlt::malloc(device, rng);
    rlt::malloc(device, evaluation_rng);
    rlt::malloc(device, ppo);
    rlt::malloc(device, ppo_buffers);
    rlt::malloc(device, dataset);
    rlt::malloc(device, runner);
    rlt::malloc(device, actor_eval_buffers);
    rlt::malloc(device, actor_buffers);
    rlt::malloc(device, critic_buffers);
    rlt::malloc(device, critic_buffers_gae);
    rlt::malloc(device, observation_normalizer);
    rlt::malloc(device, actor_optimizer);
    rlt::malloc(device, critic_optimizer);
    rlt::malloc(device, actor_deterministic_buffers);
    for (TI i = 0; i < prl::N_ENVIRONMENTS; ++i) {
        rlt::malloc(device, envs[i]);
    }
    rlt::malloc(device, evaluation_env);

    rlt::init(device);
    rlt::init(device, rng, seed);
    rlt::init(device, evaluation_rng, seed + 1);
    rlt::init(device, runner, envs, env_parameters, rng);
    rlt::init(device, observation_normalizer);
    rlt::init(device, ppo, actor_optimizer, critic_optimizer, rng);
    rlt::init(device, logger);

    if constexpr (prl::PPO_SPEC::PARAMETERS::NORMALIZE_OBSERVATIONS) {
        for (TI warmup = 0; warmup < prl::OBSERVATION_NORMALIZATION_WARMUP_STEPS; ++warmup) {
            rlt::collect(device, dataset, runner, ppo.actor, actor_eval_buffers, rng);
            rlt::update(device, observation_normalizer, dataset.observations);
        }
        rlt::set_statistics(device, ppo.actor.content, observation_normalizer.mean, observation_normalizer.std);
        rlt::set_statistics(device, ppo.critic.content, observation_normalizer.mean, observation_normalizer.std);
        rlt::init(device, runner, envs, env_parameters, rng);
    }
    rlt::init(device, evaluation_env);
    rlt::initial_state(device, evaluation_env, evaluation_parameters, display_state);
}

AntTrainingEngine::MuJoCoContext::~MuJoCoContext()
{
    rlt::free(device, ppo);
    rlt::free(device, ppo_buffers);
    rlt::free(device, dataset);
    rlt::free(device, runner);
    rlt::free(device, actor_eval_buffers);
    rlt::free(device, actor_buffers);
    rlt::free(device, critic_buffers);
    rlt::free(device, critic_buffers_gae);
    rlt::free(device, observation_normalizer);
    rlt::free(device, actor_optimizer);
    rlt::free(device, critic_optimizer);
    rlt::free(device, actor_deterministic_buffers);
    for (TI i = 0; i < prl::N_ENVIRONMENTS; ++i) {
        rlt::free(device, envs[i]);
    }
    rlt::free(device, evaluation_env);
    rlt::free(device, logger);
}

void AntTrainingEngine::MuJoCoContext::trainStep(AntTrainingMetrics& metrics, PoseSnapshot& pose)
{
    rlt::collect(device, dataset, runner, ppo.actor, actor_eval_buffers, rng);
    if constexpr (prl::PPO_SPEC::PARAMETERS::NORMALIZE_OBSERVATIONS) {
        rlt::update(device, observation_normalizer, dataset.observations);
        rlt::set_statistics(device, ppo.actor.content, observation_normalizer.mean, observation_normalizer.std);
        rlt::set_statistics(device, ppo.critic.content, observation_normalizer.mean, observation_normalizer.std);
    }

    auto all_observations_privileged_tensor = to_tensor(device, dataset.all_observations_privileged);
    auto all_observations_privileged_tensor_unsqueezed = unsqueeze(device, all_observations_privileged_tensor);
    auto all_values_tensor = to_tensor(device, dataset.all_values);
    auto all_values_tensor_unsqueezed = unsqueeze(device, all_values_tensor);
    evaluate(device, ppo.critic, all_observations_privileged_tensor_unsqueezed, all_values_tensor_unsqueezed, critic_buffers_gae, rng);
    rlt::estimate_generalized_advantages(device, dataset, typename prl::PPO_TYPE::SPEC::PARAMETERS{});
    rlt::train(device, ppo, dataset, actor_optimizer, critic_optimizer, ppo_buffers, actor_buffers, critic_buffers, rng);

    ++ppo_step;

    const float rewardMean = rlt::mean(device, dataset.rewards);
    smoothedReturn = 0.9f * smoothedReturn + 0.1f * rewardMean;

    metrics.reward = rewardMean;
    metrics.averageReward = smoothedReturn;
    metrics.episodeProgress = static_cast<float>(runner.step % prl::ON_POLICY_RUNNER_STEP_LIMIT) / static_cast<float>(prl::ON_POLICY_RUNNER_STEP_LIMIT);
    metrics.iteration = static_cast<int>(ppo_step);
    metrics.fallbackActive = false;

    ObservationMatrix observation;
    rlt::observe(device, evaluation_env, evaluation_parameters, display_state, typename penv::ENVIRONMENT::Observation{}, observation, rng);

    typename prl::PPO_TYPE::SPEC::ACTOR_TYPE::template State<> actor_state;
    auto observation_tensor = to_tensor(device, observation);
    auto observation_tensor_unsqueezed = unsqueeze(device, observation_tensor);
    auto action_tensor = to_tensor(device, display_action);
    auto action_tensor_unsqueezed = unsqueeze(device, action_tensor);
    rlt::Mode<rlt::mode::Evaluation<>> mode;
    evaluate_step(device, ppo.actor, observation_tensor_unsqueezed, actor_state, action_tensor_unsqueezed, actor_deterministic_buffers, rng, mode);

    ActionMatrix action_copy = display_action;
    rlt::step(device, evaluation_env, evaluation_parameters, display_state, action_copy, next_state, rng);
    display_state = next_state;

    pose.torsoPosition = QVector3D(display_state.q[0], display_state.q[2], display_state.q[1]);
    pose.torsoRotation = QQuaternion(display_state.q[3], display_state.q[4], display_state.q[5], display_state.q[6]);
    for (int i = 0; i < 8; ++i) {
        pose.jointAngles[static_cast<size_t>(i)] = display_state.q[7 + i];
    }
}

void AntTrainingEngine::stepMuJoCo(AntTrainingMetrics& metrics)
{
    if (!m_mujoco) {
        stepFallback(metrics);
        return;
    }
    m_mujoco->trainStep(metrics, m_pose);
}
#endif // AURORA_RL3D_HAS_MUJOCO
