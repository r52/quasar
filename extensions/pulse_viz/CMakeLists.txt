cmake_minimum_required(VERSION 3.23)

project(pulse_viz)

find_package(fmt CONFIG REQUIRED)
find_package(PulseAudio REQUIRED)

add_library(pulse_viz MODULE
  pulse_viz.cpp
)

add_dependencies(pulse_viz quasar)
target_compile_features(pulse_viz PRIVATE cxx_std_20)
target_compile_definitions(pulse_viz PRIVATE KFR_STD_COMPLEX)
target_link_libraries(pulse_viz PRIVATE kfr kfr_dft)
target_link_libraries(pulse_viz PRIVATE quasar extension-api)
target_link_libraries(pulse_viz PRIVATE fmt::fmt)
target_link_libraries(pulse_viz PRIVATE pulse-simple pulse)

install(TARGETS pulse_viz DESTINATION quasar/extensions)
