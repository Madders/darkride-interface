#
#  Copyright 2015 High Fidelity, Inc.
#  Created by Bradley Austin Davis on 2015/10/10
#
#  Distributed under the Apache License, Version 2.0.
#  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
#
macro(TARGET_TBB)
    # using VCPKG for TBB
    find_package(TBB CONFIG REQUIRED)
    target_link_libraries(${TARGET_NAME} TBB::tbb)
endmacro()
