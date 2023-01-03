cmake_minimum_required(VERSION 3.21)

project(win_simple_perf)

find_package(fmt REQUIRED)

add_library(win_simple_perf MODULE
  win_simple_perf.cpp
)

add_dependencies(win_simple_perf quasar)
target_compile_features(win_simple_perf PRIVATE cxx_std_20)
target_link_libraries(win_simple_perf PRIVATE fmt::fmt)
target_link_libraries(win_simple_perf PRIVATE quasar extension-api)

add_custom_command(TARGET win_simple_perf POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:win_simple_perf> $<TARGET_FILE_DIR:quasar>/extensions/win_simple_perf.dll
)

install(TARGETS win_simple_perf DESTINATION quasar/extensions)
