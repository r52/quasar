cmake_minimum_required(VERSION 3.21)

project(common)

set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 COMPONENTS Core REQUIRED)
find_package(spdlog REQUIRED)
qt_standard_project_setup()

add_library(common SHARED
  settings.cpp
  config.cpp
  log.cpp
  util.cpp
)

target_include_directories(common PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")
target_compile_definitions(common PUBLIC SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
target_compile_features(common PRIVATE cxx_std_20)
target_link_libraries(common PUBLIC spdlog::spdlog)
target_link_libraries(common PUBLIC Qt6::Core)

install(TARGETS common DESTINATION quasar)
