cmake_minimum_required(VERSION 3.9)

project(plugin-api)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 COMPONENTS Core WebSockets REQUIRED)

set(SOURCES
    dataplugin.cpp
    plugin_support.cpp)

target_include_directories(plugin-api PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(plugin-api SHARED ${SOURCES})
target_compile_definitions(plugin-api PRIVATE PLUGINAPI_LIB=1)
target_compile_features(plugin-api PUBLIC cxx_std_17)
target_link_libraries(plugin-api Qt5::Core Qt5::WebSockets)
