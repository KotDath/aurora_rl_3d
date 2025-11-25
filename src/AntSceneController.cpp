// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#include "AntSceneController.h"

#include <QtMath>

namespace {
constexpr int kFloorIndex = 0;
constexpr int kTorsoIndex = 1;
constexpr int kLegStartIndex = 2;
constexpr int kLegSegmentsPerLeg = 2;
constexpr int kTotalInstances = kLegStartIndex + kLegSegmentsPerLeg * 4;

}

AntSceneController::AntSceneController() = default;

void AntSceneController::setMeshSlots(const MeshSlots& meshSlots)
{
    m_slots = meshSlots;
    ensureInstances();
}

AntSceneController::UpdateResult AntSceneController::update()
{
    UpdateResult result;
    result.metricsChanged = true;
    m_engine.step(result.metrics);
    m_metrics = result.metrics;
    updateFromPose(m_engine.currentPose());
    return result;
}

void AntSceneController::ensureInstances()
{
    if (m_instances.size() == kTotalInstances) {
        return;
    }

    m_instances.resize(kTotalInstances);
    if (m_slots.floor >= 0) {
        m_instances[kFloorIndex].meshId = m_slots.floor;
    }
    if (m_slots.torso >= 0) {
        m_instances[kTorsoIndex].meshId = m_slots.torso;
    }
    for (int leg = 0; leg < 4; ++leg) {
        const int upperIndex = kLegStartIndex + leg * kLegSegmentsPerLeg;
        const int lowerIndex = upperIndex + 1;
        if (m_slots.upperLeg >= 0) {
            m_instances[upperIndex].meshId = m_slots.upperLeg;
        }
        if (m_slots.lowerLeg >= 0) {
            m_instances[lowerIndex].meshId = m_slots.lowerLeg;
        }
    }
}

void AntSceneController::updateFromPose(const AntTrainingEngine::PoseSnapshot& pose)
{
    ensureInstances();

    // Floor
    QMatrix4x4 floorMatrix;
    floorMatrix.translate(0.0f, 0.0f, 0.0f);
    floorMatrix.scale(20.0f, 0.02f, 20.0f);
    m_instances[kFloorIndex].modelMatrix = floorMatrix;

    // Torso
    QMatrix4x4 torsoMatrix;
    if (pose.torsoPose.modelMatrix.isIdentity()) {
        torsoMatrix.translate(pose.torsoPosition);
        torsoMatrix.rotate(pose.torsoRotation);
    } else {
        torsoMatrix = pose.torsoPose.modelMatrix;
    }
    m_instances[kTorsoIndex].modelMatrix = torsoMatrix;

    for (int leg = 0; leg < 4; ++leg) {
        const int upperIndex = kLegStartIndex + leg * kLegSegmentsPerLeg;
        const int lowerIndex = upperIndex + 1;
        m_instances[upperIndex].modelMatrix = pose.upperLegPoses[leg].modelMatrix;
        m_instances[lowerIndex].modelMatrix = pose.lowerLegPoses[leg].modelMatrix;
    }
}
