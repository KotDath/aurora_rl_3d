// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef GAMELOOP_H
#define GAMELOOP_H

#include <QMatrix4x4>
#include <QVector>

#include <memory>
#include <vector>

class GameLoop
{
public:
    struct RenderInstance
    {
        int meshId = -1;
        QMatrix4x4 modelMatrix;
        bool visible = true;
    };

    class GameActor;

    GameLoop();
    ~GameLoop();

    void setMeshIds(const QVector<int>& meshIds);
    void update(double deltaTime, double simTime);

    const QVector<RenderInstance>& renderInstances() const { return m_instances; }

private:
    void ensureInitialized();
    void createActors();

    QVector<int> m_meshIds;
    std::vector<std::unique_ptr<GameActor>> m_actors;
    QVector<RenderInstance> m_instances;
    bool m_initialized;
};

#endif // GAMELOOP_H
