// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#include "SimpleSceneController.h"

#include <QtMath>

namespace {
constexpr int kFloorIndex = 0;
constexpr int kBaseIndex = 1;
constexpr int kPoleIndex = 2;
constexpr int kTipIndex = 3;
constexpr int kTotalInstances = 4;
}

SimpleSceneController::SimpleSceneController() = default;

void SimpleSceneController::setMeshSlots(const MeshSlots& meshSlots)
{
    m_slots = meshSlots;
    ensureInstances();
}

SimpleSceneController::UpdateResult SimpleSceneController::update()
{
    UpdateResult result;
    m_engine.step(result.metrics);
    result.metricsChanged = true;
    m_metrics = result.metrics;
    updateFromPose(m_engine.currentPose());
    return result;
}

void SimpleSceneController::ensureInstances()
{
    if (m_instances.size() == kTotalInstances) {
        return;
    }

    m_instances.resize(kTotalInstances);
    if (m_slots.floor >= 0) {
        m_instances[kFloorIndex].meshId = m_slots.floor;
    }
    if (m_slots.base >= 0) {
        m_instances[kBaseIndex].meshId = m_slots.base;
    }
    if (m_slots.pole >= 0) {
        m_instances[kPoleIndex].meshId = m_slots.pole;
    }
    if (m_slots.tip >= 0) {
        m_instances[kTipIndex].meshId = m_slots.tip;
    }
}

void SimpleSceneController::updateFromPose(const SimpleTrainingEngine::PoseSnapshot& pose)
{
    ensureInstances();

    // Floor
    QMatrix4x4 floorMatrix;
    floorMatrix.translate(0.0f, 0.0f, 0.0f);
    floorMatrix.scale(14.0f, 0.02f, 14.0f);
    m_instances[kFloorIndex].modelMatrix = floorMatrix;

    m_instances[kBaseIndex].modelMatrix = pose.baseMatrix;
    m_instances[kPoleIndex].modelMatrix = pose.poleMatrix;
    m_instances[kTipIndex].modelMatrix = pose.tipMatrix;
}
