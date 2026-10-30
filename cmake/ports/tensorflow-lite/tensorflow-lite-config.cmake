#
#  Copyright 2026 Darkride.
#
#  Distributed under the MIT License.
#  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
#

# find_package(tensorflow-lite CONFIG): TensorFlow Lite's C API with the XNNPACK delegate, one static library
# (cmake/ports/tensorflow-lite), as the target tensorflow-lite::tensorflowlite_c.
if (TARGET tensorflow-lite::tensorflowlite_c)
  return()
endif()

get_filename_component(_TFLITE_PREFIX "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
if (WIN32)
  set(_TFLITE_LIBRARY_NAME "tensorflowlite_c.lib")
else()
  set(_TFLITE_LIBRARY_NAME "libtensorflowlite_c.a")
endif()

add_library(tensorflow-lite::tensorflowlite_c STATIC IMPORTED)
set_target_properties(tensorflow-lite::tensorflowlite_c PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${_TFLITE_PREFIX}/include"
  # the C API's functions declared for a static library: no dllexport, no default visibility
  INTERFACE_COMPILE_DEFINITIONS "TFL_STATIC_LIBRARY_BUILD"
  IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
)
if (EXISTS "${_TFLITE_PREFIX}/lib/${_TFLITE_LIBRARY_NAME}")
  set_property(TARGET tensorflow-lite::tensorflowlite_c APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
  set_target_properties(tensorflow-lite::tensorflowlite_c PROPERTIES
    IMPORTED_LOCATION_RELEASE "${_TFLITE_PREFIX}/lib/${_TFLITE_LIBRARY_NAME}"
    MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
    MAP_IMPORTED_CONFIG_MINSIZEREL Release
  )
endif()
if (EXISTS "${_TFLITE_PREFIX}/debug/lib/${_TFLITE_LIBRARY_NAME}")
  set_property(TARGET tensorflow-lite::tensorflowlite_c APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
  set_target_properties(tensorflow-lite::tensorflowlite_c PROPERTIES
    IMPORTED_LOCATION_DEBUG "${_TFLITE_PREFIX}/debug/lib/${_TFLITE_LIBRARY_NAME}"
  )
else()
  # release-only installs (VCPKG_BUILD_TYPE release): Debug links the release library
  set_target_properties(tensorflow-lite::tensorflowlite_c PROPERTIES MAP_IMPORTED_CONFIG_DEBUG Release)
endif()

set(tensorflow-lite_FOUND TRUE)
unset(_TFLITE_PREFIX)
unset(_TFLITE_LIBRARY_NAME)
