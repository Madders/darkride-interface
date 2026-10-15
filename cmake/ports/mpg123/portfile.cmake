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

vcpkg_from_sourceforge(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO mpg123/mpg123
    REF ${MPG123_VERSION}
    FILENAME "mpg123-${MPG123_VERSION}.tar.bz2"
    SHA512 ${MPG123_HASH}
    PATCHES
        0001-fix-x86-build.patch
)

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
    # Tivoli's prebuilt Windows archive is gone; build the libraries with mpg123's own CMake project.
    # That project tells x64 from x86 by the Visual Studio platform, which a Ninja build leaves empty.
    vcpkg_replace_string(${SOURCE_PATH}/ports/cmake/src/libmpg123/CMakeLists.txt
        "MSVC AND CMAKE_GENERATOR_PLATFORM STREQUAL \"x64\"" "MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8")
    vcpkg_find_acquire_program(YASM)

    vcpkg_configure_cmake(
        SOURCE_PATH ${SOURCE_PATH}/ports/cmake
        PREFER_NINJA
        OPTIONS
            -DYASM_ASSEMBLER=${YASM}
            -DUSE_MODULES=OFF
            -DBUILD_TESTING=OFF
    )
    vcpkg_install_cmake()
    vcpkg_copy_pdbs()
    # Leave out mpg123's CMake config, as the autotools builds elsewhere do: libsndfile would prefer it to its
    # own FindMpg123 and then not link mpg123, and it names import libraries for libout123 and libsyn123
    # that MSVC never writes, since those DLLs export nothing.
    file(REMOVE_RECURSE ${CURRENT_PACKAGES_DIR}/lib/cmake ${CURRENT_PACKAGES_DIR}/debug/lib/cmake)
    # mpg123.h includes fmt123.h, which this version's CMake project does not install
    file(INSTALL ${SOURCE_PATH}/src/libmpg123/fmt123.h DESTINATION ${CURRENT_PACKAGES_DIR}/include)

    file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")

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

file(INSTALL ${SOURCE_PATH}/COPYING DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)

message(STATUS "Installing done")
