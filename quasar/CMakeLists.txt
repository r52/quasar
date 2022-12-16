cmake_minimum_required(VERSION 3.21)

project(quasar VERSION 3.0.0)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_SOURCE_DIR})
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.4 COMPONENTS Core Gui Widgets Network NetworkAuth WebEngineCore WebEngineWidgets REQUIRED)
find_package(spdlog REQUIRED)
find_package(daw-json-link REQUIRED)
find_package(ZLIB REQUIRED)
find_library(USOCKETS_LIB uSockets)
find_path(UWEBSOCKETS_INCLUDE_DIRS "uwebsockets/App.h")
find_path(BSHOSHANY_THREAD_POOL_INCLUDE_DIRS "BS_thread_pool.hpp")

qt_standard_project_setup()

configure_file(version.h.in version.h @ONLY)

add_executable(quasar
  # Source
  main.cpp
  quasar.cpp
  widgets/widgetmanager.cpp
  widgets/quasarwidget.cpp
  widgets/widgetdefinition.cpp
  server/server.cpp
  server/protocol.cpp

  # UI files
  quasar.ui

  # Qt resources
  quasar.qrc

  # Windows rc
  quasar.rc
)

target_compile_features(quasar PRIVATE cxx_std_20)
target_include_directories(quasar PRIVATE ${UWEBSOCKETS_INCLUDE_DIRS})
target_include_directories(quasar PRIVATE ${BSHOSHANY_THREAD_POOL_INCLUDE_DIRS})
target_include_directories(quasar PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_include_directories(quasar PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/widgets"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/server"
)
#target_link_libraries(quasar quasar-extensionapi)
target_link_libraries(quasar PRIVATE common)
target_link_libraries(quasar PRIVATE spdlog::spdlog)
target_link_libraries(quasar PRIVATE daw::daw-json-link)
target_link_libraries(quasar PRIVATE ZLIB::ZLIB ${USOCKETS_LIB})
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
