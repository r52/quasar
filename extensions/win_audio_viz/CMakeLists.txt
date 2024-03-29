cmake_minimum_required(VERSION 3.23)

project(win_audio_viz)

find_package(fmt CONFIG REQUIRED)

add_library(win_audio_viz MODULE
  win_audio_viz.cpp
)

add_dependencies(win_audio_viz quasar)
target_compile_features(win_audio_viz PRIVATE cxx_std_20)
target_compile_definitions(win_audio_viz PRIVATE KFR_STD_COMPLEX NOMINMAX)

if (TRACY_ENABLE)
  target_link_libraries(win_audio_viz PRIVATE Tracy::TracyClient)
endif()

target_link_libraries(win_audio_viz PRIVATE kfr kfr_dft)
target_link_libraries(win_audio_viz PRIVATE quasar extension-api)
target_link_libraries(win_audio_viz PRIVATE fmt::fmt)

add_custom_command(TARGET win_audio_viz POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:win_audio_viz> $<TARGET_FILE_DIR:quasar>/extensions/$<TARGET_FILE_NAME:win_audio_viz>
)

install(TARGETS win_audio_viz DESTINATION quasar/extensions)
