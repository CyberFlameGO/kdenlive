/*
    SPDX-FileCopyrightText: 2018 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import QtQuick 2.11

Item {
    id: overlay
    // Vertical segments
    Rectangle {
        color: root.overlayColor
        height: parent.height
        width: 1
        x: parent.width / 3
    }
    Rectangle {
        color: root.overlayColor
        height: parent.height
        width: 1
        x: (parent.width / 3 ) * 2
    }
    // Horizontal segments
    Rectangle {
        color: root.overlayColor
        width: parent.width
        height: 1
        y: parent.height / 3
    }
    Rectangle {
        color: root.overlayColor
        width: parent.width
        height: 1
        y: (parent.height / 3) * 2
    }
}
