# 
#  Copyright 2015 High Fidelity, Inc.
#  Created by Bradley Austin Davis on 2015/10/10
#
#  Distributed under the Apache License, Version 2.0.
#  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
# 
macro(TARGET_GLAD)
    if (ANDROID)
        # vcpkg's glad port builds the GLES 3.2 loader on Android
        include(SelectLibraryConfigurations)
        find_library(GLAD_LIBRARY_RELEASE glad PATHS ${VCPKG_INSTALL_ROOT}/lib NO_DEFAULT_PATH)
        find_library(GLAD_LIBRARY_DEBUG glad_d PATHS ${VCPKG_INSTALL_ROOT}/debug/lib NO_DEFAULT_PATH)
        select_library_configurations(GLAD)
        set(GLAD_INCLUDE_DIRS "${VCPKG_INSTALL_ROOT}/include")
        find_library(EGL EGL)
        find_library(GLESV3 GLESv3)
        list(APPEND GLAD_EXTRA_LIBRARIES ${EGL} ${GLESV3} dl)
    else()
        if (APPLE AND CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
            # The x86_64 build used Mesa (Zink over MoltenVK) instead of Apple's OpenGL.
            find_library(MESA_LIBRARY_RELEASE glapi PATHS ${VCPKG_INSTALL_ROOT}/lib NO_DEFAULT_PATH REQUIRED)
            select_library_configurations(MESA)
            list(APPEND GLAD_EXTRA_LIBRARIES ${MESA_LIBRARY})
        else()
            # On arm64 macOS, use Apple's OpenGL 4.1 framework directly (same as Overte).
            find_package(OpenGL REQUIRED)
            list(APPEND GLAD_EXTRA_LIBRARIES ${OPENGL_LIBRARY})
        endif()

        find_library(GLAD_LIBRARY_RELEASE glad PATHS ${VCPKG_INSTALL_ROOT}/lib NO_DEFAULT_PATH)
        find_library(GLAD_LIBRARY_DEBUG glad_d PATHS ${VCPKG_INSTALL_ROOT}/debug/lib NO_DEFAULT_PATH)
        select_library_configurations(GLAD)
        if (NOT WIN32)
            list(APPEND GLAD_EXTRA_LIBRARIES dl)
        endif()
        set(GLAD_INCLUDE_DIRS ${${GLAD_UPPER}_INCLUDE_DIRS})
        if (APPLE AND NOT TARGET_NAME STREQUAL "gl")
            # macOS uses a two-level namespace: if the static glad archive is linked into several
            # dylibs (and the executable), each image gets its own function-pointer table and only
            # the one inside libtivoli-gl (where gl::initModuleGl runs) is ever loaded; the others
            # stay null and the first GL call crashes. Link glad into the gl library only and let
            # everything else resolve the glad_* symbols from its exports.
            set(GLAD_LIBRARY "")
        endif()
    endif()

    target_include_directories(${TARGET_NAME} PUBLIC ${GLAD_INCLUDE_DIRS})
    target_link_libraries(${TARGET_NAME} ${GLAD_LIBRARY})
    target_link_libraries(${TARGET_NAME} ${GLAD_EXTRA_LIBRARIES})       
endmacro()
