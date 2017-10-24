cmake_minimum_required(VERSION 3.9)

project(quasar)

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    message(FATAL_ERROR "This CMakeLists.txt is not configured for non-Linux machines! Please Use the Visual Studio solution on windows")
endif(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Linux")

set(default_build_type "Release")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
        STRING "Choose the type of build." FORCE)

    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    message("DEBUG BUILD")
else()
    add_definitions(-DNDEBUG)
endif()

add_subdirectory(plugin-api)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_SOURCE_DIR})
set(CMAKE_AUTORCC ON)

find_package(Qt5 COMPONENTS Widgets Network WebSockets WebEngineCore WebEngineWidgets REQUIRED)
find_package(Git)
find_package(Threads REQUIRED)

execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --always
    OUTPUT_VARIABLE QUASAR_GIT_VER
    OUTPUT_STRIP_TRAILING_WHITESPACE)

configure_file(src/version.h.in version.h @ONLY)

file(GLOB SOURCES "src/*.cpp")

add_executable(quasar ${SOURCES} quasar.qrc)
target_include_directories(quasar PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(quasar PUBLIC cxx_std_17)
target_link_libraries(quasar plugin-api)
target_link_libraries(quasar Threads::Threads)
target_link_libraries(quasar Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network Qt5::WebSockets Qt5::WebEngineCore Qt5::WebEngineWidgets)
