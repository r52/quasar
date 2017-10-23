cmake_minimum_required(VERSION 3.9)

project(quasar)

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    message(FATAL_ERROR "This CMakeLists.txt is not configured for non-Linux machines! Please Use the Visual Studio solution on windows")
endif(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Linux")

add_subdirectory(plugin-api)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_SOURCE_DIR})

find_package(Qt5 COMPONENTS Widgets Network WebSockets WebEngine WebEngineWidgets REQUIRED)
find_package(Git)
find_package(Threads REQUIRED)

execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --always
	OUTPUT_VARIABLE QUASAR_GIT_VER
	OUTPUT_STRIP_TRAILING_WHITESPACE)

configure_file(src/version.h.in version.h @ONLY)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

file(GLOB SOURCES "src/*.cpp")

add_executable(quasar ${SOURCES} ../quasar.qrc)

target_compile_features(quasar PUBLIC cxx_std_17)
target_link_libraries(quasar plugin-api)
target_link_libraries(quasar Threads::Threads)
target_link_libraries(quasar Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network Qt5::WebSockets Qt5::WebEngineCore Qt5::WebEngineWidgets)
