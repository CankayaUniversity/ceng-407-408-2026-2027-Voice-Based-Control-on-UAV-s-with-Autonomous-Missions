import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

Item {
    id:      root
    visible: false

    readonly property var _voiceManager: QGroundControl.voiceCommandManager

    Connections {
        target: _voiceManager

        function onCommandConfirmationRequested() {
            if (_voiceManager && _voiceManager.hasPendingConfirmation) {
                confirmDialogFactory.open()
            }
        }
    }

    QGCPopupDialogFactory {
        id:              confirmDialogFactory
        dialogComponent: confirmDialogComponent
    }

    Component {
        id: confirmDialogComponent

        QGCPopupDialog {
            id:          confirmDialog
            title:       qsTr("Confirm Voice Command")
            buttons:     Dialog.NoButton
            closePolicy: Popup.NoAutoClose

            readonly property var _intents:    _voiceManager ? _voiceManager.pendingIntents : []
            readonly property string _transcript: _voiceManager ? _voiceManager.pendingTranscript : ""
            readonly property bool _hasPending: _voiceManager ? _voiceManager.hasPendingConfirmation : false

            on_HasPendingChanged: {
                if (!_hasPending) {
                    confirmDialog.close()
                }
            }

            ColumnLayout {
                width:   Math.min(ScreenTools.defaultFontPixelWidth * 52, confirmDialog.maxContentAvailableWidth)
                spacing: ScreenTools.defaultFontPixelHeight * 0.75

                QGCLabel {
                    Layout.fillWidth: true
                    visible:          confirmDialog._transcript.length > 0
                    text:             qsTr("Heard: \"%1\"").arg(confirmDialog._transcript)
                    wrapMode:         Text.WordWrap
                    color:            qgcPal.colorGrey
                    font.italic:      true
                }

                QGCLabel {
                    Layout.fillWidth: true
                    text:             confirmDialog._intents.length > 1
                                      ? qsTr("Matched Command Sequence (%1 steps):").arg(confirmDialog._intents.length)
                                      : qsTr("Matched Command:")
                    font.bold:        true
                }

                Rectangle {
                    Layout.fillWidth:       true
                    Layout.preferredHeight: Math.min(intentListView.contentHeight + ScreenTools.defaultFontPixelHeight,
                                                     ScreenTools.defaultFontPixelHeight * 14)
                    color:                  qgcPal.windowShade
                    border.color:           qgcPal.groupBorder
                    border.width:           1
                    radius:                 ScreenTools.defaultFontPixelWidth * 0.4

                    ListView {
                        id:             intentListView
                        anchors.fill:   parent
                        anchors.margins: ScreenTools.defaultFontPixelHeight * 0.5
                        clip:           true
                        spacing:        ScreenTools.defaultFontPixelHeight * 0.4
                        model:          confirmDialog._intents

                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                        }

                        delegate: Rectangle {
                            required property var modelData
                            required property int index

                            width:        intentListView.width
                            height:       stepRow.implicitHeight + ScreenTools.defaultFontPixelHeight * 0.6
                            color:        qgcPal.window
                            border.color: qgcPal.groupBorder
                            border.width: 1
                            radius:       ScreenTools.defaultFontPixelWidth * 0.3

                            RowLayout {
                                id:                     stepRow
                                anchors.left:           parent.left
                                anchors.right:          parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.margins:        ScreenTools.defaultFontPixelWidth * 0.75
                                spacing:                ScreenTools.defaultFontPixelWidth

                                QGCLabel {
                                    text:      (index + 1) + "."
                                    font.bold: true
                                    color:     qgcPal.buttonHighlight
                                }

                                QGCLabel {
                                    Layout.fillWidth: true
                                    text:             modelData.displaySummary
                                    wrapMode:         Text.WordWrap
                                }

                                Rectangle {
                                    visible:        modelData.targetVehicleId >= 0
                                    color:          qgcPal.button
                                    border.color:   qgcPal.buttonHighlight
                                    border.width:   1
                                    radius:         ScreenTools.defaultFontPixelWidth * 0.25
                                    implicitWidth:  vehicleLabel.implicitWidth + ScreenTools.defaultFontPixelWidth
                                    implicitHeight: vehicleLabel.implicitHeight + ScreenTools.defaultFontPixelHeight * 0.25

                                    QGCLabel {
                                        id:               vehicleLabel
                                        anchors.centerIn: parent
                                        text:             qsTr("Vehicle %1").arg(modelData.targetVehicleId)
                                        font.pointSize:   ScreenTools.smallFontPointSize
                                    }
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing:          ScreenTools.defaultFontPixelWidth

                    QGCButton {
                        Layout.fillWidth: true
                        text:             qsTr("Reject (Cancel)")
                        onClicked: {
                            if (_voiceManager) {
                                _voiceManager.rejectPendingCommand()
                            }
                            confirmDialog.close()
                        }
                    }

                    QGCButton {
                        Layout.fillWidth: true
                        primary:          true
                        text:             qsTr("Approve (Execute)")
                        onClicked: {
                            if (_voiceManager) {
                                _voiceManager.approvePendingCommand()
                            }
                            confirmDialog.close()
                        }
                    }
                }
            }
        }
    }
}
