/*
    SPDX-FileCopyrightText: 2026 Daniel Campos Ramos

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "fcitxbridge.h"
#include "fcitxinputitem.h"
#include "plasmakeyboardsettings.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

#include <xkbcommon/xkbcommon-keysyms.h>

using namespace Qt::StringLiterals;

// KWin's org.kde.kwin.VirtualKeyboard on /VirtualKeyboard (kwin src/virtualkeyboard_dbus.h)
class FakeKWin : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.VirtualKeyboard")
    Q_PROPERTY(bool active READ isActive)
public:
    bool m_active = false;
    bool m_willShow = false;
    bool isActive() const
    {
        return m_active;
    }
    void setState(bool active, bool willShow)
    {
        m_active = active;
        m_willShow = willShow;
        Q_EMIT activeChanged();
    }
public Q_SLOTS:
    Q_SCRIPTABLE bool willShowOnActive()
    {
        return m_willShow;
    }
Q_SIGNALS:
    Q_SCRIPTABLE void activeChanged();
};

// Fcitx 5: org.fcitx.Fcitx.VirtualKeyboard1 on /virtualkeyboard of org.fcitx.Fcitx5 (fcitx5 src/ui/virtualkeyboard)
class FakeFcitxKeyboard : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.fcitx.Fcitx.VirtualKeyboard1")
public:
    FakeFcitxKeyboard(QDBusConnection bus, QDBusConnection backendBus)
        : m_bus(bus)
        , m_backendBus(backendBus)
    {
    }
    QDBusConnection m_bus;
    QDBusConnection m_backendBus;
    int m_shows = 0;
    int m_hides = 0;
    bool m_refuse = false;
public Q_SLOTS:
    Q_SCRIPTABLE void ShowVirtualKeyboard()
    {
        ++m_shows;
        if (m_refuse) {
            sendErrorReply(QDBusError::UnknownObject, u"No such object path '/virtualkeyboard'"_s);
            return;
        }
        // as Fcitx 5 does: its keyboard mode registers the backend, then the keyboard is asked to show
        m_backendBus.registerService(u"org.fcitx.Fcitx5.VirtualKeyboardBackend"_s);
        m_bus.asyncCall(QDBusMessage::createMethodCall(u"org.fcitx.Fcitx5.VirtualKeyboard"_s,
                                                       u"/org/fcitx/virtualkeyboard/impanel"_s,
                                                       u"org.fcitx.Fcitx5.VirtualKeyboard1"_s,
                                                       u"ShowVirtualKeyboard"_s));
    }
    Q_SCRIPTABLE void HideVirtualKeyboard()
    {
        ++m_hides;
        m_bus.asyncCall(QDBusMessage::createMethodCall(u"org.fcitx.Fcitx5.VirtualKeyboard"_s,
                                                       u"/org/fcitx/virtualkeyboard/impanel"_s,
                                                       u"org.fcitx.Fcitx5.VirtualKeyboard1"_s,
                                                       u"HideVirtualKeyboard"_s));
    }
};

// Fcitx 5's org.fcitx.Fcitx5.VirtualKeyboardBackend1 on /virtualkeyboard
class FakeFcitxBackend : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.fcitx.Fcitx5.VirtualKeyboardBackend1")
public:
    QList<QVariantList> m_keys;
    QList<int> m_selected;
    int m_nextPages = 0;
    int m_previousPages = 0;
public Q_SLOTS:
    Q_SCRIPTABLE void ProcessKeyEvent(uint keyval, uint keycode, uint state, bool isRelease, uint time)
    {
        Q_UNUSED(time)
        m_keys.append({keyval, keycode, state, isRelease});
    }
    Q_SCRIPTABLE void SelectCandidate(int index)
    {
        m_selected.append(index);
    }
    Q_SCRIPTABLE void NextPage()
    {
        ++m_nextPages;
    }
    Q_SCRIPTABLE void PrevPage()
    {
        ++m_previousPages;
    }
};

// Fcitx 5's org.fcitx.Fcitx.Controller1 on /controller (fcitx5 src/modules/dbus/dbusmodule.cpp)
struct FcitxInputMethodEntry {
    QString uniqueName, name, nativeName, icon, label, languageCode;
    bool configurable = false;
};
QDBusArgument &operator<<(QDBusArgument &arg, const FcitxInputMethodEntry &im)
{
    arg.beginStructure();
    arg << im.uniqueName << im.name << im.nativeName << im.icon << im.label << im.languageCode << im.configurable;
    arg.endStructure();
    return arg;
}
const QDBusArgument &operator>>(const QDBusArgument &arg, FcitxInputMethodEntry &im)
{
    arg.beginStructure();
    arg >> im.uniqueName >> im.name >> im.nativeName >> im.icon >> im.label >> im.languageCode >> im.configurable;
    arg.endStructure();
    return arg;
}
struct FcitxGroupItem {
    QString uniqueName, layout;
};
QDBusArgument &operator<<(QDBusArgument &arg, const FcitxGroupItem &item)
{
    arg.beginStructure();
    arg << item.uniqueName << item.layout;
    arg.endStructure();
    return arg;
}
const QDBusArgument &operator>>(const QDBusArgument &arg, FcitxGroupItem &item)
{
    arg.beginStructure();
    arg >> item.uniqueName >> item.layout;
    arg.endStructure();
    return arg;
}
Q_DECLARE_METATYPE(FcitxInputMethodEntry)
Q_DECLARE_METATYPE(FcitxGroupItem)

class FakeFcitxController : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.fcitx.Fcitx.Controller1")
public:
    QString m_current = u"mozc"_s;
    QStringList m_switchedTo;
public Q_SLOTS:
    Q_SCRIPTABLE QList<FcitxInputMethodEntry> AvailableInputMethods()
    {
        return {{u"keyboard-us"_s, u"English (US)"_s, u"English (US)"_s, u"input-keyboard"_s, u"en"_s, u"en"_s, false},
                {u"mozc"_s, u"Mozc"_s, u"Mozc"_s, u"fcitx-mozc"_s, u"あ"_s, u"ja"_s, true},
                {u"hangul"_s, u"Hangul"_s, u"한글"_s, u"fcitx-hangul"_s, u"한"_s, u"ko"_s, true}};
    }
    Q_SCRIPTABLE QString CurrentInputMethodGroup()
    {
        return u"Default"_s;
    }
    Q_SCRIPTABLE QString InputMethodGroupInfo(const QString &group, QList<FcitxGroupItem> &items)
    {
        Q_UNUSED(group)
        items = {{u"keyboard-us"_s, QString()}, {u"mozc"_s, QString()}};
        return u"us"_s;
    }
    Q_SCRIPTABLE QString CurrentInputMethod()
    {
        return m_current;
    }
    Q_SCRIPTABLE void SetCurrentIM(const QString &uniqueName)
    {
        m_switchedTo.append(uniqueName);
    }
};

class FcitxBridgeTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void cleanup();
    void testKeysForText_data();
    void testKeysForText();
    void testLayoutLocale_data();
    void testLayoutLocale();
    void testWhyNotNeeded();
    void testShownWhenKWinShowsAKeyboard();
    void testNotShownForTheMouse();
    void testKeysReachFcitx();
    void testCandidates();
    void testInputMethods();
    void testHideKey();
    void testPlainTypingWhenFcitxRefuses();

private:
    void setUpStandIns();
    void startBridge();

    QDBusConnection m_kwinBus{QString()};
    QDBusConnection m_fcitxBus{QString()};
    QDBusConnection m_backendBus{QString()};
    FakeKWin *m_kwin = nullptr;
    FakeFcitxKeyboard *m_fcitxKeyboard = nullptr;
    FakeFcitxBackend *m_backend = nullptr;
    FakeFcitxController *m_controller = nullptr;
    FcitxBridge *m_bridge = nullptr;
    int m_round = 0;
};

void FcitxBridgeTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    qDBusRegisterMetaType<FcitxInputMethodEntry>();
    qDBusRegisterMetaType<QList<FcitxInputMethodEntry>>();
    qDBusRegisterMetaType<FcitxGroupItem>();
    qDBusRegisterMetaType<QList<FcitxGroupItem>>();
}

void FcitxBridgeTest::setUpStandIns()
{
    if (!QDBusConnection::sessionBus().isConnected()) {
        QSKIP("needs a D-Bus session bus (dbus-run-session)");
    }
    ++m_round;
    m_kwinBus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, u"fakekwin%1"_s.arg(m_round));
    m_fcitxBus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, u"fakefcitx%1"_s.arg(m_round));
    m_backendBus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, u"fakebackend%1"_s.arg(m_round));
    m_kwin = new FakeKWin;
    QVERIFY(m_kwinBus.registerObject(u"/VirtualKeyboard"_s,
                                     m_kwin,
                                     QDBusConnection::ExportAllProperties | QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals));
    QVERIFY(m_kwinBus.registerService(u"org.kde.KWin"_s));
    m_fcitxKeyboard = new FakeFcitxKeyboard(m_fcitxBus, m_backendBus);
    m_backend = new FakeFcitxBackend;
    m_controller = new FakeFcitxController;
    QVERIFY(m_fcitxBus.registerObject(u"/virtualkeyboard"_s, m_fcitxKeyboard, QDBusConnection::ExportScriptableSlots));
    QVERIFY(m_fcitxBus.registerObject(u"/controller"_s, m_controller, QDBusConnection::ExportScriptableSlots));
    QVERIFY(m_fcitxBus.registerService(u"org.fcitx.Fcitx5"_s));
    QVERIFY(m_backendBus.registerObject(u"/virtualkeyboard"_s, m_backend, QDBusConnection::ExportScriptableSlots));
}

void FcitxBridgeTest::cleanup()
{
    delete m_bridge;
    m_bridge = nullptr;
    QDBusConnection::sessionBus().unregisterService(u"org.fcitx.Fcitx5.VirtualKeyboard"_s);
    QDBusConnection::sessionBus().unregisterObject(u"/org/fcitx/virtualkeyboard/impanel"_s);
    // a connection closes only when nothing holds it any more
    for (QDBusConnection *bus : {&m_kwinBus, &m_fcitxBus, &m_backendBus}) {
        const QString name = bus->name();
        *bus = QDBusConnection(QString());
        if (!name.isEmpty()) {
            QDBusConnection::disconnectFromBus(name);
        }
    }
    delete m_kwin;
    m_kwin = nullptr;
    delete m_fcitxKeyboard;
    m_fcitxKeyboard = nullptr;
    delete m_backend;
    m_backend = nullptr;
    delete m_controller;
    m_controller = nullptr;
}

void FcitxBridgeTest::startBridge()
{
    setUpStandIns();
    if (QTest::currentTestResolved()) {
        return;
    }
    m_bridge = new FcitxBridge;
    QVERIFY(m_bridge->start());
    QVERIFY(QDBusConnection::sessionBus().interface()->isServiceRegistered(u"org.fcitx.Fcitx5.VirtualKeyboard"_s));
}

void FcitxBridgeTest::testKeysForText_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QList<uint>>("keysyms");
    QTest::addColumn<QList<uint>>("states");
    // romaji and pinyin are plain letters
    QTest::newRow("nihongo") << u"nihongo"_s << QList<uint>{XKB_KEY_n, XKB_KEY_i, XKB_KEY_h, XKB_KEY_o, XKB_KEY_n, XKB_KEY_g, XKB_KEY_o} << QList<uint>{0, 0, 0, 0, 0, 0, 0};
    // Fcitx 5's Hangul engine reads Shift+Q as the tense consonant: the shift state goes with the capital
    QTest::newRow("shifted") << u"Qa"_s << QList<uint>{XKB_KEY_Q, XKB_KEY_a} << QList<uint>{FcitxBridge::ShiftState, 0};
    QTest::newRow("space and return") << u" \n"_s << QList<uint>{XKB_KEY_space, XKB_KEY_Return} << QList<uint>{0, 0};
    QTest::newRow("Dachen tone and punctuation") << u"3,/"_s << QList<uint>{XKB_KEY_3, XKB_KEY_comma, XKB_KEY_slash} << QList<uint>{0, 0, 0};
    QTest::newRow("Latin with diacritics") << u"çã"_s << QList<uint>{XKB_KEY_ccedilla, XKB_KEY_atilde} << QList<uint>{0, 0};
    QTest::newRow("a CJK character") << u"中"_s << QList<uint>{0x1004e2d} << QList<uint>{0};
}

void FcitxBridgeTest::testKeysForText()
{
    QFETCH(QString, text);
    QFETCH(QList<uint>, keysyms);
    QFETCH(QList<uint>, states);
    const auto keys = FcitxInputItem::keysForText(text);
    QCOMPARE(keys.size(), keysyms.size());
    for (int i = 0; i < keys.size(); ++i) {
        QCOMPARE(keys.at(i).first, keysyms.at(i));
        QCOMPARE(keys.at(i).second, states.at(i));
    }
}

void FcitxBridgeTest::testLayoutLocale_data()
{
    QTest::addColumn<QString>("languageCode");
    QTest::addColumn<QString>("locale");
    QTest::newRow("Mozc") << u"ja"_s << u"ja_JP"_s;
    QTest::newRow("Hangul") << u"ko"_s << u"ko_KR"_s;
    QTest::newRow("Pinyin") << u"zh_CN"_s << u"zh_CN"_s;
    QTest::newRow("Chewing") << u"zh_TW"_s << u"zh_TW"_s;
    QTest::newRow("keyboard-us") << u"en"_s << u"en_US"_s;
    QTest::newRow("keyboard-br") << u"pt_BR"_s << u"pt_BR"_s;
    QTest::newRow("unknown") << u""_s << u"en_US"_s;
}

void FcitxBridgeTest::testLayoutLocale()
{
    QFETCH(QString, languageCode);
    QFETCH(QString, locale);
    QCOMPARE(FcitxBridge::layoutLocaleForLanguage(languageCode), locale);
}

void FcitxBridgeTest::testWhyNotNeeded()
{
    auto kwinrc = KSharedConfig::openConfig(u"kwinrc"_s);
    auto setInputMethod = [&kwinrc](const QString &desktop) {
        KConfigGroup(kwinrc, u"Wayland"_s).writeEntry("InputMethod", desktop);
        kwinrc->sync();
    };
    auto setShowWithFcitx = [](bool show) {
        PlasmaKeyboardSettings::self()->setShowWithFcitx(show);
        PlasmaKeyboardSettings::self()->save();
    };
    setInputMethod(u"/usr/share/applications/org.kde.plasma.keyboard.desktop"_s);
    setShowWithFcitx(true);
    QVERIFY(!FcitxBridge::whyNotNeeded().isEmpty());
    setInputMethod(u"/usr/share/applications/org.fcitx.Fcitx5.desktop"_s);
    QVERIFY(FcitxBridge::whyNotNeeded().isEmpty());
    setShowWithFcitx(false);
    QVERIFY(!FcitxBridge::whyNotNeeded().isEmpty());
}

void FcitxBridgeTest::testShownWhenKWinShowsAKeyboard()
{
    startBridge();
    if (QTest::currentTestResolved()) {
        return;
    }
    QSignalSpy visible(m_bridge, &FcitxBridge::visibleChanged);
    // a touch on a text field in tablet mode: KWin would show a keyboard
    m_kwin->setState(true, true);
    QTRY_VERIFY(m_bridge->isVisible());
    QCOMPARE(m_fcitxKeyboard->m_shows, 1);
    QVERIFY(!m_bridge->plainTyping());
    QVERIFY(m_bridge->problem().isEmpty());
    // the text field loses the focus
    m_kwin->setState(false, false);
    QTRY_VERIFY(!m_bridge->isVisible());
}

void FcitxBridgeTest::testNotShownForTheMouse()
{
    startBridge();
    if (QTest::currentTestResolved()) {
        return;
    }
    // a click on a text field, or a touch outside tablet mode: KWin would not show a keyboard
    m_kwin->setState(true, false);
    QTest::qWait(500);
    QVERIFY(!m_bridge->isVisible());
    QCOMPARE(m_fcitxKeyboard->m_shows, 0);
    // Fcitx 5 still in its keyboard mode from an earlier touch asks to show: KWin's rule wins
    m_bridge->fcitxShow();
    QVERIFY(!m_bridge->isVisible());
}

void FcitxBridgeTest::testKeysReachFcitx()
{
    startBridge();
    if (QTest::currentTestResolved()) {
        return;
    }
    m_kwin->setState(true, true);
    QTRY_VERIFY(m_bridge->isVisible());
    m_bridge->sendKey(XKB_KEY_n, 0, false);
    m_bridge->sendKey(XKB_KEY_n, 0, true);
    m_bridge->sendKey(XKB_KEY_space, 0, false);
    m_bridge->sendKey(XKB_KEY_space, 0, true);
    QTRY_COMPARE(m_backend->m_keys.size(), 4);
    // evdev KEY_N is 49, KEY_SPACE 57; X keycodes are evdev + 8
    QCOMPARE(m_backend->m_keys.at(0), (QVariantList{uint(XKB_KEY_n), 57u, 0u, false}));
    QCOMPARE(m_backend->m_keys.at(1), (QVariantList{uint(XKB_KEY_n), 57u, 0u, true}));
    QCOMPARE(m_backend->m_keys.at(2), (QVariantList{uint(XKB_KEY_space), 65u, 0u, false}));
    QVERIFY(!m_bridge->plainTyping());
}

void FcitxBridgeTest::testCandidates()
{
    startBridge();
    if (QTest::currentTestResolved()) {
        return;
    }
    m_kwin->setState(true, true);
    QTRY_VERIFY(m_bridge->isVisible());
    QSignalSpy candidates(m_bridge, &FcitxBridge::candidatesChanged);
    auto call = [this](const QString &method, const QVariantList &args) {
        auto msg = QDBusMessage::createMethodCall(u"org.fcitx.Fcitx5.VirtualKeyboard"_s, u"/org/fcitx/virtualkeyboard/impanel"_s, u"org.fcitx.Fcitx5.VirtualKeyboard1"_s, method);
        msg.setArguments(args);
        m_fcitxBus.asyncCall(msg);
    };
    call(u"UpdatePreeditArea"_s, {u"にほんご"_s});
    call(u"UpdateCandidateArea"_s, {QStringList{u"日本語"_s, u"二本後"_s}, false, true, -1, 0});
    QTRY_COMPARE(m_bridge->candidates(), (QStringList{u"日本語"_s, u"二本後"_s}));
    QCOMPARE(m_bridge->preedit(), u"にほんご"_s);
    QCOMPARE(m_bridge->highlightedCandidate(), 0);
    QVERIFY(m_bridge->hasNextPage());
    QVERIFY(!m_bridge->hasPreviousPage());
    m_bridge->selectCandidate(1);
    m_bridge->nextPage();
    QTRY_COMPARE(m_backend->m_selected, QList<int>{1});
    QTRY_COMPARE(m_backend->m_nextPages, 1);
}

void FcitxBridgeTest::testInputMethods()
{
    startBridge();
    if (QTest::currentTestResolved()) {
        return;
    }
    // the group's input methods, in the person's order, with Fcitx 5's labels; the layout follows the current one
    QTRY_COMPARE(m_bridge->inputMethods().size(), 2);
    QCOMPARE(m_bridge->inputMethods().at(0).toMap().value(u"uniqueName"_s).toString(), u"keyboard-us"_s);
    QCOMPARE(m_bridge->currentInputMethod(), u"mozc"_s);
    QCOMPARE(m_bridge->currentInputMethodLabel(), u"あ"_s);
    QCOMPARE(m_bridge->layoutLocale(), u"ja_JP"_s);
    // the person switches on the keyboard; Fcitx 5 says which input method is now active
    m_bridge->switchInputMethod(u"keyboard-us"_s);
    QTRY_COMPARE(m_controller->m_switchedTo, QStringList{u"keyboard-us"_s});
    m_bridge->fcitxInputMethodActivated(u"keyboard-us"_s);
    QCOMPARE(m_bridge->layoutLocale(), u"en_US"_s);
    QCOMPARE(m_bridge->currentInputMethodLabel(), u"en"_s);
}

void FcitxBridgeTest::testHideKey()
{
    startBridge();
    if (QTest::currentTestResolved()) {
        return;
    }
    m_kwin->setState(true, true);
    QTRY_VERIFY(m_bridge->isVisible());
    m_bridge->hide();
    QVERIFY(!m_bridge->isVisible());
    QTRY_COMPARE(m_fcitxKeyboard->m_hides, 1);
}

void FcitxBridgeTest::testPlainTypingWhenFcitxRefuses()
{
    setUpStandIns();
    if (QTest::currentTestResolved()) {
        return;
    }
    m_fcitxKeyboard->m_refuse = true;
    m_bridge = new FcitxBridge;
    QVERIFY(m_bridge->start());
    m_kwin->setState(true, true);
    // the person is told, and the keyboard shows to type plain text
    QTRY_VERIFY(m_bridge->plainTyping());
    QVERIFY(!m_bridge->problem().isEmpty());
    QTRY_VERIFY(m_bridge->isVisible());
    // no key goes to a Fcitx 5 that refused
    m_bridge->sendKey(XKB_KEY_a, 0, false);
    QTest::qWait(300);
    QVERIFY(m_backend->m_keys.isEmpty());
}

QTEST_MAIN(FcitxBridgeTest)
#include "fcitxbridgetest.moc"
