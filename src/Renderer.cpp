// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#include "Renderer.h"
#include "RenderWindow.h"

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGlobal>
#include <QDebug>
#include <QVector>
#include <QDateTime>
#include <numeric>

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
    , m_lastTimeMs(0)
    , m_simTime(0.0)
    , m_positionAttribute(-1)
    , m_colorAttribute(-1)
    , m_matrixUniform(-1)
    , m_activeProfile(SceneProfile::Demo)
{
    m_viewMatrix.setToIdentity();
    m_viewMatrix.translate(0.0f, 0.0f, -5.0f);
    m_clearColor = QColor::fromRgbF(0.1f, 0.1f, 0.2f, 1.0f);
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

    // Calculate delta time for game loop
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    double deltaTime = (m_lastTimeMs == 0) ? (1.0/60.0) : double(currentTime - m_lastTimeMs) / 1000.0;
    m_lastTimeMs = currentTime;

    // Clamp delta time to avoid huge steps after pause
    if (deltaTime > 0.1) deltaTime = 0.1;

    m_simTime += deltaTime;

    const QVector<SceneRenderInstance>* simulatedInstances = nullptr;
    if (m_activeProfile == SceneProfile::Demo) {
        m_gameLoop.update(deltaTime, m_simTime);
        simulatedInstances = &m_gameLoop.renderInstances();
    } else if (m_activeProfile == SceneProfile::AntTraining && m_antController) {
        AntSceneController::UpdateResult updateResult = m_antController->update();
        simulatedInstances = &m_antController->instances();
        if (updateResult.metricsChanged && m_windowItem) {
            if (RenderWindow* window = m_windowItem.data()) {
                window->publishAntMetrics(updateResult.metrics);
            }
        }
    }

    QOpenGLFunctions* functions = QOpenGLContext::currentContext()->functions();
    functions->glClearColor(m_clearColor.redF(), m_clearColor.greenF(), m_clearColor.blueF(), m_clearColor.alphaF());
    functions->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    functions->glEnable(GL_DEPTH_TEST);

    m_shaderProgram->bind();
    m_vertexBuffer.bind();

    functions->glEnableVertexAttribArray(m_positionAttribute);
    functions->glEnableVertexAttribArray(m_colorAttribute);

    QMatrix4x4 viewMatrix = m_viewMatrix;

    auto drawInstance = [&](int meshId, const QMatrix4x4& modelMatrix) {
        if (meshId < 0 || meshId >= m_meshes.size() || meshId >= m_meshOffsets.size()) {
            return;
        }

        const MeshData& mesh = m_meshes[meshId];
        const int vertexOffset = m_meshOffsets[meshId];
        const quintptr baseOffset = quintptr(vertexOffset) * sizeof(Vertex);

        functions->glVertexAttribPointer(m_positionAttribute, 3, GL_FLOAT, GL_FALSE,
                                         sizeof(Vertex), reinterpret_cast<const void*>(baseOffset));
        functions->glVertexAttribPointer(m_colorAttribute, 4, GL_FLOAT, GL_FALSE,
                                         sizeof(Vertex), reinterpret_cast<const void*>(baseOffset + offsetof(Vertex, color)));

        QMatrix4x4 mvpMatrix = m_projectionMatrix * viewMatrix * modelMatrix;
        m_shaderProgram->setUniformValue(m_matrixUniform, mvpMatrix);

        functions->glDrawArrays(mesh.primitiveType(), 0, mesh.vertexCount());
    };

    if (simulatedInstances) {
        for (const SceneRenderInstance& instance : *simulatedInstances) {
            if (!instance.visible) {
                continue;
            }
            drawInstance(instance.meshId, instance.modelMatrix);
        }
    }

    // Render user-provided scene objects (if any)
    for (const SceneObject* obj : m_sceneObjects) {
        if (!obj || !obj->visible()) {
            continue;
        }

        QMatrix4x4 objectMatrix = obj->transformMatrix();
        objectMatrix.rotate(m_simTime * obj->rotationSpeed(), obj->rotationAxis());

        drawInstance(obj->meshId(), objectMatrix);
    }

    functions->glDisableVertexAttribArray(m_positionAttribute);
    functions->glDisableVertexAttribArray(m_colorAttribute);

    m_vertexBuffer.release();
    m_shaderProgram->release();

    // Schedule next frame to continue the game loop
    update();
}

void OpenGLRenderer::synchronize(QQuickFramebufferObject* item)
{
    RenderWindow* window = qobject_cast<RenderWindow*>(item);
    if (!window) {
        return;
    }

    m_windowItem = window;

    const SceneProfile requestedProfile = window->sceneProfile();
    if (m_meshes.isEmpty() || m_activeProfile != requestedProfile) {
        applyProfile(requestedProfile);

        if (m_activeProfile == SceneProfile::Demo && window->sceneObjects().isEmpty()) {
            m_sceneObjects.clear();

            SceneObject* obj1 = new SceneObject();
            obj1->setMeshId(0);
            obj1->setPosition(QVector3D(-0.3f, 0.0f, 0.0f));
            obj1->setInitialRotation(45.0f);
            m_sceneObjects << obj1;

            SceneObject* obj2 = new SceneObject();
            obj2->setMeshId(0);
            obj2->setPosition(QVector3D(0.3f, 0.0f, 0.0f));
            obj2->setInitialRotation(120.0f);
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

void OpenGLRenderer::applyProfile(SceneProfile profile)
{
    qDeleteAll(m_sceneObjects);
    m_sceneObjects.clear();
    m_meshes.clear();
    m_meshOffsets.clear();
    m_antController.reset();

    m_activeProfile = profile;

    if (profile == SceneProfile::Demo) {
        setupDemoMeshes();
        m_clearColor = QColor::fromRgbF(0.1f, 0.1f, 0.2f, 1.0f);
        m_viewMatrix.setToIdentity();
        m_viewMatrix.translate(0.0f, 0.0f, -5.0f);
    } else {
        setupAntMeshes();
        m_clearColor = QColor::fromRgbF(0.02f, 0.03f, 0.05f, 1.0f);
        m_viewMatrix.setToIdentity();
        m_viewMatrix.lookAt(QVector3D(4.0f, 3.0f, 6.0f), QVector3D(0.0f, 0.3f, 0.0f), QVector3D(0.0f, 1.0f, 0.0f));
    }

    setupGeometry();
}

void OpenGLRenderer::setupDemoMeshes()
{
    MeshData redCube = MeshData::createColoredCube(0.8f, QVector4D(1.0f, 0.2f, 0.2f, 1.0f));
    MeshData greenCube = MeshData::createColoredCube(0.8f, QVector4D(0.2f, 1.0f, 0.2f, 1.0f));
    MeshData blueCube = MeshData::createColoredCube(0.8f, QVector4D(0.2f, 0.2f, 1.0f, 1.0f));
    MeshData yellowCube = MeshData::createColoredCube(0.8f, QVector4D(1.0f, 1.0f, 0.2f, 1.0f));
    MeshData purpleCube = MeshData::createColoredCube(0.8f, QVector4D(1.0f, 0.2f, 1.0f, 1.0f));
    MeshData cyanCube = MeshData::createColoredCube(0.8f, QVector4D(0.2f, 1.0f, 1.0f, 1.0f));

    m_meshes << redCube << greenCube << blueCube << yellowCube << purpleCube << cyanCube;

    QVector<int> meshIds;
    meshIds.reserve(m_meshes.size());
    for (int i = 0; i < m_meshes.size(); ++i) {
        meshIds.append(i);
    }
    m_gameLoop.setMeshIds(meshIds);
}

void OpenGLRenderer::setupAntMeshes()
{
    const int floorId = m_meshes.size();
    m_meshes << MeshData::createPlane(20.0f, 20.0f, QVector4D(0.15f, 0.18f, 0.2f, 1.0f));

    const int torsoId = m_meshes.size();
    m_meshes << MeshData::createColoredCube(0.9f, QVector4D(0.7f, 0.4f, 1.0f, 1.0f));

    const int legUpperId = m_meshes.size();
    m_meshes << MeshData::createColoredCube(0.5f, QVector4D(0.9f, 0.5f, 0.3f, 1.0f));

    const int legLowerId = m_meshes.size();
    m_meshes << MeshData::createColoredCube(0.4f, QVector4D(0.3f, 0.8f, 1.0f, 1.0f));

    m_antController = std::make_unique<AntSceneController>();
    AntSceneController::MeshSlots meshSlots;
    meshSlots.floor = floorId;
    meshSlots.torso = torsoId;
    meshSlots.upperLeg = legUpperId;
    meshSlots.lowerLeg = legLowerId;
    m_antController->setMeshSlots(meshSlots);
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

    // Собираем все вершины из всех мешей и считаем смещения
    QVector<Vertex> allVertices;
    allVertices.reserve(std::accumulate(m_meshes.begin(), m_meshes.end(), 0, [](int sum, const MeshData& mesh) {
        return sum + mesh.vertexCount();
    }));

    m_meshOffsets.resize(m_meshes.size());

    int currentOffset = 0;
    for (int i = 0; i < m_meshes.size(); ++i) {
        m_meshOffsets[i] = currentOffset;
        const QVector<Vertex>& vertices = m_meshes[i].vertices();
        allVertices << vertices;
        currentOffset += vertices.size();
    }

    if (!allVertices.isEmpty()) {
        m_vertexBuffer.allocate(allVertices.constData(), allVertices.size() * sizeof(Vertex));
    } else {
        m_vertexBuffer.allocate(nullptr, 0);
    }
    m_vertexBuffer.release();
}

void OpenGLRenderer::updateProjectionMatrix(const QSize& size)
{
    float aspectRatio = float(size.width()) / float(size.height());
    m_projectionMatrix.setToIdentity();
    // Use perspective projection instead of orthographic
    m_projectionMatrix.perspective(45.0f, aspectRatio, 0.1f, 100.0f);
}
