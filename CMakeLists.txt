cmake_minimum_required(VERSION 3.21)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "Vcpkg toolchain file")

set(CMAKE_ENABLE_EXPORTS ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

project(quasar-project)

# include(cmake/CPM.cmake)

set(default_build_type "Release")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
        STRING "Choose the type of build." FORCE)

    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Tracy options
option(TRACY_ENABLE "" OFF)
option(TRACY_ON_DEMAND "" ON)
option(TRACY_STATIC "" OFF)

# Quasar options
option(BUILD_SAMPLE_EXTENSIONS "Build sample extensions (Windows only)" ON)
option(BUILD_SPOTIFY_API "Build Spotify API extension (optional)" ON)

add_subdirectory(3rdparty/tracy)

add_subdirectory(quasar)

if(WIN32 AND BUILD_SAMPLE_EXTENSIONS)
    add_subdirectory(extensions/win_simple_perf)

    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        option(KFR_ENABLE_DFT_MULTIARCH "" ON)
        add_subdirectory(3rdparty/kfr)
        add_subdirectory(extensions/win_audio_viz)
    endif()
endif()

if (BUILD_SPOTIFY_API)
    add_subdirectory(extensions/quasar-spotify-api)
endif()
