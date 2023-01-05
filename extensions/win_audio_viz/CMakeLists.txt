cmake_minimum_required(VERSION 3.21)

project(win_audio_viz)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

find_package(fmt REQUIRED)

add_library(kissfft STATIC
  deps/kissfft/kiss_fft.c
  deps/kissfft/kiss_fftr.c
)

add_library(win_audio_viz MODULE
  win_audio_viz.cpp
)

target_include_directories(kissfft PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/deps")

add_dependencies(win_audio_viz quasar)
target_compile_features(win_audio_viz PRIVATE cxx_std_20)
target_link_libraries(win_audio_viz PRIVATE kissfft)
target_link_libraries(win_audio_viz PRIVATE quasar extension-api)
target_link_libraries(win_audio_viz PRIVATE fmt::fmt)


add_custom_command(TARGET win_audio_viz POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:win_audio_viz> $<TARGET_FILE_DIR:quasar>/extensions/win_audio_viz.dll
)

install(TARGETS win_audio_viz DESTINATION quasar/extensions)
