// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#include "GameLoop.h"

#include <QtMath>
#include <QQuaternion>
#include <QVector3D>

#include <iostream>

class GameLoop::GameActor
{
public:
    virtual ~GameActor() = default;
    virtual void update(double deltaTime, double simTime) = 0;
    virtual SceneRenderInstance instance() const = 0;
};

namespace
{
    constexpr float kDefaultScale = 0.8f;

    using RenderInstance = SceneRenderInstance;

    RenderInstance makeInstance(int meshId)
    {
        RenderInstance instance;
        instance.meshId = meshId;
        instance.modelMatrix.setToIdentity();
        return instance;
    }

    class BaseActor : public GameLoop::GameActor
    {
    public:
        BaseActor(int meshIndex, const QVector3D& basePosition, float scale = kDefaultScale)
            : m_instance(makeInstance(meshIndex))
            , m_position(basePosition)
            , m_scale(scale)
        {
        }

        RenderInstance instance() const override
        {
            return m_instance;
        }

    protected:
        void buildMatrix(const QQuaternion& rotation, const QVector3D& additionalTranslation = QVector3D())
        {
            QMatrix4x4 matrix;
            matrix.translate(m_position + additionalTranslation);
            matrix.rotate(rotation);
            matrix.scale(m_scale);
            m_instance.modelMatrix = matrix;
        }

        void setVisible(bool visible)
        {
            m_instance.visible = visible;
        }

        RenderInstance m_instance;
        QVector3D m_position;
        float m_scale;
    };

    class RotatingActor : public BaseActor
    {
    public:
        RotatingActor(int meshIndex,
                      const QVector3D& position,
                      const QVector3D& axis,
                      float speedDegPerSec)
            : BaseActor(meshIndex, position)
            , m_axis(axis.normalized())
            , m_speed(speedDegPerSec)
            , m_angle(0.0f)
        {
        }

        void update(double deltaTime, double /*simTime*/) override
        {
            m_angle += float(deltaTime) * m_speed;
            QQuaternion rotation = QQuaternion::fromAxisAndAngle(m_axis, m_angle);
            buildMatrix(rotation);
        }

    private:
        QVector3D m_axis;
        float m_speed;
        float m_angle;
    };

    class OrbitActor : public BaseActor
    {
    public:
        OrbitActor(int meshIndex,
                   const QVector3D& position,
                   float orbitRadius,
                   float orbitSpeed,
                   float verticalAmplitude)
            : BaseActor(meshIndex, position)
            , m_radius(orbitRadius)
            , m_orbitSpeed(orbitSpeed)
            , m_verticalAmplitude(verticalAmplitude)
        {
        }

        void update(double /*deltaTime*/, double simTime) override
        {
            const float angle = float(simTime) * m_orbitSpeed;
            const float x = qCos(angle) * m_radius;
            const float z = qSin(angle) * m_radius;
            const float y = qSin(angle * 0.5f) * m_verticalAmplitude;

            QQuaternion rotation = QQuaternion::fromAxisAndAngle(QVector3D(0.0f, 1.0f, 0.0f), angle * 50.0f);
            buildMatrix(rotation, QVector3D(x, y, z));
        }

    private:
        float m_radius;
        float m_orbitSpeed;
        float m_verticalAmplitude;
    };

    class LoggingActor : public BaseActor
    {
    public:
        LoggingActor(int meshIndex, const QVector3D& position, float pulseSpeed)
            : BaseActor(meshIndex, position)
            , m_pulseSpeed(pulseSpeed)
            , m_loggedSecond(-1)
        {
        }

        void update(double /*deltaTime*/, double simTime) override
        {
            // Pulse scale between 0.6 and 1.2
            const float pulse = 0.9f + 0.3f * qSin(float(simTime) * m_pulseSpeed);
            const float angle = float(simTime) * 25.0f;
            QQuaternion rotation = QQuaternion::fromAxisAndAngle(QVector3D(1.0f, 1.0f, 0.0f).normalized(), angle);

            QMatrix4x4 matrix;
            matrix.translate(m_position);
            matrix.rotate(rotation);
            matrix.scale(pulse);
            m_instance.modelMatrix = matrix;

            const int currentSecond = int(simTime);
            if (currentSecond != m_loggedSecond) {
                m_loggedSecond = currentSecond;
                std::cout << "[GameLoop] Pulsating actor tick at t=" << currentSecond << "s" << std::endl;
            }
        }

    private:
        float m_pulseSpeed;
        int m_loggedSecond;
    };

} // namespace

GameLoop::GameLoop()
    : m_initialized(false)
{
}

GameLoop::~GameLoop() = default;

void GameLoop::setMeshIds(const QVector<int>& meshIds)
{
    m_meshIds = meshIds;
    m_initialized = false;
    m_actors.clear();
    m_instances.clear();
}

void GameLoop::update(double deltaTime, double simTime)
{
    if (m_meshIds.isEmpty()) {
        return;
    }

    ensureInitialized();

    m_instances.resize(int(m_actors.size()));

    for (std::size_t i = 0; i < m_actors.size(); ++i) {
        auto& actor = m_actors[i];
        actor->update(deltaTime, simTime);
        m_instances[int(i)] = actor->instance();
    }
}

void GameLoop::ensureInitialized()
{
    if (m_initialized) {
        return;
    }

    createActors();
    m_initialized = true;
}

void GameLoop::createActors()
{
    m_actors.clear();

    if (m_meshIds.isEmpty()) {
        return;
    }

    const auto meshId = [this](int index) {
        return (index >= 0 && index < m_meshIds.size()) ? m_meshIds[index] : m_meshIds.first();
    };

    m_actors.reserve(3);

    m_actors.emplace_back(new RotatingActor(meshId(0), QVector3D(-1.2f, 0.0f, 0.0f), QVector3D(0.0f, 1.0f, 0.0f), 45.0f));
    m_actors.emplace_back(new OrbitActor(meshId(1), QVector3D(0.0f, 0.0f, 0.0f), 1.5f, 0.9f, 0.4f));
    m_actors.emplace_back(new LoggingActor(meshId(2), QVector3D(1.2f, 0.0f, 0.0f), 3.5f));
}
