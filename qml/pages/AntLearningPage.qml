// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

import QtQuick 2.0
import Sailfish.Silica 1.0
import AuroraRL3D 1.0
import "../components"

Page {
    objectName: "antLearningPage"
    allowedOrientations: Orientation.LandscapeMask

    RenderWindow {
        id: trainingWindow
        anchors.fill: parent
        sceneProfile: RenderWindow.SceneAntTraining
        transformOrigin: Item.Center
        rotation: 180
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"

        Column {
            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
                margins: Theme.paddingLarge
            }
            spacing: Theme.paddingMedium

            Label {
                text: qsTr("Learning Ant agent")
                font.pixelSize: Theme.fontSizeLarge
                color: "white"
                style: Text.Outline
                styleColor: "black"
            }

            Row {
                spacing: Theme.paddingLarge

                Label {
                    text: qsTr("Step: %1").arg(trainingWindow.antIteration)
                    color: "white"
                    font.pixelSize: Theme.fontSizeMedium
                }

                Label {
                    text: qsTr("Reward: %1")
                            .arg(trainingWindow.antLastReward.toFixed(1))
                    color: "lightgreen"
                    font.pixelSize: Theme.fontSizeMedium
                }

                Label {
                    text: qsTr("Avg: %1")
                            .arg(trainingWindow.antAverageReward.toFixed(1))
                    color: "#88d7ff"
                    font.pixelSize: Theme.fontSizeMedium
                }
            }

            Row {
                width: parent.width
                spacing: Theme.paddingMedium

                Item {
                    width: parent.width * 0.65
                    height: progressEpisode.implicitHeight
                    Rectangle {
                        anchors.fill: parent
                        color: "#33000000"
                        radius: Theme.paddingSmall
                    }
                    ProgressBar {
                        id: progressEpisode
                        anchors.fill: parent
                        value: trainingWindow.antEpisodeProgress
                        label: qsTr("Episode progress")
                    }
                }

                Label {
                    text: qsTr("Height: %1 m").arg(trainingWindow.antHeight.toFixed(2))
                    color: "white"
                    font.pixelSize: Theme.fontSizeMedium
                    horizontalAlignment: Text.AlignRight
                    width: parent.width * 0.35 - Theme.paddingMedium
                }
            }

            Item {
                width: parent.width
                height: progressHealth.implicitHeight
                Rectangle {
                    anchors.fill: parent
                    color: "#33000000"
                    radius: Theme.paddingSmall
                }
                ProgressBar {
                    id: progressHealth
                    anchors.fill: parent
                    value: trainingWindow.antHealth
                    label: qsTr("Agent health")
                }
            }

            Label {
                text: trainingWindow.antFallbackActive ?
                          qsTr("Fallback kinematics") :
                          qsTr("MuJoCo PPO streaming")
                font.pixelSize: Theme.fontSizeSmall
                color: trainingWindow.antFallbackActive ? "#ffca28" : "#8bc34a"
            }
        }

        Column {
            id: actionColumn
            anchors {
                bottom: parent.bottom
                right: parent.right
                margins: Theme.paddingLarge
            }
            spacing: Theme.paddingMedium

            Button {
                id: backButton
                text: qsTr("Back")
                onClicked: pageStack.replace(Qt.resolvedUrl("MainPage.qml"))
                z: 2
            }
        }

        VirtualJoystick {
            id: antJoystick
            anchors {
                left: parent.left
                bottom: parent.bottom
                margins: Theme.paddingLarge
            }
            onValueChanged: trainingWindow.cameraInput = Qt.point(value.x, value.y)
        }

        Column {
            id: verticalControls
            anchors {
                right: parent.right
                verticalCenter: parent.verticalCenter
                margins: Theme.paddingLarge
            }
            spacing: Theme.paddingMedium
            z: 2

            Button {
                text: "\u2191"
                width: Theme.itemSizeHuge
                onPressed: upTimer.start(); onReleased: upTimer.stop()
                Timer { id: upTimer; interval: 50; repeat: true; running: false; onTriggered: trainingWindow.addCameraHeightDelta(0.05) }
            }

            Button {
                text: "\u2193"
                width: Theme.itemSizeHuge
                onPressed: downTimer.start(); onReleased: downTimer.stop()
                Timer { id: downTimer; interval: 50; repeat: true; running: false; onTriggered: trainingWindow.addCameraHeightDelta(-0.05) }
            }
        }

        MouseArea {
            z: 1
            anchors {
                top: parent.top
                bottom: actionColumn.top
                bottomMargin: Theme.paddingMedium
                left: parent.horizontalCenter
                right: parent.right
            }
            acceptedButtons: Qt.LeftButton
            preventStealing: true
            hoverEnabled: true
            property real lastX: 0
            property real lastY: 0
            onPressed: {
                lastX = mouse.x
                lastY = mouse.y
            }
            onPositionChanged: if (pressed) {
                var dx = lastX - mouse.x
                var dy = lastY - mouse.y
                lastX = mouse.x
                lastY = mouse.y
                trainingWindow.addCameraLookDelta(dx, dy)
            }
        }
    }
}
