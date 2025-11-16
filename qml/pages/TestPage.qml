// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

import QtQuick 2.0
import Sailfish.Silica 1.0
import AuroraRL3D 1.0

Page {
    objectName: "testPage"
    allowedOrientations: Orientation.Landscape

    property int objectIndex: 0

    // RenderWindow на весь экран
    RenderWindow {
        id: renderWindow
        anchors.fill: parent
    }

    // Overlay с кнопками и информацией
    Rectangle {
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        color: "transparent"

        // Верхняя панель с заголовком и счетчиком
        Rectangle {
            id: headerPanel
            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
            }
            height: Theme.itemSizeLarge
            color: "transparent"

            Column {
                anchors {
                    left: parent.left
                    verticalCenter: parent.verticalCenter
                    margins: Theme.paddingMedium
                }
                spacing: Theme.paddingSmall

                Label {
                    text: qsTr("3D Game Loop Demo")
                    color: "white"
                    font.pixelSize: Theme.fontSizeLarge
                    font.bold: true
                    style: Text.Outline
                    styleColor: "black"
                }

                Label {
                    text: qsTr("Random Colors & Rotation")
                    color: "lightblue"
                    font.pixelSize: Theme.fontSizeSmall
                    style: Text.Outline
                    styleColor: "black"
                }
            }

            Label {
                anchors {
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                    margins: Theme.paddingMedium
                }
                text: qsTr("Objects: %1").arg(renderWindow.objectCount)
                color: "white"
                font.pixelSize: Theme.fontSizeSmall
                style: Text.Outline
                styleColor: "black"
            }
        }

        // Панель с кнопками внизу
        Rectangle {
            id: buttonPanel
            anchors {
                bottom: parent.bottom
                left: parent.left
                right: parent.right
            }
            height: Theme.itemSizeLarge * 3 + Theme.paddingMedium * 2
            color: "transparent"

            Column {
                anchors {
                    centerIn: parent
                }
                spacing: Theme.paddingSmall

                Button {
                    text: qsTr("Add Random Cube")
                    anchors.horizontalCenter: parent.horizontalCenter
                    onClicked: {
                        for (var i = 0; i < 100; ++i) {
                            var newObject = Qt.createQmlObject('import AuroraRL3D 1.0; SceneObject {}', buttonPanel)
                            newObject.position = Qt.vector3d(Math.random() * 3 - 1.5, Math.random() * 2 - 1, 0)
                            newObject.rotation = Qt.quaternion(Math.random(), Math.random(), Math.random(), Math.random())
                            newObject.initialRotation = Math.random() * 360
                            newObject.meshId = Math.floor(Math.random() * 6) // 0-5 для разных цветов
                            newObject.visible = true
                            renderWindow.addObject(newObject)
                            objectIndex++
                        }
                    }
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Game Loop Active")
                    color: "lightgreen"
                    font.pixelSize: Theme.fontSizeSmall
                    style: Text.Outline
                    styleColor: "black"
                }

                Button {
                    text: qsTr("Clear All")
                    anchors.horizontalCenter: parent.horizontalCenter
                    onClicked: {
                        renderWindow.clearObjects()
                        objectIndex = 0
                    }
                }

                Button {
                    text: qsTr("Back to Main")
                    anchors.horizontalCenter: parent.horizontalCenter
                    onClicked: pageStack.pop()
                }
            }
        }
    }
}
