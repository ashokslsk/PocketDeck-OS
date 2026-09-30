add_executable(ToolsCoreTest
  ToolsCoreTest.cpp
  ${REPO_ROOT}/src/activities/tools/ToolsStore.cpp
  ${REPO_ROOT}/src/activities/tools/ToolsLog.cpp
  ${REPO_ROOT}/src/activities/tools/KannadaFont.cpp
  ${REPO_ROOT}/src/activities/tools/PanchangaFestivals.cpp
  ${REPO_ROOT}/src/activities/tools/MantraLibrary.cpp
)

target_include_directories(ToolsCoreTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/lib/Memory
  ${REPO_ROOT}/src/activities/tools
)

target_compile_definitions(ToolsCoreTest PRIVATE REPO_ROOT_DIR="${REPO_ROOT}")

target_link_libraries(ToolsCoreTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(ToolsCoreTest)
