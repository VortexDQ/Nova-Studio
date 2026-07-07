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

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets Multimedia)
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

# --- nova_project: project file I/O, autosave, backups ---
add_library(nova_project STATIC
    src/project/Project.cpp
    src/project/ProjectIO.cpp
    src/project/ProjectStore.cpp
)
target_include_directories(nova_project PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(nova_project PUBLIC nova_core nova_timeline Qt6::Core)

# --- nova_media: FFmpeg decode wrapper ---
add_library(nova_media STATIC
    src/media/Decoder.cpp
    src/media/AudioExtractor.cpp
    src/media/MediaProbe.cpp
    src/media/VideoExporter.cpp
    src/media/StockAssetGenerator.cpp
    src/media/ImageIO.cpp
    src/media/MediaRecorder.cpp
    src/media/AudioExporter.cpp
    src/media/GifExporter.cpp
    include/nova/media/MediaRecorder.h
)
target_include_directories(nova_media PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(nova_media PUBLIC nova_core PkgConfig::FFMPEG Qt6::Gui Qt6::Multimedia)

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
    src/ui/SidebarPanel.cpp
    src/ui/ProjectWelcomeDialog.cpp
    src/ui/PreviewOverlay.cpp
    include/nova/ui/MainWindow.h
    include/nova/ui/TimelineWidget.h
    include/nova/ui/SidebarPanel.h
    include/nova/ui/ProjectWelcomeDialog.h
    include/nova/ui/PreviewOverlay.h
)
target_include_directories(nova_ui PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(nova_ui PUBLIC nova_core nova_timeline nova_project nova_media nova_renderer Qt6::Widgets Qt6::OpenGLWidgets Qt6::Multimedia)

# --- nova_studio: application executable ---
add_executable(nova_studio src/app/main.cpp)
target_link_libraries(nova_studio PRIVATE nova_ui Qt6::Widgets Qt6::OpenGLWidgets Qt6::Multimedia)

if(APPLE)
    set_target_properties(nova_studio PROPERTIES MACOSX_BUNDLE TRUE)
endif()

if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin" "${Qt6_DIR}/../../bin")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET nova_studio POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                    --no-translations
                    --no-system-d3d-compiler
                    "$<TARGET_FILE:nova_studio>"
            COMMENT "Deploying Qt runtime plugins for nova_studio")
    endif()
endif()

if(NOVA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(TARGETS nova_studio RUNTIME DESTINATION bin BUNDLE DESTINATION .)
install(DIRECTORY templates/ DESTINATION share/nova-studio/templates)
