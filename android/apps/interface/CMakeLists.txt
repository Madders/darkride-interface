set(TARGET_NAME native-lib)
setup_hifi_library()
link_hifi_libraries(shared task networking gl gpu qml image fbx hfm render-utils physics entities octree ${PLATFORM_GL_BACKEND})
target_opengl()

set(INTERFACE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../interface")
add_subdirectory("${INTERFACE_DIR}" "libraries/interface")
include_directories("${INTERFACE_DIR}/src")

# Voice: the open PCM/zlib codec. The HiFi codec is a closed SDK whose Android build came from
# cdn.tivolicloud.com, which is gone. On Android, PluginManager loads libplugins_lib*.so from the app's
# library folder.
set(PCM_CODEC_PLUGIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../plugins/pcm-codec-plugin")
add_subdirectory("${PCM_CODEC_PLUGIN_DIR}" "libraries/pcmCodecPlugin")
set_target_properties(pcm-codec-plugin PROPERTIES OUTPUT_NAME "plugins_libpcm-codec-plugin")

target_link_libraries(native-lib android log m interface)
target_include_directories(native-lib PRIVATE "libraries/ui/src")
target_link_libraries(native-lib ui)
add_dependencies(native-lib pcm-codec-plugin)
