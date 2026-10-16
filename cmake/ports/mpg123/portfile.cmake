set(MPG123_VERSION 1.26.3)
set(MPG123_HASH 7574331afaecf3f867455df4b7012e90686ad6ac8c5b5e820244204ea7088bf2b02c3e75f53fe71c205f9eca81fef93f1d969c8d0d1ee9775dc05482290f7b2d)

#architecture detection
if(VCPKG_TARGET_ARCHITECTURE STREQUAL "x86")
   set(MPG123_ARCH Win32)
   set(MPG123_CONFIGURATION _x86)
elseif(VCPKG_TARGET_ARCHITECTURE STREQUAL "x64")
   set(MPG123_ARCH x64)
   set(MPG123_CONFIGURATION _x86)
elseif(VCPKG_TARGET_ARCHITECTURE STREQUAL "arm")
   set(MPG123_ARCH ARM)
   set(MPG123_CONFIGURATION _Generic)
elseif(VCPKG_TARGET_ARCHITECTURE STREQUAL "arm64")
   set(MPG123_ARCH ARM64)
   set(MPG123_CONFIGURATION _Generic)
else()
   message(FATAL_ERROR "unsupported architecture")
endif()

#linking
if (VCPKG_LIBRARY_LINKAGE STREQUAL dynamic)
    set(MPG123_CONFIGURATION_SUFFIX _Dll)
endif()

if (NOT VCPKG_TARGET_IS_WINDOWS)
    vcpkg_from_sourceforge(
        OUT_SOURCE_PATH SOURCE_PATH
        REPO mpg123/mpg123
        REF ${MPG123_VERSION}
        FILENAME "mpg123-${MPG123_VERSION}.tar.bz2"
        SHA512 ${MPG123_HASH}
        PATCHES
            0001-fix-x86-build.patch
    )
endif()

# include(${CURRENT_INSTALLED_DIR}/share/yasm-tool-helper/yasm-tool-helper.cmake)
# yasm_tool_helper(APPEND_TO_PATH)

if(VCPKG_TARGET_IS_UWP)
    vcpkg_install_msbuild(
        SOURCE_PATH ${SOURCE_PATH}
        PROJECT_SUBPATH ports/MSVC++/2015/uwp/libmpg123/libmpg123.vcxproj
        OPTIONS /p:UseEnv=True
        PLATFORM ${MPG123_ARCH}
        RELEASE_CONFIGURATION Release_uwp
        DEBUG_CONFIGURATION Debug_uwp
    )
    file(INSTALL
        ${SOURCE_PATH}/ports/MSVC++/mpg123.h
        ${SOURCE_PATH}/src/libmpg123/fmt123.h
        ${SOURCE_PATH}/src/libmpg123/mpg123.h.in
        DESTINATION ${CURRENT_PACKAGES_DIR}/include
    )
elseif(VCPKG_TARGET_IS_WINDOWS)
    # vcpkg_install_msbuild(
    #     SOURCE_PATH ${SOURCE_PATH}
    #     PROJECT_SUBPATH ports/MSVC++/2015/win32/libmpg123/libmpg123.vcxproj
    #     OPTIONS /p:UseEnv=True
    #     RELEASE_CONFIGURATION Release${MPG123_CONFIGURATION}${MPG123_CONFIGURATION_SUFFIX}
    #     DEBUG_CONFIGURATION Debug${MPG123_CONFIGURATION}${MPG123_CONFIGURATION_SUFFIX}
    # )
    # file(INSTALL
    #     ${SOURCE_PATH}/ports/MSVC++/mpg123.h
    #     ${SOURCE_PATH}/src/libmpg123/fmt123.h
    #     ${SOURCE_PATH}/src/libmpg123/mpg123.h.in
    #     DESTINATION ${CURRENT_PACKAGES_DIR}/include
    # )

    # Tivoli's prebuilt zip is gone with cdn.tivolicloud.com: use mpg123's own x64 build and make
    # the MSVC import library from its .def (the 1.26 CMake build can't target x64 under Ninja)
    vcpkg_download_distfile(
        MPG123_ARCHIVE
        URLS https://www.mpg123.de/download/win64/${MPG123_VERSION}/mpg123-${MPG123_VERSION}-x86-64.zip
        SHA512 affa3fca6fe5ac72778567096f88fc8c4c794c399ac278aa9488e569418c0f6701bb5c0d70d2477f1b8a4f10338bce4a6edced1218599c492be7ee574c6d32c9
        FILENAME mpg123-${MPG123_VERSION}-x86-64.zip
    )

    vcpkg_extract_source_archive_ex(
        OUT_SOURCE_PATH MPG123_EXTRACTED
        ARCHIVE ${MPG123_ARCHIVE}
    )

    file(INSTALL ${MPG123_EXTRACTED}/mpg123.h ${MPG123_EXTRACTED}/fmt123.h DESTINATION ${CURRENT_PACKAGES_DIR}/include)
    foreach(BUILD_DIR IN ITEMS "" "debug/")
        if (BUILD_DIR STREQUAL "debug/" AND VCPKG_BUILD_TYPE STREQUAL "release")
            continue()
        endif()
        file(INSTALL ${MPG123_EXTRACTED}/libmpg123-0.dll DESTINATION ${CURRENT_PACKAGES_DIR}/${BUILD_DIR}bin)
        file(MAKE_DIRECTORY ${CURRENT_PACKAGES_DIR}/${BUILD_DIR}lib)
        vcpkg_execute_required_process(
            COMMAND lib /nologo /machine:x64 /def:${MPG123_EXTRACTED}/libmpg123-0.def /out:mpg123.lib
            WORKING_DIRECTORY ${CURRENT_PACKAGES_DIR}/${BUILD_DIR}lib
            LOGNAME lib-${TARGET_TRIPLET}-${BUILD_DIR}
        )
        file(REMOVE ${CURRENT_PACKAGES_DIR}/${BUILD_DIR}lib/mpg123.exp)
    endforeach()
    file(INSTALL ${MPG123_EXTRACTED}/COPYING.txt DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)

elseif(VCPKG_TARGET_IS_OSX OR VCPKG_TARGET_IS_LINUX)
    set(MPG123_OPTIONS
        --disable-dependency-tracking
    )

    # Find cross-compiler prefix
    if(VCPKG_CHAINLOAD_TOOLCHAIN_FILE)
        include("${VCPKG_CHAINLOAD_TOOLCHAIN_FILE}")
    endif()
    if(CMAKE_C_COMPILER)
        vcpkg_execute_required_process(
            COMMAND ${CMAKE_C_COMPILER} -dumpmachine
            WORKING_DIRECTORY ${CURRENT_BUILDTREES_DIR}
            LOGNAME dumpmachine-${TARGET_TRIPLET}
        )
        file(READ ${CURRENT_BUILDTREES_DIR}/dumpmachine-${TARGET_TRIPLET}-out.log MPG123_HOST)
        string(REPLACE "\n" "" MPG123_HOST "${MPG123_HOST}")
        message(STATUS "Cross-compiling with ${CMAKE_C_COMPILER}")
        message(STATUS "Detected autoconf triplet --host=${MPG123_HOST}")
        set(MPG123_OPTIONS
            --host=${MPG123_HOST}
            ${MPG123_OPTIONS}
        )
    endif()

    vcpkg_configure_make(
        SOURCE_PATH ${SOURCE_PATH}
        OPTIONS ${MPG123_OPTIONS}
    )
    vcpkg_install_make()

    file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")
endif()

if (NOT VCPKG_TARGET_IS_WINDOWS)
    file(INSTALL ${SOURCE_PATH}/COPYING DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)
endif()

message(STATUS "Installing done")
