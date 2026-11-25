/*
    SPDX-FileCopyrightText: 2026 Daniel Campos Ramos

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QtWaylandClient/QWaylandClientExtensionTemplate>

#include "qwayland-fake-input.h"

/**
 * Keys typed through KWin's fake input device (org_kde_kwin_fake_input version 6, keyboard_keysym), for the
 * Fcitx 5 keyboard when Fcitx 5 does not answer. KWin grants the protocol to the executable whose desktop entry
 * lists it in X-KDE-Wayland-Interfaces (org.kde.plasma.keyboard.fcitx.desktop).
 */
class FakeKeyboard : public QWaylandClientExtensionTemplate<FakeKeyboard>, public QtWayland::org_kde_kwin_fake_input
{
    Q_OBJECT
public:
    explicit FakeKeyboard(QObject *parent = nullptr);
    ~FakeKeyboard() override;

    void sendKeysym(uint keysym, bool pressed);

private:
    bool m_authenticated = false;
    bool m_warned = false;
};
