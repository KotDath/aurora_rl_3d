// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef RENDERWINDOW_H
#define RENDERWINDOW_H

#include <QtQuick/QQuickFramebufferObject>
#include <QQmlListProperty>
#include "SceneObject.h"
#include "Renderer.h"

class RenderWindow : public QQuickFramebufferObject
{
    Q_OBJECT
    Q_PROPERTY(QQmlListProperty<SceneObject> objects READ objects)
    Q_PROPERTY(int objectCount READ objectCount NOTIFY objectCountChanged)

public:
    RenderWindow(QQuickItem* parent = nullptr);
    ~RenderWindow();

    OpenGLRenderer* createRenderer() const override;

    QQmlListProperty<SceneObject> objects();
    int objectCount() const;

    Q_INVOKABLE void addObject(SceneObject* object);
    Q_INVOKABLE void removeObject(int index);
    Q_INVOKABLE void clearObjects();
    Q_INVOKABLE SceneObject* getObject(int index) const;

    // Internal method for renderer synchronization
    const QVector<SceneObject*>& sceneObjects() const;

signals:
    void objectCountChanged();
    void objectAdded(int index);
    void objectRemoved(int index);

private:
    static void appendObject(QQmlListProperty<SceneObject>* list, SceneObject* object);
    static int objectCount(QQmlListProperty<SceneObject>* list);
    static SceneObject* objectAt(QQmlListProperty<SceneObject>* list, int index);
    static void clearObjects(QQmlListProperty<SceneObject>* list);

    QVector<SceneObject*> m_objects;
};

#endif // RENDERWINDOW_H