/*
    SPDX-FileCopyrightText: 2024 Aleix Pol i Gonzalez <aleixpol@kde.org>
    SPDX-FileCopyrightText: 2025 Kristen McWilliam <kristen@kde.org>
    SPDX-FileCopyrightText: 2026 Daniel Campos Ramos

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "config-plasma-keyboard.h"
#include "fcitx/fcitxbridge.h"
#include "inputpanelintegration.h"
#include "layoutpathhelper.h"
#include "logging.h"
#include "plasmakeyboardsettings.h"
#include <plasma_keyboard_version.h>

#include <KAboutData>
#include <KConfigWatcher>
#include <KCrash>
#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include <LayerShellQt/Window>

#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QWindow>
#include <qpa/qwindowsysteminterface.h>

#include <cstring>

int main(int argc, char **argv)
{
    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));

    // --fcitx: Plasma Keyboard as Fcitx 5's keyboard, started at login while Fcitx 5 is KWin's input method
    bool fcitxMode = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--fcitx") == 0) {
            fcitxMode = true;
        }
    }

    initLayoutsPath(fcitxMode);

    QGuiApplication application(argc, argv);

    KLocalizedString::setApplicationDomain("plasma-keyboard");

    KAboutData aboutData(QStringLiteral("plasma-keyboard"),
                         i18n("Plasma Keyboard"),
                         QStringLiteral(PLASMA_KEYBOARD_VERSION_STRING),
                         i18n("An on-screen keyboard for Plasma"),
                         KAboutLicense::GPL,
                         i18n("Copyright 2024, Aleix Pol Gonzalez"));

    aboutData.addAuthor(i18n("Aleix Pol Gonzalez"), i18n("Author"), QStringLiteral("aleixpol@kde.org"));
    aboutData.setOrganizationDomain("kde.org");
    aboutData.setDesktopFileName(QStringLiteral("org.kde.plasma.keyboard"));
    application.setWindowIcon(QIcon::fromTheme(QStringLiteral("input-keyboard-virtual")));
    aboutData.setProgramLogo(application.windowIcon());

    KAboutData::setApplicationData(aboutData);

    KCrash::initialize();

    {
        QCommandLineParser parser;
        aboutData.setupCommandLine(&parser);
        parser.addOption(QCommandLineOption(QStringLiteral("fcitx"),
                                            i18nc("@info:shell", "Run as the on-screen keyboard of Fcitx 5, when Fcitx 5 is KWin's input method")));
        parser.process(application);
        aboutData.processCommandLine(&parser);
    }

    if (fcitxMode) {
        const QString reason = FcitxBridge::whyNotNeeded();
        if (!reason.isEmpty()) {
            qInfo().noquote() << "plasma-keyboard --fcitx:" << reason;
            return 0;
        }
    }

    if (!PLASMA_KEYBOARD_SOUND_ENABLED) {
        PlasmaKeyboardSettings::self()->setSoundEnabled(false);
    }

    if (!PLASMA_KEYBOARD_VIBRATION_ENABLED) {
        PlasmaKeyboardSettings::self()->setVibrationEnabled(false);
    }

    // Listen to config updates from kcm, and reparse
    auto watcher = KConfigWatcher::create(PlasmaKeyboardSettings::self()->sharedConfig());
    // clang-format off
    QObject::connect(watcher.get(),
        &KConfigWatcher::configChanged,
        &application,
        [](const KConfigGroup &, const QByteArrayList &) {
            PlasmaKeyboardSettings::self()->sharedConfig()->reparseConfiguration();
            PlasmaKeyboardSettings::self()->load();
        });
    // clang-format on

    QQmlApplicationEngine view;
    KLocalization::setupLocalizedContext(&view);

    if (fcitxMode) {
        auto bridge = new FcitxBridge(&application);
        if (!bridge->start()) {
            qCCritical(PlasmaKeyboard) << "Cannot be Fcitx 5's on-screen keyboard:" << bridge->problem();
            return 1;
        }
        QObject::connect(&view, &QQmlApplicationEngine::objectCreated, &application, [](QObject *object) {
            auto window = qobject_cast<QWindow *>(object);
            if (!window) {
                qCCritical(PlasmaKeyboard) << "The keyboard for Fcitx 5 has no window";
                exit(1);
            }
            // A layer at the bottom of the screen that never takes the keyboard focus: the text field keeps it, and with
            // it Fcitx 5's input context. Its scope makes it a dock for KWin, which keeps docks at the screen's plane on a
            // 3D output, as it keeps its own input panel (any other scope is a normal window, which KWin sinks when it is
            // not the active one). No exclusive zone: the windows under the keys stay as they are.
            auto layer = LayerShellQt::Window::get(window);
            layer->setScope(QStringLiteral("dock"));
            layer->setLayer(LayerShellQt::Window::LayerOverlay);
            layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorBottom | LayerShellQt::Window::AnchorLeft
                                                            | LayerShellQt::Window::AnchorRight));
            layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
            layer->setExclusiveZone(0);
            // on the screen of the last touch, as KWin places its own input panel on its active output
            layer->setWantsToBeOnActiveScreen(true);
        });
        view.setInitialProperties({{QStringLiteral("bridge"), QVariant::fromValue(bridge)}});
        view.load(QUrl(QStringLiteral("qrc:/qt/qml/org/kde/plasma/keyboard/FcitxMain.qml")));
        qCDebug(PlasmaKeyboard) << "Starting Plasma Keyboard as Fcitx 5's keyboard";
        return application.exec();
    }

    QObject::connect(&view, &QQmlApplicationEngine::objectCreated, &application, [](QObject *object) {
        auto window = qobject_cast<QWindow *>(object);
        const bool initSuccessful = initInputPanelIntegration(window, InputPanelRole::Keyboard);

        if (!initSuccessful) {
            qCCritical(PlasmaKeyboard)
                << "Cannot run plasma-keyboard standalone. You can enable it in Plasma's System Settings app, on the “Virtual Keyboard” page.";
            exit(1);
        }

        window->requestActivate();
        window->setVisible(true);
    });
    view.load(QUrl(QStringLiteral("qrc:/qt/qml/org/kde/plasma/keyboard/main.qml")));

    qCDebug(PlasmaKeyboard) << "Starting Plasma Keyboard application";

    return application.exec();
}
