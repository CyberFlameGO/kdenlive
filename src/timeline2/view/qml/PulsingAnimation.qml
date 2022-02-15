/*
    SPDX-FileCopyrightText: 2015 Meltytech LLC
    SPDX-FileCopyrightText: 2015 Harald Hvaal <harald.hvaal@gmail.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.11

SequentialAnimation {
    property alias target: firstAnim.target

    loops: Animation.Infinite
    NumberAnimation {
        id: firstAnim
        property: "opacity"
        from: 0.3
        to: 0
        duration: 800
    }
    NumberAnimation {
        target: firstAnim.target
        property: "opacity"
        from: 0
        to: 0.3
        duration: 800
    }
    PauseAnimation { duration: 300 }
}

