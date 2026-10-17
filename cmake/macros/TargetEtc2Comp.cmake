#
#  Copyright 2018 High Fidelity, Inc.
#  Created by Sam Gondelman on 5/2/2018
#
#  Distributed under the Apache License, Version 2.0.
#  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
#
macro(TARGET_ETC2COMP)
    find_library(ETC2COMP_LIBRARY_DEBUG EtcLib PATHS ${VCPKG_INSTALL_ROOT}/debug/lib/ NO_DEFAULT_PATH)
    find_library(ETC2COMP_LIBRARY_RELEASE EtcLib PATHS ${VCPKG_INSTALL_ROOT}/lib/ NO_DEFAULT_PATH)
    select_library_configurations(ETC2COMP)
    target_link_libraries(${TARGET_NAME} ${ETC2COMP_LIBRARIES})
endmacro()

