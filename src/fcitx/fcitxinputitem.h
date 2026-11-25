/*
    SPDX-FileCopyrightText: 2026 Daniel Campos Ramos

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QList>
#include <QPair>
#include <QPointer>
#include <QQuickItem>
#include <qqmlintegration.h>

#include "fcitxbridge.h"

/**
 * The item Qt Virtual Keyboard types into when Plasma Keyboard is Fcitx 5's keyboard: every commit and key becomes
 * an X keysym with Fcitx 5's key states, handed to Fcitx 5 through the bridge, so Fcitx 5's input method converts
 * the keys as it converts a physical keyboard's.
 */
class FcitxInputItem : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(FcitxBridge *bridge READ bridge WRITE setBridge NOTIFY bridgeChanged)

public:
    FcitxInputItem();

    FcitxBridge *bridge() const;
    void setBridge(FcitxBridge *bridge);

    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

    /** The keysym and Fcitx 5 key states of each character of @p text. */
    static QList<QPair<uint, uint>> keysForText(const QString &text);
    /** Fcitx 5 key states of Qt keyboard modifiers. */
    static uint statesFor(Qt::KeyboardModifiers modifiers);

Q_SIGNALS:
    void bridgeChanged();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;

private:
    void sendKeyEvent(QKeyEvent *event, bool release);
    void updateVisibility();
    void followActiveOutput(const QString &name);

    QPointer<FcitxBridge> m_bridge;
};
