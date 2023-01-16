cmake_minimum_required(VERSION 3.21)

project(quasar VERSION 3.0.0)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_SOURCE_DIR})
set(CMAKE_AUTORCC ON)

find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(jsoncons CONFIG REQUIRED)
find_package(libuv CONFIG REQUIRED)
find_library(USOCKETS_LIB_RELEASE NAMES uSockets PATHS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib" NO_DEFAULT_PATH)
find_library(USOCKETS_LIB_DEBUG   NAMES uSockets PATHS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/lib" NO_DEFAULT_PATH)
find_path(UWEBSOCKETS_INCLUDE_DIRS "uwebsockets/App.h")
find_path(BSHOSHANY_THREAD_POOL_INCLUDE_DIRS "BS_thread_pool.hpp")
find_package(Qt6 6.4 CONFIG COMPONENTS Core Gui Widgets Network NetworkAuth Svg WebEngineCore WebEngineWidgets REQUIRED)

#CPMFindPackage(
#   NAME glaze
#   GIT_REPOSITORY https://github.com/stephenberry/glaze
#   GIT_TAG main
#)

qt_standard_project_setup()

configure_file(version.h.in version.h @ONLY)

# API files
add_library(extension-api INTERFACE)

target_sources(extension-api INTERFACE
  FILE_SET HEADERS
    BASE_DIRS api
    FILES api/extension_api.h api/extension_types.h api/extension_support.h api/extension_support.hpp
)

add_executable(quasar WIN32
  # Source
  main.cpp
  quasar.cpp
  widgets/widgetmanager.cpp
  widgets/quasarwidget.cpp

  extension/extension.cpp
  extension/extension_support.cpp

  server/server.cpp

  common/settings.cpp
  common/config.cpp
  common/log.cpp
  common/util.cpp

  internal/applauncher.cpp

  config/configdialog.cpp
  config/launchereditdialog.cpp
  config/extensionpage.cpp

  # UI files
  quasar.ui
  config/configdialog.ui
  config/launchereditdialog.ui
  config/extensionpage.ui

  # Qt resources
  quasar.qrc

  # Windows rc
  quasar.rc
)

 # Headers for integration
 target_sources(quasar PRIVATE
 FILE_SET HEADERS
   FILES widgets/widgetdefinition.h common/timer.h
)

target_compile_features(quasar PRIVATE cxx_std_20)
target_compile_definitions(quasar PRIVATE SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE JSONCONS_HAS_STD_SPAN JSONCONS_HAS_STD_ENDIAN)

target_include_directories(quasar PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_include_directories(quasar PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")
target_include_directories(quasar PRIVATE ${UWEBSOCKETS_INCLUDE_DIRS})
target_include_directories(quasar PRIVATE ${BSHOSHANY_THREAD_POOL_INCLUDE_DIRS})

target_link_libraries(quasar PUBLIC $<$<CONFIG:RelWithDebInfo>:Tracy::TracyClient>)
target_link_libraries(quasar PRIVATE extension-api)
target_link_libraries(quasar PRIVATE fmt::fmt spdlog::spdlog)
target_link_libraries(quasar PRIVATE jsoncons)
target_link_libraries(quasar PRIVATE ZLIB::ZLIB $<IF:$<TARGET_EXISTS:uv_a>,uv_a,uv> debug ${USOCKETS_LIB_DEBUG} optimized ${USOCKETS_LIB_RELEASE})
target_link_libraries(quasar PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::NetworkAuth Qt6::Svg Qt6::WebEngineCore Qt6::WebEngineWidgets)

add_custom_command(TARGET quasar POST_BUILD
  COMMAND $<$<CONFIG:RelWithDebInfo>:${CMAKE_COMMAND}> -E copy $<$<CONFIG:RelWithDebInfo>:$<TARGET_FILE:Tracy::TracyClient>> $<TARGET_FILE_DIR:quasar>
)

if(WIN32)
  add_custom_command(TARGET quasar POST_BUILD
    COMMAND Qt6::windeployqt
    ARGS $<TARGET_FILE:quasar>
  )
endif()

install(TARGETS extension-api FILE_SET HEADERS DESTINATION quasar/include)
install(TARGETS quasar DESTINATION quasar)
