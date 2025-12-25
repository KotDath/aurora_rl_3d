// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef ANTSCENECONTROLLER_H
#define ANTSCENECONTROLLER_H

#include <QVector>
#include <QObject>
#include <QThread>
#include <QTimer>
#include <array>

#include "SceneTypes.h"
#include "AntTrainingEngine.h"

class AntTrainerWorker : public QObject
{
    Q_OBJECT
public:
    explicit AntTrainerWorker(QObject* parent = nullptr);
    ~AntTrainerWorker();

public slots:
    void step();

signals:
    void metricsReady(const AntTrainingMetrics& metrics, const AntTrainingEngine::PoseSnapshot& pose);

private:
    AntTrainingEngine m_engine;
};

class AntSceneController
    : public QObject
{
    Q_OBJECT
public:
    struct MeshSlots
    {
        int floor = -1;
        int torso = -1;
        int upperLeg = -1;
        int lowerLeg = -1;
    };

    struct Dimensions
    {
        // Match MuJoCo ant.xml: torso sphere size 0.25, hip-to-knee ~0.28, knee-to-ankle ~0.20, capsule radius ~0.08
        static constexpr float TorsoRadius = 0.25f;
        static constexpr float UpperLegLength = 0.2828427124f; // sqrt(0.2^2 + 0.2^2)
        static constexpr float LowerLegLength = 0.5656854249f; // sqrt(0.4^2 + 0.4^2)
        static constexpr float UpperLegRadius = 0.08f;
        static constexpr float LowerLegRadius = 0.08f;
    };

    struct UpdateResult
    {
        bool metricsChanged = false;
        AntTrainingMetrics metrics;
    };

    explicit AntSceneController(QObject* parent = nullptr);
    ~AntSceneController();

    void setMeshSlots(const MeshSlots& meshSlots);
    UpdateResult update();
    const QVector<SceneRenderInstance>& instances() const { return m_instances; }
    QVector3D torsoPosition() const { return m_pose.torsoPosition; }

private:
    void ensureInstances();
    void updateFromPose(const AntTrainingEngine::PoseSnapshot& pose);

    MeshSlots m_slots;
    QVector<SceneRenderInstance> m_instances;
    AntTrainingEngine::PoseSnapshot m_pose{};
    AntTrainingMetrics m_metrics;
    bool m_metricsUpdated{false};

    QThread m_workerThread;
    AntTrainerWorker* m_worker{nullptr};
    QTimer m_tickTimer;

private slots:
    void handleMetrics(const AntTrainingMetrics& metrics, const AntTrainingEngine::PoseSnapshot& pose);
};

#endif // ANTSCENECONTROLLER_H
