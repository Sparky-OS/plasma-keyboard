/*
    SPDX-FileCopyrightText: 2024 Aleix Pol i Gonzalez <aleixpol@kde.org>
    SPDX-FileCopyrightText: 2026 Kristen McWilliam <kristen@kde.org>
    SPDX-FileCopyrightText: 2026 Daniel Campos Ramos

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.VirtualKeyboard
import QtQuick.VirtualKeyboard.Settings

import org.kde.plasma.keyboard
import org.kde.plasma.keyboard.lib as PlasmaKeyboard

/**
 * Plasma Keyboard as Fcitx 5's keyboard (plasma-keyboard --fcitx): the keys of Qt Virtual Keyboard, typed through
 * Fcitx 5, with the preedit, the candidates and the input methods Fcitx 5 hands over in a bar above them.
 * The window is a layer-shell surface at the bottom of the screen that never takes the keyboard focus, so the text
 * field keeps it and Fcitx 5 keeps its input context.
 */
InputPanelWindow {
    id: root

    required property FcitxBridge bridge

    color: "transparent"
    visible: false
    width: Screen.width
    height: panel.height

    interactiveRegion: Qt.rect(0, 0, width, height)

    FcitxInputItem {
        id: input
        focus: true
        bridge: root.bridge
    }

    Rectangle {
        id: panel

        width: root.width
        height: candidateBar.height + inputPanel.height
        color: PlasmaKeyboard.BreezeConstants.keyboardBackgroundColor

        FcitxCandidateBar {
            id: candidateBar
            bridge: root.bridge
            width: parent.width
        }

        InputPanel {
            id: inputPanel
            y: candidateBar.height
            anchors.horizontalCenter: parent.horizontalCenter

            // height is calculated by InputPanel
            width: inputPanel.keyboard.style ? inputPanel.keyboard.style.aspectRatio * inputPanel.keyboard.style.targetKeyboardHeight : 0

            focusPolicy: Qt.NoFocus
            // the language key switches Fcitx 5's input method
            externalLanguageSwitchEnabled: true
            onExternalLanguageSwitch: (localeList, currentIndex) => candidateBar.listingInputMethods = !candidateBar.listingInputMethods
        }
    }

    function applyLayout() {
        VirtualKeyboardSettings.activeLocales = [root.bridge.layoutLocale];
        VirtualKeyboardSettings.locale = root.bridge.layoutLocale;
    }

    Connections {
        target: root.bridge
        function onInputMethodsChanged() {
            root.applyLayout();
        }
    }

    Component.onCompleted: {
        VirtualKeyboardSettings.styleName = "Breeze";
        root.applyLayout();
    }
}
