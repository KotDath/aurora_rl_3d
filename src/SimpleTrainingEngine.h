// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef SIMPLETRAININGENGINE_H
#define SIMPLETRAININGENGINE_H

#include <QMatrix4x4>
#include <QElapsedTimer>
#include <memory>
#include <random>

#include "TrainingTypes.h"

class SimpleTrainingEngine
{
public:
    struct PoseSnapshot
    {
        QMatrix4x4 baseMatrix;
        QMatrix4x4 poleMatrix;
        QMatrix4x4 tipMatrix;
        qreal angle = 0.0;
        qreal angularVelocity = 0.0;
    };

    SimpleTrainingEngine();
    ~SimpleTrainingEngine();

    void step(SimpleTrainingMetrics& metrics);
    const PoseSnapshot& currentPose() const { return m_pose; }

private:
    void resetEpisode();
    void updatePose();

    struct PendulumContext;
    std::unique_ptr<PendulumContext> m_ctx;
    std::mt19937 m_rng;
    float m_angle;
    float m_angularVelocity;
    PoseSnapshot m_pose;
    int m_episodeStep = 0;
    int m_iteration = 0;
    float m_averageReward = 0.0f;
    QElapsedTimer m_stepTimer;
    SimpleTrainingMetrics m_lastMetrics;
};

#endif // SIMPLETRAININGENGINE_H
