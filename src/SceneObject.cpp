// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#include "SceneObject.h"
#include <QtGlobal>

SceneObject::SceneObject(QObject* parent)
    : QObject(parent)
    , m_position(0.0f, 0.0f, 0.0f)
    , m_scale(1.0f, 1.0f, 1.0f)
    , m_rotation(1.0f, 0.0f, 0.0f, 0.0f)
    , m_visible(true)
    , m_meshId(-1)
    , m_materialId(-1)
    , m_initialRotation(0.0f)
    , m_rotationAxis(
        QVector3D(
            (qrand() % 200 - 100) / 100.0f,  // -1 to 1
            (qrand() % 200 - 100) / 100.0f,  // -1 to 1
            (qrand() % 200 - 100) / 100.0f   // -1 to 1
        ).normalized()
    )
    , m_rotationSpeed(20.0f + (qrand() % 60)) // 20-80 degrees/second
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