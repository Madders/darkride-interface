#
#  TargetDarkrideChat.cmake
#  cmake/macros
#
#  Darkride chat's Rust library (libraries/chat/native: Matrix through matrix-rust-sdk, behind a C interface),
#  built by cargo inside this build tree through Corrosion (MIT, https://github.com/corrosion-rs/corrosion), the
#  usual CMake bridge to cargo. Corrosion comes from a pinned release tarball, as Jolt does (TargetJolt.cmake), and
#  not from vcpkg, whose hash would change for every checkout sharing its install.
#
#  The tarball is taken from $TIVOLI_DOWNLOADS/corrosion-v<version>.tar.gz or ../downloads/corrosion-v<version>.tar.gz
#  beside this checkout when either exists, and from GitHub otherwise. Its SHA-256 is checked in every case.
#
#  Building needs Rust: rustup (https://rustup.rs), with which the toolchain native/rust-toolchain.toml names is
#  used, and installed here the first time; without rustup, the cargo on the PATH, if new enough for matrix-rust-sdk
#  (1.96).
#
#  Copyright 2026 Darkride.
#
#  Distributed under the MIT License.
#
macro(TARGET_DARKRIDE_CHAT)
    set(DARKRIDE_CHAT_NATIVE_DIR "${CMAKE_SOURCE_DIR}/libraries/chat/native")
    if (NOT TARGET darkride_chat)
        # the toolchain the library was built and measured with, unless the builder chose one
        find_program(DARKRIDE_CHAT_RUSTUP rustup HINTS "$ENV{CARGO_HOME}/bin" "$ENV{HOME}/.cargo/bin")
        if (NOT DEFINED Rust_TOOLCHAIN AND DARKRIDE_CHAT_RUSTUP)
            file(STRINGS "${DARKRIDE_CHAT_NATIVE_DIR}/rust-toolchain.toml" DARKRIDE_CHAT_CHANNEL REGEX "^channel *=")
            string(REGEX REPLACE "^channel *= *\"([^\"]*)\".*" "\\1" DARKRIDE_CHAT_CHANNEL "${DARKRIDE_CHAT_CHANNEL}")
            execute_process(COMMAND "${DARKRIDE_CHAT_RUSTUP}" toolchain list OUTPUT_VARIABLE DARKRIDE_CHAT_TOOLCHAINS)
            if (NOT DARKRIDE_CHAT_TOOLCHAINS MATCHES "(^|\n)${DARKRIDE_CHAT_CHANNEL}-")
                message(STATUS "Installing Rust ${DARKRIDE_CHAT_CHANNEL} for chat's library")
                execute_process(COMMAND "${DARKRIDE_CHAT_RUSTUP}" toolchain install "${DARKRIDE_CHAT_CHANNEL}" --profile minimal)
            endif()
            set(Rust_TOOLCHAIN "${DARKRIDE_CHAT_CHANNEL}")
        endif()

        include(FetchContent)
        set(CORROSION_VERSION "0.6.1")
        set(CORROSION_SHA256 "e9e95b1ee2bad52681f347993fb1a5af5cce458c5ce8a2636c9e476e4babf8e3")
        set(CORROSION_TARBALL "corrosion-v${CORROSION_VERSION}.tar.gz")
        # one URL only: CMake refuses a list that mixes a local path with web addresses
        set(CORROSION_URL "https://github.com/corrosion-rs/corrosion/archive/refs/tags/v${CORROSION_VERSION}.tar.gz")
        foreach(CORROSION_DIR "${CMAKE_SOURCE_DIR}/../downloads" "$ENV{TIVOLI_DOWNLOADS}")
            if (NOT "${CORROSION_DIR}" STREQUAL "" AND EXISTS "${CORROSION_DIR}/${CORROSION_TARBALL}")
                get_filename_component(CORROSION_URL "${CORROSION_DIR}/${CORROSION_TARBALL}" ABSOLUTE)
            endif()
        endforeach()
        message(STATUS "Corrosion ${CORROSION_VERSION} from ${CORROSION_URL}")

        FetchContent_Declare(corrosion
            URL "${CORROSION_URL}"
            URL_HASH SHA256=${CORROSION_SHA256}
        )
        FetchContent_MakeAvailable(corrosion)

        # release in every configuration (opt-level z with full LTO, native/Cargo.toml): a debug build of
        # matrix-rust-sdk is slow and large, and a viewer debugging session rarely steps into it
        corrosion_import_crate(
            MANIFEST_PATH "${DARKRIDE_CHAT_NATIVE_DIR}/Cargo.toml"
            CRATES darkride-chat-ffi
            PROFILE release
            LOCKED
        )
        # the registry seen when configuring, so a later build from a shell without the Rust environment still
        # finds the downloaded crates instead of fetching them again
        if (DEFINED ENV{CARGO_HOME})
            corrosion_set_env_vars(darkride_chat "CARGO_HOME=$ENV{CARGO_HOME}")
        endif()

        target_include_directories(darkride_chat INTERFACE "${DARKRIDE_CHAT_NATIVE_DIR}/ffi/include")
        # what the library needs from the system beyond what Corrosion found for any Rust static library
        if (APPLE)
            find_library(DARKRIDE_CHAT_SECURITY Security)
            find_library(DARKRIDE_CHAT_CORE_FOUNDATION CoreFoundation)
            target_link_libraries(darkride_chat INTERFACE ${DARKRIDE_CHAT_SECURITY} ${DARKRIDE_CHAT_CORE_FOUNDATION} iconv)
        elseif (WIN32)
            target_link_libraries(darkride_chat INTERFACE bcrypt advapi32 kernel32 ntdll userenv ws2_32 dbghelp)
        endif()
    endif()
    target_link_libraries(${TARGET_NAME} darkride_chat)
endmacro()
