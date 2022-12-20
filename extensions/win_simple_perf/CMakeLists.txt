cmake_minimum_required(VERSION 3.21)

project(win_simple_perf)

add_library(win_simple_perf SHARED
  win_simple_perf.cpp
)

add_dependencies(win_simple_perf extension-api extension)
target_compile_features(win_simple_perf PRIVATE cxx_std_20)
target_link_libraries(win_simple_perf PRIVATE extension-api extension)

install(TARGETS win_simple_perf DESTINATION quasar/extensions)
