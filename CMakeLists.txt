cmake_minimum_required(VERSION 3.23)

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

if (TRACY_ENABLE)
    add_subdirectory(3rdparty/tracy)
endif()

add_subdirectory(quasar)

if(BUILD_SAMPLE_EXTENSIONS)
    if(WIN32)
        add_subdirectory(extensions/win_simple_perf)

        if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            option(KFR_ENABLE_DFT_MULTIARCH "" ON)
            add_subdirectory(3rdparty/kfr EXCLUDE_FROM_ALL)
            add_subdirectory(extensions/win_audio_viz)
        else()
            message("quasar: win_audio_viz extension requires Clang. Skip building win_audio_viz extension.")
        endif()
    elseif(LINUX)
        find_package(PulseAudio QUIET)
        if(PULSEAUDIO_FOUND)
            message("quasar: PulseAudio found")
            if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
                set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
                set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
                add_subdirectory(3rdparty/kfr EXCLUDE_FROM_ALL)
                add_subdirectory(extensions/pulse_viz)
            else()
                message("quasar: pulse_viz extension requires Clang. Skip building pulse_viz extension.")
            endif()
        else()
            message("quasar: PulseAudio devel packages not found. Try installing libpulse-dev on Ubuntu/Debian. Skip building pulse_viz extension.")
        endif()
    endif()
endif()

if(BUILD_SPOTIFY_API)
    add_subdirectory(extensions/quasar-spotify-api)
endif()

if(WIN32)
    add_subdirectory(updater)
endif()
