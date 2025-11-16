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
    int iteration = 0;
    bool fallbackActive = false;
};

enum class SceneProfile
{
    Demo = 0,
    AntTraining,
    PerspectiveTest
};

Q_DECLARE_METATYPE(AntTrainingMetrics)
Q_DECLARE_METATYPE(SceneProfile)

#endif // TRAININGTYPES_H
