#
#  Copyright 2015 High Fidelity, Inc.
#  Created by Bradley Austin Davis on 2015/10/10
#
#  Distributed under the Apache License, Version 2.0.
#  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
#
macro(TARGET_NVTT)
    find_library(NVTT_LIBRARY_RELEASE nvtt PATHS ${VCPKG_INSTALL_ROOT}/lib NO_DEFAULT_PATH)
    find_library(NVTT_LIBRARY_DEBUG nvtt PATHS ${VCPKG_INSTALL_ROOT}/debug/lib NO_DEFAULT_PATH)
    select_library_configurations(NVTT)
    target_link_libraries(${TARGET_NAME} ${NVTT_LIBRARIES})
    if ((NOT WIN32) AND (NOT ANDROID) AND (NOT APPLE))
        find_package(OpenMP)
        target_link_libraries(${TARGET_NAME} OpenMP::OpenMP_C OpenMP::OpenMP_CXX)
    endif()
endmacro()
