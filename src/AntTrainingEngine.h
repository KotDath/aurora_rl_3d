// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef ANTTRAININGENGINE_H
#define ANTTRAININGENGINE_H

#include <array>
#include <memory>

#include <QQuaternion>
#include <QVector3D>

#include "TrainingTypes.h"

#ifndef AURORA_RL3D_HAS_MUJOCO
#  if defined(__has_include)
#    if __has_include(<mujoco/mujoco.h>)
#      define AURORA_RL3D_HAS_MUJOCO 1
#    else
#      define AURORA_RL3D_HAS_MUJOCO 0
#    endif
#  else
#    define AURORA_RL3D_HAS_MUJOCO 0
#  endif
#endif

class AntTrainingEngine
{
public:
    struct PoseSnapshot
    {
        QVector3D torsoPosition{0.0f, 0.6f, 0.0f};
        QQuaternion torsoRotation;
        std::array<float, 8> jointAngles{};
    };

    AntTrainingEngine();
    ~AntTrainingEngine();

    void step(AntTrainingMetrics& metrics);
    const PoseSnapshot& currentPose() const { return m_pose; }

private:
    void stepFallback(AntTrainingMetrics& metrics);
#if AURORA_RL3D_HAS_MUJOCO
    void stepMuJoCo(AntTrainingMetrics& metrics);
    struct MuJoCoContext;
    std::unique_ptr<MuJoCoContext> m_mujoco;
#endif

    PoseSnapshot m_pose;
    bool m_usingFallback;
    float m_timeAccumulator;
    float m_averageReward;
    int m_iteration;
};

#endif // ANTTRAININGENGINE_H
