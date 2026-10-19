#
#  TargetMeshoptimizer.cmake
#  cmake/macros
#
#  meshoptimizer (MIT, https://github.com/zeux/meshoptimizer), the mesh simplifier the model baker makes automatic
#  LODs with (AUTOMATIC-LODS.md), compiled inside this build tree from a pinned release tarball, as Jolt is
#  (TargetJolt.cmake): a vcpkg port would change the vcpkg hash and rebuild every dependency beside the shared
#  install the other checkouts use. Only the simplifier, the vertex cache optimiser and their allocator are built.
#
#  The tarball is taken from $TIVOLI_DOWNLOADS/meshoptimizer-v<version>.tar.gz or ../downloads/ beside this checkout
#  when either exists, and from GitHub otherwise. Its SHA-256 is checked in every case.
#
#  Distributed under the Apache License, Version 2.0.
#  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
#
macro(TARGET_MESHOPTIMIZER)
    if (NOT TARGET meshoptimizer)
        include(FetchContent)
        set(MESHOPTIMIZER_VERSION "1.3")
        set(MESHOPTIMIZER_SHA256 "ca4f47d451201593904b9e3067fa54b65db94bc34ab5173864950862b748a145")
        set(MESHOPTIMIZER_TARBALL "meshoptimizer-v${MESHOPTIMIZER_VERSION}.tar.gz")
        # one URL only: CMake refuses a list that mixes a local path with web addresses
        set(MESHOPTIMIZER_URL "https://github.com/zeux/meshoptimizer/archive/refs/tags/v${MESHOPTIMIZER_VERSION}.tar.gz")
        foreach(MESHOPTIMIZER_DIR "${CMAKE_SOURCE_DIR}/../downloads" "$ENV{TIVOLI_DOWNLOADS}")
            if (NOT "${MESHOPTIMIZER_DIR}" STREQUAL "" AND EXISTS "${MESHOPTIMIZER_DIR}/${MESHOPTIMIZER_TARBALL}")
                get_filename_component(MESHOPTIMIZER_URL "${MESHOPTIMIZER_DIR}/${MESHOPTIMIZER_TARBALL}" ABSOLUTE)
            endif()
        endforeach()
        message(STATUS "meshoptimizer ${MESHOPTIMIZER_VERSION} from ${MESHOPTIMIZER_URL}")

        FetchContent_Declare(meshoptimizer_sources
            URL "${MESHOPTIMIZER_URL}"
            URL_HASH SHA256=${MESHOPTIMIZER_SHA256}
            SOURCE_SUBDIR src   # has no CMakeLists.txt, so only the sources are fetched; the library is defined below
        )
        FetchContent_MakeAvailable(meshoptimizer_sources)

        set(MESHOPTIMIZER_SOURCE_DIR "${meshoptimizer_sources_SOURCE_DIR}/src")
        add_library(meshoptimizer STATIC
            "${MESHOPTIMIZER_SOURCE_DIR}/simplifier.cpp"
            "${MESHOPTIMIZER_SOURCE_DIR}/vcacheoptimizer.cpp"
            "${MESHOPTIMIZER_SOURCE_DIR}/allocator.cpp")
        target_include_directories(meshoptimizer SYSTEM PUBLIC "${MESHOPTIMIZER_SOURCE_DIR}")
        # linked into the libraries' shared builds on Linux, as Jolt is
        set_target_properties(meshoptimizer PROPERTIES FOLDER "hidden/externals" POSITION_INDEPENDENT_CODE ON)
    endif()
    target_link_libraries(${TARGET_NAME} meshoptimizer)
endmacro()
