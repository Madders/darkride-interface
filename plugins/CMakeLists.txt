#
#  Created by Bradley Austin Davis on 2015/10/25
#  Copyright 2015 High Fidelity, Inc.
#
#  Distributed under the Apache License, Version 2.0.
#  See the accompanying file LICENSE or http:#www.apache.org/licenses/LICENSE-2.0.html
#

# add the plugin directories
file(GLOB PLUGIN_SUBDIRS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/*")
list(REMOVE_ITEM PLUGIN_SUBDIRS "CMakeFiles")

# client-side plugins: gamepads, OpenVR and OpenXR headsets, and the Rift's own SDK where it is installed by hand
if (NOT SERVER_ONLY AND NOT ANDROID)
  if (EXISTS "${VCPKG_INSTALL_ROOT}/lib/LibOVR.lib")
    set(DIR "oculus-plugin")
    add_subdirectory(${DIR})
  endif()
  set(DIR "sdl2-plugin")
  add_subdirectory(${DIR})
  set(DIR "midi-plugin")
  add_subdirectory(${DIR})
  set(DIR "openvr-plugin")
  add_subdirectory(${DIR})
  set(DIR "openxr-plugin")
  add_subdirectory(${DIR})
  # webcam face tracking: Mac and Windows (TensorFlow Lite's port is built for those two; not Linux yet)
  if (APPLE OR WIN32)
    set(DIR "face-tracking-plugin")
    add_subdirectory(${DIR})
  endif()
endif()

# audio codecs, for the client and the servers
if (NOT DISABLE_PCM_CODEC)
  set(DIR "pcm-codec-plugin")
  add_subdirectory(${DIR})
else()
  message(STATUS "PCM codec disabled")
endif()

# Opus (opus-codec-plugin/src/OpusCodec.h), where vcpkg has built libopus; a build without it leaves the codec out
if (NOT DISABLE_OPUS_CODEC)
  find_package(Opus CONFIG QUIET)
  if (Opus_FOUND)
    set(DIR "opus-codec-plugin")
    add_subdirectory(${DIR})
  else()
    message(STATUS "Opus codec unavailable: no libopus in this build's vcpkg")
  endif()
else()
  message(STATUS "Opus codec disabled")
endif()
