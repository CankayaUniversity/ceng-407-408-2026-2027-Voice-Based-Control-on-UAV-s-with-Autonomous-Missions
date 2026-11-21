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

            readonly property var _intents:       _voiceManager ? _voiceManager.pendingIntents : []
            readonly property var _enabledStates: _voiceManager ? _voiceManager.pendingIntentEnabled : []
            readonly property int _activeCount:   _voiceManager ? _voiceManager.activePendingIntentCount : 0
            readonly property string _transcript: _voiceManager ? _voiceManager.pendingTranscript : ""
            readonly property bool _hasPending:   _voiceManager ? _voiceManager.hasPendingConfirmation : false

            // Returns the shared vehicle ID when every intent targets the same vehicle, or -1 if mixed/unspecified.
            readonly property int _sharedVehicleId: {
                var sharedId = -1
                for (var i = 0; i < _intents.length; ++i) {
                    var vid = _intents[i].targetVehicleId
                    if (vid < 0) {
                        continue
                    }
                    if (sharedId < 0) {
                        sharedId = vid
                    } else if (sharedId !== vid) {
                        return -1
                    }
                }
                return sharedId
            }

            on_HasPendingChanged: {
                if (!_hasPending) {
                    confirmDialog.close()
                }
            }

            ColumnLayout {
                width:   Math.min(ScreenTools.defaultFontPixelWidth * 54, confirmDialog.maxContentAvailableWidth)
                spacing: ScreenTools.defaultFontPixelHeight * 0.65

                QGCLabel {
                    Layout.fillWidth: true
                    visible:          confirmDialog._transcript.length > 0
                    text:             qsTr("Heard: \"%1\"").arg(confirmDialog._transcript)
                    wrapMode:         Text.WordWrap
                    color:            qgcPal.colorGrey
                    font.italic:      true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing:          ScreenTools.defaultFontPixelWidth

                    QGCLabel {
                        Layout.fillWidth: true
                        text: qsTr("Matched Command Sequence:")
                        font.bold: true
                    }

                    Rectangle {
                        visible:        confirmDialog._sharedVehicleId >= 0
                        color:          qgcPal.windowShade
                        border.color:   qgcPal.buttonHighlight
                        border.width:   1
                        radius:         ScreenTools.defaultFontPixelWidth * 0.3
                        implicitWidth:  headerVehicleLabel.implicitWidth + ScreenTools.defaultFontPixelWidth * 1.2
                        implicitHeight: headerVehicleLabel.implicitHeight + ScreenTools.defaultFontPixelHeight * 0.3

                        QGCLabel {
                            id:               headerVehicleLabel
                            anchors.centerIn: parent
                            text:             qsTr("Vehicle %1").arg(confirmDialog._sharedVehicleId)
                            font.pointSize:   ScreenTools.smallFontPointSize
                            font.bold:        true
                        }
                    }
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
                        id:              intentListView
                        anchors.fill:    parent
                        anchors.margins: ScreenTools.defaultFontPixelHeight * 0.5
                        clip:            true
                        spacing:         ScreenTools.defaultFontPixelHeight * 0.45
                        model:           confirmDialog._intents

                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                        }

                        delegate: Rectangle {
                            id: stepCard

                            required property var modelData
                            required property int index

                            readonly property bool isEnabled: index < confirmDialog._enabledStates.length
                                                              ? confirmDialog._enabledStates[index]
                                                              : true

                            readonly property int activeStepNumber: {
                                if (!isEnabled) {
                                    return 0
                                }
                                var n = 0
                                for (var i = 0; i <= index; ++i) {
                                    if (i >= confirmDialog._enabledStates.length || confirmDialog._enabledStates[i]) {
                                        n++
                                    }
                                }
                                return n
                            }

                            width:        intentListView.width
                            height:       stepRow.implicitHeight + ScreenTools.defaultFontPixelHeight * 0.7
                            color:        isEnabled
                                          ? (stepMouse.containsMouse ? qgcPal.button : qgcPal.window)
                                          : (stepMouse.containsMouse ? qgcPal.window : qgcPal.windowShade)
                            border.color: isEnabled ? qgcPal.buttonHighlight : qgcPal.groupBorder
                            border.width: isEnabled ? 1.5 : 1
                            radius:       ScreenTools.defaultFontPixelWidth * 0.35
                            opacity:      isEnabled ? 1.0 : 0.6
                            clip:         true

                            Rectangle {
                                anchors.left:   parent.left
                                anchors.top:    parent.top
                                anchors.bottom: parent.bottom
                                width:          ScreenTools.defaultFontPixelWidth * 0.4
                                color:          stepCard.isEnabled ? qgcPal.buttonHighlight : "transparent"
                            }

                            RowLayout {
                                id:                     stepRow
                                anchors.left:           parent.left
                                anchors.right:          parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.leftMargin:     ScreenTools.defaultFontPixelWidth * 1.1
                                anchors.rightMargin:    ScreenTools.defaultFontPixelWidth * 0.75
                                spacing:                ScreenTools.defaultFontPixelWidth

                                QGCLabel {
                                    Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 2.5
                                    text:                  stepCard.isEnabled ? (stepCard.activeStepNumber + ".") : "—"
                                    font.bold:             stepCard.isEnabled
                                    color:                 stepCard.isEnabled ? qgcPal.buttonHighlight : qgcPal.colorGrey
                                }

                                QGCLabel {
                                    Layout.fillWidth:    true
                                    Layout.minimumWidth: 0
                                    text:                stepCard.modelData.displaySummary
                                    wrapMode:            Text.Wrap
                                    font.strikeout:      !stepCard.isEnabled
                                    color:               stepCard.isEnabled ? qgcPal.text : qgcPal.colorGrey
                                }

                                Rectangle {
                                    visible:        confirmDialog._sharedVehicleId < 0 && stepCard.modelData.targetVehicleId >= 0
                                    color:          qgcPal.windowShade
                                    border.color:   qgcPal.groupBorder
                                    border.width:   1
                                    radius:         ScreenTools.defaultFontPixelWidth * 0.25
                                    implicitWidth:  vehicleLabel.implicitWidth + ScreenTools.defaultFontPixelWidth
                                    implicitHeight: vehicleLabel.implicitHeight + ScreenTools.defaultFontPixelHeight * 0.25

                                    QGCLabel {
                                        id:               vehicleLabel
                                        anchors.centerIn: parent
                                        text:             qsTr("Vehicle %1").arg(stepCard.modelData.targetVehicleId)
                                        font.pointSize:   ScreenTools.smallFontPointSize
                                    }
                                }

                                Rectangle {
                                    color:          stepCard.isEnabled ? qgcPal.windowShade : "transparent"
                                    border.color:   stepCard.isEnabled ? qgcPal.buttonHighlight : qgcPal.groupBorder
                                    border.width:   1
                                    radius:         ScreenTools.defaultFontPixelWidth * 0.25
                                    implicitWidth:  statusLabel.implicitWidth + ScreenTools.defaultFontPixelWidth * 1.0
                                    implicitHeight: statusLabel.implicitHeight + ScreenTools.defaultFontPixelHeight * 0.25

                                    QGCLabel {
                                        id:               statusLabel
                                        anchors.centerIn: parent
                                        text:             stepCard.isEnabled ? qsTr("Active") : qsTr("Skipped")
                                        font.pointSize:   ScreenTools.smallFontPointSize
                                        color:            stepCard.isEnabled ? qgcPal.buttonHighlight : qgcPal.colorGrey
                                    }
                                }
                            }

                            MouseArea {
                                id:           stepMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape:  Qt.PointingHandCursor
                                onClicked: {
                                    if (_voiceManager) {
                                        _voiceManager.togglePendingIntent(stepCard.index)
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
                        enabled:          confirmDialog._activeCount > 0
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
