vcpkg_download_distfile(
	OVR_ARCHIVE
	URLS https://cdn.tivolicloud.com/dependencies/ovr_sdk_win_1.43.0.zip
	SHA512 7bee0adffbbe7decfdd69ffc289a48fd2ecfa52b0368448c0902f9267033a0049e4a2acdb739b1fbd371f4cdcf0b2faa0f83c9471cc3cc9ca5b6110a1b784e97
	FILENAME ovr_sdk_win_1.43.0.zip
)

vcpkg_extract_source_archive_ex(
	OUT_SOURCE_PATH OVR_SOURCE_PATH
	ARCHIVE ${OVR_ARCHIVE}
	NO_REMOVE_ONE_LEVEL
)

file(COPY ${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt DESTINATION ${OVR_SOURCE_PATH})

vcpkg_configure_cmake(
    SOURCE_PATH ${OVR_SOURCE_PATH}
)

vcpkg_install_cmake()
vcpkg_copy_pdbs()

file(GLOB OVR_HEADER_FILES ${OVR_SOURCE_PATH}/LibOVR/Include/*.h)
file(COPY ${OVR_HEADER_FILES} DESTINATION ${CURRENT_PACKAGES_DIR}/include/)

file(GLOB OVR_EXTRA_HEADER_FILES ${OVR_SOURCE_PATH}/LibOVR/Include/Extras/*.h)  
file(COPY ${OVR_EXTRA_HEADER_FILES} DESTINATION ${CURRENT_PACKAGES_DIR}/include/Extras/)

file(COPY ${OVR_SOURCE_PATH}/LICENSE.txt
	DESTINATION ${CURRENT_PACKAGES_DIR}/share/ovr/copyright
)
