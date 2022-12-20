cmake_minimum_required(VERSION 3.21)

project(extension)

set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 COMPONENTS Core REQUIRED)
find_package(spdlog REQUIRED)
find_package(daw-json-link REQUIRED)
find_package(ZLIB REQUIRED)
find_library(USOCKETS_LIB uSockets)
find_path(UWEBSOCKETS_INCLUDE_DIRS "uwebsockets/App.h")
find_path(BSHOSHANY_THREAD_POOL_INCLUDE_DIRS "BS_thread_pool.hpp")

qt_standard_project_setup()

# API files
add_library(extension-api INTERFACE)

target_sources(extension-api INTERFACE
  FILE_SET HEADERS
    BASE_DIRS api
    FILES api/extension_api.h api/extension_types.h api/extension_support.h
)

# Actual lib
add_library(extension SHARED
  extension.cpp
  extension_support.cpp

  server/server.cpp
  server/protocol.cpp
)

add_dependencies(extension common)
target_include_directories(extension PRIVATE ${UWEBSOCKETS_INCLUDE_DIRS})
target_include_directories(extension PRIVATE ${BSHOSHANY_THREAD_POOL_INCLUDE_DIRS})
target_include_directories(extension PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")
# Replace with jsoncons vcpkg lib when VS2022 compile bugs fixed
target_include_directories(extension PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/../include")
target_compile_features(extension PRIVATE cxx_std_20)
target_link_libraries(extension PRIVATE common extension-api)
target_link_libraries(extension PRIVATE daw::daw-json-link)
target_link_libraries(extension PRIVATE ZLIB::ZLIB ${USOCKETS_LIB})
target_link_libraries(extension PRIVATE spdlog::spdlog)
target_link_libraries(extension PRIVATE Qt6::Core)


install(TARGETS extension-api FILE_SET HEADERS DESTINATION quasar/include)
install(TARGETS extension DESTINATION quasar)
