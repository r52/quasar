cmake_minimum_required(VERSION 3.9)

project(quasar-extensionapi)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 COMPONENTS Core WebSockets REQUIRED)

set(SOURCES
    dataextension.cpp
    extension_support.cpp)

add_library(quasar-extensionapi SHARED ${SOURCES})
target_compile_definitions(quasar-extensionapi PRIVATE PLUGINAPI_LIB=1)
target_compile_features(quasar-extensionapi PUBLIC cxx_std_17)
target_link_libraries(quasar-extensionapi Qt5::Core Qt5::WebSockets)
target_include_directories(quasar-extensionapi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

install(TARGETS quasar-extensionapi DESTINATION quasar)
