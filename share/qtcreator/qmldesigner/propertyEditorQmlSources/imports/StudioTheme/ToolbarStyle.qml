// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0 WITH Qt-GPL-exception-1.0

import QtQuick

ControlStyle {
    controlSize: Qt.size(Values.topLevelComboWidth, Values.topLevelComboHeight)
    baseIconFontSize: Values.topLevelComboIcon
    smallIconFontSize: Values.baseFont
    background.idle: Values.themeControlBackground_toolbarIdle
    background.hover: Values.themeControlBackground_topToolbarHover
    background.globalHover: Values.themeControlBackground_topToolbarHover
    border.idle: Values.controlOutline_toolbarIdle
    text.hover: Values.themeTextColor
}
