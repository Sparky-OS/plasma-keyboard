/*
    SPDX-FileCopyrightText: 2026 Daniel Campos Ramos

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.plasma.keyboard
import org.kde.plasma.keyboard.lib as PlasmaKeyboard

/**
 * The bar above the keys in Fcitx 5 mode: the input method (a touch lists the others), the preedit, the candidates
 * (a touch chooses one), the pages, and what went wrong when Fcitx 5 does not answer.
 */
Item {
    id: bar

    required property FcitxBridge bridge

    // true while the input methods are listed in place of the candidates
    property bool listingInputMethods: false

    implicitHeight: Kirigami.Units.gridUnit * 3
    height: implicitHeight

    // the system's negative-text colour, moved away from the keyboard's background just enough to be read there: a
    // contrast of 4.5:1 or more (WCAG 2.x, the relative luminance of sRGB)
    readonly property color problemColor: readableOn(Kirigami.Theme.negativeTextColor, PlasmaKeyboard.BreezeConstants.keyboardBackgroundColor)

    function luminance(c) {
        function channel(v) {
            return v <= 0.03928 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4);
        }
        return 0.2126 * channel(c.r) + 0.7152 * channel(c.g) + 0.0722 * channel(c.b);
    }
    function contrast(a, b) {
        const la = luminance(a);
        const lb = luminance(b);
        return (Math.max(la, lb) + 0.05) / (Math.min(la, lb) + 0.05);
    }
    function readableOn(foreground, background) {
        // towards white on a dark keyboard, towards black on a light one
        const target = luminance(background) < 0.18 ? 1 : 0;
        for (let step = 0; step <= 20; ++step) {
            const t = step / 20;
            const c = Qt.rgba(foreground.r + (target - foreground.r) * t, foreground.g + (target - foreground.g) * t,
                              foreground.b + (target - foreground.b) * t, 1);
            if (contrast(c, background) >= 4.5) {
                return c;
            }
        }
        return target ? "white" : "black";
    }

    component BarButton: Rectangle {
        id: button

        property string label
        property bool current: false
        signal activated()

        Layout.fillHeight: true
        Layout.preferredWidth: Math.max(height * 1.4, buttonText.implicitWidth + Kirigami.Units.largeSpacing * 2)
        radius: PlasmaKeyboard.BreezeConstants.buttonRadius
        color: tap.pressed || current ? PlasmaKeyboard.BreezeConstants.normalKeyPressedBackgroundColor
                                      : PlasmaKeyboard.BreezeConstants.normalKeyBackgroundColor
        border.width: current ? 2 : 0
        border.color: Kirigami.Theme.highlightColor

        Accessible.role: Accessible.Button
        Accessible.name: label
        Accessible.onPressAction: button.activated()

        Text {
            id: buttonText
            anchors.centerIn: parent
            text: button.label
            color: PlasmaKeyboard.BreezeConstants.keyTextColor
            font.family: PlasmaKeyboard.BreezeConstants.fontFamily
            font.pixelSize: button.height * 0.5
        }

        TapHandler {
            id: tap
            onTapped: button.activated()
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.smallSpacing
        spacing: Kirigami.Units.smallSpacing

        BarButton {
            id: inputMethodButton
            label: bar.bridge.currentInputMethodLabel
            current: bar.listingInputMethods
            Accessible.description: i18nc("@info:tooltip", "Input method: touch to choose another")
            onActivated: bar.listingInputMethods = !bar.listingInputMethods
        }

        // what went wrong, in place of the candidates
        Text {
            visible: bar.bridge.problem !== "" && !bar.listingInputMethods
            Layout.fillWidth: true
            Layout.fillHeight: true
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            text: bar.bridge.problem
            color: bar.problemColor
            font.pixelSize: bar.height * 0.3
        }

        // the input methods of Fcitx 5's current group
        Repeater {
            model: bar.listingInputMethods ? bar.bridge.inputMethods : []
            delegate: BarButton {
                required property var modelData
                label: modelData.label + "  " + modelData.name
                current: modelData.uniqueName === bar.bridge.currentInputMethod
                onActivated: {
                    bar.bridge.switchInputMethod(modelData.uniqueName);
                    bar.listingInputMethods = false;
                }
            }
        }

        Text {
            id: preeditText
            visible: !bar.listingInputMethods && bar.bridge.problem === "" && text !== ""
            Layout.fillHeight: true
            verticalAlignment: Text.AlignVCenter
            text: bar.bridge.preedit
            color: PlasmaKeyboard.BreezeConstants.keyTextColor
            font.underline: true
            font.pixelSize: bar.height * 0.45
        }

        BarButton {
            visible: !bar.listingInputMethods && bar.bridge.hasPreviousPage
            label: "‹"
            Accessible.description: i18nc("@action:button", "Previous candidates")
            onActivated: bar.bridge.previousPage()
        }

        ListView {
            id: candidateList
            visible: !bar.listingInputMethods && bar.bridge.problem === ""
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: ListView.Horizontal
            spacing: Kirigami.Units.smallSpacing
            clip: true
            model: bar.bridge.candidates
            delegate: BarButton {
                required property int index
                required property string modelData
                height: candidateList.height
                width: Math.max(height * 1.4, implicitWidth)
                implicitWidth: label.length * height * 0.5 + Kirigami.Units.largeSpacing * 2
                label: modelData
                current: index === bar.bridge.highlightedCandidate
                onActivated: bar.bridge.selectCandidate(index)
            }
        }

        // while the input methods are listed, they line up after the button
        Item {
            visible: bar.listingInputMethods
            Layout.fillWidth: true
        }

        BarButton {
            visible: !bar.listingInputMethods && bar.bridge.hasNextPage
            label: "›"
            Accessible.description: i18nc("@action:button", "Next candidates")
            onActivated: bar.bridge.nextPage()
        }
    }
}
