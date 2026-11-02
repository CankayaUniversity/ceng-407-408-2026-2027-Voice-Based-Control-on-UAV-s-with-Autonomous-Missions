import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FactControls

SettingsPage {
    id: root
    objectName: "settingsPage_VoiceCommand"

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    SettingsGroupLayout {
        heading: qsTr("Microphone Device")
        Layout.fillWidth: true

        ColumnLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelHeight / 2

            QGCLabel {
                text: qsTr("Audio Input Source")
                font.bold: true
            }

            QGCComboBox {
                id: micComboBox
                Layout.fillWidth: true
                model: QGroundControl.audioDeviceManager.availableMicrophones
                currentIndex: QGroundControl.audioDeviceManager.selectedMicrophoneIndex
                enabled: QGroundControl.audioDeviceManager.availableMicrophones.length > 0

                onActivated: (index) => {
                    QGroundControl.audioDeviceManager.selectedMicrophoneIndex = index
                }
            }

            QGCLabel {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Select the microphone to be used for voice control.")
                font.pointSize: ScreenTools.smallFontPointSize
            }

            QGCLabel {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("No audio input devices detected on your system. Connect a microphone to enable voice commands.")
                visible: QGroundControl.audioDeviceManager.availableMicrophones.length === 0
                font.pointSize: ScreenTools.smallFontPointSize
                color: qgcPal.colorOrange
            }
        }
    }

    SettingsGroupLayout {
        heading: qsTr("Device Specifications")
        Layout.fillWidth: true
        visible: QGroundControl.audioDeviceManager.isMicrophoneActive

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: ScreenTools.defaultFontPixelWidth * 4
            rowSpacing: ScreenTools.defaultFontPixelHeight / 4

            QGCLabel {
                text: qsTr("Status:")
                font.pointSize: ScreenTools.smallFontPointSize
                font.bold: true
            }
            QGCLabel {
                text: QGroundControl.audioDeviceManager.isMicrophoneActive ? qsTr("Ready / Active") : qsTr("Not Available")
                font.pointSize: ScreenTools.smallFontPointSize
                color: QGroundControl.audioDeviceManager.isMicrophoneActive ? qgcPal.colorGreen : qgcPal.colorRed
            }

            QGCLabel {
                text: qsTr("Sample Rate:")
                font.pointSize: ScreenTools.smallFontPointSize
                font.bold: true
            }
            QGCLabel {
                text: QGroundControl.audioDeviceManager.sampleRate > 0 ? (QGroundControl.audioDeviceManager.sampleRate + " Hz") : qsTr("N/A")
                font.pointSize: ScreenTools.smallFontPointSize
            }

            QGCLabel {
                text: qsTr("Channels:")
                font.pointSize: ScreenTools.smallFontPointSize
                font.bold: true
            }
            QGCLabel {
                text: {
                    var ch = QGroundControl.audioDeviceManager.channelCount
                    if (ch === 1) return qsTr("1 (Mono)")
                    if (ch === 2) return qsTr("2 (Stereo)")
                    if (ch > 2) return qsTr("%1 channels").arg(ch)
                    return qsTr("N/A")
                }
                font.pointSize: ScreenTools.smallFontPointSize
            }

            QGCLabel {
                text: qsTr("Sample Format:")
                font.pointSize: ScreenTools.smallFontPointSize
                font.bold: true
            }
            QGCLabel {
                text: QGroundControl.audioDeviceManager.sampleFormat !== "" ? QGroundControl.audioDeviceManager.sampleFormat : qsTr("N/A")
                font.pointSize: ScreenTools.smallFontPointSize
            }

            QGCLabel {
                text: qsTr("OS Default:")
                font.pointSize: ScreenTools.smallFontPointSize
                font.bold: true
            }
            QGCLabel {
                text: QGroundControl.audioDeviceManager.isDefaultDevice ? qsTr("Yes") : qsTr("No")
                font.pointSize: ScreenTools.smallFontPointSize
            }
        }
    }
}
