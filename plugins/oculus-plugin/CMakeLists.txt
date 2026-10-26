#
#  Created by Bradley Austin Davis on 2015/10/25
#  Copyright 2015 High Fidelity, Inc.
#
#  Distributed under the Apache License, Version 2.0.
#  See the accompanying file LICENSE or http:#www.apache.org/licenses/LICENSE-2.0.html
#

if (WIN32 AND (NOT USE_GLES))


  set(TARGET_NAME oculus-plugin)
  setup_hifi_plugin(Multimedia)
  link_hifi_libraries(
    shared task gl shaders gpu ${PLATFORM_GL_BACKEND} controllers ui qml
    plugins ui-plugins display-plugins input-plugins
    audio-client networking render-utils graphics
    ${PLATFORM_GL_BACKEND}
  )
  include_hifi_library_headers(octree)

  find_library(OVR_LIBRARY_RELEASE LibOVR PATHS ${VCPKG_INSTALL_ROOT}/lib NO_DEFAULT_PATH)
  find_library(OVR_LIBRARY_DEBUG LibOVR PATHS ${VCPKG_INSTALL_ROOT}/debug/lib NO_DEFAULT_PATH)
  select_library_configurations(OVR)
  target_include_directories(${TARGET_NAME} PRIVATE ${VCPKG_INSTALL_ROOT}/include)
  target_link_libraries(${TARGET_NAME} ${OVR_LIBRARIES})
  target_link_libraries(${TARGET_NAME} Winmm.lib)

endif()
