vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO KhronosGroup/SPIRV-Tools
    REF v2018.5
    SHA512 068a39e15111f24ad2a6b27e7ada786b3124b239aa8b13e187a4d512044db57a8e6a0fccadd0451155e1f57c96c8dec91a2338996c59fc883007cf7be07f2cad
    HEAD_REF master
)

vcpkg_from_github(
    OUT_SOURCE_PATH SPIRV_HEADERS_PATH
    REPO KhronosGroup/SPIRV-Headers
    REF 801cca8104245c07e8cc53292da87ee1b76946fe
    SHA512 2bfc37beec1f6afb565fa7dad08eb838c8fe4bacda7f80a3a6c75d80c7eb1caea7e7716dc1da11b337b723a0870700d524c6617c5b7cab8b28048fa8c0785ba9
    HEAD_REF master
)

# v2018.5's generator scripts use what newer Pythons removed: the 'rU' file mode (3.11) and distutils (3.12)
file(READ "${SOURCE_PATH}/utils/update_build_version.py" _contents)
string(REPLACE "mode='rU'" "mode='r'" _contents "${_contents}")
file(WRITE "${SOURCE_PATH}/utils/update_build_version.py" "${_contents}")
file(READ "${SOURCE_PATH}/utils/generate_registry_tables.py" _contents)
string(REPLACE "import distutils.dir_util" "" _contents "${_contents}")
string(REPLACE "distutils.dir_util.mkpath(os.path.dirname(args.generator_output))"
    "os.makedirs(os.path.dirname(args.generator_output) or '.', exist_ok=True)" _contents "${_contents}")
file(WRITE "${SOURCE_PATH}/utils/generate_registry_tables.py" "${_contents}")

vcpkg_find_acquire_program(PYTHON3)
get_filename_component(PYTHON3_DIR "${PYTHON3}" DIRECTORY)
vcpkg_add_to_path("${PYTHON3_DIR}")

vcpkg_configure_cmake(
    SOURCE_PATH ${SOURCE_PATH}
    PREFER_NINJA
    OPTIONS
        -DSPIRV-Headers_SOURCE_DIR=${SPIRV_HEADERS_PATH}
        -DSPIRV_WERROR=OFF
)

vcpkg_install_cmake()

file(REMOVE_RECURSE ${CURRENT_PACKAGES_DIR}/debug/include)
file(GLOB EXES "${CURRENT_PACKAGES_DIR}/bin/*")
file(COPY ${EXES} DESTINATION ${CURRENT_PACKAGES_DIR}/tools)
file(REMOVE ${EXES})
file(REMOVE_RECURSE ${CURRENT_PACKAGES_DIR}/bin ${CURRENT_PACKAGES_DIR}/debug/bin)

# Handle copyright
file(COPY ${SOURCE_PATH}/LICENSE DESTINATION ${CURRENT_PACKAGES_DIR}/share/spirv-tools-windows)
file(RENAME ${CURRENT_PACKAGES_DIR}/share/spirv-tools-windows/LICENSE ${CURRENT_PACKAGES_DIR}/share/spirv-tools-windows/copyright)
