cmake_minimum_required(VERSION 3.21)

project(quasar-project)

set(default_build_type "Release")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
        STRING "Choose the type of build." FORCE)

    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

#set(CMAKE_SKIP_BUILD_RPATH FALSE)
#set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
#set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/quasar")
#set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

add_subdirectory(common)
add_subdirectory(extension-api)

if(WIN32)
    add_subdirectory(extensions/win_simple_perf)
endif()

add_subdirectory(quasar)
