# SPDX-FileCopyrightText: 2024 Aleix Pol <aleixpol@kde.org>
# SPDX-FileCopyrightText: 2025 Devin Lin <devin@kde.org>
# SPDX-FileCopyrightText: 2025 Kristen McWilliam <kristen@kde.org>
#
# SPDX-License-Identifier: BSD-2-Clause

configure_file(config-plasma-keyboard.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-plasma-keyboard.h)

add_executable(plasma-keyboard)

ecm_add_qtwayland_client_protocol(plasma-keyboard PROTOCOL ${WaylandProtocols_DATADIR}/unstable/input-method/input-method-unstable-v1.xml BASENAME input-method-unstable-v1)
ecm_add_qtwayland_client_protocol(plasma-keyboard PROTOCOL ${WaylandProtocols_DATADIR}/unstable/text-input/text-input-unstable-v1.xml BASENAME text-input-unstable-v1)
ecm_add_qtwayland_client_protocol(plasma-keyboard PROTOCOL "${Wayland_DATADIR}/wayland.xml" BASENAME wayland)
ecm_add_qtwayland_client_protocol(plasma-keyboard PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/fake-input.xml BASENAME fake-input)

target_sources(plasma-keyboard PRIVATE
    main.cpp
    inputpanelintegration.cpp
    inputpanelintegration.h
    inputpanelrole.h
    inputlisteneritem.cpp
    inputlisteneritem.h
    inputmethod.cpp
    inputmethod_p.h
    inputpanelwindow.cpp
    inputpanelwindow.h
    inputplugin.cpp
    inputplugin.h
    qwaylandinputpanelshellintegration.cpp
    qwaylandinputpanelshellintegration_p.h
    qwaylandinputpanelsurface.cpp
    qwaylandinputpanelsurface_p.h
    overlay/overlaycontroller.cpp
    overlay/overlaycontroller.h
    overlay/candidatemodel.cpp
    overlay/candidatemodel.h
    overlay/overlaytrigger.cpp
    overlay/overlaytrigger.h
    overlay/longpresstrigger.cpp
    overlay/longpresstrigger.h
    overlay/diacriticsdataloader.cpp
    overlay/diacriticsdataloader.h
    overlay/diacritics.qrc
    overlay/prefixquerytrigger.cpp
    overlay/prefixquerytrigger.h
    overlay/textexpansiontrigger.cpp
    overlay/textexpansiontrigger.h
    fcitx/fakekeyboard.cpp
    fcitx/fakekeyboard.h
    fcitx/fcitxbridge.cpp
    fcitx/fcitxbridge.h
    fcitx/fcitxinputitem.cpp
    fcitx/fcitxinputitem.h
)

if(PLASMA_KEYBOARD_VIBRATION_ENABLED)
    set_source_files_properties(dbus/org.sigxcpu.Feedback.Haptic.xml PROPERTIES INCLUDE vibrationevent.h)
    qt_add_dbus_interfaces(dbusinterface_SRCS
        dbus/org.sigxcpu.Feedback.Haptic.xml)

    target_sources(plasma-keyboard PRIVATE
        vibration.cpp
        vibration.h
        vibrationevent.h
        ${dbusinterface_SRCS}
    )
endif()

if(PLASMA_KEYBOARD_SOUND_ENABLED)
    target_sources(plasma-keyboard PRIVATE
        soundresources.qrc
    )
endif()

ecm_qt_declare_logging_category(plasma-keyboard
    HEADER logging.h
    IDENTIFIER "PlasmaKeyboard"
    CATEGORY_NAME "org.kde.plasma.keyboard"
    DESCRIPTION "Plasma Keyboard"
    DEFAULT_SEVERITY Warning
)

ecm_add_qml_module(plasma-keyboard
    URI "org.kde.plasma.keyboard"
    GENERATE_PLUGIN_SOURCE
    DEPENDENCIES
    QtQuick
)

ecm_target_qml_sources(plasma-keyboard SOURCES
    qml/main.qml
    qml/LanguagePopup.qml
    qml/LanguagePopupDelegate.qml
    qml/DiacriticsOverlay.qml
    qml/OverlayWindow.qml
    qml/FcitxMain.qml
    qml/FcitxCandidateBar.qml
)
ecm_finalize_qml_module(plasma-keyboard)

kconfig_add_kcfg_files(plasma-keyboard GENERATE_MOC plasmakeyboardsettings.kcfgc)

target_include_directories(plasma-keyboard PRIVATE
    ${CMAKE_BINARY_DIR}
    # Add directories to include path for QML type registration
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/overlay
    ${CMAKE_CURRENT_SOURCE_DIR}/fcitx
)

target_link_libraries(plasma-keyboard
    PRIVATE
    KF6::CoreAddons
    KF6::I18n
    KF6::I18nQml
    KF6::ConfigCore
    KF6::ConfigGui
    KF6::Crash
    LayerShellQt::Interface
    PkgConfig::XKBCommon
    Plasma::PlasmaQuick
    Qt::DBus
    Qt::Gui
    Qt::GuiPrivate
    Qt::VirtualKeyboard
    Qt::WaylandClient
    Qt::WaylandClientPrivate
    Wayland::Client
)

install(TARGETS plasma-keyboard ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(PROGRAMS org.kde.plasma.keyboard.desktop DESTINATION ${KDE_INSTALL_APPDIR})

# Plasma Keyboard as Fcitx 5's keyboard: started at login (it ends at once unless KWin's input method is Fcitx 5 and the
# showWithFcitx setting is on), and an application entry whose absolute Exec lets KWin grant it its fake input device
configure_file(org.kde.plasma.keyboard.fcitx.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/org.kde.plasma.keyboard.fcitx.desktop @ONLY)
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/org.kde.plasma.keyboard.fcitx.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.plasma.keyboard.fcitx.desktop DESTINATION ${KDE_INSTALL_AUTOSTARTDIR})

add_subdirectory(styles)
add_subdirectory(qmlplugin)

install(DIRECTORY layouts DESTINATION ${CMAKE_INSTALL_PREFIX}/share/plasma/keyboard)
# The layouts of Fcitx 5 mode (layouts-fcitx). Korean and Chinese (Taiwan): QWERTY keys labelled as Fcitx 5's engine reads
# them, their other pages Qt Virtual Keyboard's fallback (empty markers). Japanese and Chinese (China): the fallback layout,
# whose Latin keys Mozc and Pinyin read. Every other locale: Plasma Keyboard's own layout. Markers and links are made here,
# at install: a Debian source's patch cannot carry an empty file or a link.
set(_fcitx_layouts ${CMAKE_INSTALL_PREFIX}/share/plasma/keyboard/layouts-fcitx)
foreach(_layout ko_KR zh_TW)
    install(FILES layouts-fcitx/${_layout}/main.qml DESTINATION ${_fcitx_layouts}/${_layout})
    foreach(_page dialpad digits handwriting numbers symbols)
        install(CODE "file(TOUCH \"\$ENV{DESTDIR}${_fcitx_layouts}/${_layout}/${_page}.fallback\")")
    endforeach()
endforeach()
foreach(_layout ja_JP zh_CN)
    install(CODE "file(CREATE_LINK ../layouts/fallback \"\$ENV{DESTDIR}${_fcitx_layouts}/${_layout}\" SYMBOLIC)")
endforeach()
file(GLOB _plasma_keyboard_layouts RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/layouts ${CMAKE_CURRENT_SOURCE_DIR}/layouts/*)
foreach(_layout ${_plasma_keyboard_layouts})
    if(NOT _layout MATCHES "^(ko_KR|zh_TW|ja_JP|zh_CN)$")
        install(CODE "file(CREATE_LINK ../layouts/${_layout} \"\$ENV{DESTDIR}${_fcitx_layouts}/${_layout}\" SYMBOLIC)")
    endif()
endforeach()
install(DIRECTORY overlay/diacritics DESTINATION ${CMAKE_INSTALL_PREFIX}/share/plasma/keyboard)

install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/org.kde.plasma.keyboard.metainfo.xml
    DESTINATION ${KDE_INSTALL_METAINFODIR}
)
