// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-FileCopyrightText: 2026 Daniel Campos Ramos
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
//
// Plasma Keyboard as Fcitx 5's keyboard: the keys of the fallback layout (layouts/fallback/main.qml), labelled
// with the bopomofo and tones of Fcitx 5's Chewing engine (Dachen, its default keyboard). The keys type Latin keysyms; Fcitx 5's engine makes them Zhuyin and convert it.
// Made by make-fcitx-layouts.py; edit the script, not this file.

import QtQuick
import QtQuick.VirtualKeyboard
import QtQuick.VirtualKeyboard.Components
import QtQuick.Layouts

KeyboardLayout {
    inputMode: InputEngine.InputMode.Latin
    keyWeight: 160
    KeyboardRow {
        Key {
            key: Qt.Key_1
            text: "1"
            displayText: "ㄅ"
        }
        Key {
            key: Qt.Key_2
            text: "2"
            displayText: "ㄉ"
        }
        Key {
            key: Qt.Key_3
            text: "3"
            displayText: "ˇ"
        }
        Key {
            key: Qt.Key_4
            text: "4"
            displayText: "ˋ"
        }
        Key {
            key: Qt.Key_5
            text: "5"
            displayText: "ㄓ"
        }
        Key {
            key: Qt.Key_6
            text: "6"
            displayText: "ˊ"
        }
        Key {
            key: Qt.Key_7
            text: "7"
            displayText: "˙"
        }
        Key {
            key: Qt.Key_8
            text: "8"
            displayText: "ㄚ"
        }
        Key {
            key: Qt.Key_9
            text: "9"
            displayText: "ㄞ"
        }
        Key {
            key: Qt.Key_0
            text: "0"
            displayText: "ㄢ"
        }
        Key {
            key: Qt.Key_Minus
            text: "-"
            displayText: "ㄦ"
        }
    }
    KeyboardRow {
        Key {
            key: Qt.Key_Q
            text: "q"
            displayText: "ㄆ"
        }
        Key {
            key: Qt.Key_W
            text: "w"
            displayText: "ㄊ"
        }
        Key {
            key: Qt.Key_E
            text: "e"
            displayText: "ㄍ"
        }
        Key {
            key: Qt.Key_R
            text: "r"
            displayText: "ㄐ"
        }
        Key {
            key: Qt.Key_T
            text: "t"
            displayText: "ㄔ"
        }
        Key {
            key: Qt.Key_Y
            text: "y"
            displayText: "ㄗ"
        }
        Key {
            key: Qt.Key_U
            text: "u"
            displayText: "ㄧ"
        }
        Key {
            key: Qt.Key_I
            text: "i"
            displayText: "ㄛ"
        }
        Key {
            key: Qt.Key_O
            text: "o"
            displayText: "ㄟ"
        }
        Key {
            key: Qt.Key_P
            text: "p"
            displayText: "ㄣ"
        }
    }
    KeyboardRow {
        Key {
            key: Qt.Key_A
            text: "a"
            displayText: "ㄇ"
        }
        Key {
            key: Qt.Key_S
            text: "s"
            displayText: "ㄋ"
        }
        Key {
            key: Qt.Key_D
            text: "d"
            displayText: "ㄎ"
        }
        Key {
            key: Qt.Key_F
            text: "f"
            displayText: "ㄑ"
        }
        Key {
            key: Qt.Key_G
            text: "g"
            displayText: "ㄕ"
        }
        Key {
            key: Qt.Key_H
            text: "h"
            displayText: "ㄘ"
        }
        Key {
            key: Qt.Key_J
            text: "j"
            displayText: "ㄨ"
        }
        Key {
            key: Qt.Key_K
            text: "k"
            displayText: "ㄜ"
        }
        Key {
            key: Qt.Key_L
            text: "l"
            displayText: "ㄠ"
        }
        Key {
            key: Qt.Key_Semicolon
            text: ";"
            displayText: "ㄤ"
        }
        BackspaceKey {
        }
    }
    KeyboardRow {
        Key {
            key: Qt.Key_Z
            text: "z"
            displayText: "ㄈ"
        }
        Key {
            key: Qt.Key_X
            text: "x"
            displayText: "ㄌ"
        }
        Key {
            key: Qt.Key_C
            text: "c"
            displayText: "ㄏ"
        }
        Key {
            key: Qt.Key_V
            text: "v"
            displayText: "ㄒ"
        }
        Key {
            key: Qt.Key_B
            text: "b"
            displayText: "ㄖ"
        }
        Key {
            key: Qt.Key_N
            text: "n"
            displayText: "ㄙ"
        }
        Key {
            key: Qt.Key_M
            text: "m"
            displayText: "ㄩ"
        }
        Key {
            key: Qt.Key_Comma
            text: ","
            displayText: "ㄝ"
        }
        Key {
            key: Qt.Key_Period
            text: "."
            displayText: "ㄡ"
        }
        Key {
            key: Qt.Key_Slash
            text: "/"
            displayText: "ㄥ"
        }
    }
    KeyboardRow {
        SymbolModeKey {
        }
        ChangeLanguageKey {
        }
        SpaceKey {
        }
        HideKeyboardKey {
        }
        EnterKey {
        }
    }
}
