// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef RENDERER_H
#define RENDERER_H

#include <QtQuick/QQuickFramebufferObject>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLFramebufferObject>
#include <QMatrix4x4>
#include <QVector3D>
#include <QtOpenGL/QGLFunctions>
#include <QSize>
#include <QPointer>
#include <QColor>
#include <memory>

#include "SceneObject.h"
#include "MeshData.h"
#include "GameLoop.h"
#include "SceneTypes.h"
#include "AntSceneController.h"
#include "TrainingTypes.h"

class RenderWindow;

class OpenGLRenderer : public QQuickFramebufferObject::Renderer
{
public:
    OpenGLRenderer();
    ~OpenGLRenderer();

    void render() override;
    void synchronize(QQuickFramebufferObject* item) override;
    QOpenGLFramebufferObject* createFramebufferObject(const QSize& size) override;

public slots:
    void updateSceneObjects(const QVector<SceneObject*>& objects);

private:
    void addNewObjects(const QVector<SceneObject*>& objects);

private:
    void initializeGL();
    void cleanupGL();
    void setupShaders();
    void setupGeometry();
    void updateProjectionMatrix(const QSize& size);
    void applyProfile(SceneProfile profile);
    void setupDemoMeshes();
    void setupAntMeshes();
    void setupPerspectiveTestMeshes();
    void updatePerspectiveScene();

    struct PerspectiveObject
    {
        int meshId = -1;
        QVector3D position;
        QVector3D scale = QVector3D(1.0f, 1.0f, 1.0f);
        QVector3D rotationAxis = QVector3D(0.0f, 1.0f, 0.0f);
        float rotationSpeed = 0.0f;
        float baseRotation = 0.0f;
    };

    QOpenGLShaderProgram* m_shaderProgram;
    QOpenGLBuffer m_vertexBuffer;

    QMatrix4x4 m_projectionMatrix;
    QMatrix4x4 m_viewMatrix;
    QSize m_viewportSize;

    QVector<SceneObject*> m_sceneObjects;
    QVector<MeshData> m_meshes;
    QVector<int> m_meshOffsets;
    QVector<PerspectiveObject> m_perspectiveObjects;
    QVector<SceneRenderInstance> m_perspectiveInstances;
    GameLoop m_gameLoop;
    std::unique_ptr<AntSceneController> m_antController;
    bool m_glInitialized;
    float m_rotationAngle;

    // Game loop timing
    qint64 m_lastTimeMs;
    double m_simTime;

    int m_positionAttribute;
    int m_colorAttribute;
    int m_normalAttribute;
    int m_matrixUniform;
    int m_normalMatrixUniform;
    int m_lightDirectionUniform;
    int m_lightColorUniform;
    int m_ambientColorUniform;
    int m_viewPositionUniform;
    int m_shininessUniform;
    SceneProfile m_activeProfile;
    QPointer<RenderWindow> m_windowItem;
    QColor m_clearColor;
};

#endif // RENDERER_H
