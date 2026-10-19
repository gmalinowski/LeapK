# SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.20)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" PROJECT_VERSION_STRING LIMIT_COUNT 1)
string(STRIP "${PROJECT_VERSION_STRING}" PROJECT_VERSION_STRING)
if(NOT PROJECT_VERSION_STRING MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION must contain a semantic version such as 1.2.3")
endif()
project(active-window-outline VERSION "${PROJECT_VERSION_STRING}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(QT_MAJOR_VERSION 6)
set(QT_DEFAULT_MAJOR_VERSION 6)

set(KWIN_BACKEND "WAYLAND" CACHE STRING "KWin backend to build for (WAYLAND or X11)")
set_property(CACHE KWIN_BACKEND PROPERTY STRINGS WAYLAND X11)

if(KWIN_BACKEND STREQUAL "WAYLAND")
    set(KWIN_PACKAGE KWin)
    set(KWIN_TARGET KWin::kwin)
    set(KWIN_EFFECT_NAMESPACE "kwin/effects/plugins")
    set(KWIN_CONFIG_NAMESPACE "kwin/effects/configs")
elseif(KWIN_BACKEND STREQUAL "X11")
    set(KWIN_PACKAGE KWinX11)
    set(KWIN_TARGET KWinX11::kwin)
    set(KWIN_EFFECT_NAMESPACE "kwin-x11/effects/plugins")
    set(KWIN_CONFIG_NAMESPACE "kwin-x11/effects/configs")
else()
    message(FATAL_ERROR
        "Unknown KWIN_BACKEND='${KWIN_BACKEND}'. Expected exactly WAYLAND or X11.")
endif()

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

find_package(Qt6 REQUIRED COMPONENTS Core DBus Gui Widgets Quick)
find_package(KF6 REQUIRED COMPONENTS CoreAddons ConfigWidgets KCMUtils)
find_package(${KWIN_PACKAGE} REQUIRED)

if(KWIN_BACKEND STREQUAL "WAYLAND")
    set(DETECTED_KWIN_VERSION "${KWin_VERSION}")
else()
    set(DETECTED_KWIN_VERSION "${KWinX11_VERSION}")
endif()

# --- Effect plugin ---
kcoreaddons_add_plugin(active-window-outline
    SOURCES activewindowoutline.cpp
    INSTALL_NAMESPACE "${KWIN_EFFECT_NAMESPACE}"
)
target_link_libraries(active-window-outline PRIVATE
    ${KWIN_TARGET}
)

if(KWIN_BACKEND STREQUAL "X11")
    target_compile_definitions(active-window-outline PRIVATE
        ACTIVE_WINDOW_OUTLINE_X11
        ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
        ACTIVE_WINDOW_OUTLINE_PREPAINT_PRESENT_TIME
    )
    # KWin X11 6.7 headers use C++23 library types such as std::expected.
    if(DETECTED_KWIN_VERSION VERSION_GREATER_EQUAL "6.7")
        target_compile_features(active-window-outline PRIVATE cxx_std_23)
    endif()
else()
    if(DETECTED_KWIN_VERSION VERSION_LESS "6.6")
        target_compile_definitions(active-window-outline PRIVATE
            ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
        )
    endif()
    if(DETECTED_KWIN_VERSION VERSION_LESS "6.7")
        target_compile_definitions(active-window-outline PRIVATE
            ACTIVE_WINDOW_OUTLINE_PREPAINT_PRESENT_TIME
        )
    endif()
endif()

# --- Config module plugin ---
qt6_wrap_ui(CONFIG_UI_SRCS activewindowoutline_config.ui)

kcoreaddons_add_plugin(kwin_active-window-outline_config
    SOURCES activewindowoutline_config.cpp ${CONFIG_UI_SRCS}
    INSTALL_NAMESPACE "${KWIN_CONFIG_NAMESPACE}"
)
target_include_directories(kwin_active-window-outline_config PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(kwin_active-window-outline_config PRIVATE
    Qt6::DBus
    Qt6::Widgets
    KF6::CoreAddons
    KF6::ConfigWidgets
    KF6::KCMUtils
)
target_compile_definitions(kwin_active-window-outline_config PRIVATE
    ACTIVE_WINDOW_OUTLINE_VERSION="${PROJECT_VERSION}"
)
