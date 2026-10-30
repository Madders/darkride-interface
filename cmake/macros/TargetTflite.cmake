#
#  TargetTflite.cmake
#  cmake/macros
#
#  TensorFlow Lite (LiteRT, Apache-2.0): its C API with the XNNPACK delegate, one static library built from source by
#  the vcpkg port cmake/ports/tensorflow-lite (Mac and Windows), for webcam face tracking. Defines HAVE_TFLITE.
#
#  Copyright 2026 Darkride.
#
#  Distributed under the MIT License.
#  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
#
macro(TARGET_TFLITE)
    find_package(tensorflow-lite CONFIG REQUIRED)
    target_link_libraries(${TARGET_NAME} tensorflow-lite::tensorflowlite_c)
    target_compile_definitions(${TARGET_NAME} PRIVATE HAVE_TFLITE)
    if (APPLE)
        # The library is compiled with hidden symbols, but KleidiAI's assembly kernels (Arm's, inside XNNPACK) are
        # global whatever the setting: keep them inside the module that links it too
        target_link_options(${TARGET_NAME} PRIVATE "LINKER:-unexported_symbol,_kai_*")
    endif()
endmacro()
