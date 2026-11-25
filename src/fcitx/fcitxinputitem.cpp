/*
    SPDX-FileCopyrightText: 2026 Daniel Campos Ramos

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "fcitxinputitem.h"
#include "logging.h"

#include <QGuiApplication>
#include <QInputMethod>
#include <QKeyEvent>
#include <QQuickWindow>
#include <QtGui/private/qxkbcommon_p.h>
#include <qpa/qwindowsysteminterface.h>

#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon.h>

FcitxInputItem::FcitxInputItem()
{
    setFlag(ItemAcceptsInputMethod, true);
    // the keyboard's own hide key hides Qt's input panel: the keyboard then hides for Fcitx 5 too
    connect(QGuiApplication::inputMethod(), &QInputMethod::visibleChanged, this, [this] {
        if (m_bridge && m_bridge->isVisible() && !QGuiApplication::inputMethod()->isVisible()) {
            m_bridge->hide();
        }
    });
}

FcitxBridge *FcitxInputItem::bridge() const
{
    return m_bridge;
}

void FcitxInputItem::setBridge(FcitxBridge *bridge)
{
    if (m_bridge == bridge) {
        return;
    }
    if (m_bridge) {
        disconnect(m_bridge, nullptr, this, nullptr);
    }
    m_bridge = bridge;
    if (m_bridge) {
        connect(m_bridge, &FcitxBridge::visibleChanged, this, &FcitxInputItem::updateVisibility);
    }
    Q_EMIT bridgeChanged();
    updateVisibility();
}

void FcitxInputItem::updateVisibility()
{
    QQuickWindow *win = window();
    if (!win || !m_bridge) {
        return;
    }
    if (m_bridge->isVisible()) {
        win->setVisible(true);
        // KWin never gives the keyboard's surface the keyboard focus (it would take it from the text field), so the
        // window is made Qt's focus window here, as the input panel surface does on configure
        QWindowSystemInterface::handleFocusWindowChanged(win, Qt::ActiveWindowFocusReason);
        forceActiveFocus();
        QGuiApplication::inputMethod()->update(Qt::ImQueryAll);
        QGuiApplication::inputMethod()->show();
    } else {
        QGuiApplication::inputMethod()->hide();
        QGuiApplication::inputMethod()->reset();
        win->setVisible(false);
    }
}

QVariant FcitxInputItem::inputMethodQuery(Qt::InputMethodQuery query) const
{
    switch (query) {
    case Qt::ImEnabled:
        return true;
    case Qt::ImHints:
        // Fcitx 5's input method does the conversion and the capitalisation: Qt Virtual Keyboard types the keys as they are
        return QVariant::fromValue<int>(Qt::ImhNoPredictiveText | Qt::ImhNoAutoUppercase);
    case Qt::ImSurroundingText:
    case Qt::ImCurrentSelection:
    case Qt::ImTextBeforeCursor:
    case Qt::ImTextAfterCursor:
        return QString();
    case Qt::ImCursorPosition:
    case Qt::ImAnchorPosition:
        return 0;
    default:
        return {};
    }
}

QList<QPair<uint, uint>> FcitxInputItem::keysForText(const QString &text)
{
    QList<QPair<uint, uint>> keys;
    const auto codePoints = text.toUcs4();
    for (const char32_t codePoint : codePoints) {
        uint keysym = xkb_utf32_to_keysym(codePoint);
        if (codePoint == U'\n' || codePoint == U'\r') {
            keysym = XKB_KEY_Return;
        }
        if (keysym == XKB_KEY_NoSymbol) {
            continue;
        }
        const uint states = QChar::isUpper(codePoint) ? uint(FcitxBridge::ShiftState) : 0u;
        keys.append({keysym, states});
    }
    return keys;
}

uint FcitxInputItem::statesFor(Qt::KeyboardModifiers modifiers)
{
    uint states = 0;
    if (modifiers & Qt::ShiftModifier) {
        states |= FcitxBridge::ShiftState;
    }
    if (modifiers & Qt::ControlModifier) {
        states |= FcitxBridge::ControlState;
    }
    if (modifiers & Qt::AltModifier) {
        states |= FcitxBridge::AltState;
    }
    return states;
}

void FcitxInputItem::sendKeyEvent(QKeyEvent *event, bool release)
{
    if (!m_bridge) {
        return;
    }
    const QString text = event->text();
    const bool printable = !text.isEmpty() && !text.at(0).isNonCharacter() && text.at(0) >= u' ' && text.at(0) != QChar(0x7f);
    if (printable) {
        for (const auto &[keysym, states] : keysForText(text)) {
            m_bridge->sendKey(keysym, states | statesFor(event->modifiers()), release);
        }
    } else {
        const QList<xkb_keysym_t> keysyms = QXkbCommon::toKeysym(event);
        for (const xkb_keysym_t keysym : keysyms) {
            m_bridge->sendKey(keysym, statesFor(event->modifiers()), release);
        }
    }
    event->accept();
}

void FcitxInputItem::keyPressEvent(QKeyEvent *event)
{
    // Qt::Key_Context1 is the keyboard's own key for its special keys
    if (event->key() == Qt::Key_Context1) {
        return;
    }
    sendKeyEvent(event, false);
}

void FcitxInputItem::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Context1) {
        return;
    }
    sendKeyEvent(event, true);
}

void FcitxInputItem::inputMethodEvent(QInputMethodEvent *event)
{
    if (!m_bridge) {
        return;
    }
    // Qt Virtual Keyboard's plain input method commits what each key types; Fcitx 5 gets it as the keys themselves
    for (const auto &[keysym, states] : keysForText(event->commitString())) {
        m_bridge->sendKey(keysym, states, false);
        m_bridge->sendKey(keysym, states, true);
    }
    if (!event->preeditString().isEmpty()) {
        qCDebug(PlasmaKeyboard) << "Ignoring a preedit from Qt Virtual Keyboard in Fcitx 5 mode:" << event->preeditString();
    }
    event->accept();
}

#include "moc_fcitxinputitem.cpp"
