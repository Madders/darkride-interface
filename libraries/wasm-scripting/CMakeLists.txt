set(TARGET_NAME wasm-scripting)
setup_hifi_library(Gui)
link_hifi_libraries(shared)

target_wasmtime()
