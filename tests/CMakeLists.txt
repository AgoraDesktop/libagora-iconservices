# libagora-iconservices' own suite.
#
# THE APPLICATION ICON'S FIXTURES ARE BUILT AT RUN TIME, NOT COMMITTED: the test writes an `Icons.icns`
# holding two PNG representations into a private copy of the fixture bundle, so the tree carries no binary
# asset and the `.icns` reader is measured against a container the test itself composed.

add_executable(iconservices iconservices.c)
target_compile_definitions(iconservices PRIVATE FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
target_link_libraries(iconservices PRIVATE IconServices CoreFoundation::CoreFoundation AgoraDrawing::Drawing AgoraLaunchServices::LaunchServices)

add_test(NAME iconservices COMMAND iconservices)

set(_agora_runtime_dirs
    "$<TARGET_FILE_DIR:IconServices>"
    "$<TARGET_FILE_DIR:CoreFoundation::CoreFoundation>"
    "$<TARGET_FILE_DIR:AgoraDrawing::Drawing>"
    "$<TARGET_FILE_DIR:AgoraLaunchServices::LaunchServices>")
list(REMOVE_DUPLICATES _agora_runtime_dirs)
string(REPLACE ";" ":" _agora_ld_path "${_agora_runtime_dirs}")
set_tests_properties(iconservices PROPERTIES ENVIRONMENT "LD_LIBRARY_PATH=${_agora_ld_path}")
