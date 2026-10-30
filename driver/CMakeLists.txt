include(${CMAKE_SOURCE_DIR}/cmake/BodySyncRc.cmake)
add_library(driver_bodysync SHARED driver_main.cpp)
set_target_properties(driver_bodysync PROPERTIES PREFIX "" OUTPUT_NAME "driver_bodysync"
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/stage/driver/bodysync/bin/win64)
target_include_directories(driver_bodysync PRIVATE ${TP}/openvr/headers)
target_link_libraries(driver_bodysync PRIVATE bodysync_core)
bodysync_add_rc(driver_bodysync "BodySync SteamVR driver" "driver_bodysync.dll")
# driver folder layout expected by SteamVR: <dir>/driver.vrdrivermanifest, bin/win64/driver_bodysync.dll, resources/...
add_custom_command(TARGET driver_bodysync POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/bodysync/driver.vrdrivermanifest ${CMAKE_BINARY_DIR}/stage/driver/bodysync/driver.vrdrivermanifest
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/bodysync/resources ${CMAKE_BINARY_DIR}/stage/driver/bodysync/resources)

# mock-host integration test for the driver (loads the built DLL; run from the build directory)
add_executable(DriverHarness ${CMAKE_SOURCE_DIR}/tests/test_main.cpp ${CMAKE_SOURCE_DIR}/tests/driver_harness.cpp)
target_include_directories(DriverHarness PRIVATE ${CMAKE_SOURCE_DIR}/tests ${TP}/openvr/headers)
target_link_libraries(DriverHarness PRIVATE bodysync_engine)
set_target_properties(DriverHarness PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_dependencies(DriverHarness driver_bodysync)
add_test(NAME driver_mock_host COMMAND DriverHarness)
