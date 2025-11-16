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

struct LegDescriptor
{
    QVector3D anchor;
    QVector3D restDirection;
    float sideSign;
};

constexpr std::array<LegDescriptor, 4> kLegDescriptors = {{
    { QVector3D(0.32f, -0.05f, 0.28f), QVector3D(0.8f, -0.15f, 0.6f), 1.0f },   // front left
    { QVector3D(0.32f, -0.05f, -0.28f), QVector3D(0.8f, -0.15f, -0.6f), -1.0f }, // front right
    { QVector3D(-0.32f, -0.05f, 0.28f), QVector3D(-0.8f, -0.15f, 0.6f), 1.0f },  // back left
    { QVector3D(-0.32f, -0.05f, -0.28f), QVector3D(-0.8f, -0.15f, -0.6f), -1.0f } // back right
}};

QMatrix4x4 buildSegmentMatrix(const QVector3D& start, const QVector3D& direction, float length)
{
    QVector3D dir = direction;
    if (dir.lengthSquared() < 1e-6f) {
        dir = QVector3D(0.0f, -1.0f, 0.0f);
    }
    dir.normalize();
    QVector3D center = start + dir * (length * 0.5f);
    QQuaternion rotation = QQuaternion::rotationTo(QVector3D(0.0f, 1.0f, 0.0f), dir);
    QMatrix4x4 matrix;
    matrix.translate(center);
    matrix.rotate(rotation);
    return matrix;
}
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
    const LegDescriptor& descriptor = kLegDescriptors[legIndex];

    const QVector3D hipAnchorWorld = pose.torsoPosition + pose.torsoRotation.rotatedVector(descriptor.anchor);
    QVector3D hipAxisWorld = pose.torsoRotation.rotatedVector(QVector3D(0.0f, 1.0f, 0.0f));
    if (hipAxisWorld.lengthSquared() < 1e-6f) {
        hipAxisWorld = QVector3D(0.0f, 1.0f, 0.0f);
    }
    hipAxisWorld.normalize();

    QVector3D restDirection = pose.torsoRotation.rotatedVector(descriptor.restDirection);
    if (restDirection.lengthSquared() < 1e-6f) {
        restDirection = QVector3D(descriptor.sideSign, -0.1f, descriptor.sideSign * 0.4f);
    }
    restDirection.normalize();

    QQuaternion hipRotation = QQuaternion::fromAxisAndAngle(hipAxisWorld, qRadiansToDegrees(hipAngle));
    QVector3D upperDirection = hipRotation.rotatedVector(restDirection).normalized();
    m_instances[upperIndex].modelMatrix = buildSegmentMatrix(hipAnchorWorld, upperDirection, AntSceneController::Dimensions::UpperLegLength);

    const QVector3D kneePosition = hipAnchorWorld + upperDirection * AntSceneController::Dimensions::UpperLegLength;
    QVector3D downward = pose.torsoRotation.rotatedVector(QVector3D(0.0f, -1.0f, 0.0f));
    if (downward.lengthSquared() < 1e-6f) {
        downward = QVector3D(0.0f, -1.0f, 0.0f);
    }
    downward.normalize();

    QVector3D planeNormal = QVector3D::crossProduct(upperDirection, downward);
    if (planeNormal.lengthSquared() < 1e-6f) {
        planeNormal = pose.torsoRotation.rotatedVector(QVector3D(0.0f, 0.0f, descriptor.sideSign));
    }
    planeNormal.normalize();

    QQuaternion kneeRotation = QQuaternion::fromAxisAndAngle(planeNormal, qRadiansToDegrees(kneeAngle));
    QVector3D lowerDirection = kneeRotation.rotatedVector(downward).normalized();
    m_instances[lowerIndex].modelMatrix = buildSegmentMatrix(kneePosition, lowerDirection, AntSceneController::Dimensions::LowerLegLength);
}
