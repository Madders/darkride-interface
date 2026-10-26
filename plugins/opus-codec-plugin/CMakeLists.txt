#
#  The Opus audio codec plug-in, for the client and the audio mixer (src/OpusCodec.h). libopus comes from vcpkg.
#
#  Copyright 2026 Darkride.
#
#  Distributed under the MIT License.
#

set(TARGET_NAME opus-codec-plugin)
setup_hifi_client_server_plugin()
link_hifi_libraries(audio plugins)
target_link_libraries(${TARGET_NAME} Opus::opus)
if (BUILD_SERVER)
  install_beside_console()
endif ()
