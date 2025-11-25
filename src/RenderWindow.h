// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef RENDERWINDOW_H
#define RENDERWINDOW_H

#include <QtQuick/QQuickFramebufferObject>
#include <QQmlListProperty>
#include <QPointF>
#include "SceneObject.h"
#include "TrainingTypes.h"

class OpenGLRenderer;

class RenderWindow : public QQuickFramebufferObject
{
    Q_OBJECT
    Q_PROPERTY(QQmlListProperty<SceneObject> objects READ objects)
    Q_PROPERTY(int objectCount READ objectCount NOTIFY objectCountChanged)
    Q_PROPERTY(SceneProfile sceneProfile READ sceneProfile WRITE setSceneProfile NOTIFY sceneProfileChanged)
    Q_PROPERTY(QPointF cameraInput READ cameraInput WRITE setCameraInput NOTIFY cameraInputChanged)
    Q_PROPERTY(qreal antLastReward READ antLastReward NOTIFY antMetricsChanged)
    Q_PROPERTY(qreal antAverageReward READ antAverageReward NOTIFY antMetricsChanged)
    Q_PROPERTY(qreal antEpisodeProgress READ antEpisodeProgress NOTIFY antMetricsChanged)
    Q_PROPERTY(int antIteration READ antIteration NOTIFY antMetricsChanged)
    Q_PROPERTY(bool antFallbackActive READ antFallbackActive NOTIFY antMetricsChanged)
    Q_PROPERTY(qreal simpleLastReward READ simpleLastReward NOTIFY simpleMetricsChanged)
    Q_PROPERTY(qreal simpleAverageReward READ simpleAverageReward NOTIFY simpleMetricsChanged)
    Q_PROPERTY(qreal simpleEpisodeProgress READ simpleEpisodeProgress NOTIFY simpleMetricsChanged)
    Q_PROPERTY(int simpleIteration READ simpleIteration NOTIFY simpleMetricsChanged)
    Q_PROPERTY(qreal simpleAngle READ simpleAngle NOTIFY simpleMetricsChanged)
    Q_PROPERTY(qreal simpleAngularVelocity READ simpleAngularVelocity NOTIFY simpleMetricsChanged)

public:
    Q_ENUM(SceneProfile)

    enum SceneProfileValue {
        SceneDemo = static_cast<int>(SceneProfile::Demo),
        SceneAntTraining = static_cast<int>(SceneProfile::AntTraining),
        SceneSimpleTraining = static_cast<int>(SceneProfile::SimpleTraining),
        ScenePerspectiveTest = static_cast<int>(SceneProfile::PerspectiveTest)
    };
    Q_ENUM(SceneProfileValue)

    RenderWindow(QQuickItem* parent = nullptr);
    ~RenderWindow();

    QQuickFramebufferObject::Renderer* createRenderer() const override;

    QQmlListProperty<SceneObject> objects();
    int objectCount() const;

    Q_INVOKABLE void addObject(SceneObject* object);
    Q_INVOKABLE void removeObject(int index);
    Q_INVOKABLE void clearObjects();
    Q_INVOKABLE SceneObject* getObject(int index) const;
    Q_INVOKABLE void setSceneProfile(SceneProfile profile);
    Q_INVOKABLE void addCameraLookDelta(qreal deltaX, qreal deltaY);
    Q_INVOKABLE void addCameraHeightDelta(qreal delta);

    // Internal method for renderer synchronization
    const QVector<SceneObject*>& sceneObjects() const;
    SceneProfile sceneProfile() const { return m_sceneProfile; }
    QPointF cameraInput() const { return m_cameraInput; }
    void setCameraInput(const QPointF& input);
    QPointF takeCameraLookDelta();
    qreal takeCameraHeightDelta();

    qreal antLastReward() const { return m_antMetrics.reward; }
    qreal antAverageReward() const { return m_antMetrics.averageReward; }
    qreal antEpisodeProgress() const { return m_antMetrics.episodeProgress; }
    int antIteration() const { return m_antMetrics.iteration; }
    bool antFallbackActive() const { return m_antMetrics.fallbackActive; }

    qreal simpleLastReward() const { return m_simpleMetrics.reward; }
    qreal simpleAverageReward() const { return m_simpleMetrics.averageReward; }
    qreal simpleEpisodeProgress() const { return m_simpleMetrics.episodeProgress; }
    int simpleIteration() const { return m_simpleMetrics.iteration; }
    qreal simpleAngle() const { return m_simpleMetrics.angle; }
    qreal simpleAngularVelocity() const { return m_simpleMetrics.angularVelocity; }

    void publishAntMetrics(const AntTrainingMetrics& metrics);
    void publishSimpleMetrics(const SimpleTrainingMetrics& metrics);

signals:
    void objectCountChanged();
    void objectAdded(int index);
    void objectRemoved(int index);
    void sceneProfileChanged();
    void cameraInputChanged();
    void antMetricsChanged();
    void simpleMetricsChanged();

private slots:
    void flushAntMetrics();
    void flushSimpleMetrics();

private:
    static void appendObject(QQmlListProperty<SceneObject>* list, SceneObject* object);
    static int objectCount(QQmlListProperty<SceneObject>* list);
    static SceneObject* objectAt(QQmlListProperty<SceneObject>* list, int index);
    static void clearObjects(QQmlListProperty<SceneObject>* list);

    void applyAntMetrics(const AntTrainingMetrics& metrics);
    void applySimpleMetrics(const SimpleTrainingMetrics& metrics);

    QVector<SceneObject*> m_objects;
    SceneProfile m_sceneProfile;
    QPointF m_cameraInput;
    QPointF m_cameraLookDelta;
    qreal m_cameraHeightDelta = 0.0;
    AntTrainingMetrics m_antMetrics;
    AntTrainingMetrics m_pendingMetrics;
    bool m_metricsQueued = false;
    SimpleTrainingMetrics m_simpleMetrics;
    SimpleTrainingMetrics m_pendingSimpleMetrics;
    bool m_simpleMetricsQueued = false;
};

#endif // RENDERWINDOW_H
