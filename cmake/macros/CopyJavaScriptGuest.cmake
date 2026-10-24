#
#  CopyJavaScriptGuest.cmake
#  cmake/macros
#
#  The JavaScript guest, darkride-js.wasm (QuickJS-NG compiled to WebAssembly with the shim that gives a script
#  the engine's objects; see tools/wasm-js), copied beside a target's resources after the target builds. The
#  engine looks for it at resources/wasm/darkride-js.wasm (Contents/Resources/wasm on macOS), or where
#  $DARKRIDE_JS_WASM points.
#
#  The module is taken from, in this order: $ENV{DARKRIDE_JS_WASM}; tools/wasm-js/build/darkride-js.wasm (a local
#  build.sh); ../downloads/darkride-js-<version>.wasm beside the checkout (a published build). When
#  none exists at configure time the client builds without it and JavaScript stays on the QtScript engine,
#  with a warning here.
#
#  The version is QuickJS-NG's, then a revision after the dash that goes up whenever the guest's imports or
#  exports change, so that a published module only reaches the builds whose host matches it. Revision 2 asks
#  the host whether to stop (darkride.interrupted), which the time limit per script needs; a host from before
#  it refuses that import.
#
#  Distributed under the Apache License, Version 2.0.
#  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
#
macro(COPY_JAVASCRIPT_GUEST DESTINATION_DIR)
    set(JAVASCRIPT_GUEST_VERSION "0.17.0-2")
    set(JAVASCRIPT_GUEST "")
    foreach(JAVASCRIPT_GUEST_CANDIDATE
            "$ENV{DARKRIDE_JS_WASM}"
            "${CMAKE_SOURCE_DIR}/tools/wasm-js/build/darkride-js.wasm"
            "${CMAKE_SOURCE_DIR}/../downloads/darkride-js-${JAVASCRIPT_GUEST_VERSION}.wasm")
        if (NOT "${JAVASCRIPT_GUEST_CANDIDATE}" STREQUAL "" AND EXISTS "${JAVASCRIPT_GUEST_CANDIDATE}" AND "${JAVASCRIPT_GUEST}" STREQUAL "")
            set(JAVASCRIPT_GUEST "${JAVASCRIPT_GUEST_CANDIDATE}")
        endif()
    endforeach()
    if ("${JAVASCRIPT_GUEST}" STREQUAL "")
        message(WARNING "No JavaScript guest module: JavaScript runs on the QtScript engine only. Put darkride-js-${JAVASCRIPT_GUEST_VERSION}.wasm in ${CMAKE_SOURCE_DIR}/../downloads or build one with tools/wasm-js/build.sh.")
    else()
        message(STATUS "JavaScript guest from ${JAVASCRIPT_GUEST}")
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${DESTINATION_DIR}/wasm"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${JAVASCRIPT_GUEST}" "${DESTINATION_DIR}/wasm/darkride-js.wasm")
    endif()
endmacro()
