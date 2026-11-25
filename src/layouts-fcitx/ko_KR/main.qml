// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-FileCopyrightText: 2026 Daniel Campos Ramos
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
//
// Plasma Keyboard as Fcitx 5's keyboard: the keys of the fallback layout (layouts/fallback/main.qml), labelled
// with the jamo of Fcitx 5's Hangul engine (Dubeolsik, its default keyboard). The keys type Latin keysyms; Fcitx 5's engine makes them Hangul.
// Made by make-fcitx-layouts.py; edit the script, not this file.

import QtQuick
import QtQuick.VirtualKeyboard
import QtQuick.VirtualKeyboard.Components
import QtQuick.Layouts

KeyboardLayout {
    inputMode: InputEngine.InputMode.Latin
    keyWeight: 160
    readonly property real normalKeyWidth: normalKey.width
    readonly property real functionKeyWidth: mapFromItem(normalKey, normalKey.width / 2, 0).x
    KeyboardRow {
        Key {
            key: Qt.Key_Q
            text: "q"
            displayText: "ㅂ"
            smallText: "ㅃ"
            smallTextVisible: true
        }
        Key {
            id: normalKey
            key: Qt.Key_W
            text: "w"
            displayText: "ㅈ"
            smallText: "ㅉ"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_E
            text: "e"
            displayText: "ㄷ"
            smallText: "ㄸ"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_R
            text: "r"
            displayText: "ㄱ"
            smallText: "ㄲ"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_T
            text: "t"
            displayText: "ㅅ"
            smallText: "ㅆ"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_Y
            text: "y"
            displayText: "ㅛ"
        }
        Key {
            key: Qt.Key_U
            text: "u"
            displayText: "ㅕ"
        }
        Key {
            key: Qt.Key_I
            text: "i"
            displayText: "ㅑ"
        }
        Key {
            key: Qt.Key_O
            text: "o"
            displayText: "ㅐ"
            smallText: "ㅒ"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_P
            text: "p"
            displayText: "ㅔ"
            smallText: "ㅖ"
            smallTextVisible: true
        }
    }
    KeyboardRow {
        KeyboardRow {
            Layout.preferredWidth: functionKeyWidth
            Layout.fillWidth: false
            FillerKey {
            }
            Key {
                key: Qt.Key_A
                text: "a"
                displayText: "ㅁ"
                weight: normalKeyWidth
                Layout.fillWidth: false
            }
        }
        Key {
            key: Qt.Key_S
            text: "s"
            displayText: "ㄴ"
        }
        Key {
            key: Qt.Key_D
            text: "d"
            displayText: "ㅇ"
        }
        Key {
            key: Qt.Key_F
            text: "f"
            displayText: "ㄹ"
        }
        Key {
            key: Qt.Key_G
            text: "g"
            displayText: "ㅎ"
        }
        Key {
            key: Qt.Key_H
            text: "h"
            displayText: "ㅗ"
        }
        Key {
            key: Qt.Key_J
            text: "j"
            displayText: "ㅓ"
        }
        Key {
            key: Qt.Key_K
            text: "k"
            displayText: "ㅏ"
        }
        KeyboardRow {
            Layout.preferredWidth: functionKeyWidth
            Layout.fillWidth: false
            Key {
                key: Qt.Key_L
                text: "l"
                displayText: "ㅣ"
                weight: normalKeyWidth
                Layout.fillWidth: false
            }
            FillerKey {
            }
        }
    }
    KeyboardRow {
        ShiftKey {
            weight: functionKeyWidth
            Layout.fillWidth: false
        }
        Key {
            key: Qt.Key_Z
            text: "z"
            displayText: "ㅋ"
        }
        Key {
            key: Qt.Key_X
            text: "x"
            displayText: "ㅌ"
        }
        Key {
            key: Qt.Key_C
            text: "c"
            displayText: "ㅊ"
        }
        Key {
            key: Qt.Key_V
            text: "v"
            displayText: "ㅍ"
        }
        Key {
            key: Qt.Key_B
            text: "b"
            displayText: "ㅠ"
        }
        Key {
            key: Qt.Key_N
            text: "n"
            displayText: "ㅜ"
        }
        Key {
            key: Qt.Key_M
            text: "m"
            displayText: "ㅡ"
        }
        BackspaceKey {
            weight: functionKeyWidth
            Layout.fillWidth: false
        }
    }
    KeyboardRow {
        SymbolModeKey {
            weight: functionKeyWidth
            Layout.fillWidth: false
        }
        ChangeLanguageKey {
            weight: normalKeyWidth
            Layout.fillWidth: false
        }
        Key {
            key: Qt.Key_Comma
            weight: normalKeyWidth
            Layout.fillWidth: false
            text: ","
            smallText: "\u2699"
            smallTextVisible: keyboard.isFunctionPopupListAvailable()
            highlighted: true
        }
        SpaceKey {
        }
        Key {
            key: Qt.Key_Period
            weight: normalKeyWidth
            Layout.fillWidth: false
            text: "."
            smallText: "!?"
            smallTextVisible: true
            highlighted: true
        }
        HideKeyboardKey {
            weight: normalKeyWidth
            Layout.fillWidth: false
        }
        EnterKey {
            weight: functionKeyWidth
            Layout.fillWidth: false
        }
    }
}
