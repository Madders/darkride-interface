vcpkg_fail_port_install(ON_TARGET "uwp")

# Android: TBB 2020's make build assumes the NDK's old layout (a sysroot at its root, gone by r22), so build oneTBB
# 2021, which has a CMake build for current NDKs. Darkride only uses its concurrent containers and parallel_for,
# which oneTBB keeps under the same tbb/*.h headers, and its package config provides the same TBB::tbb target.
if (VCPKG_TARGET_IS_ANDROID)
    vcpkg_from_github(
        OUT_SOURCE_PATH SOURCE_PATH
        REPO oneapi-src/oneTBB
        REF v2021.13.0
        SHA512 1f536b98c4eb331c3ba21e4b36362c932d7852b974ea7237b5f7bffb0e996d76f688c7ac534cb56f5866cfdffd5d3f015521b4c2ca21e584503070782ca82a2f
        HEAD_REF master
    )
    # Qt defines "emit" as an empty macro, and profiling.h declares emit() methods: hide Qt's macro in that header
    file(READ ${SOURCE_PATH}/include/oneapi/tbb/profiling.h _profiling)
    string(REPLACE "#define __TBB_profiling_H" "#define __TBB_profiling_H\n#pragma push_macro(\"emit\")\n#undef emit" _profiling "${_profiling}")
    string(REGEX REPLACE "(#endif[^\n]*__TBB_profiling_H[^\n]*)\n*$" "#pragma pop_macro(\"emit\")\n\\1\n" _profiling "${_profiling}")
    file(WRITE ${SOURCE_PATH}/include/oneapi/tbb/profiling.h "${_profiling}")
    vcpkg_configure_cmake(
        SOURCE_PATH ${SOURCE_PATH}
        PREFER_NINJA
        OPTIONS
            -DTBB_TEST=OFF
            -DTBB_EXAMPLES=OFF
            -DTBB_STRICT=OFF
            -DTBBMALLOC_BUILD=OFF
    )
    vcpkg_install_cmake()
    vcpkg_fixup_cmake_targets(CONFIG_PATH lib/cmake/TBB TARGET_PATH share/tbb)
    file(REMOVE_RECURSE ${CURRENT_PACKAGES_DIR}/debug/include ${CURRENT_PACKAGES_DIR}/debug/share ${CURRENT_PACKAGES_DIR}/share/doc)
    file(INSTALL ${SOURCE_PATH}/LICENSE.txt DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)
    return()
endif()

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO oneapi-src/oneTBB
    REF eca91f16d7490a8abfdee652dadf457ec820cc37 # 2020_U3
    SHA512 7144e1dc68304b5358e6ea330431b6f0c61fadb147efa353a5b242777d6fabf7b8cf99b79cffb51b49b911dd17a9f1879619d6eebdf319f23ec3235c89cffc25
    HEAD_REF tbb_2019
    PATCHES
        fix-static-build.patch
        terminate-when-buildtool-notfound.patch
)

# GCC 13 rejects task_prefix declaring a task() member after using the name for class tbb::task
# ("changes meaning of 'task'"); qualify the earlier uses, as oneTBB later did.
vcpkg_replace_string(${SOURCE_PATH}/include/tbb/task.h "friend void make_critical( task& );" "friend void make_critical( tbb::task& );")
vcpkg_replace_string(${SOURCE_PATH}/include/tbb/task.h "friend bool is_critical( task& );" "friend bool is_critical( tbb::task& );")
vcpkg_replace_string(${SOURCE_PATH}/include/tbb/task.h "        task* next_offloaded;" "        tbb::task* next_offloaded;")

file(COPY ${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt DESTINATION ${SOURCE_PATH})

if (NOT VCPKG_TARGET_IS_WINDOWS)
    vcpkg_configure_cmake(
        SOURCE_PATH ${SOURCE_PATH}
        PREFER_NINJA
    )

    vcpkg_install_cmake()

    # Settings for TBBConfigInternal.cmake.in
    if (VCPKG_LIBRARY_LINKAGE STREQUAL static)
        set(TBB_LIB_EXT a)
    else()
        if (VCPKG_TARGET_IS_LINUX)
            set(TBB_LIB_EXT "so.2")
        elseif(VCPKG_TARGET_IS_OSX)
            set(TBB_LIB_EXT "dylib")
        else()
            set(TBB_LIB_EXT "so")
        endif()
    endif()
    set(TBB_LIB_PREFIX lib)
else()
    if (VCPKG_CRT_LINKAGE STREQUAL static)
        set(RELEASE_CONFIGURATION Release-MT)
        set(DEBUG_CONFIGURATION Debug-MT)
    else()
        set(RELEASE_CONFIGURATION Release)
        set(DEBUG_CONFIGURATION Debug)
    endif()

    macro(CONFIGURE_PROJ_FILE arg)
        set(CONFIGURE_FILE_NAME ${arg})
        set(CONFIGURE_BAK_FILE_NAME ${arg}.bak)
        if (NOT EXISTS ${CONFIGURE_BAK_FILE_NAME})
            configure_file(${CONFIGURE_FILE_NAME} ${CONFIGURE_BAK_FILE_NAME} COPYONLY)
        endif()
        configure_file(${CONFIGURE_BAK_FILE_NAME} ${CONFIGURE_FILE_NAME} COPYONLY)
        if (VCPKG_LIBRARY_LINKAGE STREQUAL static)
            file(READ ${CONFIGURE_FILE_NAME} SLN_CONFIGURE)
            string(REPLACE "<ConfigurationType>DynamicLibrary<\/ConfigurationType>"
                        "<ConfigurationType>StaticLibrary<\/ConfigurationType>" SLN_CONFIGURE "${SLN_CONFIGURE}")
            string(REPLACE "\/D_CRT_SECURE_NO_DEPRECATE"
                        "\/D_CRT_SECURE_NO_DEPRECATE \/DIN_CILK_STATIC" SLN_CONFIGURE "${SLN_CONFIGURE}")
            file(WRITE ${CONFIGURE_FILE_NAME} "${SLN_CONFIGURE}")
        else()
            file(READ ${CONFIGURE_FILE_NAME} SLN_CONFIGURE)
            string(REPLACE "\/D_CRT_SECURE_NO_DEPRECATE"
                        "\/D_CRT_SECURE_NO_DEPRECATE \/DIN_CILK_RUNTIME" SLN_CONFIGURE "${SLN_CONFIGURE}")
            file(WRITE ${CONFIGURE_FILE_NAME} "${SLN_CONFIGURE}")
        endif()
    endmacro()

    CONFIGURE_PROJ_FILE(${SOURCE_PATH}/build/vs2013/tbb.vcxproj)
    CONFIGURE_PROJ_FILE(${SOURCE_PATH}/build/vs2013/tbbmalloc.vcxproj)
    CONFIGURE_PROJ_FILE(${SOURCE_PATH}/build/vs2013/tbbmalloc_proxy.vcxproj)

    vcpkg_install_msbuild(
        SOURCE_PATH ${SOURCE_PATH}
        PROJECT_SUBPATH build/vs2013/makefile.sln
        RELEASE_CONFIGURATION ${RELEASE_CONFIGURATION}
        DEBUG_CONFIGURATION ${DEBUG_CONFIGURATION}
    )
    # Settings for TBBConfigInternal.cmake.in
    set(TBB_LIB_EXT lib)
    set(TBB_LIB_PREFIX)
endif()

file(COPY
  ${SOURCE_PATH}/include/tbb
  ${SOURCE_PATH}/include/serial
  DESTINATION ${CURRENT_PACKAGES_DIR}/include)

# Settings for TBBConfigInternal.cmake.in
if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
    set(TBB_DEFAULT_COMPONENTS tbb tbbmalloc)
else()
    set(TBB_DEFAULT_COMPONENTS tbb tbbmalloc tbbmalloc_proxy)
endif()

file(READ "${SOURCE_PATH}/include/tbb/tbb_stddef.h" _tbb_stddef)
string(REGEX REPLACE ".*#define TBB_VERSION_MAJOR ([0-9]+).*" "\\1" _tbb_ver_major "${_tbb_stddef}")
string(REGEX REPLACE ".*#define TBB_VERSION_MINOR ([0-9]+).*" "\\1" _tbb_ver_minor "${_tbb_stddef}")
string(REGEX REPLACE ".*#define TBB_INTERFACE_VERSION ([0-9]+).*" "\\1" TBB_INTERFACE_VERSION "${_tbb_stddef}")
set(TBB_VERSION "${_tbb_ver_major}.${_tbb_ver_minor}")
set(TBB_RELEASE_DIR "\${_tbb_root}/lib")
set(TBB_DEBUG_DIR "\${_tbb_root}/debug/lib")

configure_file(
    ${SOURCE_PATH}/cmake/templates/TBBConfigInternal.cmake.in
    ${CURRENT_PACKAGES_DIR}/share/tbb/TBBConfig.cmake
    @ONLY
)

configure_file(
    ${SOURCE_PATH}/cmake/templates/TBBConfigVersion.cmake.in
    ${CURRENT_PACKAGES_DIR}/share/tbb/TBBConfigVersion.cmake
    @ONLY
)

file(READ ${CURRENT_PACKAGES_DIR}/share/tbb/TBBConfig.cmake _contents)
string(REPLACE
    "get_filename_component(_tbb_root \"\${_tbb_root}\" PATH)"
    "get_filename_component(_tbb_root \"\${_tbb_root}\" PATH)\nget_filename_component(_tbb_root \"\${_tbb_root}\" PATH)"
    _contents
    "${_contents}"
)
string(REPLACE
    "set(_tbb_release_lib \"/${TBB_LIB_PREFIX}"
    "set(_tbb_release_lib \"\${_tbb_root}/lib/${TBB_LIB_PREFIX}"
    _contents
    "${_contents}"
)
string(REPLACE
    "set(_tbb_debug_lib \"/${TBB_LIB_PREFIX}"
    "set(_tbb_debug_lib \"\${_tbb_root}/debug/lib/${TBB_LIB_PREFIX}"
    _contents
    "${_contents}"
)

string(REPLACE "SHARED IMPORTED)" "UNKNOWN IMPORTED)" _contents "${_contents}")
file(WRITE ${CURRENT_PACKAGES_DIR}/share/tbb/TBBConfig.cmake "${_contents}")

file(COPY ${CMAKE_CURRENT_LIST_DIR}/usage DESTINATION ${CURRENT_PACKAGES_DIR}/share/tbb)
# Handle copyright
file(INSTALL ${SOURCE_PATH}/LICENSE DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)
