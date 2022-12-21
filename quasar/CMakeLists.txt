cmake_minimum_required(VERSION 3.21)

project(quasar VERSION 3.0.0)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_SOURCE_DIR})
set(CMAKE_AUTORCC ON)

find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(Qt6 6.4 COMPONENTS Core Gui Widgets Network NetworkAuth WebEngineCore WebEngineWidgets REQUIRED)

CPMFindPackage(
   NAME glaze
   GIT_REPOSITORY https://github.com/stephenberry/glaze
   GIT_TAG main
)

qt_standard_project_setup()

configure_file(version.h.in version.h @ONLY)

add_executable(quasar
  # Source
  main.cpp
  quasar.cpp
  widgets/widgetmanager.cpp
  widgets/quasarwidget.cpp

  # UI files
  quasar.ui

  # Qt resources
  quasar.qrc

  # Windows rc
  quasar.rc
)

 # Headers for integration
 target_sources(quasar PRIVATE
 FILE_SET HEADERS
   FILES widgets/widgetdefinition.h
)

add_dependencies(quasar common extension)
target_compile_features(quasar PRIVATE cxx_std_20)
target_include_directories(quasar PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_include_directories(quasar PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")
target_link_libraries(quasar PRIVATE common extension)
target_link_libraries(quasar PRIVATE fmt::fmt)
target_link_libraries(quasar PRIVATE spdlog::spdlog)
target_link_libraries(quasar PRIVATE glaze::glaze)
target_link_libraries(quasar PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::NetworkAuth Qt6::WebEngineCore Qt6::WebEngineWidgets)

# Copy libraries
add_custom_command(TARGET quasar POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:common> $<TARGET_FILE_DIR:quasar>
)
add_custom_command(TARGET quasar POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:extension> $<TARGET_FILE_DIR:quasar>
)

if(WIN32)
  add_custom_command(TARGET quasar POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:win_simple_perf> $<TARGET_FILE_DIR:quasar>/extensions/win_simple_perf.dll
  )
endif()

#add_custom_command(TARGET quasar POST_BUILD 
#    COMMAND Qt6::windeployqt
#    ARGS $<TARGET_FILE:quasar>
#)

install(TARGETS quasar DESTINATION quasar)
