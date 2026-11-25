/*
    SPDX-FileCopyrightText: 2026 Daniel Campos Ramos

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "fcitxbridge.h"
#include "fakekeyboard.h"
#include "logging.h"
#include "plasmakeyboardsettings.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QDBusArgument>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QDateTime>
#include <QFileInfo>
#include <QHash>

#include <xkbcommon/xkbcommon.h>

using namespace Qt::StringLiterals;

namespace
{
// fcitx5 src/ui/virtualkeyboard/virtualkeyboard.cpp
const QString s_keyboardName = u"org.fcitx.Fcitx5.VirtualKeyboard"_s;
const QString s_keyboardPath = u"/org/fcitx/virtualkeyboard/impanel"_s;
const QString s_backendName = u"org.fcitx.Fcitx5.VirtualKeyboardBackend"_s;
const QString s_backendPath = u"/virtualkeyboard"_s;
const QString s_backendInterface = u"org.fcitx.Fcitx5.VirtualKeyboardBackend1"_s;
const QString s_fcitxName = u"org.fcitx.Fcitx5"_s;
const QString s_fcitxKeyboardPath = u"/virtualkeyboard"_s;
const QString s_fcitxKeyboardInterface = u"org.fcitx.Fcitx.VirtualKeyboard1"_s;
// fcitx5 src/modules/dbus/dbusmodule.cpp
const QString s_controllerPath = u"/controller"_s;
const QString s_controllerInterface = u"org.fcitx.Fcitx.Controller1"_s;
// kwin src/virtualkeyboard_dbus.h
const QString s_kwinName = u"org.kde.KWin"_s;
const QString s_kwinPath = u"/VirtualKeyboard"_s;
const QString s_kwinInterface = u"org.kde.kwin.VirtualKeyboard"_s;

// How long Fcitx 5 has to answer a request to show its keyboard before the keys type plain text
constexpr int s_answerTimeoutMs = 1500;

// A call to Fcitx 5 never starts it: a Fcitx 5 started by D-Bus activation is not KWin's input method (it would bind KWin's
// input method protocol beside it and take KWin's input panel), so an absent Fcitx 5 answers "no such service" instead
QDBusMessage fcitxCall(const QString &service, const QString &path, const QString &interface, const QString &method)
{
    QDBusMessage message = QDBusMessage::createMethodCall(service, path, interface, method);
    message.setAutoStartService(false);
    return message;
}

QHash<uint, uint> usKeycodes()
{
    QHash<uint, uint> keycodes;
    xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!context) {
        return keycodes;
    }
    const xkb_rule_names names{"evdev", "pc105", "us", "", ""};
    xkb_keymap *keymap = xkb_keymap_new_from_names(context, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (keymap) {
        for (xkb_keycode_t code = xkb_keymap_min_keycode(keymap); code <= xkb_keymap_max_keycode(keymap); ++code) {
            for (xkb_level_index_t level = 0; level < 2; ++level) {
                const xkb_keysym_t *syms = nullptr;
                const int count = xkb_keymap_key_get_syms_by_level(keymap, code, 0, level, &syms);
                for (int i = 0; i < count; ++i) {
                    if (!keycodes.contains(syms[i])) {
                        keycodes.insert(syms[i], code);
                    }
                }
            }
        }
        xkb_keymap_unref(keymap);
    }
    xkb_context_unref(context);
    return keycodes;
}
}

/**
 * org.fcitx.Fcitx5.VirtualKeyboard1 on /org/fcitx/virtualkeyboard/impanel: what Fcitx 5 calls on its keyboard.
 */
class FcitxKeyboardService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.fcitx.Fcitx5.VirtualKeyboard1")

public:
    explicit FcitxKeyboardService(FcitxBridge *bridge)
        : QObject(bridge)
        , m_bridge(bridge)
    {
    }

public Q_SLOTS:
    void ShowVirtualKeyboard()
    {
        m_bridge->fcitxShow();
    }
    void HideVirtualKeyboard()
    {
        m_bridge->fcitxHide();
    }
    void UpdatePreeditCaret(int caret)
    {
        m_bridge->fcitxPreeditCaret(caret);
    }
    void UpdatePreeditArea(const QString &text)
    {
        m_bridge->fcitxPreeditArea(text);
    }
    void UpdateCandidateArea(const QStringList &candidates, bool hasPrev, bool hasNext, int pageIndex, int globalCursorIndex)
    {
        m_bridge->fcitxCandidateArea(candidates, hasPrev, hasNext, pageIndex, globalCursorIndex);
    }
    void NotifyIMActivated(const QString &uniqueName)
    {
        m_bridge->fcitxInputMethodActivated(uniqueName);
    }
    void NotifyIMDeactivated(const QString &uniqueName)
    {
        m_bridge->fcitxInputMethodDeactivated(uniqueName);
    }
    void NotifyIMListChanged()
    {
        m_bridge->fcitxInputMethodListChanged();
    }

private:
    FcitxBridge *m_bridge;
};

FcitxBridge::FcitxBridge(QObject *parent)
    : QObject(parent)
    , m_bus(QDBusConnection::sessionBus())
{
    m_answerTimer.setSingleShot(true);
    m_answerTimer.setInterval(s_answerTimeoutMs);
    connect(&m_answerTimer, &QTimer::timeout, this, [this] {
        if (m_kwinWants && !m_fcitxShown) {
            enterPlainTyping(i18nc("@info:status", "Fcitx 5 does not answer. The keys type plain text, without conversion."));
        }
    });
}

FcitxBridge::~FcitxBridge() = default;

QString FcitxBridge::whyNotNeeded()
{
    const KConfigGroup wayland(KSharedConfig::openConfig(u"kwinrc"_s), u"Wayland"_s);
    const QString inputMethod = wayland.readEntry("InputMethod", QString());
    if (!QFileInfo(inputMethod).fileName().contains("fcitx"_L1, Qt::CaseInsensitive)) {
        return u"KWin's input method is \"%1\", not Fcitx 5: Plasma Keyboard has nothing to do as Fcitx 5's keyboard."_s.arg(inputMethod);
    }
    if (!PlasmaKeyboardSettings::self()->showWithFcitx()) {
        return u"Showing Plasma Keyboard for Fcitx 5 is turned off (plasmakeyboardrc [General] showWithFcitx)."_s;
    }
    return {};
}

QString FcitxBridge::layoutLocaleForLanguage(const QString &languageCode)
{
    if (languageCode.startsWith("ja"_L1)) {
        return u"ja_JP"_s;
    }
    if (languageCode.startsWith("ko"_L1)) {
        return u"ko_KR"_s;
    }
    if (languageCode == "zh_TW"_L1 || languageCode == "zh_HK"_L1 || languageCode == "zh_MO"_L1) {
        return u"zh_TW"_s;
    }
    if (languageCode.startsWith("zh"_L1)) {
        return u"zh_CN"_s;
    }
    if (languageCode.contains(u'_')) {
        return languageCode;
    }
    static const QHash<QString, QString> s_countries = {
        {u"en"_s, u"en_US"_s},
        {u"de"_s, u"de_DE"_s},
        {u"es"_s, u"es_ES"_s},
        {u"fr"_s, u"fr_FR"_s},
        {u"pl"_s, u"pl_PL"_s},
        {u"pt"_s, u"pt_PT"_s},
    };
    return s_countries.value(languageCode, u"en_US"_s);
}

bool FcitxBridge::start()
{
    m_service = new FcitxKeyboardService(this);
    if (!m_bus.registerObject(s_keyboardPath, m_service, QDBusConnection::ExportAllSlots)) {
        qCCritical(PlasmaKeyboard) << "Cannot export the Fcitx 5 keyboard object" << s_keyboardPath;
        return false;
    }
    if (!m_bus.registerService(s_keyboardName)) {
        setProblem(i18nc("@info:status", "Another program is already the on-screen keyboard of Fcitx 5."));
        qCCritical(PlasmaKeyboard) << "The name" << s_keyboardName << "is owned by another program:" << m_bus.lastError().message();
        return false;
    }

    m_fakeKeyboard = new FakeKeyboard(this);

    auto backendWatcher = new QDBusServiceWatcher(s_backendName, m_bus, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(backendWatcher, &QDBusServiceWatcher::serviceOwnerChanged, this, [this](const QString &, const QString &, const QString &newOwner) {
        m_backendAvailable = !newOwner.isEmpty();
        qCDebug(PlasmaKeyboard) << "Fcitx 5 keyboard backend" << (m_backendAvailable ? "registered" : "gone");
    });
    m_backendAvailable = m_bus.interface()->isServiceRegistered(s_backendName);

    auto fcitxWatcher = new QDBusServiceWatcher(s_fcitxName, m_bus, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(fcitxWatcher, &QDBusServiceWatcher::serviceOwnerChanged, this, [this](const QString &, const QString &, const QString &newOwner) {
        if (!newOwner.isEmpty()) {
            refreshInputMethods();
        } else if (m_visible || m_kwinWants) {
            enterPlainTyping(i18nc("@info:status", "Fcitx 5 does not answer. The keys type plain text, without conversion."));
        }
    });

    m_bus.connect(s_kwinName, s_kwinPath, s_kwinInterface, u"activeChanged"_s, this, SLOT(kwinActiveChanged()));
    // a text field asked for the keyboard, also one that already has the focus (a second touch on it): KWin's own rule
    // decides again, as for a field that becomes active (the edition's KWin 6.7.4-2+stereo3d67 says it)
    m_bus.connect(s_kwinName, s_kwinPath, s_kwinInterface, u"showRequested"_s, this, SLOT(kwinShowRequested()));

    refreshInputMethods();
    kwinActiveChanged();
    return true;
}

void FcitxBridge::kwinActiveChanged()
{
    auto get = QDBusMessage::createMethodCall(s_kwinName, s_kwinPath, u"org.freedesktop.DBus.Properties"_s, u"Get"_s);
    get << s_kwinInterface << u"active"_s;
    auto watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(get), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
        call->deleteLater();
        const QDBusPendingReply<QDBusVariant> reply = *call;
        if (reply.isError()) {
            qCWarning(PlasmaKeyboard) << "Cannot read KWin's virtual keyboard state:" << reply.error().message();
            return;
        }
        if (!reply.value().variant().toBool()) {
            setKWinWantsKeyboard(false);
            return;
        }
        askKWinWhetherToShow();
    });
}

void FcitxBridge::kwinShowRequested()
{
    qCDebug(PlasmaKeyboard) << "KWin: a text field asked for the keyboard";
    kwinActiveChanged();
}

void FcitxBridge::tellKWinShown(bool shown)
{
    // KWin keeps the focused window clear of this keyboard's window, as it does for its own input panel; it finds the
    // window by this process, so no coordinates go with the call
    auto tell = QDBusMessage::createMethodCall(s_kwinName, s_kwinPath, s_kwinInterface, u"setOnScreenKeyboardShown"_s);
    tell << shown;
    auto watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(tell), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
        call->deleteLater();
        const QDBusPendingReply<> reply = *call;
        if (reply.isError() && !m_toldKWinFailure) {
            m_toldKWinFailure = true;
            qCWarning(PlasmaKeyboard) << "KWin cannot keep the field typed into clear of this keyboard:" << reply.error().name() << reply.error().message();
        }
    });
}

void FcitxBridge::askKWinWhetherToShow()
{
    // KWin's own rule for showing a keyboard on the active text field: the last input came from a touch screen or a pen,
    // the visibility mode, and "Only when in Tablet Mode"
    auto willShow = QDBusMessage::createMethodCall(s_kwinName, s_kwinPath, s_kwinInterface, u"willShowOnActive"_s);
    auto watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(willShow), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
        call->deleteLater();
        const QDBusPendingReply<bool> reply = *call;
        if (reply.isError()) {
            qCWarning(PlasmaKeyboard) << "Cannot ask KWin whether to show the keyboard:" << reply.error().message();
            return;
        }
        qCDebug(PlasmaKeyboard) << "KWin: text field active, show the keyboard:" << reply.value();
        setKWinWantsKeyboard(reply.value());
    });
}

void FcitxBridge::setKWinWantsKeyboard(bool wants)
{
    m_kwinWants = wants;
    if (wants) {
        askFcitxToShow();
    } else {
        m_answerTimer.stop();
    }
    updateVisible();
}

void FcitxBridge::askFcitxToShow()
{
    // one request until Fcitx 5 answers or the wait for its answer ends
    if (m_fcitxShown || m_answerTimer.isActive()) {
        return;
    }
    // Puts Fcitx 5 in its on-screen keyboard mode, which makes its keyboard addon the user interface; the addon then
    // calls ShowVirtualKeyboard on this keyboard
    auto show = fcitxCall(s_fcitxName, s_fcitxKeyboardPath, s_fcitxKeyboardInterface, u"ShowVirtualKeyboard"_s);
    auto watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(show), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
        call->deleteLater();
        const QDBusPendingReply<> reply = *call;
        if (reply.isError()) {
            qCWarning(PlasmaKeyboard) << "Fcitx 5 refused to show its keyboard:" << reply.error().name() << reply.error().message();
            enterPlainTyping(i18nc("@info:status", "Fcitx 5 does not answer. The keys type plain text, without conversion."));
        }
    });
    m_answerTimer.start();
}

void FcitxBridge::setFcitxShown(bool shown)
{
    m_fcitxShown = shown;
    if (shown) {
        m_answerTimer.stop();
        leavePlainTyping();
    }
    updateVisible();
}

void FcitxBridge::enterPlainTyping(const QString &reason)
{
    qCWarning(PlasmaKeyboard) << "Plain typing:" << reason;
    setProblem(reason);
    if (!m_plainTyping) {
        m_plainTyping = true;
        Q_EMIT plainTypingChanged();
    }
    if (!m_preedit.isEmpty() || !m_candidates.isEmpty()) {
        m_preedit.clear();
        m_candidates.clear();
        Q_EMIT preeditChanged();
        Q_EMIT candidatesChanged();
    }
    updateVisible();
}

void FcitxBridge::leavePlainTyping()
{
    setProblem(QString());
    if (m_plainTyping) {
        m_plainTyping = false;
        Q_EMIT plainTypingChanged();
    }
}

void FcitxBridge::setProblem(const QString &problem)
{
    if (m_problem != problem) {
        m_problem = problem;
        Q_EMIT problemChanged();
    }
}

void FcitxBridge::updateVisible()
{
    const bool visible = m_kwinWants && (m_fcitxShown || m_plainTyping);
    if (m_visible != visible) {
        m_visible = visible;
        qCDebug(PlasmaKeyboard) << "Fcitx 5 keyboard visible:" << visible;
        tellKWinShown(visible);
        Q_EMIT visibleChanged();
    }
}

uint FcitxBridge::keycodeFor(uint keysym) const
{
    static const QHash<uint, uint> s_keycodes = usKeycodes();
    return s_keycodes.value(keysym, 0);
}

void FcitxBridge::sendKey(uint keysym, uint states, bool release)
{
    if (m_plainTyping || !m_backendAvailable) {
        if (!m_plainTyping) {
            enterPlainTyping(i18nc("@info:status", "Fcitx 5 does not answer. The keys type plain text, without conversion."));
        }
        m_fakeKeyboard->sendKeysym(keysym, !release);
        return;
    }
    auto key = fcitxCall(s_backendName, s_backendPath, s_backendInterface, u"ProcessKeyEvent"_s);
    key << keysym << keycodeFor(keysym) << states << release << uint(QDateTime::currentMSecsSinceEpoch() & 0xffffffff);
    auto watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(key), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, keysym, release](QDBusPendingCallWatcher *call) {
        call->deleteLater();
        const QDBusPendingReply<> reply = *call;
        if (reply.isError()) {
            qCWarning(PlasmaKeyboard) << "Fcitx 5 refused a key:" << reply.error().message();
            enterPlainTyping(i18nc("@info:status", "Fcitx 5 does not answer. The keys type plain text, without conversion."));
            // the key that was refused is typed now, so nothing the person typed is lost
            m_fakeKeyboard->sendKeysym(keysym, !release);
        }
    });
}

void FcitxBridge::selectCandidate(int index)
{
    auto call = fcitxCall(s_backendName, s_backendPath, s_backendInterface, u"SelectCandidate"_s);
    call << index;
    m_bus.asyncCall(call);
}

void FcitxBridge::previousPage()
{
    m_bus.asyncCall(fcitxCall(s_backendName, s_backendPath, s_backendInterface, u"PrevPage"_s));
}

void FcitxBridge::nextPage()
{
    m_bus.asyncCall(fcitxCall(s_backendName, s_backendPath, s_backendInterface, u"NextPage"_s));
}

void FcitxBridge::switchInputMethod(const QString &uniqueName)
{
    auto call = fcitxCall(s_fcitxName, s_controllerPath, s_controllerInterface, u"SetCurrentIM"_s);
    call << uniqueName;
    auto watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *switched) {
        switched->deleteLater();
        readCurrentInputMethod();
    });
}

void FcitxBridge::hide()
{
    m_bus.asyncCall(fcitxCall(s_fcitxName, s_fcitxKeyboardPath, s_fcitxKeyboardInterface, u"HideVirtualKeyboard"_s));
    m_answerTimer.stop();
    m_kwinWants = false;
    updateVisible();
}

void FcitxBridge::fcitxShow()
{
    setFcitxShown(true);
    // Fcitx 5 asks to show at every focus change while its keyboard mode is on; KWin says whether that change came from a
    // touch or a pen (a touch on another text field brings the keyboard back, a click does not)
    if (!m_kwinWants) {
        askKWinWhetherToShow();
    }
    // Fcitx 5 tells its keyboard which input method is active only while its keyboard mode is on: read it now
    readCurrentInputMethod();
}

void FcitxBridge::fcitxHide()
{
    setFcitxShown(false);
}

void FcitxBridge::fcitxPreeditCaret(int caret)
{
    if (m_preeditCaret != caret) {
        m_preeditCaret = caret;
        Q_EMIT preeditChanged();
    }
}

void FcitxBridge::fcitxPreeditArea(const QString &text)
{
    if (m_preedit != text) {
        m_preedit = text;
        Q_EMIT preeditChanged();
    }
}

void FcitxBridge::fcitxCandidateArea(const QStringList &candidates, bool hasPrev, bool hasNext, int pageIndex, int globalCursorIndex)
{
    qCDebug(PlasmaKeyboard) << "Fcitx 5 candidates:" << candidates << "page" << pageIndex << "previous" << hasPrev << "next" << hasNext;
    m_candidates = candidates;
    m_hasPrev = hasPrev;
    m_hasNext = hasNext;
    m_highlighted = globalCursorIndex;
    Q_EMIT candidatesChanged();
}

void FcitxBridge::fcitxInputMethodActivated(const QString &uniqueName)
{
    m_currentInputMethod = uniqueName;
    updateLayoutLocale();
    Q_EMIT inputMethodsChanged();
}

void FcitxBridge::fcitxInputMethodDeactivated(const QString &uniqueName)
{
    Q_UNUSED(uniqueName)
}

void FcitxBridge::fcitxInputMethodListChanged()
{
    refreshInputMethods();
}

void FcitxBridge::refreshInputMethods()
{
    // AvailableInputMethods: a(ssssssb) uniqueName, name, nativeName, icon, label, languageCode, configurable
    auto available = fcitxCall(s_fcitxName, s_controllerPath, s_controllerInterface, u"AvailableInputMethods"_s);
    auto watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(available), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
        call->deleteLater();
        const QDBusMessage reply = call->reply();
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
            qCWarning(PlasmaKeyboard) << "Cannot read Fcitx 5's input methods:" << reply.errorMessage();
            return;
        }
        QHash<QString, QVariantMap> known;
        const QDBusArgument list = reply.arguments().at(0).value<QDBusArgument>();
        list.beginArray();
        while (!list.atEnd()) {
            QString uniqueName, name, nativeName, icon, label, languageCode;
            bool configurable = false;
            list.beginStructure();
            list >> uniqueName >> name >> nativeName >> icon >> label >> languageCode >> configurable;
            list.endStructure();
            known.insert(uniqueName,
                         {{u"uniqueName"_s, uniqueName}, {u"name"_s, name}, {u"label"_s, label.isEmpty() ? name : label}, {u"languageCode"_s, languageCode}});
        }
        list.endArray();
        readInputMethodGroup(known);
    });
}

void FcitxBridge::readInputMethodGroup(const QHash<QString, QVariantMap> &known)
{
    auto group = fcitxCall(s_fcitxName, s_controllerPath, s_controllerInterface, u"CurrentInputMethodGroup"_s);
    auto watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(group), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, known](QDBusPendingCallWatcher *call) {
        call->deleteLater();
        const QDBusPendingReply<QString> groupReply = *call;
        if (groupReply.isError()) {
            qCWarning(PlasmaKeyboard) << "Cannot read Fcitx 5's input method group:" << groupReply.error().message();
            return;
        }
        // InputMethodGroupInfo(group): s default layout, a(ss) uniqueName and layout, in the order the person set
        auto info = fcitxCall(s_fcitxName, s_controllerPath, s_controllerInterface, u"InputMethodGroupInfo"_s);
        info << groupReply.value();
        auto infoWatcher = new QDBusPendingCallWatcher(m_bus.asyncCall(info), this);
        connect(infoWatcher, &QDBusPendingCallWatcher::finished, this, [this, known](QDBusPendingCallWatcher *infoCall) {
            infoCall->deleteLater();
            const QDBusMessage infoReply = infoCall->reply();
            if (infoReply.type() != QDBusMessage::ReplyMessage || infoReply.arguments().size() < 2) {
                qCWarning(PlasmaKeyboard) << "Cannot read Fcitx 5's input method group:" << infoReply.errorMessage();
                return;
            }
            QVariantList inputMethods;
            const QDBusArgument items = infoReply.arguments().at(1).value<QDBusArgument>();
            items.beginArray();
            while (!items.atEnd()) {
                QString uniqueName, layout;
                items.beginStructure();
                items >> uniqueName >> layout;
                items.endStructure();
                inputMethods.append(known.value(uniqueName, {{u"uniqueName"_s, uniqueName}, {u"name"_s, uniqueName}, {u"label"_s, uniqueName}}));
            }
            items.endArray();
            m_inputMethods = inputMethods;
            readCurrentInputMethod();
        });
    });
}

void FcitxBridge::readCurrentInputMethod()
{
    auto current = fcitxCall(s_fcitxName, s_controllerPath, s_controllerInterface, u"CurrentInputMethod"_s);
    auto watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(current), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
        call->deleteLater();
        const QDBusPendingReply<QString> reply = *call;
        if (reply.isError()) {
            qCWarning(PlasmaKeyboard) << "Cannot read Fcitx 5's current input method:" << reply.error().message();
        } else {
            m_currentInputMethod = reply.value();
        }
        updateLayoutLocale();
        qCDebug(PlasmaKeyboard) << "Fcitx 5 input methods:" << m_inputMethods << "current:" << m_currentInputMethod << "layout:" << m_layoutLocale;
        Q_EMIT inputMethodsChanged();
    });
}

void FcitxBridge::updateLayoutLocale()
{
    for (const QVariant &entry : std::as_const(m_inputMethods)) {
        const QVariantMap im = entry.toMap();
        if (im.value(u"uniqueName"_s).toString() == m_currentInputMethod) {
            m_layoutLocale = layoutLocaleForLanguage(im.value(u"languageCode"_s).toString());
            return;
        }
    }
}

bool FcitxBridge::isVisible() const
{
    return m_visible;
}

bool FcitxBridge::plainTyping() const
{
    return m_plainTyping;
}

QString FcitxBridge::problem() const
{
    return m_problem;
}

QString FcitxBridge::preedit() const
{
    return m_preedit;
}

int FcitxBridge::preeditCaret() const
{
    return m_preeditCaret;
}

QStringList FcitxBridge::candidates() const
{
    return m_candidates;
}

int FcitxBridge::highlightedCandidate() const
{
    return m_highlighted;
}

bool FcitxBridge::hasPreviousPage() const
{
    return m_hasPrev;
}

bool FcitxBridge::hasNextPage() const
{
    return m_hasNext;
}

QString FcitxBridge::currentInputMethod() const
{
    return m_currentInputMethod;
}

QString FcitxBridge::currentInputMethodLabel() const
{
    for (const QVariant &entry : m_inputMethods) {
        const QVariantMap im = entry.toMap();
        if (im.value(u"uniqueName"_s).toString() == m_currentInputMethod) {
            return im.value(u"label"_s).toString();
        }
    }
    return m_currentInputMethod;
}

QVariantList FcitxBridge::inputMethods() const
{
    return m_inputMethods;
}

QString FcitxBridge::layoutLocale() const
{
    return m_layoutLocale;
}

#include "fcitxbridge.moc"
#include "moc_fcitxbridge.cpp"
