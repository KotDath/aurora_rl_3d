// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#include "RenderWindow.h"
#include "Renderer.h"

#include <QDebug>
#include <QMetaObject>
#include <QtGlobal>

RenderWindow::RenderWindow(QQuickItem* parent)
    : QQuickFramebufferObject(parent)
    , m_sceneProfile(SceneProfile::Demo)
    , m_cameraInput(0.0, 0.0)
{
    setFlag(QQuickItem::ItemHasContents, true);
}

RenderWindow::~RenderWindow()
{
    qDeleteAll(m_objects);
    m_objects.clear();
}

QQuickFramebufferObject::Renderer* RenderWindow::createRenderer() const
{
    return new OpenGLRenderer();
}

QQmlListProperty<SceneObject> RenderWindow::objects()
{
    return QQmlListProperty<SceneObject>(this, nullptr,
                                       &RenderWindow::appendObject,
                                       &RenderWindow::objectCount,
                                       &RenderWindow::objectAt,
                                       &RenderWindow::clearObjects);
}

int RenderWindow::objectCount() const
{
    return m_objects.size();
}

void RenderWindow::addObject(SceneObject* object)
{
    if (object) {
        object->setParent(this);
        m_objects.append(object);

        emit objectAdded(m_objects.size() - 1);
        emit objectCountChanged();

        update();
    }
}

void RenderWindow::removeObject(int index)
{
    if (index >= 0 && index < m_objects.size()) {
        delete m_objects.takeAt(index);

        emit objectRemoved(index);
        emit objectCountChanged();

        update();
    }
}

void RenderWindow::clearObjects()
{
    qDeleteAll(m_objects);
    m_objects.clear();

    emit objectCountChanged();
    update();
}

void RenderWindow::setCameraInput(const QPointF& input)
{
    auto clamp = [](qreal v) {
        return qMin<qreal>(1.0f, qMax<qreal>(-1.0f, v));
    };
    QPointF clamped(clamp(input.x()), clamp(input.y()));
    if (m_cameraInput == clamped) {
        return;
    }
    m_cameraInput = clamped;
    emit cameraInputChanged();
    update();
}

void RenderWindow::addCameraLookDelta(qreal deltaX, qreal deltaY)
{
    m_cameraLookDelta.rx() += deltaX;
    m_cameraLookDelta.ry() += deltaY;
    update();
}

void RenderWindow::addCameraHeightDelta(qreal delta)
{
    m_cameraHeightDelta += delta;
    update();
}

QPointF RenderWindow::takeCameraLookDelta()
{
    QPointF delta = m_cameraLookDelta;
    m_cameraLookDelta = QPointF(0.0, 0.0);
    return delta;
}

qreal RenderWindow::takeCameraHeightDelta()
{
    qreal delta = m_cameraHeightDelta;
    m_cameraHeightDelta = 0.0;
    return delta;
}

SceneObject* RenderWindow::getObject(int index) const
{
    if (index >= 0 && index < m_objects.size()) {
        return m_objects[index];
    }
    return nullptr;
}

void RenderWindow::setSceneProfile(SceneProfile profile)
{
    if (m_sceneProfile == profile) {
        return;
    }
    m_sceneProfile = profile;
    emit sceneProfileChanged();
    update();
}

void RenderWindow::appendObject(QQmlListProperty<SceneObject>* list, SceneObject* object)
{
    RenderWindow* window = qobject_cast<RenderWindow*>(list->object);
    if (window && object) {
        window->m_objects.append(object);
        emit window->objectAdded(window->m_objects.size() - 1);
        emit window->objectCountChanged();
        window->update();
    }
}

int RenderWindow::objectCount(QQmlListProperty<SceneObject>* list)
{
    RenderWindow* window = qobject_cast<RenderWindow*>(list->object);
    return window ? window->m_objects.size() : 0;
}

SceneObject* RenderWindow::objectAt(QQmlListProperty<SceneObject>* list, int index)
{
    RenderWindow* window = qobject_cast<RenderWindow*>(list->object);
    return (window && index >= 0 && index < window->m_objects.size()) ? window->m_objects[index] : nullptr;
}

void RenderWindow::clearObjects(QQmlListProperty<SceneObject>* list)
{
    RenderWindow* window = qobject_cast<RenderWindow*>(list->object);
    if (window) {
        qDeleteAll(window->m_objects);
        window->m_objects.clear();
        emit window->objectCountChanged();
        window->update();
    }
}

const QVector<SceneObject*>& RenderWindow::sceneObjects() const
{
    return m_objects;
}

void RenderWindow::publishAntMetrics(const AntTrainingMetrics& metrics)
{
    m_pendingMetrics = metrics;
    if (!m_metricsQueued) {
        m_metricsQueued = true;
        QMetaObject::invokeMethod(this, "flushAntMetrics", Qt::QueuedConnection);
    }
}

void RenderWindow::flushAntMetrics()
{
    m_metricsQueued = false;
    applyAntMetrics(m_pendingMetrics);
}

void RenderWindow::applyAntMetrics(const AntTrainingMetrics& metrics)
{
    m_antMetrics = metrics;
    emit antMetricsChanged();
}

void RenderWindow::publishSimpleMetrics(const SimpleTrainingMetrics& metrics)
{
    m_pendingSimpleMetrics = metrics;
    if (!m_simpleMetricsQueued) {
        m_simpleMetricsQueued = true;
        QMetaObject::invokeMethod(this, "flushSimpleMetrics", Qt::QueuedConnection);
    }
}

void RenderWindow::flushSimpleMetrics()
{
    m_simpleMetricsQueued = false;
    applySimpleMetrics(m_pendingSimpleMetrics);
}

void RenderWindow::applySimpleMetrics(const SimpleTrainingMetrics& metrics)
{
    m_simpleMetrics = metrics;
    emit simpleMetricsChanged();
}
