cmake_minimum_required(VERSION 3.21)

project(extension)

set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 COMPONENTS Core REQUIRED)
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(ZLIB REQUIRED)
find_library(USOCKETS_LIB uSockets)
find_path(UWEBSOCKETS_INCLUDE_DIRS "uwebsockets/App.h")
find_path(JSONCONS_INCLUDE_DIRS "jsoncons/json.hpp" PATHS ../include)
find_path(BSHOSHANY_THREAD_POOL_INCLUDE_DIRS "BS_thread_pool.hpp")

CPMFindPackage(
   NAME glaze
   GIT_REPOSITORY https://github.com/stephenberry/glaze
   GIT_TAG main
)

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
)

target_sources(extension PUBLIC
  FILE_SET HEADERS
    FILES server/protocol.h
)

add_dependencies(extension common)
target_include_directories(extension PRIVATE ${UWEBSOCKETS_INCLUDE_DIRS})
target_include_directories(extension PRIVATE ${BSHOSHANY_THREAD_POOL_INCLUDE_DIRS})
# Replace with jsoncons vcpkg lib when VS2022 compile bugs fixed
target_include_directories(extension PRIVATE ${JSONCONS_INCLUDE_DIRS})
target_include_directories(extension PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")
target_compile_features(extension PRIVATE cxx_std_20)
target_link_libraries(extension PRIVATE common extension-api)
target_link_libraries(extension PRIVATE fmt::fmt)
target_link_libraries(extension PRIVATE glaze::glaze)
target_link_libraries(extension PUBLIC ZLIB::ZLIB ${USOCKETS_LIB})
target_link_libraries(extension PRIVATE spdlog::spdlog)
target_link_libraries(extension PRIVATE Qt6::Core)


install(TARGETS extension-api FILE_SET HEADERS DESTINATION quasar/include)
install(TARGETS extension DESTINATION quasar)
