cmake_minimum_required(VERSION 3.9)

project(quasar-pluginapi)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 COMPONENTS Core WebSockets REQUIRED)

set(SOURCES
    dataplugin.cpp
    plugin_support.cpp)

add_library(quasar-pluginapi SHARED ${SOURCES})
target_compile_definitions(quasar-pluginapi PRIVATE PLUGINAPI_LIB=1)
target_compile_features(quasar-pluginapi PUBLIC cxx_std_17)
target_link_libraries(quasar-pluginapi Qt5::Core Qt5::WebSockets)
target_include_directories(quasar-pluginapi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

install(TARGETS quasar-pluginapi DESTINATION quasar)
