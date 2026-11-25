# SPDX-FileCopyrightText: 2026 Aleix Pol <aleixpol@kde.org>
# SPDX-License-Identifier: BSD-2-Clause

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test WaylandCompositor)
include(ECMAddTests)

file(GENERATE
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mockinputmethodcompositor_config.h
    CONTENT "#pragma once\n#define PLASMA_KEYBOARD_BINARY_PATH \"$<TARGET_FILE:plasma-keyboard>\"\n"
)

ecm_add_test(mockinputmethodcompositor.cpp LINK_LIBRARIES
    KF6::ConfigCore
    PkgConfig::XKBCommon
    Qt::Core
    Qt::Gui
    Qt::GuiPrivate
    Qt::Test
    Qt::WaylandCompositor
    Wayland::Server
)

qt6_generate_wayland_protocol_server_sources(mockinputmethodcompositor
    FILES
        ${WaylandProtocols_DATADIR}/unstable/input-method/input-method-unstable-v1.xml
        ${Wayland_DATADIR}/wayland.xml
)

# Plasma Keyboard as Fcitx 5's keyboard: the bridge against stand-ins of KWin and Fcitx 5 on a session bus
find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS DBus Quick)
ecm_add_test(fcitxbridgetest.cpp
    ${CMAKE_SOURCE_DIR}/src/fcitx/fcitxbridge.cpp
    ${CMAKE_SOURCE_DIR}/src/fcitx/fcitxinputitem.cpp
    ${CMAKE_SOURCE_DIR}/src/fcitx/fakekeyboard.cpp
    TEST_NAME fcitxbridgetest
    LINK_LIBRARIES
        KF6::ConfigCore
        KF6::ConfigGui
        KF6::I18n
        PkgConfig::XKBCommon
        Qt::DBus
        Qt::GuiPrivate
        Qt::Quick
        Qt::Test
        Qt::WaylandClient
        Qt::WaylandClientPrivate
        Wayland::Client
)
ecm_qt_declare_logging_category(fcitxbridgetest
    HEADER logging.h
    IDENTIFIER "PlasmaKeyboard"
    CATEGORY_NAME "org.kde.plasma.keyboard"
)
kconfig_add_kcfg_files(fcitxbridgetest GENERATE_MOC ${CMAKE_SOURCE_DIR}/src/plasmakeyboardsettings.kcfgc)
ecm_add_qtwayland_client_protocol(fcitxbridgetest PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/fake-input.xml BASENAME fake-input)
target_include_directories(fcitxbridgetest PRIVATE ${CMAKE_SOURCE_DIR}/src/fcitx ${CMAKE_CURRENT_BINARY_DIR})
