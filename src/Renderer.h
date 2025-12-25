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
#include <QVector2D>
#include <QtOpenGL/QGLFunctions>
#include <QSize>
#include <QPointer>
#include <QColor>
#include <QHash>
#include <memory>

#include "SceneObject.h"
#include "MeshData.h"
#include "GameLoop.h"
#include "SceneTypes.h"
#include "AntSceneController.h"
#include "SimpleSceneController.h"
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
    void setupSimpleAgentMeshes();
    void setupPerspectiveTestMeshes();
    void updatePerspectiveScene();
    void createGizmos();
    void updateCameraFromInput(float deltaTime);
    void cleanupShaderPrograms();
    void setCameraView(const QVector3D& position, const QVector3D& target);

    struct PerspectiveObject
    {
        int meshId = -1;
        QVector3D position;
        QVector3D scale = QVector3D(1.0f, 1.0f, 1.0f);
        QVector3D rotationAxis = QVector3D(0.0f, 1.0f, 0.0f);
        float rotationSpeed = 0.0f;
        float baseRotation = 0.0f;
    };

    struct ShaderProgramBinding
    {
        QOpenGLShaderProgram* program = nullptr;
        int matrixUniform = -1;
        int modelMatrixUniform = -1;
        int normalMatrixUniform = -1;
        int lightDirectionUniform = -1;
        int lightColorUniform = -1;
        int ambientColorUniform = -1;
        int viewPositionUniform = -1;
        int shininessUniform = -1;
        int checkerScaleUniform = -1;
        int checkerColorLightUniform = -1;
        int checkerColorDarkUniform = -1;
    };

    QHash<MaterialType, ShaderProgramBinding> m_shaderPrograms;
    QOpenGLShaderProgram* m_boundShaderProgram;
    QOpenGLBuffer m_vertexBuffer;

    QMatrix4x4 m_projectionMatrix;
    QMatrix4x4 m_viewMatrix;
    QSize m_viewportSize;

    QVector<SceneObject*> m_sceneObjects;
    QVector<MeshData> m_meshes;
    QVector<int> m_meshOffsets;
    QVector<PerspectiveObject> m_perspectiveObjects;
    QVector<SceneRenderInstance> m_perspectiveInstances;
    QVector2D m_cameraInput;
    QVector2D m_cameraLookDelta;
    QVector3D m_cameraPosition;
    QVector3D m_cameraTarget;
    float m_cameraYaw;
    float m_cameraPitch;
    float m_cameraHeightDelta = 0.0f;
    bool m_followAntCamera = false;
    float m_followRadius = 8.0f;
    GameLoop m_gameLoop;
    std::unique_ptr<AntSceneController> m_antController;
    std::unique_ptr<SimpleSceneController> m_simpleController;
    bool m_glInitialized;
    float m_rotationAngle;

    // Game loop timing
    qint64 m_lastTimeMs;
    double m_simTime;

    SceneProfile m_activeProfile;
    QPointer<RenderWindow> m_windowItem;
    QColor m_clearColor;
};

#endif // RENDERER_H
