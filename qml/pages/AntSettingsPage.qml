import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: setupPage
    objectName: "antSettingsPage"
    allowedOrientations: Orientation.All

    PageHeader {
        title: qsTr("Ant training setup")
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: setupPage.status !== PageStatus.Active
        visible: running
        z: 2
    }

    Column {
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.paddingLarge
        spacing: Theme.paddingLarge

        Label {
            text: qsTr("Перед запуском настроите тренировку")
            font.pixelSize: Theme.fontSizeLarge
            horizontalAlignment: Text.AlignHCenter
            width: parent.width
        }

        Slider {
            id: stepSlider
            width: parent.width
            minimumValue: 1
            maximumValue: 128
            stepSize: 1
            value: 1
            label: qsTr("Шагов за кадр (step)")
            valueText: value.toFixed(0)
        }

        Slider {
            id: renderSlider
            width: parent.width
            minimumValue: 1
            maximumValue: 32
            stepSize: 1
            value: 1
            label: qsTr("Отрисовка каждые N итераций")
            valueText: value.toFixed(0)
        }

        Button {
            text: qsTr("Запустить агента")
            enabled: setupPage.status === PageStatus.Active
            onClicked: pageStack.replace(
                         Qt.resolvedUrl("AntLearningPage.qml"),
                         {
                             trainRepeats: stepSlider.value,
                             renderEvery: renderSlider.value
                         })
        }
    }
}
