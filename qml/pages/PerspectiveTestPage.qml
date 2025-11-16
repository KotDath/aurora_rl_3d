// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

import QtQuick 2.0
import Sailfish.Silica 1.0
import AuroraRL3D 1.0

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
            text: qsTr("Back")
            anchors {
                horizontalCenter: parent.horizontalCenter
                bottom: parent.bottom
                margins: Theme.paddingLarge
            }
            onClicked: pageStack.pop()
        }
    }
}
