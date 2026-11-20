cmake_minimum_required(VERSION 3.16)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(raycast-2d LANGUAGES CXX)

# CMake 4.x generators no longer inject /EHsc for MSVC, so unwind semantics must be requested explicitly
# (C4530 otherwise fires on every std::filesystem / std::vector instantiation).
if (MSVC)
    add_compile_options(/EHsc)
endif()

set(asset_root "${CMAKE_CURRENT_SOURCE_DIR}/assets")
# Release allowlist: legacy maps and atlases remain on disk for regression tests.
set(asset_files
    "${asset_root}/game.cfg"
    "${asset_root}/maps/drowned_light.map"
    "${asset_root}/textures/drowned_light.atlas"
    "${asset_root}/story/keeper.story")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${asset_files})
set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
include(cmake/embed_assets.cmake)

set(world_sources src/world_map.cpp src/game_resource.cpp)

set(RAYCAST_2D_SOURCES
    src/app.cpp
    src/net_session.cpp
    src/console_ui.cpp
    src/hud_ui.cpp
    src/story_runtime.cpp
    src/camera.cpp
    src/main.cpp
    src/screen.cpp
    ${world_sources}
)

if (WIN32)
    list(APPEND RAYCAST_2D_SOURCES src/platform_terminal_windows.cpp)
else()
    list(APPEND RAYCAST_2D_SOURCES src/platform_terminal_posix.cpp)
endif()

add_executable(raycast-2d ${RAYCAST_2D_SOURCES})

if(WIN32)
    target_link_libraries(raycast-2d PRIVATE ws2_32 user32 gdi32 winmm)
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
    add_executable(render_bench EXCLUDE_FROM_ALL tests/render_bench.cpp src/camera.cpp src/screen.cpp ${world_sources})
    target_compile_features(render_bench PRIVATE cxx_std_17)
    target_include_directories(render_bench PRIVATE include "${generated_dir}")
    add_executable(render_scene_test tests/render_scene.cpp src/camera.cpp src/screen.cpp ${world_sources} src/hud_ui.cpp src/story_runtime.cpp)
    target_compile_features(render_scene_test PRIVATE cxx_std_17)
    target_include_directories(render_scene_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include "${generated_dir}")
    if (MSVC)
        target_compile_options(render_scene_test PRIVATE /utf-8)
        target_compile_options(render_bench PRIVATE /utf-8)
    endif()
    add_test(NAME render_scene COMMAND render_scene_test)
    set_tests_properties(render_scene PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

target_compile_features(raycast-2d PRIVATE cxx_std_17)
target_include_directories(raycast-2d PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include "${generated_dir}")

if (MSVC)
    target_compile_options(raycast-2d PRIVATE /W4 /permissive- /utf-8 $<$<CONFIG:Release>:/GL /Gy /Gw>)
    target_link_options(raycast-2d PRIVATE "/SUBSYSTEM:WINDOWS" "/ENTRY:mainCRTStartup" $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF /INCREMENTAL:NO /DEBUG:NONE>)
else()
    target_compile_options(raycast-2d PRIVATE -Wall -Wextra -Wpedantic)
endif()
