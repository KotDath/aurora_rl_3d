// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#include "AntTrainingEngine.h"
#include "AntPPOConfig.h"
#include "SimpleSceneController.h"
#include "SimpleTrainingEngine.h"

#include <random>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <cstdint>
#include <cstring>

// RLtools
#include <rl_tools/operations/cpu_mux.h>
#include <rl_tools/nn/optimizers/adam/instance/operations_generic.h>
#include <rl_tools/nn/operations_cpu_mux.h>
#include <rl_tools/nn/layers/standardize/operations_generic.h>
#include <rl_tools/nn_models/mlp_unconditional_stddev/operations_generic.h>
#include <rl_tools/nn_models/mlp/operations_generic.h>                    // <-- добавлено
#include <rl_tools/nn_models/sequential/operations_generic.h>
#include <rl_tools/nn/optimizers/adam/operations_generic.h>
#include <rl_tools/rl/environments/pendulum/operations_generic.h>        // <-- добавлено
#if defined(RL_TOOLS_ENABLE_HDF5) && !defined(RL_TOOLS_DISABLE_HDF5)
#include <rl_tools/nn/layers/standardize/persist.h>
#include <rl_tools/nn_models/persist.h>
#include <rl_tools/nn_models/sequential/persist.h>
#endif
namespace rlt = RL_TOOLS_NAMESPACE_WRAPPER::rl_tools;

#if defined(RL_TOOLS_BACKEND_ENABLE_MKL) && !defined(RL_TOOLS_BACKEND_DISABLE_BLAS)
#include <rl_tools/rl/components/on_policy_runner/operations_cpu_mkl.h>
#else
#if defined(RL_TOOLS_BACKEND_ENABLE_ACCELERATE) && !defined(RL_TOOLS_BACKEND_DISABLE_BLAS)
#include <rl_tools/rl/components/on_policy_runner/operations_cpu_accelerate.h>
#else
#include <rl_tools/rl/components/on_policy_runner/operations_cpu.h>
#endif
#endif

#include <rl_tools/rl/algorithms/ppo/operations_generic.h>
#include <rl_tools/rl/components/running_normalizer/operations_generic.h>
#if defined(RL_TOOLS_ENABLE_HDF5) && !defined(RL_TOOLS_DISABLE_HDF5)
#include <rl_tools/rl/components/running_normalizer/persist.h>
#endif
#include <rl_tools/rl/utils/evaluation/operations_generic.h>
#include <mujoco/mujoco.h>

#include <filesystem>
#include <sstream>
#include <string>
#if defined(RL_TOOLS_ENABLE_HDF5) && !defined(RL_TOOLS_DISABLE_HDF5)
#include <highfive/H5File.hpp>
#endif

#ifdef RL_TOOLS_RL_ENVIRONMENTS_MUJOCO_ANT_TRAINING_TEST
#include <gtest/gtest.h>
#endif

// Qt includes last - to avoid namespace conflicts with RLtools
#include <QtMath>
#include <QVector3D>
#include <QQuaternion>
#include <QDebug>
#include <QElapsedTimer>

namespace {
using DEVICE = rlt::devices::DefaultCPU;
using RNG = typename DEVICE::SPEC::RANDOM::ENGINE<>;
using TI = typename DEVICE::index_t;

template <typename ENVIRONMENT>
void allocMujocoEnvSafe(ENVIRONMENT& env)
{
    using T = typename ENVIRONMENT::T;
    using TIEnv = typename ENVIRONMENT::TI;
    constexpr TIEnv error_length = 1000;
    char error[error_length] = "Could not load model";

    mjVFS* vfs = new mjVFS;
    mj_defaultVFS(vfs);
    mj_makeEmptyFileVFS(vfs, "model.xml", rlt::rl::environments::mujoco::ant::model_xml_len);
    int file_idx = mj_findFileVFS(vfs, "model.xml");
    std::memcpy(vfs->filedata[file_idx],
                rlt::rl::environments::mujoco::ant::model_xml,
                rlt::rl::environments::mujoco::ant::model_xml_len);
    env.model = mj_loadXML("model.xml", vfs, error, error_length);
    mj_deleteFileVFS(vfs, "model.xml");
    delete vfs;

    if (!env.model) {
        throw std::runtime_error(std::string("MuJoCo Ant model load failed: ") + error);
    }
    env.data = mj_makeData(env.model);
    if (!env.data) {
        mj_deleteModel(env.model);
        env.model = nullptr;
        throw std::runtime_error("MuJoCo Ant data allocation failed");
    }

    for (TIEnv state_i = 0; state_i < ENVIRONMENT::SPEC::STATE_DIM_Q; state_i++) {
        env.init_q[state_i] = env.data->qpos[state_i];
    }
    for (TIEnv state_i = 0; state_i < ENVIRONMENT::SPEC::STATE_DIM_Q_DOT; state_i++) {
        env.init_q_dot[state_i] = env.data->qvel[state_i];
    }
}

// MuJoCo-Ant PPO configuration copied locally from RLtools parameters_0
using penv = aurora::ant::ppo_config::Environment<double, TI>;
using prl = aurora::ant::ppo_config::RL<double, TI, typename penv::ENVIRONMENT>;

using NormalizerSpec =
    rlt::rl::components::running_normalizer::Specification<double, TI, penv::ENVIRONMENT::Observation::DIM>;
using Normalizer = rlt::rl::components::RunningNormalizer<NormalizerSpec>;
} // namespace

AntTrainingEngine::AntTrainingEngine()
    : m_usingFallback(true)
    , m_timeAccumulator(0.0f)
    , m_averageReward(0.0f)
    , m_iteration(0)
{
    qInfo() << "[Ant] Initializing training engine (MuJoCo backend)";
    try {
        m_mujoco = std::make_unique<MuJoCoContext>();
        m_usingFallback = false;
        qInfo() << "[Ant] MuJoCo backend initialized successfully";
    } catch (const std::exception& e) {
        qWarning() << "MuJoCo PPO initialization failed:" << e.what();
        m_mujoco.reset();
        m_usingFallback = true;
        qWarning() << "[Ant] Falling back to kinematic demo";
    }
}

AntTrainingEngine::~AntTrainingEngine() = default;

void AntTrainingEngine::step(AntTrainingMetrics& metrics)
{
    const int kMinStepMs = 40; // ~25 Hz cap to match pendulum cadence
    if (m_stepTimer.isValid()) {
        if (m_stepTimer.elapsed() < kMinStepMs && m_hasLastMetrics) {
            metrics = m_lastMetrics;
            return;
        }
        m_stepTimer.restart();
    } else {
        m_stepTimer.start();
    }

    static int stepLogCount = 0;
    const bool logStep = stepLogCount < 5;
    if (logStep) {
        qDebug() << "[Ant] step requested"
                 << "usingFallback:" << m_usingFallback
                 << "hasMuJoCo:" << static_cast<bool>(m_mujoco);
        ++stepLogCount;
        if (stepLogCount == 5) {
            qDebug() << "[Ant] Suppressing further per-step logs";
        }
    }

    if (!m_usingFallback && m_mujoco) {
        stepMuJoCo(metrics);
    } else {
        if (logStep) {
            qWarning() << "[Ant] MuJoCo unavailable, running fallback animation";
        }
        stepFallback(metrics);
    }

    m_lastMetrics = metrics;
    m_hasLastMetrics = true;
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
    m_pose.torsoRotation = QQuaternion::fromAxisAndAngle(
        QVector3D(0.0f, 1.0f, 0.0f),
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

    if (m_iteration <= 3 || (m_iteration % 200) == 0) {
        qDebug() << "[Ant] Fallback step" << m_iteration
                 << "reward" << rewardSample
                 << "avg" << m_averageReward;
    }
}

struct AntTrainingEngine::MuJoCoContext
{
    MuJoCoContext();
    ~MuJoCoContext();

    void trainStep(AntTrainingMetrics& metrics, PoseSnapshot& pose);

    DEVICE device;
    typename DEVICE::SPEC::LOGGING logger;
    RNG rng;

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

    int torso_geom_id{-1};
    int upper_geom_ids[4]{-1, -1, -1, -1};
    int lower_geom_ids[4]{-1, -1, -1, -1};

    TI seed = 600;
    TI ppo_step = 0;
    float smoothedReturn = 0.0f;

    static void stateToPose(const penv::ENVIRONMENT::State& state, PoseSnapshot& pose);
    void fillSegmentPosesFromSimulation(const penv::ENVIRONMENT& env, PoseSnapshot& pose);
};

AntTrainingEngine::MuJoCoContext::MuJoCoContext()
{
    qInfo() << "[Ant] Allocating MuJoCo context and RL Tools buffers";
    rlt::malloc(device, rng);
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
    for (TI i = 0; i < prl::N_ENVIRONMENTS; ++i) {
        rlt::malloc(device, envs[i]);
    }

    rlt::init(device);
    rlt::init(device, rng, seed);
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

    const mjModel* model = envs[0].model;
    torso_geom_id = mj_name2id(model, mjOBJ_GEOM, "torso_geom");
    upper_geom_ids[0] = mj_name2id(model, mjOBJ_GEOM, "left_leg_geom");
    upper_geom_ids[1] = mj_name2id(model, mjOBJ_GEOM, "right_leg_geom");
    upper_geom_ids[2] = mj_name2id(model, mjOBJ_GEOM, "back_leg_geom");
    upper_geom_ids[3] = mj_name2id(model, mjOBJ_GEOM, "rightback_leg_geom");
    lower_geom_ids[0] = mj_name2id(model, mjOBJ_GEOM, "left_ankle_geom");
    lower_geom_ids[1] = mj_name2id(model, mjOBJ_GEOM, "right_ankle_geom");
    lower_geom_ids[2] = mj_name2id(model, mjOBJ_GEOM, "third_ankle_geom");
    lower_geom_ids[3] = mj_name2id(model, mjOBJ_GEOM, "fourth_ankle_geom");

    qInfo() << "[Ant] MuJoCo context ready; geom ids torso" << torso_geom_id
            << "upper" << upper_geom_ids[0] << upper_geom_ids[1]
            << upper_geom_ids[2] << upper_geom_ids[3]
            << "lower" << lower_geom_ids[0] << lower_geom_ids[1]
            << lower_geom_ids[2] << lower_geom_ids[3];
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
    for (TI i = 0; i < prl::N_ENVIRONMENTS; ++i) {
        rlt::free(device, envs[i]);
    }
    rlt::free(device, logger);
}

void AntTrainingEngine::MuJoCoContext::stateToPose(
    const penv::ENVIRONMENT::State& state, PoseSnapshot& pose)
{
    // RL Tools orders q as [x, y, z, qw, qx, qy, qz, hinges...].
    pose.torsoPosition = QVector3D(state.q[0], state.q[2], state.q[1]);
    pose.torsoRotation = QQuaternion(state.q[3], state.q[4], state.q[5], state.q[6]);
    for (int i = 0; i < 8; ++i) {
        pose.jointAngles[static_cast<size_t>(i)] = state.q[7 + i];
    }
}

namespace {
QMatrix4x4 makeModelMatrix(const mjtNum* pos, const mjtNum* quat)
{
    // MuJoCo quaternion: [w, x, y, z]. Swap Y/Z in position.
    QVector3D position(pos[0], pos[2], pos[1]);
    QQuaternion rotation(quat[0], quat[1], quat[2], quat[3]);
    QMatrix4x4 matrix;
    matrix.translate(position);
    matrix.rotate(rotation);
    return matrix;
}

QMatrix4x4 makeModelMatrixFromXMat(const mjtNum* pos, const mjtNum* xmat)
{
    float Rm[3][3] = {
        {static_cast<float>(xmat[0]), static_cast<float>(xmat[1]), static_cast<float>(xmat[2])},
        {static_cast<float>(xmat[3]), static_cast<float>(xmat[4]), static_cast<float>(xmat[5])},
        {static_cast<float>(xmat[6]), static_cast<float>(xmat[7]), static_cast<float>(xmat[8])},
    };

    auto rs = [&](int i, int j) -> float {
        int pi = (i == 1) ? 2 : (i == 2 ? 1 : 0);
        int pj = (j == 1) ? 2 : (j == 2 ? 1 : 0);
        return Rm[pi][pj];
    };

    QVector3D col0(rs(0, 0), rs(1, 0), rs(2, 0));
    QVector3D col1(rs(0, 1), rs(1, 1), rs(2, 1));
    QVector3D col2(rs(0, 2), rs(1, 2), rs(2, 2));
    QVector3D position(pos[0], pos[2], pos[1]);

    QMatrix4x4 m;
    m.setColumn(0, QVector4D(col0, 0.0f));
    m.setColumn(1, QVector4D(col1, 0.0f));
    m.setColumn(2, QVector4D(col2, 0.0f));
    m.setColumn(3, QVector4D(position, 1.0f));
    return m;
}
} // namespace

void AntTrainingEngine::MuJoCoContext::fillSegmentPosesFromSimulation(
    const penv::ENVIRONMENT& env, PoseSnapshot& pose)
{
    const mjData* data = env.data;
    const mjModel* model = env.model;

    auto geomMatrix = [&](int geom_id, float meshLength, float meshRadius, bool sphere) -> QMatrix4x4 {
        if (geom_id < 0) {
            return QMatrix4x4();
        }
        const mjtNum* pos = data->geom_xpos + 3 * geom_id;
        const mjtNum* xmat = data->geom_xmat + 9 * geom_id;
        QMatrix4x4 m = makeModelMatrixFromXMat(pos, xmat);
        const mjtNum* size = model->geom_size + 3 * geom_id;
        if (sphere) {
            float r = static_cast<float>(size[0]);
            m.scale(r / meshRadius, r / meshRadius, r / meshRadius);
        } else {
            float r = static_cast<float>(size[0]);
            float halfLen = static_cast<float>(size[1]);
            float lenScale = (halfLen * 2.0f) / meshLength;
            float rScale = r / meshRadius;
            m.scale(rScale, lenScale, rScale);
        }
        return m;
    };

    constexpr float kTorsoRadius = 0.25f;
    constexpr float kUpperLen = 0.2828427124f;
    constexpr float kLowerLen = 0.5656854249f;
    constexpr float kLegRadius = 0.08f;

    pose.torsoPose.modelMatrix = geomMatrix(torso_geom_id, 1.0f, kTorsoRadius, true);
    for (int leg = 0; leg < 4; ++leg) {
        pose.upperLegPoses[static_cast<size_t>(leg)].modelMatrix =
            geomMatrix(upper_geom_ids[leg], kUpperLen, kLegRadius, false);
        pose.lowerLegPoses[static_cast<size_t>(leg)].modelMatrix =
            geomMatrix(lower_geom_ids[leg], kLowerLen, kLegRadius, false);
    }
}

void AntTrainingEngine::MuJoCoContext::trainStep(AntTrainingMetrics& metrics, PoseSnapshot& pose)
{
    const bool logThisStep = (ppo_step < 3) || ((ppo_step % 250) == 0);
    QElapsedTimer timer;
    if (logThisStep) {
        timer.start();
        qDebug() << "[Ant] MuJoCo trainStep start" << "ppo_step" << ppo_step;
    }

    rlt::collect(device, dataset, runner, ppo.actor, actor_eval_buffers, rng);
    if (logThisStep) {
        qDebug() << "[Ant] collect done" << "t_ms" << timer.elapsed();
    }

    if constexpr (prl::PPO_SPEC::PARAMETERS::NORMALIZE_OBSERVATIONS) {
        rlt::update(device, observation_normalizer, dataset.observations);
        rlt::set_statistics(device, ppo.actor.content, observation_normalizer.mean, observation_normalizer.std);
        rlt::set_statistics(device, ppo.critic.content, observation_normalizer.mean, observation_normalizer.std);
        if (logThisStep) {
            qDebug() << "[Ant] normalization done" << "t_ms" << timer.elapsed();
        }
    }

    // ---- ВАЖНО: явно через rlt:: и пространство имён evaluation ----
    auto all_observations_privileged_tensor =
        rlt::to_tensor(device, dataset.all_observations_privileged);
    auto all_observations_privileged_tensor_unsqueezed =
        rlt::unsqueeze(device, all_observations_privileged_tensor);

    auto all_values_tensor = rlt::to_tensor(device, dataset.all_values);
    auto all_values_tensor_unsqueezed = rlt::unsqueeze(device, all_values_tensor);

    rlt::evaluate(
        device,
        ppo.critic,
        all_observations_privileged_tensor_unsqueezed,
        all_values_tensor_unsqueezed,
        critic_buffers_gae,
        rng);
    // ---------------------------------------------------------------

    if (logThisStep) {
        qDebug() << "[Ant] evaluate critic done" << "t_ms" << timer.elapsed();
    }

    rlt::estimate_generalized_advantages(device, dataset, typename prl::PPO_TYPE::SPEC::PARAMETERS{});
    if (logThisStep) {
        qDebug() << "[Ant] GAE estimation done" << "t_ms" << timer.elapsed();
    }

    rlt::train(device, ppo, dataset,
               actor_optimizer, critic_optimizer,
               ppo_buffers, actor_buffers, critic_buffers, rng);
    if (logThisStep) {
        qDebug() << "[Ant] PPO train done" << "t_ms" << timer.elapsed();
    }

    ++ppo_step;

    constexpr TI render_env_index = 0;
    auto& render_env = rlt::get(runner.environments, 0, render_env_index);
    auto& render_state = rlt::get(runner.states, 0, render_env_index);
    const TI render_episode_step = rlt::get(runner.episode_step, 0, render_env_index);
    const bool render_truncated = rlt::get(runner.truncated, 0, render_env_index);

    const float rewardMean = rlt::mean(device, dataset.rewards);
    const float renderReward = static_cast<float>(render_env.last_reward);
    smoothedReturn = 0.9f * smoothedReturn + 0.1f * renderReward;

    metrics.reward = renderReward;
    metrics.averageReward = smoothedReturn;
    metrics.episodeProgress =
        qMin(1.0f, static_cast<float>(render_episode_step) /
                       static_cast<float>(prl::ON_POLICY_RUNNER_STEP_LIMIT));
    if (render_truncated) {
        metrics.episodeProgress = 1.0f;
    }
    metrics.iteration = static_cast<int>(ppo_step);
    metrics.fallbackActive = false;

    stateToPose(render_state, pose);
    fillSegmentPosesFromSimulation(render_env, pose);

    if (logThisStep) {
        qDebug() << "[Ant] MuJoCo trainStep done" << "ppo_step" << ppo_step
                 << "rewardMean" << rewardMean
                 << "renderReward" << renderReward
                 << "smoothed" << smoothedReturn
                 << "progress" << metrics.episodeProgress
                 << "episodeStep" << render_episode_step
                 << "t_ms" << timer.elapsed();
    }
}

void AntTrainingEngine::stepMuJoCo(AntTrainingMetrics& metrics)
{
    if (!m_mujoco) {
        stepFallback(metrics);
        return;
    }
    const TI currentStep = m_mujoco->ppo_step;
    if (currentStep < 3 || (currentStep % 250) == 0) {
        qDebug() << "[Ant] Dispatching MuJoCo step" << currentStep;
    }
    m_mujoco->trainStep(metrics, m_pose);
}

// ============================================================================
//                         SimpleTrainingEngine / Pendulum
// ============================================================================

namespace {
namespace simple_train {
using DEVICE = rlt::devices::DefaultCPU;
using RNG = typename DEVICE::SPEC::RANDOM::ENGINE<>;
using TI = typename DEVICE::index_t;
using T = double;

template <typename T_, typename TI_>
struct environment {
    using ENVIRONMENT_SPEC = rlt::rl::environments::pendulum::Specification<T_, TI_>;
    using ENVIRONMENT = rlt::rl::environments::Pendulum<ENVIRONMENT_SPEC>;
};

template <typename T_, typename TI_, typename ENVIRONMENT>
struct rl {
    static constexpr TI_ BATCH_SIZE = 4;
    static constexpr TI_ N_ENVIRONMENTS = 4;
    static constexpr TI_ ON_POLICY_RUNNER_STEP_LIMIT = 200;
    static constexpr TI_ ON_POLICY_RUNNER_STEPS_PER_ENV = 1;

    using OPTIMIZER_PARAMETERS = rlt::nn::optimizers::adam::Specification<T_, TI_>;
    using OPTIMIZER = rlt::nn::optimizers::Adam<OPTIMIZER_PARAMETERS>;
    using CAPABILITY_ADAM = rlt::nn::capability::Gradient<rlt::nn::parameters::Adam>;

    template <typename CAPABILITY>
    struct Actor {
        using ACTOR_INPUT_SHAPE =
            rlt::tensor::Shape<TI_, 1, BATCH_SIZE, ENVIRONMENT::Observation::DIM>;
        using ACTOR_SPEC =
            rlt::nn_models::mlp::Configuration<T_, TI_, ENVIRONMENT::ACTION_DIM,
                                                3, 32,
                                                rlt::nn::activation_functions::TANH,
                                                rlt::nn::activation_functions::IDENTITY>;
        using ACTOR = rlt::nn_models::mlp_unconditional_stddev::BindConfiguration<ACTOR_SPEC>;
        template <typename T_CONTENT, typename T_NEXT_MODULE = rlt::nn_models::sequential::OutputModule>
        using Module = typename rlt::nn_models::sequential::Module<T_CONTENT, T_NEXT_MODULE>;
        using MODULE_CHAIN = Module<ACTOR>;
        using MODEL = rlt::nn_models::sequential::Build<CAPABILITY, MODULE_CHAIN, ACTOR_INPUT_SHAPE>;
    };

    using ACTOR_TYPE = typename Actor<CAPABILITY_ADAM>::MODEL;

    using CRITIC_INPUT_SHAPE =
        rlt::tensor::Shape<TI_, 1, BATCH_SIZE, ENVIRONMENT::Observation::DIM>;
    using CRITIC_SPEC =
        rlt::nn_models::mlp::Configuration<T_, TI_, 1,
                                           3, 32,
                                           rlt::nn::activation_functions::TANH,
                                           rlt::nn::activation_functions::IDENTITY>;
    using CRITIC = rlt::nn_models::mlp::BindConfiguration<CRITIC_SPEC>;
    template <typename T_CONTENT, typename T_NEXT_MODULE = rlt::nn_models::sequential::OutputModule>
    using Module = typename rlt::nn_models::sequential::Module<T_CONTENT, T_NEXT_MODULE>;
    using MODULE_CHAIN = Module<CRITIC>;
    using CRITIC_TYPE =
        rlt::nn_models::sequential::Build<CAPABILITY_ADAM, MODULE_CHAIN, CRITIC_INPUT_SHAPE>;

    struct PPO_PARAMETERS : rlt::rl::algorithms::ppo::DefaultParameters<T_, TI_, BATCH_SIZE> {
        static constexpr TI_ N_EPOCHS = 1;
        static constexpr T_ GAMMA = 0.99;
        static constexpr bool NORMALIZE_OBSERVATIONS = false;
        static constexpr bool NORMALIZE_ADVANTAGE = true;
    };

    using PPO_SPEC =
        rlt::rl::algorithms::ppo::Specification<T_, TI_, ENVIRONMENT,
                                                ACTOR_TYPE, CRITIC_TYPE, PPO_PARAMETERS>;
    using PPO_TYPE = rlt::rl::algorithms::PPO<PPO_SPEC>;
    using PPO_BUFFERS_TYPE =
        rlt::rl::algorithms::ppo::Buffers<rlt::rl::algorithms::ppo::BufferSpecification<PPO_SPEC>>;

    using ON_POLICY_RUNNER_SPEC =
        rlt::rl::components::on_policy_runner::Specification<T_, TI_, ENVIRONMENT,
                                                             N_ENVIRONMENTS,
                                                             ON_POLICY_RUNNER_STEP_LIMIT>;
    using ON_POLICY_RUNNER_TYPE =
        rlt::rl::components::OnPolicyRunner<ON_POLICY_RUNNER_SPEC>;
    using ON_POLICY_RUNNER_DATASET_SPEC =
        rlt::rl::components::on_policy_runner::DatasetSpecification<
            ON_POLICY_RUNNER_SPEC, ON_POLICY_RUNNER_STEPS_PER_ENV>;
    using ON_POLICY_RUNNER_DATASET_TYPE =
        rlt::rl::components::on_policy_runner::Dataset<ON_POLICY_RUNNER_DATASET_SPEC>;

    using ACTOR_EVAL_TYPE =
        typename ACTOR_TYPE::template CHANGE_BATCH_SIZE<TI_, ON_POLICY_RUNNER_SPEC::N_ENVIRONMENTS>;
    using ACTOR_EVAL_BUFFERS = typename ACTOR_EVAL_TYPE::template Buffer<>;
    using ACTOR_BUFFERS = typename ACTOR_TYPE::template Buffer<>;
    using CRITIC_BUFFERS = typename CRITIC_TYPE::template Buffer<>;
    using CRITIC_GAE =
        typename CRITIC_TYPE::template CHANGE_BATCH_SIZE<TI_, ON_POLICY_RUNNER_DATASET_SPEC::STEPS_TOTAL_ALL>;
    using CRITIC_BUFFERS_GAE = typename CRITIC_GAE::template Buffer<>;

    using ACTOR_OPTIMIZER = OPTIMIZER;
    using CRITIC_OPTIMIZER = OPTIMIZER;
};
} // namespace simple_train
} // namespace

struct SimpleTrainingEngine::PendulumContext
{
    simple_train::DEVICE device;
    simple_train::RNG rng;

    using penv = simple_train::environment<double, simple_train::TI>;
    using prl = simple_train::rl<double, simple_train::TI, typename penv::ENVIRONMENT>;

    typename prl::PPO_TYPE ppo;
    typename prl::PPO_BUFFERS_TYPE ppo_buffers;
    typename prl::ON_POLICY_RUNNER_TYPE runner;
    typename prl::ON_POLICY_RUNNER_DATASET_TYPE dataset;
    typename prl::ACTOR_OPTIMIZER actor_optimizer;
    typename prl::CRITIC_OPTIMIZER critic_optimizer;
    typename prl::ACTOR_EVAL_BUFFERS actor_eval_buffers;
    typename prl::ACTOR_BUFFERS actor_buffers;
    typename prl::CRITIC_BUFFERS critic_buffers;
    typename prl::CRITIC_BUFFERS_GAE critic_buffers_gae;
    typename penv::ENVIRONMENT envs[prl::ON_POLICY_RUNNER_SPEC::N_ENVIRONMENTS];
    typename penv::ENVIRONMENT::Parameters env_parameters[prl::ON_POLICY_RUNNER_SPEC::N_ENVIRONMENTS];

    float smoothedReturn = 0.0f;
    simple_train::TI ppo_step = 0;
};

SimpleTrainingEngine::SimpleTrainingEngine()
    : m_ctx(std::make_unique<PendulumContext>())
    , m_rng(std::random_device{}())
    , m_angle(0.0f)
    , m_angularVelocity(0.0f)
    , m_lastMetrics()
{
    qInfo() << "[Simple] Initializing pendulum context";

    rlt::malloc(m_ctx->device, m_ctx->rng);
    rlt::malloc(m_ctx->device, m_ctx->ppo);
    rlt::malloc(m_ctx->device, m_ctx->ppo_buffers);
    rlt::malloc(m_ctx->device, m_ctx->dataset);
    rlt::malloc(m_ctx->device, m_ctx->runner);
    rlt::malloc(m_ctx->device, m_ctx->actor_eval_buffers);
    rlt::malloc(m_ctx->device, m_ctx->actor_buffers);
    rlt::malloc(m_ctx->device, m_ctx->critic_buffers);
    rlt::malloc(m_ctx->device, m_ctx->critic_buffers_gae);
    rlt::malloc(m_ctx->device, m_ctx->actor_optimizer);
    rlt::malloc(m_ctx->device, m_ctx->critic_optimizer);

    using prl = PendulumContext::prl;
    for (simple_train::TI i = 0; i < prl::ON_POLICY_RUNNER_SPEC::N_ENVIRONMENTS; ++i) {
        rlt::malloc(m_ctx->device, m_ctx->envs[i]);
    }

    rlt::init(m_ctx->device, m_ctx->rng, static_cast<int>(m_rng()));
    rlt::init(m_ctx->device, m_ctx->ppo,
              m_ctx->actor_optimizer, m_ctx->critic_optimizer, m_ctx->rng);
    rlt::init(m_ctx->device, m_ctx->runner,
              m_ctx->envs, m_ctx->env_parameters, m_ctx->rng);

    m_stepTimer.start();
    resetEpisode();
}

SimpleTrainingEngine::~SimpleTrainingEngine() = default;

void SimpleTrainingEngine::resetEpisode()
{
    using prl = PendulumContext::prl;
    for (simple_train::TI env = 0;
         env < prl::ON_POLICY_RUNNER_SPEC::N_ENVIRONMENTS; ++env) {
        rlt::sample_initial_state(
            m_ctx->device,
            m_ctx->envs[env],
            m_ctx->env_parameters[env],
            rlt::get(m_ctx->runner.states, 0, env),
            m_ctx->rng);
    }
    m_ctx->ppo_step = 0;
    m_episodeStep = 0;

    auto& s0 = rlt::get(m_ctx->runner.states, 0, 0);
    qInfo() << "[Simple] Episode reset theta" << s0.theta
            << "theta_dot" << s0.theta_dot;

    updatePose();
}

void SimpleTrainingEngine::updatePose()
{
    const float kBaseRadius = SimpleSceneController::Dimensions::BaseRadius;
    const float kPoleLength = SimpleSceneController::Dimensions::PoleLength;
    const float kPoleRadius = SimpleSceneController::Dimensions::PoleRadius;
    const float kTipRadius = SimpleSceneController::Dimensions::TipRadius;

    auto& render_state = rlt::get(m_ctx->runner.states, 0, 0);
    m_angle = static_cast<float>(render_state.theta);
    m_angularVelocity = static_cast<float>(render_state.theta_dot);

    m_pose.angle = static_cast<qreal>(m_angle);
    m_pose.angularVelocity = static_cast<qreal>(m_angularVelocity);

    QMatrix4x4 baseMatrix;
    baseMatrix.translate(0.0f, kBaseRadius, 0.0f);
    baseMatrix.scale(kBaseRadius);
    m_pose.baseMatrix = baseMatrix;

    QMatrix4x4 poleMatrix;
    poleMatrix.rotate(qRadiansToDegrees(static_cast<float>(m_pose.angle)),
                      QVector3D(0.0f, 0.0f, 1.0f));
    poleMatrix.translate(0.0f, 2.0f * kBaseRadius + kPoleLength * 0.5f, 0.0f);
    poleMatrix.scale(kPoleRadius, kPoleLength, kPoleRadius);
    m_pose.poleMatrix = poleMatrix;

    QMatrix4x4 tipMatrix;
    tipMatrix.rotate(qRadiansToDegrees(static_cast<float>(m_pose.angle)),
                     QVector3D(0.0f, 0.0f, 1.0f));
    tipMatrix.translate(0.0f, 2.0f * kBaseRadius + kPoleLength + kTipRadius, 0.0f);
    tipMatrix.scale(kTipRadius);
    m_pose.tipMatrix = tipMatrix;
}

void SimpleTrainingEngine::step(SimpleTrainingMetrics& metrics)
{
    const int kMinStepMs = 40; // ~25 Hz
    if (!m_stepTimer.isValid()) {
        m_stepTimer.start();
    } else if (m_stepTimer.elapsed() < kMinStepMs) {
        metrics = m_lastMetrics;
        return;
    }
    m_stepTimer.restart();

    QElapsedTimer collectStart;
    collectStart.start();

    rlt::collect(m_ctx->device, m_ctx->dataset, m_ctx->runner,
                 m_ctx->ppo.actor, m_ctx->actor_eval_buffers, m_ctx->rng);

    // ---- Аналогичный фикс: rlt:: + evaluation::evaluate ----
    auto all_observations =
        rlt::to_tensor(m_ctx->device, m_ctx->dataset.all_observations_privileged);
    auto all_observations_unsqueezed =
        rlt::unsqueeze(m_ctx->device, all_observations);

    auto all_values =
        rlt::to_tensor(m_ctx->device, m_ctx->dataset.all_values);
    auto all_values_unsqueezed =
        rlt::unsqueeze(m_ctx->device, all_values);

    rlt::evaluate(
        m_ctx->device,
        m_ctx->ppo.critic,
        all_observations_unsqueezed,
        all_values_unsqueezed,
        m_ctx->critic_buffers_gae,
        m_ctx->rng);
    // --------------------------------------------------------

    using prl = PendulumContext::prl;
    rlt::estimate_generalized_advantages(
        m_ctx->device, m_ctx->dataset, typename prl::PPO_TYPE::SPEC::PARAMETERS{});

    rlt::train(m_ctx->device, m_ctx->ppo, m_ctx->dataset,
               m_ctx->actor_optimizer, m_ctx->critic_optimizer,
               m_ctx->ppo_buffers, m_ctx->actor_buffers,
               m_ctx->critic_buffers, m_ctx->rng);

    ++m_ctx->ppo_step;

    const float rewardMean = rlt::mean(m_ctx->device, m_ctx->dataset.rewards);
    m_ctx->smoothedReturn = 0.9f * m_ctx->smoothedReturn + 0.1f * rewardMean;

    metrics.reward = rewardMean;
    metrics.averageReward = m_ctx->smoothedReturn;

    const float episodeLimit = static_cast<float>(prl::ON_POLICY_RUNNER_STEP_LIMIT);
    metrics.episodeProgress =
        static_cast<float>(m_episodeStep % prl::ON_POLICY_RUNNER_STEP_LIMIT) / episodeLimit;
    metrics.iteration = static_cast<int>(m_ctx->ppo_step);
    metrics.angle = m_angle;
    metrics.angularVelocity = m_angularVelocity;

    auto& render_env = rlt::get(m_ctx->runner.environments, 0, 0);
    Q_UNUSED(render_env);

    updatePose();
    m_lastMetrics = metrics;

    // Если маятник слишком завалился — обрываем эпизод
    if (qAbs(m_angle) > 1.6f) {
        metrics.episodeProgress = 0.0f;
        m_lastMetrics = metrics;
        resetEpisode();
        return;
    }

    ++m_episodeStep;
    if (m_episodeStep >= prl::ON_POLICY_RUNNER_STEP_LIMIT) {
        resetEpisode();
    }

    if (m_ctx->ppo_step <= 3 || (m_ctx->ppo_step % 200) == 0) {
        qDebug() << "[Simple] train step" << m_ctx->ppo_step
                 << "rewardMean" << rewardMean
                 << "avg" << m_ctx->smoothedReturn
                 << "progress" << metrics.episodeProgress
                 << "collect+train_ms" << collectStart.elapsed();
    }
}
