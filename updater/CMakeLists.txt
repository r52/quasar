project(updater)

find_package(minizip-ng CONFIG REQUIRED)
find_package(Qt6 6.4 CONFIG COMPONENTS Core REQUIRED)

add_executable(updater main.cpp)

target_compile_features(updater PRIVATE cxx_std_20)
target_link_libraries(updater PRIVATE MINIZIP::minizip-ng)
target_link_libraries(updater PRIVATE Qt6::Core)

add_custom_command(TARGET updater POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:updater> $<TARGET_FILE_DIR:quasar>
)
