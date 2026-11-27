# =============================================================================
#  Lake Fishing 3D  -  CMake build script
#  C++17 + OpenGL (fixed function) + FreeGLUT
#
#  FreeGLUT is built from the vendored source in third_party/freeglut as a
#  static library, so the game has no runtime DLL dependency on GLUT.
#  Set -DLF_USE_SYSTEM_FREEGLUT=ON to use an installed FreeGLUT instead.
# =============================================================================
cmake_minimum_required(VERSION 3.16)
project(LakeFishing3D VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(LF_USE_SYSTEM_FREEGLUT "Use a system-installed FreeGLUT instead of third_party/freeglut" OFF)

find_package(OpenGL REQUIRED)

# ---------------------------------------------------------------- FreeGLUT --
if(WIN32 AND LF_USE_SYSTEM_FREEGLUT)
    message(FATAL_ERROR "Physical Arabic/English keys require the bundled FreeGLUT build.")
endif()
if(LF_USE_SYSTEM_FREEGLUT)
    find_package(GLUT REQUIRED)
    set(LF_GLUT_TARGET GLUT::GLUT)
else()
    # freeglut 3.6 declares an old cmake_minimum_required; allow it on CMake 4.x
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    set(FREEGLUT_BUILD_DEMOS        OFF CACHE BOOL "" FORCE)
    set(FREEGLUT_BUILD_SHARED_LIBS  OFF CACHE BOOL "" FORCE)
    set(FREEGLUT_BUILD_STATIC_LIBS  ON  CACHE BOOL "" FORCE)
    set(FREEGLUT_PRINT_WARNINGS     OFF CACHE BOOL "" FORCE)
    set(INSTALL_PDB                 OFF CACHE BOOL "" FORCE)
    add_subdirectory(third_party/freeglut EXCLUDE_FROM_ALL)
    if(WIN32)
        target_compile_definitions(freeglut_static PRIVATE LF_PHYSICAL_KEYS)
    endif()
    set(LF_GLUT_TARGET freeglut_static)
    set(LF_GLUT_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/freeglut/include)
endif()

# ------------------------------------------------------ Game logic library --
# Pure C++ game logic (no OpenGL calls).
add_library(lf_logic STATIC
    src/core/World.cpp
    src/game/Types.cpp
    src/game/Effects.cpp
    src/game/Fish.cpp
    src/game/FishingSystem.cpp
    src/game/Game.cpp
    src/game/AutoPlayer.cpp
)
target_compile_definitions(lf_logic PUBLIC $<$<CONFIG:Debug>:LF_DEVELOPMENT>)

target_include_directories(lf_logic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(MSVC)
    target_compile_options(lf_logic PRIVATE /W3)
    target_compile_definitions(lf_logic PUBLIC _CRT_SECURE_NO_WARNINGS NOMINMAX _USE_MATH_DEFINES)
else()
    target_compile_options(lf_logic PRIVATE -Wall -Wextra)
endif()

# ------------------------------------------------------------ Game program --
add_library(lf_progress STATIC src/app/SaveStore.cpp)
target_link_libraries(lf_progress PUBLIC lf_logic)
if(NOT MSVC)
    target_compile_options(lf_progress PRIVATE -Wall -Wextra)
endif()
add_executable(LakeFishing3D
    src/audio/SoundEffects.cpp
    src/app/Platform.cpp
    src/app/main.cpp
    src/app/Diagnostics.cpp
    src/app/DemoDirector.cpp
    src/render/Camera.cpp
    src/render/Primitives.cpp
    src/render/Textures.cpp
    src/render/Sky.cpp
    src/render/Terrain.cpp
    src/render/Pier.cpp
    src/render/WaterRenderer.cpp
    src/render/FishRenderer.cpp
    src/render/RodRenderer.cpp
    src/render/Renderer.cpp
    src/ui/HUD.cpp
    src/ui/UIFont.cpp
    src/ui/Artwork.cpp
)
target_link_libraries(LakeFishing3D PRIVATE lf_progress ${LF_GLUT_TARGET} OpenGL::GL)
if(WIN32)
    enable_language(RC)
    target_sources(LakeFishing3D PRIVATE assets/app/game.rc)
    set_source_files_properties(assets/app/game.rc PROPERTIES OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/app/fishlack.ico")
    target_include_directories(LakeFishing3D PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/assets/app)
    target_link_libraries(LakeFishing3D PRIVATE winmm shell32)
endif()
if(LF_GLUT_INCLUDE)
    target_include_directories(LakeFishing3D PRIVATE ${LF_GLUT_INCLUDE})
endif()
if(MINGW)
    # Static C++ runtime; the only MinGW DLL left (libwinpthread) is copied next
    # to the executable so it runs from Explorer without MinGW on the PATH.
    target_link_options(LakeFishing3D PRIVATE -static-libgcc -static-libstdc++)
    get_filename_component(LF_MINGW_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
    if(EXISTS "${LF_MINGW_BIN}/libwinpthread-1.dll")
        add_custom_command(TARGET LakeFishing3D POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${LF_MINGW_BIN}/libwinpthread-1.dll" "$<TARGET_FILE_DIR:LakeFishing3D>")
    endif()
    # Release builds are GUI applications (no console window).
    target_link_options(LakeFishing3D PRIVATE $<$<CONFIG:Release>:-mwindows>)
endif()
if(NOT MSVC)
    target_compile_options(LakeFishing3D PRIVATE -Wall -Wextra)
endif()
set_target_properties(LakeFishing3D PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Baked UI artwork is copied beside the executable; no image/font loader library.
add_custom_command(TARGET LakeFishing3D POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:LakeFishing3D>/assets")

# Re-stage artwork after regeneration even when C++ source did not change.
file(GLOB LF_FONT_ARTWORK CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/*" "${CMAKE_CURRENT_SOURCE_DIR}/assets/art/*" "${CMAKE_CURRENT_SOURCE_DIR}/assets/sfx/*.wav")
set_property(TARGET LakeFishing3D APPEND PROPERTY LINK_DEPENDS ${LF_FONT_ARTWORK})
