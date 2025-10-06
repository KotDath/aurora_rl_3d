// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#include "SceneObject.h"

SceneObject::SceneObject(QObject* parent)
    : QObject(parent)
    , m_position(0.0f, 0.0f, 0.0f)
    , m_scale(1.0f, 1.0f, 1.0f)
    , m_rotation(1.0f, 0.0f, 0.0f, 0.0f)
    , m_visible(true)
    , m_meshId(-1)
    , m_materialId(-1)
    , m_initialRotation(0.0f)
{
}

QMatrix4x4 SceneObject::transformMatrix() const
{
    QMatrix4x4 matrix;
    matrix.translate(m_position);
    matrix.rotate(m_rotation);
    matrix.scale(m_scale);
    return matrix;
}