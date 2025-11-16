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

constexpr std::array<QVector3D, 4> kLegAnchors = {
    QVector3D(0.32f, -0.05f, 0.28f),
    QVector3D(0.32f, -0.05f, -0.28f),
    QVector3D(-0.32f, -0.05f, 0.28f),
    QVector3D(-0.32f, -0.05f, -0.28f)
};

constexpr std::array<float, 4> kLegDirections = { 1.0f, 1.0f, -1.0f, -1.0f };
constexpr std::array<float, 4> kSideSigns = { 1.0f, -1.0f, 1.0f, -1.0f };
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
    floorMatrix.translate(0.0f, -0.01f, 0.0f);
    floorMatrix.scale(20.0f, 0.02f, 20.0f);
    m_instances[kFloorIndex].modelMatrix = floorMatrix;

    // Torso
    QMatrix4x4 torsoMatrix;
    torsoMatrix.translate(pose.torsoPosition);
    torsoMatrix.rotate(pose.torsoRotation);
    torsoMatrix.scale(0.7f, 0.2f, 0.45f);
    m_instances[kTorsoIndex].modelMatrix = torsoMatrix;

    for (int leg = 0; leg < 4; ++leg) {
        const float hipAngle = pose.jointAngles[leg * 2];
        const float kneeAngle = pose.jointAngles[leg * 2 + 1];
        updateLeg(leg, hipAngle, kneeAngle, pose);
    }
}

void AntSceneController::updateLeg(int legIndex, float hipAngle, float kneeAngle, const AntTrainingEngine::PoseSnapshot& pose)
{
    const int upperIndex = kLegStartIndex + legIndex * kLegSegmentsPerLeg;
    const int lowerIndex = upperIndex + 1;

    QMatrix4x4 base;
    base.translate(pose.torsoPosition);
    base.rotate(pose.torsoRotation);
    base.translate(kLegAnchors[legIndex]);

    const float hipDeg = qRadiansToDegrees(hipAngle);
    const float kneeDeg = qRadiansToDegrees(kneeAngle);
    const float xDir = kLegDirections[legIndex];
    const float side = kSideSigns[legIndex];

    QMatrix4x4 upper = base;
    upper.rotate(hipDeg, 0.0f, 1.0f, 0.0f);
    upper.translate(xDir * 0.25f, 0.0f, 0.06f * side);
    upper.scale(0.5f, 0.08f, 0.08f);
    m_instances[upperIndex].modelMatrix = upper;

    QMatrix4x4 lower = base;
    lower.rotate(hipDeg, 0.0f, 1.0f, 0.0f);
    lower.translate(xDir * 0.5f, -0.05f, 0.06f * side);
    lower.rotate(kneeDeg, 0.0f, 0.0f, side);
    lower.translate(xDir * 0.24f, -0.18f, 0.0f);
    lower.scale(0.45f, 0.06f, 0.06f);
    m_instances[lowerIndex].modelMatrix = lower;
}
