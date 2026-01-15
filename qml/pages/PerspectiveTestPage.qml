// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

import QtQuick 2.0
import Sailfish.Silica 1.0
import AuroraRL3D 1.0
import "../components"

Page {
    objectName: "perspectiveTestPage"
    allowedOrientations: Orientation.LandscapeMask

    RenderWindow {
        id: renderWindow
        anchors.fill: parent
        sceneProfile: RenderWindow.ScenePerspectiveTest
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
            spacing: Theme.paddingSmall

            Label {
                text: qsTr("Perspective Lighting Test")
                color: "white"
                font.pixelSize: Theme.fontSizeExtraLarge
                font.bold: true
                style: Text.Outline
                styleColor: "black"
            }

            Label {
                text: qsTr("Animated cubes, cylinders and UV spheres lit with the Bill-Phong shader")
                color: "lightsteelblue"
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.Wrap
                style: Text.Outline
                styleColor: "black"
            }
        }

        Button {
            id: backButton
            text: qsTr("Back")
            anchors {
                horizontalCenter: parent.horizontalCenter
                bottom: parent.bottom
                margins: Theme.paddingLarge
            }
            onClicked: pageStack.replace(Qt.resolvedUrl("MainPage.qml"))
            z: 2
        }

        VirtualJoystick {
            id: movementJoystick
            anchors {
                left: parent.left
                bottom: parent.bottom
                margins: Theme.paddingLarge
            }
            onValueChanged: renderWindow.cameraInput = Qt.point(value.x, value.y)
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
                Timer { id: upTimer; interval: 50; repeat: true; running: false; onTriggered: renderWindow.addCameraHeightDelta(0.05) }
            }

            Button {
                text: "\u2193"
                width: Theme.itemSizeHuge
                onPressed: downTimer.start(); onReleased: downTimer.stop()
                Timer { id: downTimer; interval: 50; repeat: true; running: false; onTriggered: renderWindow.addCameraHeightDelta(-0.05) }
            }
        }

        MouseArea {
            z: 1
            anchors {
                top: parent.top
                bottom: backButton.top
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
                renderWindow.addCameraLookDelta(dx, dy)
            }
        }
    }
}
