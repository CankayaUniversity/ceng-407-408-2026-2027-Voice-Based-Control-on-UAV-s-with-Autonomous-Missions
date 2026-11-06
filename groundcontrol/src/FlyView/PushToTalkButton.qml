import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlyView

Item {
    id: root

    width: ScreenTools.defaultFontPixelWidth * 7
    height: width

    readonly property bool isMicrophoneActive: QGroundControl.voiceCommandManager ? QGroundControl.voiceCommandManager.isMicrophoneActive : false
    readonly property bool isVehicleConnected: QGroundControl.voiceCommandManager ? QGroundControl.voiceCommandManager.isVehicleConnected : false
    readonly property bool isSttModelAvailable: QGroundControl.voiceCommandManager ? QGroundControl.voiceCommandManager.isSttModelAvailable : false
    readonly property bool isReady: QGroundControl.voiceCommandManager ? QGroundControl.voiceCommandManager.isReady : false

    visible: isMicrophoneActive
    enabled: isMicrophoneActive && isVehicleConnected

    readonly property bool isRecording: QGroundControl.voiceCommandManager ? QGroundControl.voiceCommandManager.isRecording : false
    readonly property string pttKey: QGroundControl.voiceCommandManager ? QGroundControl.voiceCommandManager.pushToTalkKey : "P"

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    // Main button body
    Rectangle {
        id: buttonBody
        anchors.fill: parent
        radius: ScreenTools.defaultFontPixelWidth / 2
        color: {
            if (root.isRecording) {
                return qgcPal.colorRed
            }
            if (root.isReady && mouseArea.containsMouse) {
                return qgcPal.toolStripHoverColor
            }
            return qgcPal.windowTransparent
        }
        border.color: root.isRecording ? "white" : "transparent"
        border.width: root.isRecording ? 1 : 0

        Behavior on color {
            ColorAnimation { duration: 120 }
        }

        QGCColoredImage {
            id: micIcon
            anchors.centerIn: parent
            width: parent.width * 0.5
            height: width
            sourceSize.width: width
            sourceSize.height: height
            source: "qrc:/InstrumentValueIcons/mic.svg"
            color: root.isRecording ? "white" : qgcPal.text
            opacity: root.isReady ? 1.0 : 0.4
            fillMode: Image.PreserveAspectFit
            mipmap: true
            smooth: true
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        preventStealing: true

        onPressed: {
            if (root.enabled && QGroundControl.voiceCommandManager) {
                QGroundControl.voiceCommandManager.startRecording()
            }
        }
        onReleased: {
            if (QGroundControl.voiceCommandManager) {
                QGroundControl.voiceCommandManager.stopRecording()
            }
        }
        onCanceled: {
            if (QGroundControl.voiceCommandManager) {
                QGroundControl.voiceCommandManager.stopRecording()
            }
        }
    }

    ToolTip.visible: mouseArea.containsMouse && !root.isRecording
    ToolTip.delay: 500
    ToolTip.text: {
        if (!root.isMicrophoneActive) {
            return qsTr("Push to Talk: No microphone detected")
        }
        if (!root.isVehicleConnected) {
            return qsTr("Push to Talk: No vehicle connected")
        }
        if (!root.isSttModelAvailable) {
            return qsTr("Model for voice recognition is not found")
        }
        return qsTr("Push to Talk (%1)\nHold to record voice command").arg(root.pttKey)
    }
}
