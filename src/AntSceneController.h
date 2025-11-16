// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef ANTSCENECONTROLLER_H
#define ANTSCENECONTROLLER_H

#include <QVector>
#include <array>

#include "SceneTypes.h"
#include "AntTrainingEngine.h"

class AntSceneController
{
public:
    struct MeshSlots
    {
        int floor = -1;
        int torso = -1;
        int upperLeg = -1;
        int lowerLeg = -1;
    };

    struct Dimensions
    {
        static constexpr float TorsoRadius = 0.32f;
        static constexpr float UpperLegLength = 0.45f;
        static constexpr float LowerLegLength = 0.55f;
        static constexpr float UpperLegRadius = 0.09f;
        static constexpr float LowerLegRadius = 0.07f;
    };

    struct UpdateResult
    {
        bool metricsChanged = false;
        AntTrainingMetrics metrics;
    };

    AntSceneController();

    void setMeshSlots(const MeshSlots& meshSlots);
    UpdateResult update();
    const QVector<SceneRenderInstance>& instances() const { return m_instances; }

private:
    void ensureInstances();
    void updateFromPose(const AntTrainingEngine::PoseSnapshot& pose);
    void updateLeg(int legIndex, float hipAngle, float kneeAngle, const AntTrainingEngine::PoseSnapshot& pose);

    MeshSlots m_slots;
    QVector<SceneRenderInstance> m_instances;
    AntTrainingEngine m_engine;
    AntTrainingMetrics m_metrics;
};

#endif // ANTSCENECONTROLLER_H
