cmake_minimum_required(VERSION 3.9)

project(quasar)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_SOURCE_DIR})
set(CMAKE_AUTORCC ON)

find_package(Qt5 5.12 COMPONENTS Core Gui Widgets Network NetworkAuth WebSockets WebEngine WebEngineCore WebEngineWidgets REQUIRED)
find_package(Git)
find_package(Threads REQUIRED)

execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --always
    OUTPUT_VARIABLE QUASAR_GIT_VER
    OUTPUT_STRIP_TRAILING_WHITESPACE)

configure_file(version.h.in version.h @ONLY)

file(GLOB SOURCES "*.cpp")

add_executable(quasar ${SOURCES} quasar.qrc)
target_include_directories(quasar PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(quasar PUBLIC cxx_std_17)
target_link_libraries(quasar quasar-extensionapi)
target_link_libraries(quasar Threads::Threads)
target_link_libraries(quasar Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network Qt5::NetworkAuth Qt5::WebSockets Qt5::WebEngine Qt5::WebEngineCore Qt5::WebEngineWidgets)

install(TARGETS quasar DESTINATION quasar)
