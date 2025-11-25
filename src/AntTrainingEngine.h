// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef ANTTRAININGENGINE_H
#define ANTTRAININGENGINE_H

#include <array>
#include <memory>

#include <QQuaternion>
#include <QMatrix4x4>
#include <QElapsedTimer>
#include <QVector3D>

#include "TrainingTypes.h"

#define AURORA_RL3D_HAS_MUJOCO 1

class AntTrainingEngine
{
public:
struct PoseSnapshot
{
    struct SegmentPose {
        QMatrix4x4 modelMatrix;
    };

    QVector3D torsoPosition{0.0f, 0.6f, 0.0f};
    QQuaternion torsoRotation;
    std::array<float, 8> jointAngles{};
    SegmentPose torsoPose;
    std::array<SegmentPose, 4> upperLegPoses{};
    std::array<SegmentPose, 4> lowerLegPoses{};
};

    AntTrainingEngine();
    ~AntTrainingEngine();

    void step(AntTrainingMetrics& metrics);
    const PoseSnapshot& currentPose() const { return m_pose; }

private:
    void stepFallback(AntTrainingMetrics& metrics);
    void stepMuJoCo(AntTrainingMetrics& metrics);
    struct MuJoCoContext;
    std::unique_ptr<MuJoCoContext> m_mujoco;

    PoseSnapshot m_pose;
    bool m_usingFallback;
    float m_timeAccumulator;
    float m_averageReward;
    int m_iteration;
    QElapsedTimer m_stepTimer;
    AntTrainingMetrics m_lastMetrics{};
    bool m_hasLastMetrics{false};
};

#endif // ANTTRAININGENGINE_H
