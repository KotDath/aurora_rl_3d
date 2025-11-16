// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#include "Renderer.h"
#include "RenderWindow.h"
#include "MeshUtils.h"

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGlobal>
#include <QDebug>
#include <QVector>
#include <QDateTime>
#include <QVector2D>
#include <numeric>

namespace {
constexpr int kPositionAttribute = 0;
constexpr int kColorAttribute = 1;
constexpr int kNormalAttribute = 2;
}

uint qHash(MaterialType key, uint seed = 0)
{
    return ::qHash(static_cast<int>(key), seed);
}

static const char* vertexShaderSource = R"(
#version 100
attribute vec3 aPosition;
attribute vec4 aColor;
attribute vec3 aNormal;

uniform mat4 uMatrix;
uniform mat4 uModelMatrix;
uniform mat3 uNormalMatrix;

uniform vec3 uLightDirection;
uniform vec3 uLightColor;
uniform vec3 uAmbientColor;
uniform vec3 uViewPosition;

varying vec4 vColor;
varying vec3 vNormal;
varying vec3 vWorldPosition;
varying vec3 vLightDirection;
varying vec3 vLightColor;
varying vec3 vAmbientColor;
varying vec3 vViewPosition;

void main()
{
    gl_Position = uMatrix * vec4(aPosition, 1.0);
    vColor = aColor;
    vNormal = normalize(uNormalMatrix * aNormal);
    vWorldPosition = (uModelMatrix * vec4(aPosition, 1.0)).xyz;
    vLightDirection = uLightDirection;
    vLightColor = uLightColor;
    vAmbientColor = uAmbientColor;
    vViewPosition = uViewPosition;
}
)";

static const char* fragmentShaderSource = R"(
#version 100
precision mediump float;

varying vec4 vColor;
varying vec3 vNormal;
varying vec3 vWorldPosition;
varying vec3 vLightDirection;
varying vec3 vLightColor;
varying vec3 vAmbientColor;
varying vec3 vViewPosition;

uniform float uShininess;

void main()
{
    // Normalize inputs
    vec3 normal = normalize(vNormal);
    vec3 lightDir = normalize(vLightDirection);
    vec3 viewDir = normalize(vViewPosition - vWorldPosition);

    // Ambient lighting
    vec3 ambient = vAmbientColor * vColor.rgb;

    // Diffuse lighting
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * vLightColor * vColor.rgb;

    // Specular lighting
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), uShininess);
    vec3 specular = spec * vLightColor * 0.5; // Reduced specular intensity

    // Combine lighting components
    vec3 result = ambient + diffuse + specular;

    gl_FragColor = vec4(result, vColor.a);
}
)";

static const char* checkerFragmentShaderSource = R"(
#version 100
precision mediump float;

varying vec3 vWorldPosition;

uniform vec2 uCheckerScale;
uniform vec4 uCheckerColorLight;
uniform vec4 uCheckerColorDark;

void main()
{
    vec2 tiled = vWorldPosition.xz * uCheckerScale;
    float pattern = mod(floor(tiled.x) + floor(tiled.y), 2.0);
    vec4 color = mix(uCheckerColorLight, uCheckerColorDark, pattern);
    gl_FragColor = color;
}
)";

OpenGLRenderer::OpenGLRenderer()
    : m_boundShaderProgram(nullptr)
    , m_glInitialized(false)
    , m_rotationAngle(0.0f)
    , m_lastTimeMs(0)
    , m_simTime(0.0)
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
    } else if (m_activeProfile == SceneProfile::PerspectiveTest) {
        updatePerspectiveScene();
        simulatedInstances = &m_perspectiveInstances;
    }

    QOpenGLFunctions* functions = QOpenGLContext::currentContext()->functions();
    functions->glClearColor(m_clearColor.redF(), m_clearColor.greenF(), m_clearColor.blueF(), m_clearColor.alphaF());
    functions->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    functions->glEnable(GL_DEPTH_TEST);

    functions->glEnableVertexAttribArray(kPositionAttribute);
    functions->glEnableVertexAttribArray(kColorAttribute);
    functions->glEnableVertexAttribArray(kNormalAttribute);

    m_vertexBuffer.bind();
    m_boundShaderProgram = nullptr;

    QMatrix4x4 viewMatrix = m_viewMatrix;
    QMatrix4x4 inverseViewMatrix = viewMatrix.inverted();
    QVector3D viewPosition = inverseViewMatrix.column(3).toVector3D();
    QVector3D cameraForward = inverseViewMatrix.mapVector(QVector3D(0.0f, 0.0f, -1.0f));
    if (qFuzzyIsNull(cameraForward.lengthSquared())) {
        cameraForward = QVector3D(0.0f, 0.0f, -1.0f);
    }
    cameraForward.normalize();
    QVector3D lightDirection = -cameraForward;

    auto drawInstance = [&](int meshId, const QMatrix4x4& modelMatrix) {
        if (meshId < 0 || meshId >= m_meshes.size() || meshId >= m_meshOffsets.size()) {
            return;
        }

        const MeshData& mesh = m_meshes[meshId];
        auto shaderIt = m_shaderPrograms.find(mesh.materialType());
        if (shaderIt == m_shaderPrograms.end() || !shaderIt->program) {
            return;
        }

        ShaderProgramBinding& binding = shaderIt.value();
        if (binding.program != m_boundShaderProgram) {
            binding.program->bind();
            m_boundShaderProgram = binding.program;
        }

        const int vertexOffset = m_meshOffsets[meshId];
        const quintptr baseOffset = quintptr(vertexOffset) * sizeof(Vertex);

        functions->glVertexAttribPointer(kPositionAttribute, 3, GL_FLOAT, GL_FALSE,
                                         sizeof(Vertex), reinterpret_cast<const void*>(baseOffset));
        functions->glVertexAttribPointer(kColorAttribute, 4, GL_FLOAT, GL_FALSE,
                                         sizeof(Vertex), reinterpret_cast<const void*>(baseOffset + offsetof(Vertex, color)));
        functions->glVertexAttribPointer(kNormalAttribute, 3, GL_FLOAT, GL_FALSE,
                                         sizeof(Vertex), reinterpret_cast<const void*>(baseOffset + offsetof(Vertex, normal)));

        QMatrix4x4 mvpMatrix = m_projectionMatrix * viewMatrix * modelMatrix;
        if (binding.matrixUniform >= 0) {
            binding.program->setUniformValue(binding.matrixUniform, mvpMatrix);
        }
        if (binding.modelMatrixUniform >= 0) {
            binding.program->setUniformValue(binding.modelMatrixUniform, modelMatrix);
        }
        if (binding.normalMatrixUniform >= 0) {
            binding.program->setUniformValue(binding.normalMatrixUniform, modelMatrix.normalMatrix());
        }
        if (binding.lightDirectionUniform >= 0) {
            binding.program->setUniformValue(binding.lightDirectionUniform, lightDirection);
        }
        if (binding.lightColorUniform >= 0) {
            binding.program->setUniformValue(binding.lightColorUniform, QVector3D(1.0f, 1.0f, 1.0f));
        }
        if (binding.ambientColorUniform >= 0) {
            binding.program->setUniformValue(binding.ambientColorUniform, QVector3D(0.2f, 0.2f, 0.25f));
        }
        if (binding.viewPositionUniform >= 0) {
            binding.program->setUniformValue(binding.viewPositionUniform, viewPosition);
        }
        if (binding.shininessUniform >= 0) {
            binding.program->setUniformValue(binding.shininessUniform, 32.0f);
        }

        const MaterialSettings& materialSettings = mesh.materialSettings();
        if (binding.checkerScaleUniform >= 0) {
            binding.program->setUniformValue(binding.checkerScaleUniform, materialSettings.tiling);
        }
        if (binding.checkerColorLightUniform >= 0) {
            binding.program->setUniformValue(binding.checkerColorLightUniform, materialSettings.colorLight);
        }
        if (binding.checkerColorDarkUniform >= 0) {
            binding.program->setUniformValue(binding.checkerColorDarkUniform, materialSettings.colorDark);
        }

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

    if (m_boundShaderProgram) {
        m_boundShaderProgram->release();
        m_boundShaderProgram = nullptr;
    }

    m_vertexBuffer.release();

    functions->glDisableVertexAttribArray(kPositionAttribute);
    functions->glDisableVertexAttribArray(kColorAttribute);
    functions->glDisableVertexAttribArray(kNormalAttribute);

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
    m_perspectiveObjects.clear();
    m_perspectiveInstances.clear();

    m_activeProfile = profile;

    if (profile == SceneProfile::Demo) {
        setupDemoMeshes();
        m_clearColor = QColor::fromRgbF(0.1f, 0.1f, 0.2f, 1.0f);
        m_viewMatrix.setToIdentity();
        m_viewMatrix.translate(0.0f, 0.0f, -5.0f);
    } else if (profile == SceneProfile::AntTraining) {
        setupAntMeshes();
        m_clearColor = QColor::fromRgbF(0.02f, 0.03f, 0.05f, 1.0f);
        m_viewMatrix.setToIdentity();
        m_viewMatrix.lookAt(QVector3D(2.5f, 2.0f, 3.5f), QVector3D(0.0f, 0.0f, 0.0f), QVector3D(0.0f, 1.0f, 0.0f));
    } else if (profile == SceneProfile::PerspectiveTest) {
        setupPerspectiveTestMeshes();
        m_clearColor = QColor::fromRgbF(0.015f, 0.015f, 0.03f, 1.0f);
        m_viewMatrix.setToIdentity();
        m_viewMatrix.lookAt(QVector3D(6.0f, 4.0f, 8.0f), QVector3D(0.0f, 0.6f, 0.0f), QVector3D(0.0f, 1.0f, 0.0f));
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
    MeshData floorMesh = MeshData::createPlane(20.0f, 20.0f, QVector4D(0.15f, 0.18f, 0.2f, 1.0f));
    MaterialSettings checkerMaterial;
    checkerMaterial.type = MaterialType::Checkerboard;
    checkerMaterial.colorLight = QVector4D(1.0f, 1.0f, 1.0f, 1.0f);
    checkerMaterial.colorDark = QVector4D(0.05f, 0.05f, 0.05f, 1.0f);
    checkerMaterial.tiling = QVector2D(2.0f, 2.0f);
    floorMesh.setMaterialSettings(checkerMaterial);
    m_meshes << floorMesh;

    const int torsoId = m_meshes.size();
    m_meshes << MeshUtils::createUVSphere(24, 32, AntSceneController::Dimensions::TorsoRadius,
                                          QVector4D(0.7f, 0.4f, 1.0f, 1.0f));

    const int legUpperId = m_meshes.size();
    m_meshes << MeshUtils::createCylinder(24, AntSceneController::Dimensions::UpperLegRadius,
                                          AntSceneController::Dimensions::UpperLegLength,
                                          QVector4D(0.9f, 0.5f, 0.3f, 1.0f));

    const int legLowerId = m_meshes.size();
    m_meshes << MeshUtils::createCylinder(24, AntSceneController::Dimensions::LowerLegRadius,
                                          AntSceneController::Dimensions::LowerLegLength,
                                          QVector4D(0.3f, 0.8f, 1.0f, 1.0f));

    m_antController = std::make_unique<AntSceneController>();
    AntSceneController::MeshSlots meshSlots;
    meshSlots.floor = floorId;
    meshSlots.torso = torsoId;
    meshSlots.upperLeg = legUpperId;
    meshSlots.lowerLeg = legLowerId;
    m_antController->setMeshSlots(meshSlots);
}

void OpenGLRenderer::setupPerspectiveTestMeshes()
{
    m_perspectiveObjects.clear();

    auto appendPerspectiveObject = [this](int meshId, const QVector3D& position, const QVector3D& scale,
                                          const QVector3D& rotationAxis, float rotationSpeed, float baseRotation) {
        PerspectiveObject obj;
        obj.meshId = meshId;
        obj.position = position;
        obj.scale = scale;
        QVector3D axis = rotationAxis;
        if (qFuzzyIsNull(axis.lengthSquared())) {
            axis = QVector3D(0.0f, 1.0f, 0.0f);
        }
        obj.rotationAxis = axis.normalized();
        obj.rotationSpeed = rotationSpeed;
        obj.baseRotation = baseRotation;
        m_perspectiveObjects.append(obj);
    };

    const int groundId = m_meshes.size();
    MeshData groundMesh = MeshUtils::createPlane(28.0f, 28.0f, 1, 1, QVector4D(0.08f, 0.09f, 0.11f, 1.0f));
    MaterialSettings groundMaterial;
    groundMaterial.type = MaterialType::Checkerboard;
    groundMaterial.colorLight = QVector4D(0.95f, 0.95f, 0.95f, 1.0f);
    groundMaterial.colorDark = QVector4D(0.05f, 0.05f, 0.05f, 1.0f);
    groundMaterial.tiling = QVector2D(1.5f, 1.5f);
    groundMesh.setMaterialSettings(groundMaterial);
    m_meshes << groundMesh;
    appendPerspectiveObject(groundId, QVector3D(0.0f, -0.6f, 0.0f), QVector3D(1.0f, 1.0f, 1.0f),
                           QVector3D(0.0f, 1.0f, 0.0f), 0.0f, 0.0f);

    const int cubeWarmId = m_meshes.size();
    m_meshes << MeshUtils::createCube(1.0f, 1.0f, 1.0f, QVector4D(1.0f, 0.48f, 0.2f, 1.0f));
    appendPerspectiveObject(cubeWarmId, QVector3D(-3.0f, 0.4f, -2.2f), QVector3D(1.0f, 1.0f, 1.0f),
                           QVector3D(0.0f, 1.0f, 0.0f), 25.0f, 10.0f);

    const int cubeCoolId = m_meshes.size();
    m_meshes << MeshUtils::createCube(1.5f, 0.8f, 1.0f, QVector4D(0.3f, 0.9f, 1.0f, 1.0f));
    appendPerspectiveObject(cubeCoolId, QVector3D(-1.2f, 0.35f, 1.8f), QVector3D(1.0f, 1.0f, 1.0f),
                           QVector3D(0.0f, 1.0f, 0.0f), 18.0f, -20.0f);

    const int cylinderTallId = m_meshes.size();
    m_meshes << MeshUtils::createCylinder(32, 0.4f, 2.2f, QVector4D(0.9f, 0.6f, 0.3f, 1.0f));
    appendPerspectiveObject(cylinderTallId, QVector3D(2.5f, 0.5f, -1.2f), QVector3D(1.0f, 1.0f, 1.0f),
                           QVector3D(1.0f, 0.0f, 0.0f), 12.0f, 0.0f);

    const int cylinderWideId = m_meshes.size();
    m_meshes << MeshUtils::createCylinder(24, 0.6f, 1.2f, QVector4D(0.45f, 0.55f, 1.0f, 1.0f));
    appendPerspectiveObject(cylinderWideId, QVector3D(1.8f, -0.0f, 2.6f), QVector3D(1.0f, 1.0f, 1.0f),
                           QVector3D(0.0f, 0.0f, 1.0f), 20.0f, 30.0f);

    const int sphereLargeId = m_meshes.size();
    m_meshes << MeshUtils::createUVSphere(24, 36, 0.9f, QVector4D(0.95f, 0.95f, 1.0f, 1.0f));
    appendPerspectiveObject(sphereLargeId, QVector3D(0.0f, 1.0f, 0.0f), QVector3D(1.0f, 1.0f, 1.0f),
                           QVector3D(0.0f, 1.0f, 0.4f), 10.0f, 0.0f);

    const int sphereSmallId = m_meshes.size();
    m_meshes << MeshUtils::createUVSphere(20, 28, 0.5f, QVector4D(1.0f, 0.7f, 0.95f, 1.0f));
    appendPerspectiveObject(sphereSmallId, QVector3D(3.0f, 0.7f, 1.5f), QVector3D(1.0f, 1.0f, 1.0f),
                           QVector3D(0.2f, 1.0f, 0.0f), 35.0f, 45.0f);

    const int cubeStackId = m_meshes.size();
    m_meshes << MeshUtils::createCube(0.6f, 1.4f, 0.6f, QVector4D(0.5f, 0.8f, 0.4f, 1.0f));
    appendPerspectiveObject(cubeStackId, QVector3D(-4.5f, 0.3f, 1.0f), QVector3D(1.0f, 1.0f, 1.0f),
                           QVector3D(0.0f, 1.0f, 0.2f), 22.0f, -35.0f);

    m_perspectiveInstances.resize(m_perspectiveObjects.size());
}

void OpenGLRenderer::updatePerspectiveScene()
{
    if (m_perspectiveInstances.size() != m_perspectiveObjects.size()) {
        m_perspectiveInstances.resize(m_perspectiveObjects.size());
    }

    for (int i = 0; i < m_perspectiveObjects.size(); ++i) {
        const PerspectiveObject& obj = m_perspectiveObjects[i];
        SceneRenderInstance& instance = m_perspectiveInstances[i];
        instance.meshId = obj.meshId;
        instance.visible = true;

        QMatrix4x4 model;
        model.translate(obj.position);

        const float angle = obj.baseRotation + (obj.rotationSpeed * m_simTime);
        if (!qFuzzyIsNull(obj.rotationSpeed) || !qFuzzyIsNull(obj.baseRotation)) {
            model.rotate(angle, obj.rotationAxis);
        }

        model.scale(obj.scale);
        instance.modelMatrix = model;
    }
}

void OpenGLRenderer::cleanupShaderPrograms()
{
    for (auto it = m_shaderPrograms.begin(); it != m_shaderPrograms.end(); ++it) {
        if (it.value().program) {
            delete it.value().program;
            it.value().program = nullptr;
        }
    }
    m_shaderPrograms.clear();
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

    cleanupShaderPrograms();
    m_boundShaderProgram = nullptr;
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
    cleanupShaderPrograms();

    auto createProgram = [&](MaterialType material, const char* fragmentSource) {
        ShaderProgramBinding binding;
        binding.program = new QOpenGLShaderProgram();

        if (!binding.program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource)) {
            qWarning() << "Failed to compile vertex shader:" << binding.program->log();
            delete binding.program;
            binding.program = nullptr;
            return;
        }

        if (!binding.program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentSource)) {
            qWarning() << "Failed to compile fragment shader:" << binding.program->log();
            delete binding.program;
            binding.program = nullptr;
            return;
        }

        binding.program->bindAttributeLocation("aPosition", kPositionAttribute);
        binding.program->bindAttributeLocation("aColor", kColorAttribute);
        binding.program->bindAttributeLocation("aNormal", kNormalAttribute);

        if (!binding.program->link()) {
            qWarning() << "Failed to link shader program:" << binding.program->log();
            delete binding.program;
            binding.program = nullptr;
            return;
        }

        binding.matrixUniform = binding.program->uniformLocation("uMatrix");
        binding.modelMatrixUniform = binding.program->uniformLocation("uModelMatrix");
        binding.normalMatrixUniform = binding.program->uniformLocation("uNormalMatrix");
        binding.lightDirectionUniform = binding.program->uniformLocation("uLightDirection");
        binding.lightColorUniform = binding.program->uniformLocation("uLightColor");
        binding.ambientColorUniform = binding.program->uniformLocation("uAmbientColor");
        binding.viewPositionUniform = binding.program->uniformLocation("uViewPosition");
        binding.shininessUniform = binding.program->uniformLocation("uShininess");
        binding.checkerScaleUniform = binding.program->uniformLocation("uCheckerScale");
        binding.checkerColorLightUniform = binding.program->uniformLocation("uCheckerColorLight");
        binding.checkerColorDarkUniform = binding.program->uniformLocation("uCheckerColorDark");

        m_shaderPrograms.insert(material, binding);
    };

    createProgram(MaterialType::VertexColorPhong, fragmentShaderSource);
    createProgram(MaterialType::Checkerboard, checkerFragmentShaderSource);
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
    // Use perspective projection with wider FOV for better 3D visibility
    m_projectionMatrix.perspective(60.0f, aspectRatio, 0.1f, 100.0f);
}
