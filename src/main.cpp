// SPDX-FileCopyrightText: 2024 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#include <auroraapp.h>
#include <QtQuick>

#include "RenderWindow.h"
#include "SceneObject.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> application(Aurora::Application::application(argc, argv));
    application->setOrganizationName(QStringLiteral("ru.kotdath"));
    application->setApplicationName(QStringLiteral("AuroraRL3D"));

    // Register QML types
    qmlRegisterType<RenderWindow>("AuroraRL3D", 1, 0, "RenderWindow");
    qmlRegisterType<SceneObject>("AuroraRL3D", 1, 0, "SceneObject");

    QScopedPointer<QQuickView> view(Aurora::Application::createView());
    view->setSource(Aurora::Application::pathTo(QStringLiteral("qml/AuroraRL3D.qml")));
    view->show();

    return application->exec();
}
