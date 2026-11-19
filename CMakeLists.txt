cmake_minimum_required(VERSION 3.16)

project(raycast-2d LANGUAGES CXX)

set(RAYCAST_2D_SOURCES
    src/app.cpp
    src/net_session.cpp
    src/console_ui.cpp
    src/camera.cpp
    src/main.cpp
    src/screen.cpp
    src/world_map.cpp
)

if (WIN32)
    list(APPEND RAYCAST_2D_SOURCES src/platform_terminal_windows.cpp)
else()
    list(APPEND RAYCAST_2D_SOURCES src/platform_terminal_posix.cpp)
endif()

add_executable(raycast-2d ${RAYCAST_2D_SOURCES})

add_custom_command(TARGET raycast-2d POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:raycast-2d>/assets"
    VERBATIM)

if(WIN32)
    target_link_libraries(raycast-2d PRIVATE ws2_32)
endif()
include(CTest)
if (BUILD_TESTING)
    add_executable(net_loopback tests/net_loopback.cpp src/net_session.cpp)
    target_compile_features(net_loopback PRIVATE cxx_std_17)
    target_include_directories(net_loopback PRIVATE include)
    if(WIN32)
        target_link_libraries(net_loopback PRIVATE ws2_32)
    endif()
    add_test(NAME net_loopback COMMAND net_loopback)
    add_executable(render_bench EXCLUDE_FROM_ALL tests/render_bench.cpp src/camera.cpp src/screen.cpp src/world_map.cpp)
    target_compile_features(render_bench PRIVATE cxx_std_17)
    target_include_directories(render_bench PRIVATE include)
    add_executable(render_scene_test tests/render_scene.cpp src/camera.cpp src/screen.cpp src/world_map.cpp)
    target_compile_features(render_scene_test PRIVATE cxx_std_17)
    target_include_directories(render_scene_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    add_test(NAME render_scene COMMAND render_scene_test)
    set_tests_properties(render_scene PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

target_compile_features(raycast-2d PRIVATE cxx_std_17)
target_include_directories(raycast-2d PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

if (MSVC)
    target_compile_options(raycast-2d PRIVATE /W4 /permissive-)
else()
    target_compile_options(raycast-2d PRIVATE -Wall -Wextra -Wpedantic)
endif()
