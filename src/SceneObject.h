// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef SCENEOBJECT_H
#define SCENEOBJECT_H

#include <QVector3D>
#include <QQuaternion>
#include <QMatrix4x4>
#include <QObject>

class SceneObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVector3D position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(QVector3D scale READ scale WRITE setScale NOTIFY scaleChanged)
    Q_PROPERTY(QQuaternion rotation READ rotation WRITE setRotation NOTIFY rotationChanged)
    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(int meshId READ meshId WRITE setMeshId NOTIFY meshIdChanged)
    Q_PROPERTY(int materialId READ materialId WRITE setMaterialId NOTIFY materialIdChanged)
    Q_PROPERTY(float initialRotation READ initialRotation WRITE setInitialRotation NOTIFY initialRotationChanged)
    Q_PROPERTY(QVector3D rotationAxis READ rotationAxis WRITE setRotationAxis NOTIFY rotationAxisChanged)
    Q_PROPERTY(float rotationSpeed READ rotationSpeed WRITE setRotationSpeed NOTIFY rotationSpeedChanged)

public:
    SceneObject(QObject* parent = nullptr);
    Q_DISABLE_COPY(SceneObject)

    QVector3D position() const { return m_position; }
    void setPosition(const QVector3D& position) { if (m_position != position) { m_position = position; emit positionChanged(); } }

    QVector3D scale() const { return m_scale; }
    void setScale(const QVector3D& scale) { if (m_scale != scale) { m_scale = scale; emit scaleChanged(); } }

    QQuaternion rotation() const { return m_rotation; }
    void setRotation(const QQuaternion& rotation) { if (m_rotation != rotation) { m_rotation = rotation.normalized(); emit rotationChanged(); } }

    bool visible() const { return m_visible; }
    void setVisible(bool visible) { if (m_visible != visible) { m_visible = visible; emit visibleChanged(); } }

    int meshId() const { return m_meshId; }
    void setMeshId(int meshId) { if (m_meshId != meshId) { m_meshId = meshId; emit meshIdChanged(); } }

    int materialId() const { return m_materialId; }
    void setMaterialId(int materialId) { if (m_materialId != materialId) { m_materialId = materialId; emit materialIdChanged(); } }

    float initialRotation() const { return m_initialRotation; }
    void setInitialRotation(float rotation) { if (m_initialRotation != rotation) { m_initialRotation = rotation; emit initialRotationChanged(); } }

    QVector3D rotationAxis() const { return m_rotationAxis; }
    void setRotationAxis(const QVector3D& axis) { if (m_rotationAxis != axis) { m_rotationAxis = axis; emit rotationAxisChanged(); } }

    float rotationSpeed() const { return m_rotationSpeed; }
    void setRotationSpeed(float speed) { if (m_rotationSpeed != speed) { m_rotationSpeed = speed; emit rotationSpeedChanged(); } }

    QMatrix4x4 transformMatrix() const;

signals:
    void positionChanged();
    void scaleChanged();
    void rotationChanged();
    void visibleChanged();
    void meshIdChanged();
    void materialIdChanged();
    void initialRotationChanged();
    void rotationAxisChanged();
    void rotationSpeedChanged();

private:
    QVector3D m_position;
    QVector3D m_scale;
    QQuaternion m_rotation;
    bool m_visible;
    int m_meshId;
    int m_materialId;
    float m_initialRotation;
    QVector3D m_rotationAxis;
    float m_rotationSpeed;
};

#endif // SCENEOBJECT_H