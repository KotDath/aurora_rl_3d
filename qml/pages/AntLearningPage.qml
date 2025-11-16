// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

import QtQuick 2.0
import Sailfish.Silica 1.0
import AuroraRL3D 1.0

Page {
    objectName: "antLearningPage"
    allowedOrientations: Orientation.Landscape

    RenderWindow {
        id: trainingWindow
        anchors.fill: parent
        sceneProfile: RenderWindow.SceneAntTraining
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

            ProgressBar {
                width: parent.width
                value: trainingWindow.antEpisodeProgress
                label: qsTr("Episode progress")
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
            anchors {
                bottom: parent.bottom
                right: parent.right
                margins: Theme.paddingLarge
            }
            spacing: Theme.paddingMedium

            Button {
                text: qsTr("Back")
                onClicked: pageStack.pop()
            }
        }
    }
}
