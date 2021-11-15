import QtQuick.Controls 2.4
import QtQuick 2.11


MouseArea {
    id: barZone
    hoverEnabled: true
    property bool rightSide: true
    acceptedButtons: Qt.NoButton
    width: 2.4 * fontMetrics.font.pixelSize
    height: parent.height
    onEntered: {
        animator.stop()
        scenetoolbar.opacity = 1
    }
    onExited: {
        scenetoolbar.opacity = 0
    }

    Rectangle {
        id: scenetoolbar
        objectName: "scenetoolbar"
        width: barZone.width
        height: childrenRect.height
        anchors.right: barZone.right
        SystemPalette { id: myPalette; colorGroup: SystemPalette.Active }
        color: Qt.rgba(myPalette.window.r, myPalette.window.g, myPalette.window.b, 0.7)
        opacity: 0
        radius: 4
        border.color : Qt.rgba(0, 0, 0, 0.3)
        border.width: 1
        OpacityAnimator {
            id: animator
            target: scenetoolbar;
            from: 1;
            to: 0;
            duration: 2500
            running: false
        }

        function fadeBar()
        {
            animator.start()
        }

    
        
        Column {
            ToolButton {
                id: fullscreenButton
                objectName: "fullScreen"
                contentItem: Item {
                    Image {
                        source: "image://icon/view-fullscreen"
                        anchors.centerIn: parent
                        width: barZone.width - 4
                        height: width
                    }
                }
                width: barZone.width
                height: barZone.width
                focusPolicy: Qt.NoFocus
                ToolTip.visible: hovered
                ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                ToolTip.text: i18n("Switch Full Screen")
                ToolTip.timeout: 3000
                onClicked: {
                    controller.activateClipMonitor(root.isClipMonitor)
                    controller.triggerAction('monitor_fullscreen')
                }
            }
            ToolButton {
                contentItem: Item {
                    Image {
                        source: "image://icon/zoom-in"
                        anchors.centerIn: parent
                        width: barZone.width - 4
                        height: width
                    }
                }
                width: barZone.width
                height: barZone.width
                focusPolicy: Qt.NoFocus
                ToolTip.visible: hovered
                ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                ToolTip.text: i18n("Zoom in")
                ToolTip.timeout: 3000
                onClicked: {
                    controller.activateClipMonitor(root.isClipMonitor)
                    controller.triggerAction('monitor_zoomin')
                }
            }
            ToolButton {
                contentItem: Item {
                    Image {
                        source: "image://icon/zoom-out"
                        anchors.centerIn: parent
                        width: barZone.width - 4
                        height: width
                    }
                }
                width: barZone.width
                height: barZone.width
                ToolTip.visible: hovered
                focusPolicy: Qt.NoFocus
                ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                ToolTip.text: i18n("Zoom out")
                ToolTip.timeout: 3000
                onClicked: {
                    controller.activateClipMonitor(root.isClipMonitor)
                    controller.triggerAction('monitor_zoomout')
                }
            }
            ToolButton {
                objectName: "moveBar"
                contentItem: Item {
                    Image {
                        source: "image://icon/transform-move-horizontal"
                        anchors.centerIn: parent
                        width: barZone.width - 4
                        height: width
                    }
                }
                width: barZone.width
                height: barZone.width
                focusPolicy: Qt.NoFocus
                ToolTip.visible: hovered
                ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                ToolTip.text: i18n("Move Toolbar")
                ToolTip.timeout: 3000
                onClicked: {
                    if (barZone.rightSide) {
                        barZone.anchors.right = undefined
                        barZone.anchors.left = barZone.parent.left
                        barZone.rightSide = false
                        scenetoolbar.anchors.right = undefined
                        scenetoolbar.anchors.left = barZone.left
                        scenetoolbar.fadeBar()
                    } else {
                        barZone.anchors.left = undefined
                        barZone.anchors.right = barZone.parent.right
                        barZone.rightSide = true
                        scenetoolbar.anchors.left = undefined
                        scenetoolbar.anchors.right = barZone.right
                        scenetoolbar.fadeBar()
                    }
                }
            }
        }
    }
}