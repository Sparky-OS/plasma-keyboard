/*
    SPDX-FileCopyrightText: 2026 Daniel Campos Ramos

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "fakekeyboard.h"
#include "logging.h"

#include <KLocalizedString>

FakeKeyboard::FakeKeyboard(QObject *parent)
    : QWaylandClientExtensionTemplate<FakeKeyboard>(6)
{
    setParent(parent);
}

FakeKeyboard::~FakeKeyboard()
{
    if (isActive()) {
        destroy();
    }
}

void FakeKeyboard::sendKeysym(uint keysym, bool pressed)
{
    // version() reads the bound object: only once the extension is active
    if (!isActive() || QtWayland::org_kde_kwin_fake_input::version() < 6) {
        if (!m_warned) {
            m_warned = true;
            qCWarning(PlasmaKeyboard) << "KWin's fake input (version 6) is not available to Plasma Keyboard: the keys cannot type;"
                                      << "bound:" << isActive();
        }
        return;
    }
    if (!m_authenticated) {
        authenticate(i18n("Plasma Keyboard"), i18nc("@info", "Types the keys of the on-screen keyboard while Fcitx 5 does not answer"));
        m_authenticated = true;
    }
    keyboard_keysym(keysym, pressed ? 1 : 0);
}

#include "moc_fakekeyboard.cpp"
