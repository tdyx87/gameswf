cmake_minimum_required(VERSION 3.16)
project(gameswf VERSION 1.0.0 LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)

# Build options
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(GAMESWF_BUILD_PLAYER "Build gameswf_test_ogl player" ON)
option(GAMESWF_ENABLE_SOUND "Enable sound support via SDL_mixer" ON)
option(GAMESWF_ENABLE_FREETYPE "Enable FreeType for font rendering" ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find required packages
find_package(OpenGL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)

# SDL2 - try different methods
find_package(SDL2 QUIET)

# Check if SDL2 was found (either by find_package or Conan)
if(SDL2_FOUND)
    message(STATUS "SDL2 found via find_package")
    set(CONAN_SDL2_TARGET SDL2_all_configs)
    # Create a wrapper target that works in all configs
    if(NOT TARGET SDL2_all_configs)
        add_library(SDL2_all_configs STATIC IMPORTED)
        set_target_properties(SDL2_all_configs PROPERTIES
            IMPORTED_LOCATION_RELEASE "F:/conan/p/sdl18a0310ccc696/p/lib/SDL2.lib"
            IMPORTED_LOCATION_DEBUG "F:/conan/p/sdl18a0310ccc696/p/lib/SDL2.lib"
            IMPORTED_LOCATION_RELWITHDEBINFO "F:/conan/p/sdl18a0310ccc696/p/lib/SDL2.lib"
            IMPORTED_LOCATION_MINSIZEREL "F:/conan/p/sdl18a0310ccc696/p/lib/SDL2.lib"
            INTERFACE_INCLUDE_DIRECTORIES "F:/conan/p/sdl18a0310ccc696/p/include;F:/conan/p/sdl18a0310ccc696/p/include/SDL2"
            INTERFACE_LINK_LIBRARIES "F:/conan/p/sdl18a0310ccc696/p/lib/SDL2main.lib;user32.lib;gdi32.lib;imm32.lib;ole32.lib;oleaut32.lib;version.lib;uuid.lib;advapi32.lib;setupapi.lib;shell32.lib"
        )
    endif()
    set(SDL2_INCLUDE_DIRS "F:/conan/p/sdl18a0310ccc696/p/include;F:/conan/p/sdl18a0310ccc696/p/include/SDL2")
endif()

if(NOT SDL2_FOUND)
    # Try pkg-config fallback
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL2 QUIET sdl2)
    endif()
endif()

# If still not found, try manual search
if(NOT SDL2_FOUND)
    find_path(SDL2_INCLUDE_DIR SDL.h
        PATHS 
            ${CMAKE_PREFIX_PATH}/include
            $ENV{SDL2_DIR}/include
        PATH_SUFFIXES SDL2
    )
    find_library(SDL2_LIBRARY SDL2
        PATHS 
            ${CMAKE_PREFIX_PATH}/lib
            $ENV{SDL2_DIR}/lib
    )
    if(SDL2_INCLUDE_DIR AND SDL2_LIBRARY)
        set(SDL2_FOUND TRUE)
        set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIR})
        set(SDL2_LIBRARIES ${SDL2_LIBRARY})
    endif()
endif()

if(NOT SDL2_FOUND AND NOT CONAN_SDL2_TARGET)
    # Try to find SDL2 in common locations
    find_path(SDL2_INCLUDE_DIR SDL.h
        PATHS 
            "C:/SDL2/include"
            "C:/SDL2-2.0/include"
            "C:/Program Files/SDL2/include"
            "C:/Program Files (x86)/SDL2/include"
            "${CMAKE_SOURCE_DIR}/../SDL2/include"
        PATH_SUFFIXES SDL2
    )
    find_library(SDL2_LIBRARY SDL2
        PATHS 
            "C:/SDL2/lib"
            "C:/SDL2-2.0/lib"
            "C:/Program Files/SDL2/lib"
            "C:/Program Files (x86)/SDL2/lib"
            "${CMAKE_SOURCE_DIR}/../SDL2/lib"
    )
    find_library(SDL2_MAIN_LIBRARY SDL2main
        PATHS 
            "C:/SDL2/lib"
            "C:/SDL2-2.0/lib"
            "C:/Program Files/SDL2/lib"
            "C:/Program Files (x86)/SDL2/lib"
            "${CMAKE_SOURCE_DIR}/../SDL2/lib"
    )
    if(SDL2_INCLUDE_DIR AND SDL2_LIBRARY)
        set(SDL2_FOUND TRUE)
        set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIR})
        set(SDL2_LIBRARIES ${SDL2_LIBRARY} ${SDL2_MAIN_LIBRARY})
        message(STATUS "SDL2 found manually: ${SDL2_INCLUDE_DIR}")
    endif()
endif()

if(NOT SDL2_FOUND AND NOT CONAN_SDL2_TARGET)
    message(WARNING "SDL2 not found! Building without SDL2 support.")
    # Force build player even without SDL2 for testing
    # set(GAMESWF_BUILD_PLAYER OFF)
    set(GAMESWF_ENABLE_SOUND OFF)
else()
    add_definitions(-DHAVE_SDL2=1)
endif()

if(GAMESWF_ENABLE_SOUND)
    # Try Conan's CMake config first
    find_package(SDL2_mixer QUIET)
    if(TARGET SDL2_mixer::SDL2_mixer)
        set(SDL2_MIXER_FOUND TRUE)
        add_definitions(-DGAMESWF_HAVE_SDL_MIXER=1)
        message(STATUS "SDL_mixer found via CMake config")
    else()
        # Try pkg-config fallback
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(SDL2_MIXER sdl2_mixer)
        endif()
        if(SDL2_MIXER_FOUND)
            add_definitions(-DGAMESWF_HAVE_SDL_MIXER=1)
        endif()
    endif()
endif()

if(GAMESWF_ENABLE_FREETYPE)
    find_package(Freetype)
    if(FREETYPE_FOUND)
        add_definitions(-DHAVE_FREETYPE=1)
    endif()
endif()

# Platform-specific definitions
if(WIN32)
    add_definitions(-DWIN32=1 -D_WIN32=1 -D_CRT_SECURE_NO_WARNINGS)
    set(PLATFORM_LIBS ws2_32 winmm opengl32 glu32)
elseif(UNIX)
    add_definitions(-DUNIX=1)
    set(PLATFORM_LIBS m dl pthread)
endif()

# Compiler warnings and flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wno-unused-variable
        -Wno-unused-but-set-variable
        -Wno-deprecated-declarations
        -Wno-sign-compare
        -Wno-narrowing
        -fPIC
    )
elseif(MSVC)
    add_compile_options(
        /W3
        /wd4244
        /wd4267
        /wd4305
        /wd4996
    )
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${SDL2_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
    ${ZLIB_INCLUDE_DIRS}
    ${PNG_INCLUDE_DIRS}
    ${JPEG_INCLUDE_DIR}
)

# Define Z_SOLO to avoid unistd.h on Windows
if(WIN32)
    add_definitions(-DZ_SOLO)
endif()

if(FREETYPE_FOUND)
    include_directories(${FREETYPE_INCLUDE_DIRS})
endif()

if(SDL2_MIXER_FOUND)
    include_directories(${SDL2_MIXER_INCLUDE_DIRS})
endif()

# Base library sources
set(BASE_SOURCES
    base/sdl2_compat.cpp
    base/configvars.cpp
    base/container.cpp
    base/demo.cpp
    base/ear_clip_triangulate_float.cpp
    base/ear_clip_triangulate_sint16.cpp
    base/file_util.cpp
    base/image.cpp
    base/image_filters.cpp
    base/jpeg.cpp
    base/logger.cpp
    base/membuf.cpp
    base/ogl.cpp
    base/png_helper.cpp
    base/postscript.cpp
    base/triangulate_float.cpp
    base/triangulate_sint32.cpp
    base/tu_file.cpp
    base/tu_file_SDL.cpp
    base/tu_gc_singlethreaded_marksweep.cpp
    base/tu_loadlib.cpp
    base/tu_random.cpp
    base/tu_timer.cpp
    base/tu_types.cpp
    base/utf8.cpp
    base/utility.cpp
    base/zlib_adapter.cpp
    base/lzma_adapter.cpp
    base/lzma/LzmaDec.c
    base/lzma/Alloc.c
)

# Only include dlmalloc on non-Windows platforms or when not using SDL2
if(NOT WIN32)
    list(APPEND BASE_SOURCES base/dlmalloc.c)
endif()

# GameSWF library sources
set(GAMESWF_SOURCES
    gameswf/gameswf_abc.cpp
    gameswf/gameswf_action.cpp
    gameswf/gameswf_as_sprite.cpp
    gameswf/gameswf_avm2.cpp
    gameswf/gameswf_button.cpp
    gameswf/gameswf_canvas.cpp
    gameswf/gameswf_character.cpp
    gameswf/gameswf_disasm.cpp
    gameswf/gameswf_dlist.cpp
    gameswf/gameswf_environment.cpp
    gameswf/gameswf_filters.cpp
    gameswf/gameswf_font.cpp
    gameswf/gameswf_fontlib.cpp
    gameswf/gameswf_freetype.cpp
    gameswf/gameswf_function.cpp
    gameswf/gameswf_impl.cpp
    gameswf/gameswf_listener.cpp
    gameswf/gameswf_log.cpp
    gameswf/gameswf_morph2.cpp
    gameswf/gameswf_movie_def.cpp
    gameswf/gameswf_mutex.cpp
    gameswf/gameswf_object.cpp
    gameswf/gameswf_parser.cpp
    gameswf/gameswf_player.cpp
    gameswf/gameswf_render.cpp
    gameswf/gameswf_render_handler_ogl.cpp
    gameswf/gameswf_root.cpp
    gameswf/gameswf_shape.cpp
    gameswf/gameswf_sound.cpp
    gameswf/gameswf_sprite.cpp
    gameswf/gameswf_sprite_def.cpp
    gameswf/gameswf_stream.cpp
    gameswf/gameswf_styles.cpp
    gameswf/gameswf_tesselate.cpp
    gameswf/gameswf_text.cpp
    gameswf/gameswf_tools.cpp
    gameswf/gameswf_types.cpp
    gameswf/gameswf_value.cpp
    gameswf/gameswf_video_impl.cpp
    # AS3 built-in classes
    gameswf/gameswf_as3_classes.cpp
    gameswf/gameswf_as3_class_impl.cpp
    gameswf/gameswf_as3_net_media_ui_impl.cpp
    gameswf/gameswf_as3_remaining_impl.cpp
    # ActionScript classes
    gameswf/gameswf_as_classes/as_array.cpp
    gameswf/gameswf_as_classes/as_boolean.cpp
    gameswf/gameswf_as_classes/as_broadcaster.cpp
    gameswf/gameswf_as_classes/as_class.cpp
    gameswf/gameswf_as_classes/as_color.cpp
    gameswf/gameswf_as_classes/as_color_transform.cpp
    gameswf/gameswf_as_classes/as_date.cpp
    gameswf/gameswf_as_classes/as_event.cpp
    gameswf/gameswf_as_classes/as_flash.cpp
    gameswf/gameswf_as_classes/as_geom.cpp
    gameswf/gameswf_as_classes/as_global.cpp
    gameswf/gameswf_as_classes/as_graphics.cpp
    gameswf/gameswf_as_classes/as_loader_info.cpp
    gameswf/gameswf_as_classes/as_timer.cpp
    gameswf/gameswf_as_classes/as_key.cpp
    gameswf/gameswf_as_classes/as_loadvars.cpp
    gameswf/gameswf_as_classes/as_math.cpp
    gameswf/gameswf_as_classes/as_matrix.cpp
    gameswf/gameswf_as_classes/as_mcloader.cpp
    gameswf/gameswf_as_classes/as_mouse.cpp
    gameswf/gameswf_as_classes/as_mouse_event.cpp
    gameswf/gameswf_as_classes/as_netconnection.cpp
    gameswf/gameswf_as_classes/as_netstream.cpp
    gameswf/gameswf_as_classes/as_number.cpp
    gameswf/gameswf_as_classes/as_point.cpp
    gameswf/gameswf_as_classes/as_selection.cpp
    gameswf/gameswf_as_classes/as_sharedobject.cpp
    gameswf/gameswf_as_classes/as_sound.cpp
    gameswf/gameswf_as_classes/as_string.cpp
    gameswf/gameswf_as_classes/as_textformat.cpp
    gameswf/gameswf_as_classes/as_transform.cpp
    gameswf/gameswf_as_classes/as_xml.cpp
    gameswf/gameswf_as_classes/as_xmlsocket.cpp
    gameswf/gameswf_embed.cpp
)

# Add sound handler - use winmm on Windows, SDL on other platforms
if(WIN32)
    list(APPEND GAMESWF_SOURCES gameswf/gameswf_sound_handler_winmm.cpp)
    set(GAMESWF_HAVE_WINMM_SOUND TRUE)
    message(STATUS "Using Windows waveOut for sound")
elseif(SDL2_MIXER_FOUND)
    list(APPEND GAMESWF_SOURCES gameswf/gameswf_sound_handler_sdl.cpp)
endif()

# Create base library
add_library(base ${BASE_SOURCES})
if(CONAN_SDL2_TARGET)
    target_link_libraries(base PUBLIC ${CONAN_SDL2_TARGET})
else()
    target_link_libraries(base PUBLIC ${SDL2_LIBRARIES})
endif()
target_link_libraries(base
    PUBLIC
        ${ZLIB_LIBRARIES}
        ${PNG_LIBRARIES}
        ${JPEG_LIBRARIES}
        ${PLATFORM_LIBS}
)
# Fallback for zlib/png/jpeg using conan targets
if(NOT ZLIB_LIBRARIES AND TARGET ZLIB::ZLIB)
    target_link_libraries(base PUBLIC ZLIB::ZLIB)
endif()
if(NOT PNG_LIBRARIES AND TARGET PNG::PNG)
    target_link_libraries(base PUBLIC PNG::PNG)
endif()
if(NOT JPEG_LIBRARIES AND TARGET JPEG::JPEG)
    target_link_libraries(base PUBLIC JPEG::JPEG)
endif()
target_compile_definitions(base PUBLIC ${SDL2_CFLAGS_OTHER})

# Create gameswf library
add_library(gameswf ${GAMESWF_SOURCES})
target_link_libraries(gameswf
    PUBLIC
        base
        OpenGL::GL
        OpenGL::GLU
)

if(FREETYPE_FOUND)
    target_link_libraries(gameswf PUBLIC ${FREETYPE_LIBRARIES})
endif()

if(GAMESWF_HAVE_WINMM_SOUND)
    target_compile_definitions(gameswf PUBLIC GAMESWF_HAVE_WINMM_SOUND=1)
    target_link_libraries(gameswf PUBLIC winmm)
elseif(SDL2_MIXER_FOUND)
    if(TARGET SDL2_mixer::SDL2_mixer)
        target_link_libraries(gameswf PUBLIC SDL2_mixer::SDL2_mixer)
    else()
        target_link_libraries(gameswf PUBLIC ${SDL2_MIXER_LIBRARIES})
    endif()
endif()

# Build the player executable
if(GAMESWF_BUILD_PLAYER)
    add_executable(gameswf_test_ogl gameswf/gameswf_test_ogl.cpp base/sdl2_compat.cpp)
    target_compile_definitions(gameswf_test_ogl PRIVATE GAMESWF_PLAYER_MAIN)
    # Always define HAVE_SDL2 and TU_USE_SDL when building the player
    # SDL2 headers and libraries are available via conan
    target_compile_definitions(gameswf_test_ogl PRIVATE HAVE_SDL2=1 TU_USE_SDL=1)
    if(NOT SDL2_FOUND AND NOT CONAN_SDL2_TARGET)
        # SDL2 found at known Conan location
        set(SDL2_CONAN_PREFIX "F:/conan/p/sdl18a0310ccc696/p")
        target_include_directories(gameswf_test_ogl PRIVATE
            ${SDL2_CONAN_PREFIX}/include
            ${SDL2_CONAN_PREFIX}/include/SDL2
        )
        target_link_libraries(gameswf_test_ogl PRIVATE
            ${SDL2_CONAN_PREFIX}/lib/SDL2main.lib
            ${SDL2_CONAN_PREFIX}/lib/SDL2.lib
        )
    endif()
    if(GAMESWF_HAVE_WINMM_SOUND)
        target_compile_definitions(gameswf_test_ogl PRIVATE GAMESWF_HAVE_WINMM_SOUND=1)
    elseif(GAMESWF_HAVE_SDL_MIXER)
        target_compile_definitions(gameswf_test_ogl PRIVATE GAMESWF_HAVE_SDL_MIXER=1)
    endif()
    if(CONAN_SDL2_TARGET)
        target_link_libraries(gameswf_test_ogl PRIVATE gameswf ${CONAN_SDL2_TARGET})
    else()
        target_link_libraries(gameswf_test_ogl PRIVATE gameswf ${SDL2_LIBRARIES})
    endif()
endif()

# Install targets
install(TARGETS base gameswf
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

if(GAMESWF_BUILD_PLAYER)
    install(TARGETS gameswf_test_ogl RUNTIME DESTINATION bin)
endif()

# Build GLFW example if GLFW is available
option(GAMESWF_BUILD_GLFW_EXAMPLE "Build GLFW embedding example" ON)
if(GAMESWF_BUILD_GLFW_EXAMPLE)
    find_package(glfw3 QUIET)
    if(glfw3_FOUND)
        add_executable(example_glfw examples/example_glfw.cpp)
        target_link_libraries(example_glfw
            PRIVATE
                gameswf
                glfw
                OpenGL::GL
                OpenGL::GLU
        )
        target_include_directories(example_glfw PRIVATE ${CMAKE_SOURCE_DIR})
        message(STATUS "  GLFW example: Enabled")
    else()
        message(STATUS "  GLFW example: Disabled (GLFW not found)")
    endif()
endif()

# Install headers
install(DIRECTORY gameswf/ DESTINATION include/gameswf
    FILES_MATCHING PATTERN "*.h"
)
install(DIRECTORY base/ DESTINATION include/gameswf/base
    FILES_MATCHING PATTERN "*.h"
)

# Print configuration summary
message(STATUS "")
message(STATUS "GameSWF Configuration:")
message(STATUS "  SDL2:       ${SDL2_VERSION}")
message(STATUS "  OpenGL:     ${OPENGL_gl_LIBRARY}")
message(STATUS "  zlib:       ${ZLIB_VERSION_STRING}")
message(STATUS "  libpng:     ${PNG_VERSION_STRING}")
message(STATUS "  libjpeg:    Found")
if(FREETYPE_FOUND)
    message(STATUS "  FreeType:   ${FREETYPE_VERSION_STRING}")
else()
    message(STATUS "  FreeType:   Not found (font rendering limited)")
endif()
if(SDL2_MIXER_FOUND)
    message(STATUS "  SDL_mixer:  ${SDL2_MIXER_VERSION}")
else()
    message(STATUS "  SDL_mixer:  Not found (sound disabled)")
endif()
message(STATUS "")
