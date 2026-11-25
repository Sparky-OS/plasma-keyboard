/*
    SPDX-FileCopyrightText: 2026 Daniel Campos Ramos

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QDBusConnection>
#include <QHash>
#include <QVariantMap>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <qqmlintegration.h>

class FakeKeyboard;

/**
 * Plasma Keyboard as the on-screen keyboard of Fcitx 5.
 *
 * When Fcitx 5 is KWin's input method, Fcitx 5 holds the input-method connection and does the conversion
 * (Mozc, Hangul, Pinyin, Chewing). Its "DBus Virtual Keyboard" addon talks to one keyboard over D-Bus:
 * Fcitx 5 owns org.fcitx.Fcitx5.VirtualKeyboardBackend (keys, candidate choice, paging) while its keyboard mode
 * is on, and calls the keyboard that owns org.fcitx.Fcitx5.VirtualKeyboard with the preedit, the candidates and
 * the input method list (fcitx5 src/ui/virtualkeyboard/virtualkeyboard.cpp).
 *
 * The keyboard shows when KWin would show an on-screen keyboard for the text field (org.kde.kwin.VirtualKeyboard:
 * activeChanged, willShowOnActive) and Fcitx 5 asks it to show.
 *
 * When Fcitx 5 does not answer, the person is told on the keyboard and its keys type plain text through KWin's
 * fake input (org_kde_kwin_fake_input keyboard_keysym).
 */
class FcitxBridge : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("FcitxBridge is provided by the application")

    Q_PROPERTY(bool visible READ isVisible NOTIFY visibleChanged)
    Q_PROPERTY(bool plainTyping READ plainTyping NOTIFY plainTypingChanged)
    Q_PROPERTY(QString problem READ problem NOTIFY problemChanged)
    Q_PROPERTY(QString preedit READ preedit NOTIFY preeditChanged)
    Q_PROPERTY(int preeditCaret READ preeditCaret NOTIFY preeditChanged)
    Q_PROPERTY(QStringList candidates READ candidates NOTIFY candidatesChanged)
    Q_PROPERTY(int highlightedCandidate READ highlightedCandidate NOTIFY candidatesChanged)
    Q_PROPERTY(bool hasPreviousPage READ hasPreviousPage NOTIFY candidatesChanged)
    Q_PROPERTY(bool hasNextPage READ hasNextPage NOTIFY candidatesChanged)
    Q_PROPERTY(QString currentInputMethod READ currentInputMethod NOTIFY inputMethodsChanged)
    Q_PROPERTY(QString currentInputMethodLabel READ currentInputMethodLabel NOTIFY inputMethodsChanged)
    Q_PROPERTY(QVariantList inputMethods READ inputMethods NOTIFY inputMethodsChanged)
    Q_PROPERTY(QString layoutLocale READ layoutLocale NOTIFY inputMethodsChanged)

public:
    // KeyStates of fcitx-utils/keysym.h
    enum KeyState : uint {
        ShiftState = 1u << 0,
        ControlState = 1u << 2,
        AltState = 1u << 3,
    };

    explicit FcitxBridge(QObject *parent = nullptr);
    ~FcitxBridge() override;

    /**
     * Registers the keyboard on the session bus and starts following KWin and Fcitx 5.
     * @return false when the keyboard's name is owned by another program; the reason is in problem().
     */
    bool start();

    /**
     * Why the Fcitx 5 keyboard has nothing to do in this session, or an empty string when it has:
     * KWin's input method is not Fcitx 5, or the person turned the keyboard off for Fcitx 5.
     */
    static QString whyNotNeeded();

    /**
     * The Qt Virtual Keyboard locale whose keys suit an input method language of Fcitx 5 (its "languageCode").
     */
    static QString layoutLocaleForLanguage(const QString &languageCode);

    bool isVisible() const;
    bool plainTyping() const;
    QString problem() const;
    QString preedit() const;
    int preeditCaret() const;
    QStringList candidates() const;
    int highlightedCandidate() const;
    bool hasPreviousPage() const;
    bool hasNextPage() const;
    QString currentInputMethod() const;
    QString currentInputMethodLabel() const;
    QVariantList inputMethods() const;
    QString layoutLocale() const;

    /** A key of the on-screen keyboard: an X keysym with Fcitx 5's key states. */
    void sendKey(uint keysym, uint states, bool release);

    Q_INVOKABLE void selectCandidate(int index);
    Q_INVOKABLE void previousPage();
    Q_INVOKABLE void nextPage();
    Q_INVOKABLE void switchInputMethod(const QString &uniqueName);
    /** The keyboard's own hide key. */
    Q_INVOKABLE void hide();

    // org.fcitx.Fcitx5.VirtualKeyboard1, called by Fcitx 5 (see FcitxKeyboardService)
    void fcitxShow();
    void fcitxHide();
    void fcitxPreeditCaret(int caret);
    void fcitxPreeditArea(const QString &text);
    void fcitxCandidateArea(const QStringList &candidates, bool hasPrev, bool hasNext, int pageIndex, int globalCursorIndex);
    void fcitxInputMethodActivated(const QString &uniqueName);
    void fcitxInputMethodDeactivated(const QString &uniqueName);
    void fcitxInputMethodListChanged();

Q_SIGNALS:
    void visibleChanged();
    void plainTypingChanged();
    void problemChanged();
    void preeditChanged();
    void candidatesChanged();
    void inputMethodsChanged();

private Q_SLOTS:
    void kwinActiveChanged();
    void kwinShowRequested();

private:
    void askFcitxToShow();
    void askKWinWhetherToShow();
    void setKWinWantsKeyboard(bool wants);
    void setFcitxShown(bool shown);
    void enterPlainTyping(const QString &reason);
    void leavePlainTyping();
    void setProblem(const QString &problem);
    void refreshInputMethods();
    void readInputMethodGroup(const QHash<QString, QVariantMap> &known);
    void readCurrentInputMethod();
    void updateLayoutLocale();
    void updateVisible();
    void tellKWinShown(bool shown);
    uint keycodeFor(uint keysym) const;

    QDBusConnection m_bus;
    QObject *m_service = nullptr;
    FakeKeyboard *m_fakeKeyboard = nullptr;
    QTimer m_answerTimer;

    bool m_kwinWants = false;
    bool m_fcitxShown = false;
    bool m_visible = false;
    bool m_toldKWinFailure = false;
    bool m_plainTyping = false;
    bool m_backendAvailable = false;
    QString m_problem;

    QString m_preedit;
    int m_preeditCaret = -1;
    QStringList m_candidates;
    int m_highlighted = -1;
    bool m_hasPrev = false;
    bool m_hasNext = false;

    QString m_currentInputMethod;
    QVariantList m_inputMethods;
    QString m_layoutLocale = QStringLiteral("en_US");
};
