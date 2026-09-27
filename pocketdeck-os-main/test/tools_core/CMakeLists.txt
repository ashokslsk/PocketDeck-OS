add_executable(ToolsCoreTest
  ToolsCoreTest.cpp
  ${REPO_ROOT}/src/activities/tools/ToolsStore.cpp
  ${REPO_ROOT}/src/activities/tools/ToolsLog.cpp
)

target_include_directories(ToolsCoreTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/src/activities/tools
)

target_link_libraries(ToolsCoreTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(ToolsCoreTest)
