
# Declare dependencies
macro (setup_testcase_dependencies)
  link_hifi_libraries(shared test-utils shaders octree gpu graphics fbx networking entities avatars audio animation model-networking wasm-scripting script-engine)
  include_hifi_library_headers(hfm)
  include_hifi_library_headers(image)
  include_hifi_library_headers(ktx)
  include_hifi_library_headers(material-networking)
  include_hifi_library_headers(procedural)
  package_libraries_for_deployment()
endmacro ()

# EditFilterTests runs the same edit filters over the same edits on the QtScript engine and on the WebAssembly one
setup_hifi_testcase(Script Network)
