// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef RENDERER_H
#define RENDERER_H

#include <QtQuick/QQuickFramebufferObject>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLFramebufferObject>
#include <QMatrix4x4>
#include <QtOpenGL/QGLFunctions>
#include <QSize>

#include "SceneObject.h"
#include "MeshData.h"
#include "GameLoop.h"

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

    QOpenGLShaderProgram* m_shaderProgram;
    QOpenGLBuffer m_vertexBuffer;

    QMatrix4x4 m_projectionMatrix;
    QSize m_viewportSize;

    QVector<SceneObject*> m_sceneObjects;
    QVector<MeshData> m_meshes;
    QVector<int> m_meshOffsets;
    GameLoop m_gameLoop;
    bool m_glInitialized;
    float m_rotationAngle;

    // Game loop timing
    qint64 m_lastTimeMs;
    double m_simTime;

    int m_positionAttribute;
    int m_colorAttribute;
    int m_matrixUniform;
};

#endif // RENDERER_H
