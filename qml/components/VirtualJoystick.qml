// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

import QtQuick 2.0
import Sailfish.Silica 1.0

Item {
    id: root
    width: Theme.itemSizeExtraLarge * 1.4
    height: width

    property real outerRadius: width * 0.5
    property real handleRadius: outerRadius * 0.45
    property vector2d value: Qt.vector2d(0, 0)
    property real deadZone: 0.05

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: "#33000000"
        border.color: "#55ffffff"
        border.width: Theme.paddingSmall
    }

    Rectangle {
        id: handle
        width: handleRadius * 2
        height: width
        radius: width / 2
        color: "#88ffffff"
        border.color: "#66ffffff"
        x: root.width / 2 - width / 2
        y: root.height / 2 - height / 2

        Behavior on x { NumberAnimation { duration: 100; easing.type: Easing.OutQuad } }
        Behavior on y { NumberAnimation { duration: 100; easing.type: Easing.OutQuad } }
    }

    function resetHandle() {
        handle.x = root.width / 2 - handle.width / 2
        handle.y = root.height / 2 - handle.height / 2
        root.value = Qt.vector2d(0, 0)
    }

    function updateFromPoint(p) {
        var center = Qt.point(root.width / 2, root.height / 2)
        var dx = p.x - center.x
        var dy = p.y - center.y
        var distance = Math.sqrt(dx * dx + dy * dy)
        var maxDist = root.outerRadius - handleRadius * 0.2
        if (distance > maxDist) {
            dx = dx / distance * maxDist
            dy = dy / distance * maxDist
        }
        handle.x = center.x - handle.width / 2 + dx
        handle.y = center.y - handle.height / 2 + dy

        var vx = dx / maxDist
        var vy = -dy / maxDist

        if (Math.abs(vx) < root.deadZone) {
            vx = 0
        }
        if (Math.abs(vy) < root.deadZone) {
            vy = 0
        }

        var newValue = Qt.vector2d(-vx, vy)
        if (root.value.x !== newValue.x || root.value.y !== newValue.y) {
            root.value = newValue
        }
    }

    MultiPointTouchArea {
        anchors.fill: parent
        minimumTouchPoints: 1
        maximumTouchPoints: 1
        onPressed: updateFromPoint(touchPoints[0])
        onUpdated: updateFromPoint(touchPoints[0])
        onReleased: resetHandle()
        onCanceled: resetHandle()
    }

    Component.onCompleted: resetHandle()
}
