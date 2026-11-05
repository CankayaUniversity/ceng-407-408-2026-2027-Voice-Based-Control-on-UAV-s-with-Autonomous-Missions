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

    readonly property var _voiceSettings: QGroundControl.settingsManager.voiceCommandSettings
    readonly property var _modelManager: QGroundControl.voiceCommandManager ? QGroundControl.voiceCommandManager.sttModelManager : null

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

    SettingsGroupLayout {
        heading: qsTr("Microphone Test & Level")
        Layout.fillWidth: true
        visible: QGroundControl.audioDeviceManager.isMicrophoneActive

        ColumnLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelHeight / 2

            RowLayout {
                Layout.fillWidth: true
                spacing: ScreenTools.defaultFontPixelWidth * 2

                QGCButton {
                    id: testMicButton
                    text: QGroundControl.audioLevelMeter.isTesting ? qsTr("Stop Test") : qsTr("Test Microphone")
                    primary: QGroundControl.audioLevelMeter.isTesting
                    onClicked: QGroundControl.audioLevelMeter.toggleTest()
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    RowLayout {
                        Layout.fillWidth: true
                        QGCLabel {
                            text: qsTr("Signal Level:")
                            font.pointSize: ScreenTools.smallFontPointSize
                            font.bold: true
                        }
                        QGCLabel {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignRight
                            text: Math.round(QGroundControl.audioLevelMeter.audioLevel * 100) + "%"
                            font.pointSize: ScreenTools.smallFontPointSize
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: Math.max(6, ScreenTools.defaultFontPixelHeight / 3)
                        radius: height / 2
                        color: qgcPal.windowShadeLight

                        Rectangle {
                            height: parent.height
                            radius: parent.radius
                            width: parent.width * QGroundControl.audioLevelMeter.audioLevel
                            color: {
                                var lvl = QGroundControl.audioLevelMeter.audioLevel
                                if (lvl > 0.85) return qgcPal.colorRed
                                if (lvl > 0.65) return qgcPal.colorOrange
                                return qgcPal.colorGreen
                            }
                            Behavior on width {
                                NumberAnimation { duration: 50 }
                            }
                        }
                    }
                }
            }

            // Frequency spectrum equalizer bars
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 2.5
                spacing: Math.max(2, Math.round(ScreenTools.defaultFontPixelWidth / 3))

                Repeater {
                    model: QGroundControl.audioLevelMeter.bandCount

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: qgcPal.windowShadeLight
                        radius: Math.min(2, ScreenTools.defaultBorderRadius)

                        readonly property real bandValue: {
                            var bands = QGroundControl.audioLevelMeter.frequencyBands
                            return (bands && index < bands.length) ? bands[index] : 0.0
                        }

                        Rectangle {
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            anchors.right: parent.right
                            height: Math.max(ScreenTools.defaultFontPixelHeight / 4, parent.height * bandValue)
                            radius: Math.min(2, ScreenTools.defaultBorderRadius)
                            color: {
                                if (!QGroundControl.audioLevelMeter.isTesting) {
                                    return qgcPal.button
                                }
                                if (bandValue > 0.8) return qgcPal.colorRed
                                if (bandValue > 0.5) return qgcPal.colorOrange
                                return qgcPal.colorGreen
                            }
                            Behavior on height {
                                NumberAnimation { duration: 60 }
                            }
                        }
                    }
                }
            }

            QGCLabel {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Speak into the microphone to verify audio input and signal levels in real time.")
                font.pointSize: ScreenTools.smallFontPointSize
            }
        }
    }

    SettingsGroupLayout {
        heading: qsTr("Audio Processing & Recording")
        Layout.fillWidth: true

        FactCheckBoxSlider {
            Layout.fillWidth: true
            text: qsTr("Noise Suppression")
            fact: root._voiceSettings.noiseFilterEnabled
        }

        LabelledFactComboBox {
            Layout.fillWidth: true
            label: qsTr("Suppression Level")
            fact: root._voiceSettings.noiseFilterLevel
            visible: root._voiceSettings.noiseFilterEnabled.rawValue
        }

        LabelledFactComboBox {
            Layout.fillWidth: true
            label: qsTr("Max Recording Duration")
            fact: root._voiceSettings.maxRecordingDuration
        }

        LabelledFactTextField {
            Layout.fillWidth: true
            label: qsTr("Push-to-Talk Shortcut Key")
            fact: root._voiceSettings.pushToTalkKey
        }

        QGCLabel {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Filters ambient environmental noise and propeller buzz. Higher levels apply more aggressive filtering.")
            font.pointSize: ScreenTools.smallFontPointSize
        }
    }

    SettingsGroupLayout {
        heading: qsTr("Speech Recognition (STT)")
        Layout.fillWidth: true

        ColumnLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelHeight / 2

            RowLayout {
                Layout.fillWidth: true
                spacing: ScreenTools.defaultFontPixelWidth * 2

                QGCLabel {
                    text: qsTr("Speech Model")
                    Layout.fillWidth: true
                }

                QGCComboBox {
                    id: sttModelComboBox
                    Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 28
                    model: root._modelManager ? root._modelManager.availableModels : []
                    currentIndex: root._modelManager ? root._modelManager.selectedModelIndex : -1
                    enabled: root._modelManager ? root._modelManager.availableModels.length > 0 : false

                    onActivated: (index) => {
                        if (root._modelManager) {
                            root._modelManager.selectedModelIndex = index
                        }
                    }
                }

                QGCButton {
                    text: qsTr("Refresh")
                    onClicked: {
                        if (root._modelManager) {
                            root._modelManager.refreshModels()
                        }
                    }
                }
            }

            QGCLabel {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Model for voice recognition is not found. Place a model file (.bin, .gguf, .onnx, or vosk-model-*) in the models/stt directory.")
                visible: root._modelManager ? !root._modelManager.isModelAvailable : true
                font.pointSize: ScreenTools.smallFontPointSize
                color: qgcPal.colorOrange
            }
        }

        LabelledFactComboBox {
            Layout.fillWidth: true
            label: qsTr("Speech Language")
            fact: root._voiceSettings.sttLanguage
        }

        // shortcut: idle model unload timer wired in worker pipeline PR, when background worker thread is added
        LabelledFactComboBox {
            Layout.fillWidth: true
            label: qsTr("Model Memory Timeout")
            fact: root._voiceSettings.sttIdleUnloadTimeout
        }

        FactCheckBoxSlider {
            Layout.fillWidth: true
            text: qsTr("GPU Acceleration (Fallback to CPU)")
            fact: root._voiceSettings.sttUseGpu
        }

        QGCLabel {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Dynamically scans models/stt/ for offline speech recognition models. The compatible inference engine is selected automatically based on the model file.")
            font.pointSize: ScreenTools.smallFontPointSize
        }
    }

    Component.onDestruction: {
        if (QGroundControl.audioLevelMeter && QGroundControl.audioLevelMeter.isTesting) {
            QGroundControl.audioLevelMeter.stopTest()
        }
    }
}
