#
#  Copyright 2026 Darkride.
#
#  Distributed under the MIT License.
#  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
#

# TensorFlow Lite (LiteRT) for webcam face tracking (plugins/face-tracking-plugin): its C API with the XNNPACK
# delegate, built from TensorFlow's source and installed as ONE static library (libtensorflowlite_c.a, or
# tensorflowlite_c.lib on Windows) with the C API's headers. Found with find_package(tensorflow-lite CONFIG)
# (tensorflow-lite-config.cmake; cmake/macros/TargetTflite.cmake). Compiled with hidden symbols, so nothing of it is
# exported from the plugin that links it.
#
# TensorFlow Lite's CMake fetches its dependencies while it configures. Here vcpkg downloads each one first and checks
# it against its SHA512, the configure gets those folders and runs disconnected, so nothing else is fetched. The pins
# are TensorFlow 2.21.0's own (tensorflow/lite/tools/cmake/modules, and XNNPACK's cmake/Download*.cmake at its pinned
# commit): change them only together with TensorFlow's version.

vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO tensorflow/tensorflow
    REF v2.21.0
    SHA512 0bf30434cf0aa72f9ceda444e51cae76bd8af9b1cc38538060b5cae9365424e5610ee95a1c7ea225c6db6e461525b800d8302839a31fd034541863603001ffcf
    HEAD_REF master
    PATCHES
        # the benchmark tool's protos need protobuf even when the tool isn't built
        benchmark-only-when-wanted.patch
        # model_building.h befriends Tensor before declaring it. MSVC looks past the innermost namespace and binds that
        # friend to the schema's tflite::Tensor (schema_generated.h), so model_builder::Tensor can't read Buffer's
        # builder_ (C2248). Declared first here, as Graph and InterpreterInfo already are
        msvc-friend-tensor.patch
)

# <name> <GitHub repository> <commit or tag> <SHA512 of GitHub's archive of it>
set(TFLITE_DEPENDENCIES
    abseil-cpp abseil/abseil-cpp d38452e1ee03523a208362186fd42248ff2609f6
        46cbe0b6bbb699873d150082232b37e6e326e9be722209a8e4892d36957da4297ef88affd1a0ac712d69b1a63b39242acc1c60ddea020cbd360d8066c9ea18b3
    cpuinfo pytorch/cpuinfo 8a9210069b5a37dd89ed118a783945502a30a4ae
        fa7db1c8bcb3a3573976406029b5504e7ea3e30e5f3091f30b68e5a4657294b3b70b74592d743a28691dd14735a579ea9155f1ec62e345134a131848329794ce
    farmhash google/farmhash 0d859a811870d10f53a594927d0d0b97573ad06d
        7bc14931e488464c1cedbc17551fb90a8cec494d0e0860db9df8efff09000fd8d91e01060dd5c5149b1104ac4ac8bf7eb57e5b156b05ef42636938edad1518f1
    fft2d petewarden/OouraFFT v1.0
        89c6e8fd57abf26351b3efb792008a1bbe62d404a4225dcae8aa666b3782a421be071bdc9760ebb0c95b5336ee5ea517d2fa43ab915045f7cf6fd76e73578079
    flatbuffers google/flatbuffers v25.9.23
        259ae6c0b024c19c882d87c93d6ba156c15f14a61b11846170ac1b9e9c051cd3e80ae93cfe20ccb1aa30f2085cdbd4127ffa229b42cabbfed6b035ca4851c127
    fp16_headers Maratyszcza/FP16 0a92994d729ff76a58f692d3028ca1b64b145d91
        3f094f242425ea37de274eb8539dc5f8ab0c13fd5325d14180ef12e9c04e6002a110d086c4c667f7c8054af337deab096d59482eb95cc8a632c3c412b48e89d1
    fxdiv Maratyszcza/FXdiv b408327ac2a15ec3e43352421954f5b1967701d1
        d46d7c1437eccf8dc5ab5e51b0641d190839451ab059c8a24a220a9cf05a84a66b5d08c3ff730622efe491152a8378a2c726fd9abad19a3df98db40d6a13e4b9
    gemmlowp google/gemmlowp 16e8662c34917be0065110bfcd9cc27d30f52fdf
        d020c49c830f35e2568c3829d9b746285c835d3688fa77a7f08faa1bbe51e2e0fad34d5f4336f7c3fb86bc8a0afc869f34ced5464f7b3a120f3d0d34eb485ba1
    kleidiai ARM-software/kleidiai 63205aa90afa6803d8f58bc3081b69288e9f1906
        0aadcfbe9bd174fd4420a64a121d0bca86b6684b42ac810ac8a561d351bb13a2b24e601c822fcd9b814c3334c1e80c33cc222b717be8466013caf9f1d2906129
    ml_dtypes jax-ml/ml_dtypes 00d98cd92ade342fef589c0470379abb27baebe9
        de81cef9ec016fb46193314b292c0c19a2bf371b4de16ec9ca7355a5f9b2b8801a02ba53adb9048b8d4c45ce25004a71cf343074d92fbedb9fed74c9b263477d
    neon2sse intel/ARM_NEON_2_x86_SSE a15b489e1222b2087007546b4912e21293ea86ff
        1d9a4192baf05be8cd36ffa55b9bf97e09ab6f714b303bd2ca7b16bbffbb661475c11b130437cafe234fce66d046d7b041f7daf9c25263c5921584412a5c1c12
    pthreadpool google/pthreadpool 0e6ca13779b57d397a5ba6bfdcaa8a275bc8ea2e
        08ed5b56474c8dd507045dbc9e0139adcbcdd782a4e4948443995fe2d4e0421a1943ed578a1de7d39c958fa87d5d1a2b064a6a0c03a73f50a2401395a2eb9a20
    ruy google/ruy 3286a34cc8de6149ac6844107dfdffac91531e72
        af8280bcecabdaebe34d274afeb1bd3f8d39934e4b56c5c74b1d0af146a9334c01400b400c722eb8d6b6b75a7f93852be6904a733dc0ae72605f0dfe585e1373
    xnnpack google/XNNPACK 25b42dfddb0ee22170d73ff0d4b333ea1e6edfeb
        f3eb44dff4e6235116a2d8ec32e6ddf24417763fc7cfac9dbf3e4f58f90f6cf5c2b50fb9ec42df707cb22f63cf99ee7c9b53d8a8c68ecadca1fef8681bfd79e0
)
set(TFLITE_DEPENDENCY_OPTIONS)
set(TFLITE_DEPENDENCY_NAMES)
list(LENGTH TFLITE_DEPENDENCIES TFLITE_DEPENDENCY_FIELDS)
math(EXPR TFLITE_LAST_DEPENDENCY "${TFLITE_DEPENDENCY_FIELDS} / 4 - 1")
foreach(INDEX RANGE ${TFLITE_LAST_DEPENDENCY})
    math(EXPR FIRST "${INDEX} * 4")
    math(EXPR SECOND "${FIRST} + 1")
    math(EXPR THIRD "${FIRST} + 2")
    math(EXPR FOURTH "${FIRST} + 3")
    list(GET TFLITE_DEPENDENCIES ${FIRST} NAME)
    list(GET TFLITE_DEPENDENCIES ${SECOND} REPO)
    list(GET TFLITE_DEPENDENCIES ${THIRD} REF)
    list(GET TFLITE_DEPENDENCIES ${FOURTH} HASH)
    vcpkg_from_github(OUT_SOURCE_PATH DEPENDENCY_PATH REPO ${REPO} REF ${REF} SHA512 ${HASH})
    set(${NAME}_PATH "${DEPENDENCY_PATH}")
    list(APPEND TFLITE_DEPENDENCY_NAMES ${NAME})
    string(TOUPPER "${NAME}" UPPER_NAME)
    list(APPEND TFLITE_DEPENDENCY_OPTIONS "-DFETCHCONTENT_SOURCE_DIR_${UPPER_NAME}=${DEPENDENCY_PATH}")
endforeach()
vcpkg_from_gitlab(
    OUT_SOURCE_PATH eigen_PATH
    GITLAB_URL https://gitlab.com
    REPO libeigen/eigen
    REF dcbaf2d608f306450f1e74949eb87e9a22a7ef4b
    SHA512 6e024122c5494ff97eb5098b8d027003d3813a4b197ab3d9699de7a42a22fe29d6f73e03817db780fcb2a342a2dfc088a954c2b118985a11a2c3752ca58cf415
)
list(APPEND TFLITE_DEPENDENCY_NAMES eigen)
list(APPEND TFLITE_DEPENDENCY_OPTIONS "-DFETCHCONTENT_SOURCE_DIR_EIGEN=${eigen_PATH}")

# TFL_STATIC_LIBRARY_BUILD: the C API without dllexport or default visibility; with the hidden presets below, the
# library exports nothing (the plugin that links it shows only its own entry points)
string(APPEND VCPKG_C_FLAGS " -DTFL_STATIC_LIBRARY_BUILD")
string(APPEND VCPKG_CXX_FLAGS " -DTFL_STATIC_LIBRARY_BUILD")

# XNNPACK's configure writes its list of kernels with a Python script (tools/update-microkernels.py). vcpkg builds
# with a cleaned PATH, so on Windows no Python is found unless it is given: vcpkg's own (python.org's embeddable one).
# The Mac finds its own python3.
set(TFLITE_PYTHON_OPTIONS)
if (VCPKG_TARGET_IS_WINDOWS)
    vcpkg_find_acquire_program(PYTHON3)
    get_filename_component(PYTHON3_DIR "${PYTHON3}" DIRECTORY)
    vcpkg_add_to_path("${PYTHON3_DIR}")
    set(TFLITE_PYTHON_OPTIONS "-DPython_EXECUTABLE=${PYTHON3}")
endif()

vcpkg_configure_cmake(
    SOURCE_PATH "${SOURCE_PATH}/tensorflow/lite/c"
    PREFER_NINJA
    OPTIONS
        -DTENSORFLOW_SOURCE_DIR=${SOURCE_PATH}
        -DTFLITE_C_BUILD_SHARED_LIBS=OFF
        -DTFLITE_ENABLE_XNNPACK=ON
        -DTFLITE_ENABLE_GPU=OFF
        -DTFLITE_ENABLE_NNAPI=OFF
        -DTFLITE_ENABLE_EXTERNAL_DELEGATE=OFF
        -DCMAKE_C_VISIBILITY_PRESET=hidden
        -DCMAKE_CXX_VISIBILITY_PRESET=hidden
        -DCMAKE_VISIBILITY_INLINES_HIDDEN=ON
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DFETCHCONTENT_FULLY_DISCONNECTED=ON
        ${TFLITE_DEPENDENCY_OPTIONS}
        # XNNPACK's and pthreadpool's own downloads (each is skipped when its folder is given)
        -DCPUINFO_SOURCE_DIR=${cpuinfo_PATH}
        -DFP16_SOURCE_DIR=${fp16_headers_PATH}
        -DPTHREADPOOL_SOURCE_DIR=${pthreadpool_PATH}
        -DFXDIV_SOURCE_DIR=${fxdiv_PATH}
        -DKLEIDIAI_SOURCE_DIR=${kleidiai_PATH}
        -DXNNPACK_BUILD_TESTS=OFF
        -DXNNPACK_BUILD_BENCHMARKS=OFF
        ${TFLITE_PYTHON_OPTIONS}
)

# Only the C API and what it links: everything else in TensorFlow Lite's tree (tools, examples) stays unbuilt
vcpkg_build_cmake(TARGET tensorflowlite_c)

# One library from the C API's static library and those of its dependencies: every archive the target's build made
function(tflite_merge_libraries BUILD_DIR OUTPUT_DIR)
    file(MAKE_DIRECTORY "${OUTPUT_DIR}")
    if (VCPKG_TARGET_IS_WINDOWS)
        file(GLOB_RECURSE ARCHIVES LIST_DIRECTORIES false "${BUILD_DIR}/*.lib")
        file(STRINGS "${BUILD_DIR}/CMakeCache.txt" AR_LINE REGEX "^CMAKE_AR:FILEPATH=")
        string(REGEX REPLACE "^CMAKE_AR:FILEPATH=" "" LIB_TOOL "${AR_LINE}")
        set(OUTPUT "${OUTPUT_DIR}/tensorflowlite_c.lib")
        set(MERGE_COMMAND "${LIB_TOOL}" /NOLOGO "/OUT:${OUTPUT}" ${ARCHIVES})
    elseif (VCPKG_TARGET_IS_OSX)
        file(GLOB_RECURSE ARCHIVES LIST_DIRECTORIES false "${BUILD_DIR}/*.a")
        set(OUTPUT "${OUTPUT_DIR}/libtensorflowlite_c.a")
        set(MERGE_COMMAND /usr/bin/libtool -static -no_warning_for_no_symbols -o "${OUTPUT}" ${ARCHIVES})
    else()
        message(FATAL_ERROR "tensorflow-lite: no library merge for this system")
    endif()
    list(LENGTH ARCHIVES ARCHIVE_COUNT)
    if (ARCHIVE_COUNT EQUAL 0)
        message(FATAL_ERROR "tensorflow-lite: no libraries in ${BUILD_DIR}")
    endif()
    string(REPLACE ";" "\n" ARCHIVE_LIST "${ARCHIVES}")
    file(WRITE "${BUILD_DIR}/merged-libraries.txt" "${ARCHIVE_LIST}\n")
    message(STATUS "Merging ${ARCHIVE_COUNT} libraries into ${OUTPUT}")
    get_filename_component(LOG_NAME "${BUILD_DIR}" NAME)
    vcpkg_execute_required_process(
        COMMAND ${MERGE_COMMAND}
        WORKING_DIRECTORY "${BUILD_DIR}"
        LOGNAME "merge-${LOG_NAME}"
    )
endfunction()

if (NOT DEFINED VCPKG_BUILD_TYPE OR VCPKG_BUILD_TYPE STREQUAL "release")
    tflite_merge_libraries("${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel" "${CURRENT_PACKAGES_DIR}/lib")
endif()
if (NOT DEFINED VCPKG_BUILD_TYPE OR VCPKG_BUILD_TYPE STREQUAL "debug")
    tflite_merge_libraries("${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-dbg" "${CURRENT_PACKAGES_DIR}/debug/lib")
endif()

# The C API's headers and the XNNPACK delegate's: exactly what they include (checked with clang -M at 2.21.0)
foreach(HEADER
        tensorflow/compiler/mlir/lite/core/c/builtin_op_data.h
        tensorflow/compiler/mlir/lite/core/c/tflite_types.h
        tensorflow/lite/builtin_ops.h
        tensorflow/lite/c/builtin_op_data.h
        tensorflow/lite/c/c_api.h
        tensorflow/lite/c/c_api_experimental.h
        tensorflow/lite/c/c_api_opaque.h
        tensorflow/lite/c/c_api_types.h
        tensorflow/lite/c/common.h
        tensorflow/lite/core/async/c/types.h
        tensorflow/lite/core/c/builtin_op_data.h
        tensorflow/lite/core/c/c_api.h
        tensorflow/lite/core/c/c_api_experimental.h
        tensorflow/lite/core/c/c_api_opaque.h
        tensorflow/lite/core/c/c_api_types.h
        tensorflow/lite/core/c/common.h
        tensorflow/lite/core/c/operator.h
        tensorflow/lite/delegates/xnnpack/xnnpack_delegate.h)
    get_filename_component(HEADER_DIR "${HEADER}" DIRECTORY)
    file(INSTALL "${SOURCE_PATH}/${HEADER}" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${HEADER_DIR}")
endforeach()

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/tensorflow-lite-config.cmake" DESTINATION "${CURRENT_PACKAGES_DIR}/share/tensorflow-lite")

# The licences of everything in the library, for the third-party notices: TensorFlow's first, then each dependency's
set(NOTICES "${CURRENT_BUILDTREES_DIR}/copyright")
file(READ "${SOURCE_PATH}/LICENSE" TEXT)
file(WRITE "${NOTICES}" "TensorFlow Lite (LiteRT), from TensorFlow 2.21.0 (https://github.com/tensorflow/tensorflow)\n\n${TEXT}")
set(abseil-cpp_LICENSE LICENSE)
set(cpuinfo_LICENSE LICENSE)
set(eigen_LICENSE COPYING.MPL2)
set(farmhash_LICENSE COPYING)
set(fft2d_LICENSE readme.txt)
set(flatbuffers_LICENSE LICENSE)
set(fp16_headers_LICENSE LICENSE)
set(fxdiv_LICENSE LICENSE)
set(gemmlowp_LICENSE LICENSE)
set(kleidiai_LICENSE LICENSES/Apache-2.0.txt)
set(ml_dtypes_LICENSE LICENSE)
set(neon2sse_LICENSE LICENSE)
set(pthreadpool_LICENSE LICENSE)
set(ruy_LICENSE LICENSE)
set(xnnpack_LICENSE LICENSE)
foreach(NAME ${TFLITE_DEPENDENCY_NAMES})
    file(READ "${${NAME}_PATH}/${${NAME}_LICENSE}" TEXT)
    file(APPEND "${NOTICES}" "\n\n==== ${NAME} (${${NAME}_LICENSE}) ====\n\n${TEXT}")
endforeach()
file(INSTALL "${NOTICES}" DESTINATION "${CURRENT_PACKAGES_DIR}/share/tensorflow-lite")
