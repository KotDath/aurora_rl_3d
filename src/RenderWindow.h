// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef RENDERWINDOW_H
#define RENDERWINDOW_H

#include <QtQuick/QQuickFramebufferObject>
#include <QQmlListProperty>
#include "SceneObject.h"
#include "TrainingTypes.h"

class OpenGLRenderer;

class RenderWindow : public QQuickFramebufferObject
{
    Q_OBJECT
    Q_PROPERTY(QQmlListProperty<SceneObject> objects READ objects)
    Q_PROPERTY(int objectCount READ objectCount NOTIFY objectCountChanged)
    Q_PROPERTY(SceneProfile sceneProfile READ sceneProfile WRITE setSceneProfile NOTIFY sceneProfileChanged)
    Q_PROPERTY(qreal antLastReward READ antLastReward NOTIFY antMetricsChanged)
    Q_PROPERTY(qreal antAverageReward READ antAverageReward NOTIFY antMetricsChanged)
    Q_PROPERTY(qreal antEpisodeProgress READ antEpisodeProgress NOTIFY antMetricsChanged)
    Q_PROPERTY(int antIteration READ antIteration NOTIFY antMetricsChanged)
    Q_PROPERTY(bool antFallbackActive READ antFallbackActive NOTIFY antMetricsChanged)

public:
    Q_ENUM(SceneProfile)

    enum SceneProfileValue {
        SceneDemo = static_cast<int>(SceneProfile::Demo),
        SceneAntTraining = static_cast<int>(SceneProfile::AntTraining)
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

    // Internal method for renderer synchronization
    const QVector<SceneObject*>& sceneObjects() const;
    SceneProfile sceneProfile() const { return m_sceneProfile; }

    qreal antLastReward() const { return m_antMetrics.reward; }
    qreal antAverageReward() const { return m_antMetrics.averageReward; }
    qreal antEpisodeProgress() const { return m_antMetrics.episodeProgress; }
    int antIteration() const { return m_antMetrics.iteration; }
    bool antFallbackActive() const { return m_antMetrics.fallbackActive; }

    void publishAntMetrics(const AntTrainingMetrics& metrics);

signals:
    void objectCountChanged();
    void objectAdded(int index);
    void objectRemoved(int index);
    void sceneProfileChanged();
    void antMetricsChanged();

private slots:
    void flushAntMetrics();

private:
    static void appendObject(QQmlListProperty<SceneObject>* list, SceneObject* object);
    static int objectCount(QQmlListProperty<SceneObject>* list);
    static SceneObject* objectAt(QQmlListProperty<SceneObject>* list, int index);
    static void clearObjects(QQmlListProperty<SceneObject>* list);

    void applyAntMetrics(const AntTrainingMetrics& metrics);

    QVector<SceneObject*> m_objects;
    SceneProfile m_sceneProfile;
    AntTrainingMetrics m_antMetrics;
    AntTrainingMetrics m_pendingMetrics;
    bool m_metricsQueued = false;
};

#endif // RENDERWINDOW_H
