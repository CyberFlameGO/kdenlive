/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once
#include <QUrl>

// Returns true if the given file is on a removable device

bool isOnRemovableDevice(const QString &path);
bool isOnRemovableDevice(const QUrl &file);
