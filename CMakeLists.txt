cmake_minimum_required(VERSION 3.24)
project(NovaStudio LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(NOVA_BUILD_TESTS "Build Nova Studio unit tests" ON)
option(NOVA_ENABLE_SANITIZERS "Build with ASan/UBSan enabled" OFF)

if(NOVA_ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets)
find_package(PkgConfig REQUIRED)

pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavcodec
    libavformat
    libavutil
    libswscale
    libswresample
)

# --- nova_core: logging + shared utilities, header-only for now ---
add_library(nova_core INTERFACE)
target_include_directories(nova_core INTERFACE ${CMAKE_SOURCE_DIR}/include)
target_compile_features(nova_core INTERFACE cxx_std_23)

# --- nova_timeline: track/clip data model, header-only for now ---
add_library(nova_timeline INTERFACE)
target_link_libraries(nova_timeline INTERFACE nova_core)

# --- nova_media: FFmpeg decode wrapper ---
add_library(nova_media STATIC
    src/media/Decoder.cpp
)
target_include_directories(nova_media PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(nova_media PUBLIC nova_core PkgConfig::FFMPEG)

# --- nova_renderer: Qt OpenGL preview widget + shader effect pipeline ---
add_library(nova_renderer STATIC
    src/renderer/VideoPreviewWidget.cpp
    include/nova/renderer/VideoPreviewWidget.h
)
target_include_directories(nova_renderer PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(nova_renderer PUBLIC nova_core nova_media Qt6::Widgets Qt6::OpenGLWidgets)

# --- nova_ui: application shell (MainWindow, dockable panels) ---
add_library(nova_ui STATIC
    src/ui/MainWindow.cpp
    src/ui/TimelineWidget.cpp
    include/nova/ui/MainWindow.h
    include/nova/ui/TimelineWidget.h
)
target_include_directories(nova_ui PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(nova_ui PUBLIC nova_core nova_timeline nova_media nova_renderer Qt6::Widgets Qt6::OpenGLWidgets)

# --- nova_studio: application executable ---
add_executable(nova_studio src/app/main.cpp)
target_link_libraries(nova_studio PRIVATE nova_ui Qt6::Widgets Qt6::OpenGLWidgets)

if(APPLE)
    set_target_properties(nova_studio PROPERTIES MACOSX_BUNDLE TRUE)
endif()

if(NOVA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(TARGETS nova_studio RUNTIME DESTINATION bin BUNDLE DESTINATION .)
