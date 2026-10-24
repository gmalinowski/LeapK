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

list(APPEND CMAKE_AUTOMOC_MACRO_NAMES
    "KWIN_EFFECT_FACTORY"
    "KWIN_EFFECT_FACTORY_SUPPORTED"
    "KWIN_EFFECT_FACTORY_ENABLED"
    "KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED"
)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

find_package(Qt6 REQUIRED COMPONENTS Core DBus Gui Widgets Quick Network)
find_package(KF6 REQUIRED COMPONENTS CoreAddons ConfigWidgets KCMUtils GlobalAccel)
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
    KF6::GlobalAccel
    Qt6::Widgets
)
# Identify the loaded library, including uncommitted test builds.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    activewindowoutline.cpp activewindowoutline.h popupfilter.h cursoroverlay.h visibilitystate.h ambilightalgorithm.h)
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/activewindowoutline.cpp" OUTLINE_CPP_HASH)
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/activewindowoutline.h" OUTLINE_HEADER_HASH)
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/popupfilter.h" OUTLINE_POPUP_HASH)
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/cursoroverlay.h" OUTLINE_CURSOR_HASH)
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/visibilitystate.h" OUTLINE_VISIBILITY_HASH)
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/ambilightalgorithm.h" OUTLINE_AMBILIGHT_HASH)
string(SHA256 OUTLINE_BUILD_HASH "${OUTLINE_CPP_HASH}${OUTLINE_HEADER_HASH}${OUTLINE_POPUP_HASH}${OUTLINE_CURSOR_HASH}${OUTLINE_VISIBILITY_HASH}${OUTLINE_AMBILIGHT_HASH}")
string(SUBSTRING "${OUTLINE_BUILD_HASH}" 0 12 OUTLINE_BUILD_ID)
target_compile_definitions(active-window-outline PRIVATE
    ACTIVE_WINDOW_OUTLINE_VERSION="${PROJECT_VERSION}"
    ACTIVE_WINDOW_OUTLINE_BUILD_ID="${OUTLINE_BUILD_ID}"
    ACTIVE_WINDOW_OUTLINE_KWIN_VERSION="${DETECTED_KWIN_VERSION}"
    COLORPICKER_INSTALL_DIR="${KDE_INSTALL_FULL_LIBEXECDIR}"
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
    if(DETECTED_KWIN_VERSION VERSION_GREATER_EQUAL "6.7")
        target_compile_definitions(active-window-outline PRIVATE
            ACTIVE_WINDOW_OUTLINE_SHADER_POINTER_VALIDITY
            ACTIVE_WINDOW_OUTLINE_SURFACE_BLUR_REGION
        )
    endif()
    if(DETECTED_KWIN_VERSION VERSION_GREATER_EQUAL "6.8")
        target_compile_definitions(active-window-outline PRIVATE
            ACTIVE_WINDOW_OUTLINE_BOOL_PAINT_SCREEN
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
    Qt6::Network
    KF6::CoreAddons
    KF6::ConfigWidgets
    KF6::KCMUtils
)
target_compile_definitions(kwin_active-window-outline_config PRIVATE
    ACTIVE_WINDOW_OUTLINE_VERSION="${PROJECT_VERSION}"
)

# --- Standalone Color Picker helper ---
add_executable(active-window-outline-colorpicker colorpicker.cpp)
target_link_libraries(active-window-outline-colorpicker PRIVATE
    Qt6::Widgets
)
install(TARGETS active-window-outline-colorpicker
    DESTINATION ${KDE_INSTALL_LIBEXECDIR}
)

option(OUTLINE_BUILD_TESTS "Build popup and cursor regression tests" OFF)
if(OUTLINE_BUILD_TESTS)
    enable_testing()
    add_executable(outline-popup-filter-test tests/popupfilter_test.cpp)
    target_include_directories(outline-popup-filter-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME outline-popup-filter COMMAND outline-popup-filter-test)

    add_executable(outline-color-transition-test tests/colortransition_test.cpp)
    target_include_directories(outline-color-transition-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(outline-color-transition-test PRIVATE Qt6::Core Qt6::Gui)
    add_test(NAME outline-color-transition COMMAND outline-color-transition-test)

    add_executable(outline-ambilight-test tests/ambilight_test.cpp)
    target_include_directories(outline-ambilight-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(outline-ambilight-test PRIVATE Qt6::Core Qt6::Gui)
    add_test(NAME outline-ambilight COMMAND outline-ambilight-test)

    add_executable(outline-shader-effects-test tests/shadereffects_test.cpp)
    target_include_directories(outline-shader-effects-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(outline-shader-effects-test PRIVATE Qt6::Core)
    add_test(NAME outline-shader-effects COMMAND outline-shader-effects-test)

    add_executable(outline-geometry-test tests/geometry_test.cpp)
    target_include_directories(outline-geometry-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(outline-geometry-test PRIVATE Qt6::Core Qt6::Gui)
    add_test(NAME outline-geometry COMMAND outline-geometry-test)

    add_executable(outline-visibility-test tests/visibilitystate_test.cpp)
    target_include_directories(outline-visibility-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(outline-visibility-test PRIVATE Qt6::Core)
    add_test(NAME outline-visibility COMMAND outline-visibility-test)

    add_executable(outline-versioncheck-test tests/versioncheck_test.cpp)
    target_include_directories(outline-versioncheck-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(outline-versioncheck-test PRIVATE Qt6::Core)
    add_test(NAME outline-versioncheck COMMAND outline-versioncheck-test)

    add_executable(outline-precedence-test tests/precedence_test.cpp)
    target_include_directories(outline-precedence-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(outline-precedence-test PRIVATE Qt6::Core)
    add_test(NAME outline-precedence COMMAND outline-precedence-test)

    add_executable(outline-badge-test tests/badge_test.cpp)
    target_include_directories(outline-badge-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(outline-badge-test PRIVATE Qt6::Core Qt6::Gui)
    add_test(NAME outline-badge COMMAND outline-badge-test)

    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME outline-config-sync COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/configsync_test.py)
    endif()

    if(KWIN_BACKEND STREQUAL "WAYLAND" AND DETECTED_KWIN_VERSION VERSION_GREATER_EQUAL "6.6")
        add_executable(outline-cursor-overlay-test tests/cursoroverlay_test.cpp)
        target_include_directories(outline-cursor-overlay-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        target_link_libraries(outline-cursor-overlay-test PRIVATE ${KWIN_TARGET})
        target_compile_definitions(outline-cursor-overlay-test PRIVATE
            $<TARGET_PROPERTY:active-window-outline,COMPILE_DEFINITIONS>)
        add_test(NAME outline-cursor-overlay COMMAND outline-cursor-overlay-test)
    endif()
endif()
