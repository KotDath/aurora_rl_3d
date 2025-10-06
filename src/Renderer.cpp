// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#include "Renderer.h"
#include "RenderWindow.h"

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QDebug>
#include <QVector>

static const char* vertexShaderSource = R"(
#version 100
attribute vec3 aPosition;
attribute vec4 aColor;
uniform mat4 uMatrix;
varying vec4 vColor;

void main()
{
    gl_Position = uMatrix * vec4(aPosition, 1.0);
    vColor = aColor;
}
)";

static const char* fragmentShaderSource = R"(
#version 100
precision mediump float;
varying vec4 vColor;

void main()
{
    gl_FragColor = vColor;
}
)";

OpenGLRenderer::OpenGLRenderer()
    : m_shaderProgram(nullptr)
    , m_glInitialized(false)
    , m_rotationAngle(0.0f)
    , m_positionAttribute(-1)
    , m_colorAttribute(-1)
    , m_matrixUniform(-1)
{
}

OpenGLRenderer::~OpenGLRenderer()
{
    cleanupGL();
}

void OpenGLRenderer::render()
{
    if (!m_glInitialized) {
        initializeGL();
    }

    // Static objects - no rotation animation

    QOpenGLFunctions* functions = QOpenGLContext::currentContext()->functions();
    functions->glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    functions->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    functions->glEnable(GL_DEPTH_TEST);

    m_shaderProgram->bind();
    m_vertexBuffer.bind();

    functions->glEnableVertexAttribArray(m_positionAttribute);
    functions->glEnableVertexAttribArray(m_colorAttribute);

    // Set up vertex attributes
    functions->glVertexAttribPointer(m_positionAttribute, 3, GL_FLOAT, GL_FALSE,
                                     sizeof(Vertex), (void*)0);
    functions->glVertexAttribPointer(m_colorAttribute, 4, GL_FLOAT, GL_FALSE,
                                     sizeof(Vertex), (void*)offsetof(Vertex, color));

    // Render each scene object
    for (const SceneObject* obj : m_sceneObjects) {
        if (!obj || !obj->visible() || obj->meshId() < 0 || obj->meshId() >= m_meshes.size()) {
            continue;
        }

        const MeshData& mesh = m_meshes[obj->meshId()];

        // Apply static rotation to the object (only initial rotation, no animation)
        QMatrix4x4 objectMatrix = obj->transformMatrix();
        QMatrix4x4 rotationMatrix;
        rotationMatrix.rotate(obj->initialRotation(), 0.0f, 1.0f, 0.0f); // Rotate around Y axis
        rotationMatrix.rotate(obj->initialRotation() * 0.7f, 1.0f, 0.0f, 0.0f); // Also rotate around X axis

        // Move camera back
        QMatrix4x4 viewMatrix;
        viewMatrix.translate(0.0f, 0.0f, -5.0f);

        QMatrix4x4 mvpMatrix = m_projectionMatrix * viewMatrix * objectMatrix * rotationMatrix;

        m_shaderProgram->setUniformValue(m_matrixUniform, mvpMatrix);

        functions->glDrawArrays(mesh.primitiveType(), 0, mesh.vertexCount());
    }

    functions->glDisableVertexAttribArray(m_positionAttribute);
    functions->glDisableVertexAttribArray(m_colorAttribute);

    m_vertexBuffer.release();
    m_shaderProgram->release();
}

void OpenGLRenderer::synchronize(QQuickFramebufferObject* item)
{
    RenderWindow* window = qobject_cast<RenderWindow*>(item);
    if (!window) {
        return;
    }

    // Initialize meshes if empty
    if (m_meshes.isEmpty()) {
        m_meshes.clear();

        // Создаем куб одинакового размера
        MeshData cube = MeshData::createColoredCube(0.8f);
        m_meshes << cube;

        // Create default objects if no objects in window
        if (window->sceneObjects().isEmpty()) {
            SceneObject* obj1 = new SceneObject();
            obj1->setMeshId(0);
            obj1->setPosition(QVector3D(-0.3f, 0.0f, 0.0f));
            obj1->setInitialRotation(45.0f); // Fixed rotation for first cube
            m_sceneObjects << obj1;

            SceneObject* obj2 = new SceneObject();
            obj2->setMeshId(0);
            obj2->setPosition(QVector3D(0.3f, 0.0f, 0.0f));
            obj2->setInitialRotation(120.0f); // Different rotation for second cube
            m_sceneObjects << obj2;
        }
    }

    // Update scene objects from window
    updateSceneObjects(window->sceneObjects());
}

QOpenGLFramebufferObject* OpenGLRenderer::createFramebufferObject(const QSize& size)
{
    m_viewportSize = size;
    updateProjectionMatrix(size);

    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    format.setSamples(4);

    return new QOpenGLFramebufferObject(size, format);
}

void OpenGLRenderer::initializeGL()
{
    QOpenGLFunctions* functions = QOpenGLContext::currentContext()->functions();
    functions->glEnable(GL_DEPTH_TEST);
    functions->glEnable(GL_CULL_FACE);
    functions->glCullFace(GL_BACK);

    setupShaders();

    m_glInitialized = true;
}

void OpenGLRenderer::cleanupGL()
{
    // Clean up scene objects
    qDeleteAll(m_sceneObjects);
    m_sceneObjects.clear();

    if (m_vertexBuffer.isCreated()) {
        m_vertexBuffer.destroy();
    }

    if (m_shaderProgram) {
        delete m_shaderProgram;
        m_shaderProgram = nullptr;
    }
}

void OpenGLRenderer::updateSceneObjects(const QVector<SceneObject*>& objects)
{
    // Remove objects that are no longer in the window
    for (int i = m_sceneObjects.size() - 1; i >= 0; --i) {
        bool found = false;
        for (SceneObject* windowObj : objects) {
            if (windowObj && m_sceneObjects[i]->position() == windowObj->position()) {
                found = true;
                break;
            }
        }
        if (!found) {
            delete m_sceneObjects.takeAt(i);
        }
    }

    // Add new objects that weren't in the renderer before
    for (SceneObject* windowObj : objects) {
        if (!windowObj) continue;

        bool exists = false;
        for (SceneObject* rendererObj : m_sceneObjects) {
            if (rendererObj->position() == windowObj->position()) {
                exists = true;
                break;
            }
        }

        if (!exists) {
            SceneObject* newObj = new SceneObject();
            newObj->setPosition(windowObj->position());
            newObj->setScale(windowObj->scale());
            newObj->setRotation(windowObj->rotation());
            newObj->setVisible(windowObj->visible());
            newObj->setMeshId(windowObj->meshId());
            newObj->setMaterialId(windowObj->materialId());
            newObj->setInitialRotation(windowObj->initialRotation());
            m_sceneObjects << newObj;
        }
    }

    setupGeometry();
}

void OpenGLRenderer::addNewObjects(const QVector<SceneObject*>& objects)
{
    for (SceneObject* obj : objects) {
        if (obj) {
            SceneObject* newObj = new SceneObject();
            newObj->setPosition(obj->position());
            newObj->setScale(obj->scale());
            newObj->setRotation(obj->rotation());
            newObj->setVisible(obj->visible());
            newObj->setMeshId(obj->meshId());
            newObj->setMaterialId(obj->materialId());
            newObj->setInitialRotation(obj->initialRotation());
            m_sceneObjects << newObj;
        }
    }

    setupGeometry();
}

void OpenGLRenderer::setupShaders()
{
    if (m_shaderProgram) {
        delete m_shaderProgram;
    }

    m_shaderProgram = new QOpenGLShaderProgram();

    if (!m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource)) {
        qWarning() << "Failed to compile vertex shader:" << m_shaderProgram->log();
        return;
    }

    if (!m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource)) {
        qWarning() << "Failed to compile fragment shader:" << m_shaderProgram->log();
        return;
    }

    if (!m_shaderProgram->link()) {
        qWarning() << "Failed to link shader program:" << m_shaderProgram->log();
        return;
    }

    m_positionAttribute = m_shaderProgram->attributeLocation("aPosition");
    m_colorAttribute = m_shaderProgram->attributeLocation("aColor");
    m_matrixUniform = m_shaderProgram->uniformLocation("uMatrix");
}

void OpenGLRenderer::setupGeometry()
{
    if (!m_vertexBuffer.isCreated()) {
        m_vertexBuffer.create();
    }

    m_vertexBuffer.bind();

    // Собираем все вершины из всех мешей
    QVector<Vertex> allVertices;
    for (const MeshData& mesh : m_meshes) {
        allVertices << mesh.vertices();
    }

    m_vertexBuffer.allocate(allVertices.constData(), allVertices.size() * sizeof(Vertex));
    m_vertexBuffer.release();
}

void OpenGLRenderer::updateProjectionMatrix(const QSize& size)
{
    float aspectRatio = float(size.width()) / float(size.height());
    m_projectionMatrix.setToIdentity();
    // Use perspective projection instead of orthographic
    m_projectionMatrix.perspective(45.0f, aspectRatio, 0.1f, 100.0f);
}