// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef SIMPLESCENECONTROLLER_H
#define SIMPLESCENECONTROLLER_H

#include <QVector>

#include "SceneTypes.h"
#include "SimpleTrainingEngine.h"

class SimpleSceneController
{
public:
    struct MeshSlots
    {
        int floor = -1;
        int base = -1;
        int pole = -1;
        int tip = -1;
    };

    struct UpdateResult
    {
        bool metricsChanged = false;
        SimpleTrainingMetrics metrics;
    };

    struct Dimensions
    {
        static constexpr float BaseRadius = 0.20f;
        static constexpr float PoleLength = 1.4f;
        static constexpr float PoleRadius = 0.10f;
        static constexpr float TipRadius = 0.18f;
    };

    SimpleSceneController();

    void setMeshSlots(const MeshSlots& meshSlots);
    UpdateResult update();
    const QVector<SceneRenderInstance>& instances() const { return m_instances; }

private:
    void ensureInstances();
    void updateFromPose(const SimpleTrainingEngine::PoseSnapshot& pose);

    SimpleTrainingEngine m_engine;
    QVector<SceneRenderInstance> m_instances;
    MeshSlots m_slots;
    SimpleTrainingMetrics m_metrics;
};

#endif // SIMPLESCENECONTROLLER_H
