// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef TRAININGTYPES_H
#define TRAININGTYPES_H

#include <QtGlobal>
#include <QMetaType>

struct AntTrainingMetrics
{
    qreal reward = 0.0;
    qreal averageReward = 0.0;
    qreal episodeProgress = 0.0;
    qreal health = 1.0;
    qreal height = 0.0;
    int iteration = 0;
    bool fallbackActive = false;
};

struct SimpleTrainingMetrics
{
    qreal reward = 0.0;
    qreal averageReward = 0.0;
    qreal episodeProgress = 0.0;
    int iteration = 0;
    qreal angle = 0.0;
    qreal angularVelocity = 0.0;
};

enum class SceneProfile
{
    Demo = 0,
    AntTraining,
    SimpleTraining,
    PerspectiveTest
};

Q_DECLARE_METATYPE(AntTrainingMetrics)
Q_DECLARE_METATYPE(SimpleTrainingMetrics)
Q_DECLARE_METATYPE(SceneProfile)

#endif // TRAININGTYPES_H
