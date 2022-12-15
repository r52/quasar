cmake_minimum_required(VERSION 3.21)

project(quasar VERSION 3.0.0)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_SOURCE_DIR})
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.4 COMPONENTS Core Gui Widgets Network NetworkAuth WebEngineCore WebEngineWidgets REQUIRED)
find_package(spdlog REQUIRED)
find_package(daw-json-link REQUIRED)
qt_standard_project_setup()

configure_file(version.h.in version.h @ONLY)

add_executable(quasar
  # Source
  main.cpp
  quasar.cpp
  server.cpp
  widgetmanager.cpp
  quasarwidget.cpp
  widgetdefinition.cpp

  # UI files
  quasar.ui

  # Qt resources
  quasar.qrc

  # Windows rc
  quasar.rc
)
target_include_directories(quasar PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(quasar PRIVATE cxx_std_20)
#target_link_libraries(quasar quasar-extensionapi)
target_link_libraries(quasar PRIVATE common)
target_link_libraries(quasar PRIVATE spdlog::spdlog)
target_link_libraries(quasar PRIVATE daw::daw-json-link)
target_link_libraries(quasar PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::NetworkAuth Qt6::WebEngineCore Qt6::WebEngineWidgets)

# Copy library
add_custom_command(TARGET quasar POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:common> $<TARGET_FILE_DIR:quasar>
)

#add_custom_command(TARGET quasar POST_BUILD 
#    COMMAND Qt6::windeployqt
#    ARGS $<TARGET_FILE:quasar>
#)

install(TARGETS quasar DESTINATION quasar)
