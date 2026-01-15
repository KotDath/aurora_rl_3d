// SPDX-FileCopyrightText: 2024 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    objectName: "mainPage"
    allowedOrientations: Orientation.All

    PageHeader {
        objectName: "pageHeader"
        title: qsTr("AuroraRL3D")
        extraContent.children: [
            IconButton {
                objectName: "aboutButton"
                icon.source: "image://theme/icon-m-about"
                anchors.verticalCenter: parent.verticalCenter

                onClicked: pageStack.replace(Qt.resolvedUrl("AboutPage.qml"))
            }
        ]
    }

    Column {
        anchors.centerIn: parent
        spacing: Theme.paddingLarge

        Label {
            text: qsTr("3D Rendering Demo")
            font.pixelSize: Theme.fontSizeLarge
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Button {
            text: qsTr("Test 3D Render")
            anchors.horizontalCenter: parent.horizontalCenter
            onClicked: pageStack.replace(Qt.resolvedUrl("TestPage.qml"))
        }

        Button {
            text: qsTr("Test 3D Render - Learning Ant agent")
            anchors.horizontalCenter: parent.horizontalCenter
            onClicked: pageStack.replace(Qt.resolvedUrl("AntLearningPage.qml"))
        }

        Button {
            text: qsTr("Test 3D Render - Learning Simple agent")
            anchors.horizontalCenter: parent.horizontalCenter
            onClicked: pageStack.replace(Qt.resolvedUrl("SimpleLearningPage.qml"))
        }

        Button {
            text: qsTr("Perspective Test")
            anchors.horizontalCenter: parent.horizontalCenter
            onClicked: pageStack.replace(Qt.resolvedUrl("PerspectiveTestPage.qml"))
        }

        Label {
            text: qsTr("Qt 5.6 • OpenGL • QQuickFramebufferObject")
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.secondaryColor
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }
}
