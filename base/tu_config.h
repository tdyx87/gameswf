// tu_config.h	-- by Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Generic include file for configuring tu-testbed.


#ifndef TU_CONFIG_H
#define TU_CONFIG_H


// In case you need some unusual stuff to be predefined.  For example,
// maybe you #define new/delete to be something special.
#include "compatibility_include.h"

// On Windows with SDL2, use standard malloc to avoid conflicts with SDL2's dlmalloc
#ifdef _WIN32
    #include <stdlib.h>
    #ifndef tu_malloc
    #define tu_malloc(size) malloc(size)
    #endif
    #ifndef tu_realloc
    #define tu_realloc(old_ptr, new_size, old_size) realloc(old_ptr, new_size)
    #endif
    #ifndef tu_free
    #define tu_free(old_ptr, old_size) free(old_ptr)
    #endif
#else
    #include "base/dlmalloc.h"
    // #define these in compatibility_include.h if you want something different.
    #ifndef tu_malloc
    #define tu_malloc(size) dlmalloc(size)
    #endif
    #ifndef tu_realloc
    #define tu_realloc(old_ptr, new_size, old_size) dlrealloc(old_ptr, new_size)
    #endif
    #ifndef tu_free
    #define tu_free(old_ptr, old_size) dlfree(old_ptr)
    #endif
#endif

// tu_error_exit() is for fatal errors; it should not return!
// You can #define it to something else in compatibility_include.h; e.g. you could
// throw an exception, halt, whatever.
#ifndef tu_error_exit
#include <stdlib.h>	// for exit()
#define tu_error_exit(error_code, error_message) { fprintf(stderr, error_message); exit(error_code); }
#endif


// define TU_CONFIG_LINK_TO_JPEGLIB to 0 to exclude jpeg code from
// your build.  Be aware of what you're doing -- it may break
// features!
#ifndef TU_CONFIG_LINK_TO_JPEGLIB
#define TU_CONFIG_LINK_TO_JPEGLIB 1
#endif

// define TU_CONFIG_LINK_TO_ZLIB to 0 to exclude zlib code from your
// build.  Be aware of what you're doing -- it may break features that
// you need!
#ifndef TU_CONFIG_LINK_TO_ZLIB
#define TU_CONFIG_LINK_TO_ZLIB 1
#endif

// define TU_CONFIG_LINK_TO_LIBPNG to 0 to exclude libpng code from
// your build.  Be aware of what you're doing -- it may break
// features!
#ifndef TU_CONFIG_LINK_TO_LIBPNG
#define TU_CONFIG_LINK_TO_LIBPNG 1
#endif

// define TU_CONFIG_LINK_TO_LIB3DS to 1 to include 3DS file support in
// gameswf, depending on the lib3ds library
#ifndef TU_CONFIG_LINK_TO_LIB3DS
#define TU_CONFIG_LINK_TO_LIB3DS 0
#endif

// define TU_CONFIG_LINK_TO_FFMPEG to 1 to include MP3 & video support in
// gameswf, depending on the libavcode, libavutil & libavformat libraries
#ifndef TU_CONFIG_LINK_TO_FFMPEG
#define TU_CONFIG_LINK_TO_FFMPEG 0
#endif

// define TU_CONFIG_LINK_TO_FREETYPE to 1 to include dynamic font support in
// gameswf, depending on the freetype library
#ifndef TU_CONFIG_LINK_TO_FREETYPE
#define TU_CONFIG_LINK_TO_FREETYPE 1
#endif

// define TU_CONFIG_LINK_TO_THREAD to 0 to switch in gameswf to single thread mode
// define TU_CONFIG_LINK_TO_THREAD to 1 to include SDL thread & mutex support in gameswf
// define TU_CONFIG_LINK_TO_THREAD to 2 to include ... (TODO: pthread support)
#ifndef TU_CONFIG_LINK_TO_THREAD
#	ifdef HAVE_SDL2
#		define TU_CONFIG_LINK_TO_THREAD 1
#	else
#		define TU_CONFIG_LINK_TO_THREAD 0
#	endif
#endif

#if TU_CONFIG_LINK_TO_THREAD == 0 && TU_CONFIG_LINK_TO_FFMPEG == 1
#error video & MP3 requires multi thread support
#endif

// SWF shape coordinates are SB[NumBits] and can exceed the Sint16 range
// (e.g. swf2.swf shape 388 uses x=35252 twips).  Storing mesh coords in
// Sint16 wraps them (35252 -> -30284), turning an off-stage fill band into
// a quad that covers the whole stage (solid black screen).  Use float.
#ifndef TU_USES_FLOAT_AS_COORDINATE_COMPONENT
#define TU_USES_FLOAT_AS_COORDINATE_COMPONENT 1
#endif

// define TU_CONFIG_LINK_STATIC to 1 to link gameswf statically 
#ifndef TU_CONFIG_LINK_STATIC
#	define TU_CONFIG_LINK_STATIC 0
#endif // TU_CONFIG_LINK_STATIC

// define TU_ENABLE_NETWORK to 1 to enable network supprt
#ifndef TU_ENABLE_NETWORK
#	define TU_ENABLE_NETWORK 0
#endif // TU_ENABLE_NETWORK

#ifndef exported_module
#	ifdef _WIN32
#		if TU_CONFIG_LINK_STATIC == 0
#			define exported_module __declspec(dllexport)
#		else
#			define exported_module
#		endif // TU_CONFIG_LINK_STATIC == 0
#	else
#		define exported_module
#	endif // _WIN32
#endif // exported_module

// define TU_CONFIG_VERBOSE to 1 to allow verbose debugging
#ifndef TU_CONFIG_VERBOSE
#	define TU_CONFIG_VERBOSE 1
#endif

// define TU_USE_FLASH_COMPATIBLE_HITTEST to 0 to allow verbose debugging
#ifndef TU_USE_FLASH_COMPATIBLE_HITTEST
#	define TU_USE_FLASH_COMPATIBLE_HITTEST 1
#endif

// define TU_USE_SDL to 1 to use SDL sound/render handler
#ifndef TU_USE_SDL
#	ifdef HAVE_SDL2
#		define TU_USE_SDL 1
#	else
#		define TU_USE_SDL 0
#	endif
#endif

#ifndef TU_USE_OPENAL
#	define TU_USE_OPENAL 0
#endif

#ifndef TU_USE_OGLES
#	define TU_USE_OGLES 0
#endif

#endif // TU_CONFIG_H
