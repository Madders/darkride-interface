
# Declare dependencies
macro (setup_testcase_dependencies)
  link_hifi_libraries(shared wasm-scripting test-utils)
  package_libraries_for_deployment()
endmacro ()

# Script: QtScriptOverloadTests checks how the QtScript engine picks between the overloads this engine adds
setup_hifi_testcase(Script)
