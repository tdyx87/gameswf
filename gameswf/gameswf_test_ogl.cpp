// gameswf_test_ogl.cpp	-- Thatcher Ulrich <tu@tulrich.com> 2003 -*- coding: utf-8;-*-

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// A minimal test player app for the gameswf library.

#include "base/tu_memdebug.h"	// must be the first in the include list

#include "base/sdl2_compat.h"
#ifndef HAVE_SDL2
#include <SDL.h>
#include <SDL_opengl.h>
#else
#include <SDL_opengl.h>
#endif

#include "gameswf/gameswf.h"
#include <stdlib.h>
#include <stdio.h>
#include "base/utility.h"
#include "base/container.h"
#include "base/tu_file.h"
#include "base/tu_types.h"
#include "base/tu_timer.h"
#include "gameswf/gameswf_types.h"
#include "gameswf/gameswf_impl.h"
#include "gameswf/gameswf_root.h"
#include "gameswf/gameswf_freetype.h"
#include "gameswf/gameswf_player.h"

#if TU_ENABLE_NETWORK == 1
#	include "net/tu_net_file.h"
#endif

#ifdef _WIN32
#	include <Winsock.h>
#	define stricmp _stricmp
#else
#	define stricmp strcasecmp
#endif                                  

void	print_usage()
// Brief instructions.
{
	printf(
		"gameswf_test_ogl -- a test player for the gameswf library.\n"
		"\n"
		"This program has been donated to the Public Domain.\n"
		"See http://tulrich.com/geekstuff/gameswf.html for more info.\n"
		"\n"
		"usage: gameswf_test_ogl [options] movie_file.swf\n"
		"\n"
		"Plays a SWF (Shockwave Flash) movie, using OpenGL and the\n"
		"gameswf library.\n"
		"\n"
		"options:\n"
		"\n"
		"  -h          Print this info.\n"
		"  -c          Produce a core file instead of letting SDL trap it\n"
		"  -d num      Number of milliseconds to delay in main loop\n"
		"  -a <level>  Specify the antialiasing level (0,1,2,4,8,16,...)\n"
		"  -v          Be verbose; i.e. print log messages to stdout\n"
		"  -va         Be verbose about movie Actions\n"
		"  -vp         Be verbose about parsing the movie\n"
		"  -ml <bias>  Specify the texture LOD bias (float, default is -1)\n"
		"  -p          Run full speed (no sleep) and log frame rate\n"
		"  -1          Play once; exit when/if movie reaches the last frame\n"
		"  -r <0|1|2>  0 disables rendering & sound (good for batch tests)\n"
		"              1 enables rendering & sound (default setting)\n"
		"              2 enables rendering & disables sound\n"
		"  -t <sec>    Timeout and exit after the specified number of seconds\n"
		"  -b <bits>   Bit depth of output window (16 or 32, default is 16)\n"
		"  -n          Allow use of network to try to open resource URLs\n"
		"  -u          Allow pass user variables to Flash\n"
		"  -k          Disables cursor\n"
		"  -w <w>x<h>  Specify the window size, for example 1024x768\n"
		"  -f          Force realtime framerate\n"
		"  -i          Grub bitmaps from swf file\n"
		"\n"
		"keys:\n"
		"  CTRL-Q          Quit/Exit\n"
		"  CTRL-W          Quit/Exit\n"
		"  ESC             Quit/Exit\n"
		"  CTRL-P          Toggle Pause\n"
		"  CTRL-[ or kp-   Step back one frame\n"
		"  CTRL-] or kp+   Step forward one frame\n"
		"  CTRL-A          Toggle antialiasing\n"
		"  CTRL-T          Debug.  Test the set_variable() function\n"
		"  CTRL-G          Debug.  Test the get_variable() function\n"
		"  CTRL-M          Debug.  Test the call_method() function\n"
		"  CTRL-B          Toggle background color\n"
		);
}

#define OVERSIZE	1.0f

static bool s_antialiased = true;
static int s_bit_depth = 24;
static int s_delay = 10;


// by default it's used the simplest and the fastest edge antialiasing method
// if you have modern video card you can use full screen antialiasing
// full screen antialiasing level may be 2,4,8,16, ...
static int s_aa_level = 1;

static bool s_background = true;
static bool s_measure_performance = false;
// Controls whether we will try to load things over the net or not.
static bool s_allow_http = false;

static void	message_log(const char* message)
// Process a log message.
{
	if (gameswf::get_verbose_parse())
	{
		fputs(message, stdout);
		fflush(stdout);
	}
}


static void	log_callback(bool error, const char* message)
// Error callback for handling gameswf messages.
{
	if (error)
	{
		// Log, and also print to stderr.
		message_log(message);
		fputs(message, stderr);
		fflush(stderr);
	}
	else
	{
		message_log(message);
	}
}


static tu_file*	file_opener(const char* url)
// Callback function.  This opens files for the gameswf library.
{
	if (s_allow_http) 
	{
#if TU_ENABLE_NETWORK == 1
		return new_tu_net_file(url, "rb");
#else
		return NULL;
#endif
	}
	else
	{
		return new tu_file(url, "rb");
	}
}

static gameswf::key::code	translate_key(SDLKey key)
// For forwarding SDL key events to gameswf.
{
	gameswf::key::code	c(gameswf::key::INVALID);

	// Check special keys first (SDL2 has large values for these)
	switch (key)
	{
		case SDLK_UP: return gameswf::key::UP;
		case SDLK_DOWN: return gameswf::key::DOWN;
		case SDLK_LEFT: return gameswf::key::LEFT;
		case SDLK_RIGHT: return gameswf::key::RIGHT;
		case SDLK_RETURN: return gameswf::key::ENTER;
		case SDLK_ESCAPE: return gameswf::key::ESCAPE;
		case SDLK_SPACE: return gameswf::key::SPACE;
		case SDLK_BACKSPACE: return gameswf::key::BACKSPACE;
		case SDLK_TAB: return gameswf::key::TAB;
		case SDLK_HOME: return gameswf::key::HOME;
		case SDLK_END: return gameswf::key::END;
		case SDLK_PAGEUP: return gameswf::key::PGUP;
		case SDLK_PAGEDOWN: return gameswf::key::PGDN;
		case SDLK_INSERT: return gameswf::key::INSERT;
		case SDLK_DELETE: return gameswf::key::DELETEKEY;
		case SDLK_LSHIFT: case SDLK_RSHIFT: return gameswf::key::SHIFT;
		case SDLK_LCTRL: case SDLK_RCTRL: return gameswf::key::CONTROL;
		case SDLK_LALT: case SDLK_RALT: return gameswf::key::ALT;
		case SDLK_F1: return gameswf::key::F1;
		case SDLK_F2: return gameswf::key::F2;
		case SDLK_F3: return gameswf::key::F3;
		case SDLK_F4: return gameswf::key::F4;
		case SDLK_F5: return gameswf::key::F5;
		case SDLK_F6: return gameswf::key::F6;
		case SDLK_F7: return gameswf::key::F7;
		case SDLK_F8: return gameswf::key::F8;
		case SDLK_F9: return gameswf::key::F9;
		case SDLK_F10: return gameswf::key::F10;
		case SDLK_F11: return gameswf::key::F11;
		case SDLK_F12: return gameswf::key::F12;
		case SDLK_KP_0: return gameswf::key::KP_0;
		case SDLK_KP_1: return gameswf::key::KP_1;
		case SDLK_KP_2: return gameswf::key::KP_2;
		case SDLK_KP_3: return gameswf::key::KP_3;
		case SDLK_KP_4: return gameswf::key::KP_4;
		case SDLK_KP_5: return gameswf::key::KP_5;
		case SDLK_KP_6: return gameswf::key::KP_6;
		case SDLK_KP_7: return gameswf::key::KP_7;
		case SDLK_KP_8: return gameswf::key::KP_8;
		case SDLK_KP_9: return gameswf::key::KP_9;
		case SDLK_KP_ENTER: return gameswf::key::ENTER;
		case SDLK_KP_PLUS: return gameswf::key::KP_ADD;
		case SDLK_KP_MINUS: return gameswf::key::KP_SUBTRACT;
		case SDLK_KP_MULTIPLY: return gameswf::key::KP_MULTIPLY;
		case SDLK_KP_DIVIDE: return gameswf::key::KP_DIVIDE;
		case SDLK_KP_PERIOD: return gameswf::key::KP_DECIMAL;
		default: break;
	}

	// Then check alphanumeric keys
	if (key >= SDLK_0 && key <= SDLK_9)
	{
		c = (gameswf::key::code) ((key - SDLK_0) + gameswf::key::_0);
	}
	else if (key >= SDLK_a && key <= SDLK_z)
	{
		c = (gameswf::key::code) ((key - SDLK_a) + gameswf::key::A);
	}
	else if (key >= SDLK_F1 && key <= SDLK_F15)
	{
		c = (gameswf::key::code) ((key - SDLK_F1) + gameswf::key::F1);
	}
	else if (key >= SDLK_KP0 && key <= SDLK_KP9)
	{
		c = (gameswf::key::code) ((key - SDLK_KP0) + gameswf::key::KP_0);
	}
	// Remaining special keys
	else if (key == SDLK_PERIOD) c = gameswf::key::PERIOD;
	else if (key == SDLK_SLASH) c = gameswf::key::SLASH;
	else if (key == SDLK_BACKSLASH) c = gameswf::key::BACKSLASH;
	else if (key == SDLK_SEMICOLON) c = gameswf::key::SEMICOLON;
	else if (key == SDLK_QUOTE) c = gameswf::key::QUOTE;
	else if (key == SDLK_LEFTBRACKET) c = gameswf::key::LEFT_BRACKET;
	else if (key == SDLK_RIGHTBRACKET) c = gameswf::key::RIGHT_BRACKET;
	else if (key == SDLK_COMMA) c = gameswf::key::COMMA;
	else if (key == SDLK_MINUS) c = gameswf::key::MINUS;
	else if (key == SDLK_EQUALS) c = gameswf::key::EQUALS;
	else if (key == SDLK_CAPSLOCK) c = gameswf::key::CAPSLOCK;
	else if (key == SDLK_NUMLOCKCLEAR) c = gameswf::key::NUM_LOCK;

	return c;
}

static void	fs_callback(gameswf::character* movie, const char* command, const char* args)
// For handling notification callbacks from ActionScript.
{
	assert(movie);
	gameswf::gc_ptr<gameswf::player> player = movie->get_player();

	if (stricmp(command, "fullscreen") == 0)
	{
		// TODO
	}
	else
	if (stricmp(command, "set_max_volume") == 0)
	{
		// set max sound volume in percent, [0..100]
		// usefull for embedded games
		gameswf::sound_handler* s = gameswf::get_sound_handler();
		if (s)
		{
			int vol = atoi(args);
			s->set_max_volume(vol);
		}
	}
	else
	if (stricmp(command, "notify_keypress") == 0)
	{
		// simulate keypress event
		for (int i = 0, n = strlen(args); i < n; i++)
		{
			// SDL has no uppercase key codes
			SDLKey key = static_cast<SDLKey>(tolower(args[i]));
//			gameswf::key::code c = translate_key(key);
//			if (c != gameswf::key::INVALID)
//			{
//				player->notify_key_event(c, true);
//			}
			SDL_Event ev;
			memset(&ev, 0, sizeof(ev));
			ev.type = SDL_KEYDOWN;
			ev.key.keysym.sym = key;
			SDL_PushEvent(&ev);
		}
	}
	else
	if (stricmp(command, "set_delay") == 0)
	{
		// set the number of milli-seconds to delay in main loop
		int delay = atoi(args);

		// sanity check
		if (delay >= 0 && delay <= 1000)
		{
			s_delay = delay;
		}
	}
	else
	if (stricmp(command, "clear_events") == 0)
	{
		// clear queue of system events (mouse events, keyboard events, etc)
		SDL_Event	event;
		if (SDL_PollEvent(&event) == 0)
		{
			return;
		}
	}

}

int	main(int argc, char *argv[])
{

	tu_memdebug::open();

	{	// for testing memory leaks

		assert(tu_types_validate());

		const char* infile = NULL;

		float	exit_timeout = 0;
		bool	do_render = true;
		bool	do_sound = true;
		bool	do_loop = true;
		bool	sdl_abort = true;
		bool	sdl_cursor = true;
		bool	auto_click_scan = false;
		// -clk: scheduled clicks (repeatable); each is one press/release
		// pair at movie coordinates on a given loop frame.
		struct scheduled_click_t { int frame, x, y; };
		scheduled_click_t	auto_clicks[8];
		int	auto_click_count = 0;
		float	tex_lod_bias;
		bool	force_realtime_framerate = false;

	#ifdef _WIN32

		WSADATA wsaData;

		int iResult = WSAStartup( MAKEWORD(2,2), &wsaData );
		if ( iResult != NO_ERROR )
			printf("Error at WSAStartup()\n");
	#endif


		// -1.0 tends to look good.
		tex_lod_bias = -1.2f;
		tu_string flash_vars;

		int	width = 0;
		int	height = 0;

		gameswf::gc_ptr<gameswf::player> player = new gameswf::player();

		for (int arg = 1; arg < argc; arg++)
		{
			if (strcmp(argv[arg], "-sc") == 0)
			{
				// Test hook: sweep the mouse over a grid and click each point,
				// so click/mouseUp dispatch can be exercised without a user.
				auto_click_scan = true;
				// Headless: no window/GL needed, just advance the timeline.
				do_render = false;
				continue;
			}

			if (strcmp(argv[arg], "-clk") == 0)
			{
				// Test hook: one press/release at movie coordinates (x,y) on
				// loop frame <n>.  Rendering stays on, so the FBDUMP captures
				// show what the click actually did (e.g. Play -> game screen).
				// Usage: -clk <frame> <x> <y>  (repeatable, clicks in order)
				if (arg + 3 >= argc)
				{
					fprintf(stderr, "-clk needs three args: <frame> <x> <y>\n");
					exit(1);
				}
				if (auto_click_count >= 8)
				{
					fprintf(stderr, "-clk: too many clicks (max 8)\n");
					exit(1);
				}
				auto_clicks[auto_click_count].frame = atoi(argv[arg + 1]);
				auto_clicks[auto_click_count].x = atoi(argv[arg + 2]);
				auto_clicks[auto_click_count].y = atoi(argv[arg + 3]);
				auto_click_count++;
				arg += 3;
				continue;
			}

			if (argv[arg][0] == '-')
			{
				// Looks like an option.

				if (argv[arg][1] == 'h')
				{
					// Help.
					print_usage();
					exit(1);
				}
				if (argv[arg][1] == 'u')
				{
					arg++;
					if (arg < argc)
					{
						flash_vars = argv[arg];
					}
					else
					{
						fprintf(stderr, "-u arg must be followed string like myvar=x&myvar2=y and so on\n");
						print_usage();
						exit(1);
					}

				}
				else if (argv[arg][1] == 'w')
				{
					arg++;
					if (arg < argc)
					{
						width = atoi(argv[arg]);
						const char* x = strstr(argv[arg], "x");
						if (x)
						{
							height = atoi(x + 1);
						}
					}

					if (width <=0 || height <= 0)
					{
						fprintf(stderr, "-w arg must be followed by the window size\n");
						print_usage();
						exit(1);
					}
				}
				else if (argv[arg][1] == 'c')
				{
					sdl_abort = false;
				}
                else if (argv[arg][1] == 'f')
                {
                    force_realtime_framerate = true;
                }
				else if (argv[arg][1] == 'k')
				{
					sdl_cursor = false;
				}
				else if (argv[arg][1] == 'a')
				{
					// Set antialiasing on or off.
					arg++;
					if (arg < argc)
					{
						s_aa_level = atoi(argv[arg]);
						s_antialiased = s_aa_level > 0 ? true : false;
					}
					else
					{
						fprintf(stderr, "-a arg must be followed by the antialiasing level\n");
						print_usage();
						exit(1);
					}
				}
				else if (argv[arg][1] == 'b')
				{
					// Set default bit depth.
					arg++;
					if (arg < argc)
					{
						s_bit_depth = atoi(argv[arg]);
						if (s_bit_depth != 16 && s_bit_depth != 24 && s_bit_depth != 32)
						{
							fprintf(stderr, "Command-line supplied bit depth %d, but it must be 16, 24 or 32", s_bit_depth);
							print_usage();
							exit(1);
						}
					}
					else
					{
						fprintf(stderr, "-b arg must be followed by 16 or 32 to set bit depth\n");
						print_usage();
						exit(1);
					}
				}
				else if (argv[arg][1] == 'd')
				{
					// Set a delay
					arg++;
					if (arg < argc)
					{
						s_delay = atoi(argv[arg]);
					}
					else
					{
						fprintf(stderr, "-d arg must be followed by number of milli-seconds to del in the main loop\n");
						print_usage();
						exit(1);
					}
				}
				else if (argv[arg][1] == 'p')
				{
					// Enable frame-rate/performance logging.
					s_measure_performance = true;
				}
				else if (argv[arg][1] == '1')
				{
					// Play once; don't loop.
					do_loop = false;
				}
				else if (argv[arg][1] == 'r')
				{
					// Set rendering on/off.
					arg++;
					if (arg < argc)
					{
						const int render_arg = atoi(argv[arg]);
						switch (render_arg) {
						case 0:
							// Disable both
							do_render = false;
							do_sound = false;
							break;
						case 1:
							// Enable both
							do_render = true;
							do_sound = true;
							break;
						case 2:
							// Disable just sound
							do_render = true;
							do_sound = false;
							break;
						default:
							fprintf(stderr, "-r must be followed by 0, 1 or 2 (%d is invalid)\n",
								render_arg);
							print_usage();
							exit(1);
							break;
						}
					} else {
						fprintf(stderr, "-r must be followed by 0 an argument to disable/enable rendering\n");
						print_usage();
						exit(1);
					}
				}
				else if (argv[arg][1] == 't')
				{
					// Set timeout.
					arg++;
					if (arg < argc)
					{
						exit_timeout = (float) atof(argv[arg]);
					}
					else
					{
						fprintf(stderr, "-t must be followed by an exit timeout, in seconds\n");
						print_usage();
						exit(1);
					}
				}
				else if (argv[arg][1] == 'v')
				{
					// Be verbose; i.e. print log messages to stdout.
					if (argv[arg][2] == 'a')
					{
						// Enable spew re: action.
						player->verbose_action(true);
					}
					else if (argv[arg][2] == 'p')
					{
						// Enable parse spew.
						player->verbose_parse(true);
					}
					// ...
				}
				else if (argv[arg][1] == 'm')
				{
					if (argv[arg][2] == 'l') {
						arg++;
						tex_lod_bias = (float) atof(argv[arg]);
						//printf("Texture LOD Bais is no %f\n", tex_lod_bias);
					}
					else
					{
						fprintf(stderr, "unknown variant of -m arg\n");
						print_usage();
						exit(1);
					}
				}
				else if (argv[arg][1] == 'n')
				{
					s_allow_http = true;
				}
				else if (argv[arg][1] == 'i')
				{
					player->set_separate_thread(false);
					player->set_log_bitmap_info(true);
				}
			}
			else
			{
				infile = argv[arg];
			}
		}

		if (infile == NULL)
		{
			printf("no input file\n");
			print_usage();
			exit(1);
		}

		player->set_force_realtime_framerate(force_realtime_framerate);

		// use this for multifile games
		// workdir is used when LoadMovie("myfile.swf", _root) is called
		{
			tu_string workdir;
			// Find last slash or backslash.
 			const char* ptr = infile + strlen(infile);
			for (; ptr >= infile && *ptr != '/' && *ptr != '\\'; ptr--) {}
			// Use everything up to last slash as the "workdir".
			int len = ptr - infile + 1;
			if (len > 0)
			{
				tu_string workdir(infile, len);
				player->set_workdir(workdir.c_str());
			}
		}

		gameswf::register_file_opener_callback(file_opener);
		gameswf::register_fscommand_callback(fs_callback);
		if (gameswf::get_verbose_parse())
		{
			gameswf::register_log_callback(log_callback);
		}
		
		gameswf::sound_handler*	sound = NULL;
		gameswf::render_handler*	render = NULL;
		if (do_render)
		{
#if TU_USE_SDL == 1
			render = gameswf::create_render_handler_ogl();
			gameswf::set_render_handler(render);
#endif

#if TU_USE_OGLES == 1
			render = gameswf::create_render_handler_ogles();
			gameswf::set_render_handler(render);
#endif

#if TU_CONFIG_LINK_TO_FREETYPE == 1
			gameswf::set_glyph_provider(gameswf::create_glyph_provider_freetype());
#else
			gameswf::set_glyph_provider(gameswf::create_glyph_provider_tu());
#endif
		}

		//
		//	set_proxy("192.168.1.201", 8080);
		//

		// gameswf::set_use_cache_files(true);

		player->set_flash_vars(flash_vars);
		{
			fprintf(stderr, "Loading SWF file: %s\n", infile);
			gameswf::gc_ptr<gameswf::root>	m = player->load_file(infile);
			if (m == NULL)
			{
				fprintf(stderr, "error: load_file returned NULL\n");
				exit(1);
			}

			fprintf(stderr, "SWF loaded: %dx%d @ %.2f fps, %d frames\n",
				m->get_movie_width(), m->get_movie_height(),
				m->get_movie_fps(), m->get_frame_count());

			if (width == 0 || height == 0)
			{
				width = m->get_movie_width();
				height = m->get_movie_height();
			}

			// Ensure minimum window size so the window is visible
			const int MIN_WIDTH = 800;
			const int MIN_HEIGHT = 600;
			if (width < MIN_WIDTH || height < MIN_HEIGHT)
			{
				float scale = std::max((float)MIN_WIDTH / width, (float)MIN_HEIGHT / height);
				width = (int)(width * scale);
				height = (int)(height * scale);
			}

			float scale_x = (float) width / m->get_movie_width();
			float scale_y = (float) height / m->get_movie_height();

			float	movie_fps = m->get_movie_fps();

			if (do_render)
			{
				// Initialize the SDL subsystems we're using. Linux
				// and Darwin use Pthreads for SDL threads, Win32
				// doesn't. Otherwise the SDL event loop just polls.
				Uint32 sdl_init_flags = SDL_INIT_VIDEO;
				if (do_sound)
				{
					sdl_init_flags |= SDL_INIT_AUDIO;
				}
				if (sdl_abort)
				{
					//  Other flags are SDL_INIT_JOYSTICK | SDL_INIT_CDROM
	#ifdef _WIN32
					if (SDL_Init(sdl_init_flags))
	#else
					if (SDL_Init(sdl_init_flags | SDL_INIT_EVENTTHREAD))
	#endif
					{
						fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
						exit(1);
					}
				}
				else
				{
					fprintf(stderr, "warning: SDL won't trap core dumps \n");
	#ifdef _WIN32
					if (SDL_Init(sdl_init_flags | SDL_INIT_NOPARACHUTE | SDL_INIT_EVENTTHREAD))
	#else
					if (SDL_Init(sdl_init_flags | SDL_INIT_NOPARACHUTE))
	#endif
					{
						fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
						exit(1);
					}
				}

				atexit(SDL_Quit);

				SDL_EnableKeyRepeat(250, 33);
				SDL_ShowCursor(sdl_cursor ? SDL_ENABLE : SDL_DISABLE);

				// Create sound handler AFTER SDL_Init
				if (do_sound)
				{
#ifdef GAMESWF_HAVE_WINMM_SOUND
					sound = gameswf::create_sound_handler_winmm();
#elif GAMESWF_HAVE_SDL_MIXER
					sound = gameswf::create_sound_handler_sdl();
#elif TU_USE_OPENAL == 1
					sound = gameswf::create_sound_handler_openal();
#endif
					gameswf::set_sound_handler(sound);
				}

				switch (s_bit_depth)
				{
					case 16:
						// 16-bit color, surface creation is likely to succeed.
						SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
						SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
						SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
						SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 15);
						SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
						SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 5);
						break;

				case 24:
					// 24-bit color
					SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
					SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);	// 8-bit stencil for mask support
					break;

					case 32:
						// 32-bit color etc, for getting dest alpha,
						// for MULTIPASS_ANTIALIASING (see gameswf_render_handler_ogl.cpp).
						SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
						SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
						SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
						SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
						SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
						SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
						SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
						break;

					default:
						assert(0);
				}

				// try to enable FSAA
				if (s_aa_level > 1)
				{
					SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
					SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, s_aa_level);
				}

				// Change the LOD BIAS values to tweak blurriness.
				if (tex_lod_bias != 0.0f) {
	#ifdef FIX_I810_LOD_BIAS	
					// If 2D textures weren't previously enabled, enable
					// them now and force the driver to notice the update,
					// then disable them again.
					if (!glIsEnabled(GL_TEXTURE_2D)) {
						// Clearing a mask of zero *should* have no
						// side effects, but coupled with enbling
						// GL_TEXTURE_2D it works around a segmentation
						// fault in the driver for the Intel 810 chip.
						glEnable(GL_TEXTURE_2D);
						glClear(0);
						glDisable(GL_TEXTURE_2D);
					}
	#endif // FIX_I810_LOD_BIAS
					glTexEnvf(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, tex_lod_bias);
				}

				// Set the video mode.
				if (do_render)
				{
				//				if (SDL_SetVideoMode(width, height, s_bit_depth, SDL_OPENGL | SDL_RESIZABLE) == 0)
					if (SDL_SetVideoMode(width, height, s_bit_depth, SDL_OPENGL) == 0)
					{
						fprintf(stderr, "SDL_SetVideoMode() failed.");
						exit(1);
					}

					render->open();
				}
				render->set_antialiased(s_antialiased);

				// Turn on alpha blending.
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

				// Turn on line smoothing.  Antialiased lines can be used to
				// smooth the outsides of shapes.
				glEnable(GL_LINE_SMOOTH);
				glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);	// GL_NICEST, GL_FASTEST, GL_DONT_CARE

				glMatrixMode(GL_PROJECTION);
				glOrtho(-OVERSIZE, OVERSIZE, OVERSIZE, -OVERSIZE, -1, 1);
				glMatrixMode(GL_MODELVIEW);
				glLoadIdentity();

				// We don't need lighting effects
				glDisable(GL_LIGHTING);
				// glColorPointer(4, GL_UNSIGNED_BYTE, 0, *);
				// glInterleavedArrays(GL_T2F_N3F_V3F, 0, *)
				// NOTE: glPushAttrib(GL_ALL_ATTRIB_BITS) was removed because the
				// matching glPopAttrib was commented out, leaving the attrib stack
				// permanently unbalanced. On some Windows OpenGL drivers (especially
				// AMD/ATI), this silently corrupts the draw buffer state and disables
				// rendering — producing a black screen despite valid GL calls.
			}

			// Mouse state.
			int	mouse_x = 0;
			int	mouse_y = 0;
			int	mouse_buttons = 0;

			float	speed_scale = 1.0f;
			Uint32	start_ticks = 0;
			if (do_render)
			{
				start_ticks = tu_timer::get_ticks();
			}
			Uint32	last_ticks = start_ticks;
			int	frame_counter = 0;
			int	last_logged_fps = last_ticks;
			int fps = 0;

			//TODO
	//		gameswf::player* p = gameswf::create_player();
	//		p.run();


			for (;;)
			{
				Uint32	ticks;
				if (do_render)
				{
					ticks = tu_timer::get_ticks();
				}
				else
				{
					// Simulate time.
					ticks = last_ticks + (Uint32) (1000.0f / movie_fps);
				}
				int	delta_ticks = ticks - last_ticks;
				float	delta_t = delta_ticks / 1000.f;
				last_ticks = ticks;

				// Check auto timeout counter.
				if (exit_timeout > 0
					&& ticks - start_ticks > (Uint32) (exit_timeout * 1000))
				{
					// Auto exit now.
					break;
				}

				bool ret = true;
				if (do_render)
				{
					SDL_Event	event;
					// Handle input.
					while (ret)
					{
						if (SDL_PollEvent(&event) == 0)
						{
							break;
						}

						//printf("EVENT Type is %d\n", event.type);
						switch (event.type)
						{
						case SDL_WINDOWEVENT:
							// Handle SDL2 window events
							if (event.window.event == SDL_WINDOWEVENT_CLOSE)
							{
								goto done;
							}
							break;

						case SDL_USEREVENT:
							//printf("SDL_USER_EVENT at %s, code %d%d\n", __FUNCTION__, __LINE__, event.user.code);
							ret = false;
							break;
						case SDL_KEYDOWN:
							{
								SDLKey	key = event.key.keysym.sym;
								bool	ctrl = (event.key.keysym.mod & KMOD_CTRL) != 0;

								if (key == SDLK_ESCAPE
									|| (ctrl && key == SDLK_q)
									|| (ctrl && key == SDLK_w))
								{
									goto done;
								}
								else if (ctrl && key == SDLK_p)
								{
									// Toggle paused state.
									if (m->get_play_state() == gameswf::character::STOP)
									{
										m->set_play_state(gameswf::character::PLAY);
									}
									else
									{
										m->set_play_state(gameswf::character::STOP);
									}
								}
								else if (ctrl && key == SDLK_i)
								{
									// Init library, for detection of memory leaks (for testing purposes)
	/*
									// Clean up gameswf as much as possible, so valgrind will help find actual leaks.

									gameswf::set_sound_handler(NULL);
									delete sound;

									gameswf::set_render_handler(NULL);
									delete render;

									if (do_render)
									{
										if (do_sound)
									{
#ifdef GAMESWF_HAVE_WINMM_SOUND
										sound = gameswf::create_sound_handler_winmm();
#elif GAMESWF_HAVE_SDL_MIXER
										sound = gameswf::create_sound_handler_sdl();
#elif TU_USE_OPENAL == 1
										sound = gameswf::create_sound_handler_openal();
#endif
										gameswf::set_sound_handler(sound);
									}
										render = gameswf::create_render_handler_ogl();
										gameswf::set_render_handler(render);
									}
	*/
									// Load the actual movie.
									m = player->load_file(infile);
									if (m == NULL)
									{
										exit(1);
									}
								}
								else if (ctrl && (key == SDLK_LEFTBRACKET || key == SDLK_KP_MINUS))
								{
									m->goto_frame(m->get_current_frame()-1);
								}
								else if (ctrl && (key == SDLK_RIGHTBRACKET || key == SDLK_KP_PLUS))
								{
									m->goto_frame(m->get_current_frame()+1);
								}
								else if (ctrl && key == SDLK_a)
								{
									// Toggle antialiasing.
									s_antialiased = !s_antialiased;
									if (render)
									{
										render->set_antialiased(s_antialiased);
									}
								}
								else if (ctrl && key == SDLK_t)
								{
									// test text replacement / variable setting:
									m->set_variable("test.text", "set_edit_text was here...\nanother line of text for you to see in the text box");
								}
								else if (ctrl && key == SDLK_g)
								{
									// test get_variable.
									message_log("testing get_variable: '");
									message_log(m->get_variable("test.text"));
									message_log("'\n");
								}
								else if (ctrl && key == SDLK_m)
								{
									// Test call_method.
									const char* result = m->call_method(
										"test_call",
										"%d, %f, %s, %ls",
										200,
										1.0f,
										"Test string",
										L"Test long string");

									if (result)
									{
										message_log("call_method: result = ");
										message_log(result);
										message_log("\n");
									}
									else
									{
										message_log("call_method: null result\n");
									}
								}
								else if (ctrl && key == SDLK_b)
								{
									// toggle background color.
									s_background = !s_background;
								}
	//							else if (ctrl && key == SDLK_f)	//xxxxxx
	//							{
	//								extern bool gameswf_debug_show_paths;
	//								gameswf_debug_show_paths = !gameswf_debug_show_paths;
	//							}
								else if (ctrl && key == SDLK_EQUALS)
								{
									float	f = gameswf::get_curve_max_pixel_error();
									f *= 1.1f;
									gameswf::set_curve_max_pixel_error(f);
									printf("curve error tolerance = %f\n", f);
								}
								else if (ctrl && key == SDLK_MINUS)
								{
									float	f = gameswf::get_curve_max_pixel_error();
									f *= 0.9f;
									gameswf::set_curve_max_pixel_error(f);
									printf("curve error tolerance = %f\n", f);
								} else if (ctrl && key == SDLK_F2) {
									// Toggle wireframe.
									static bool wireframe_mode = false;
									wireframe_mode = !wireframe_mode;
									if (wireframe_mode) {
										glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
									} else {
										glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
									}
									// TODO: clean up this interafce and re-enable.
									// 					} else if (ctrl && key == SDLK_d) {
									// 						// Flip a special debug flag.
									// 						gameswf_tesselate_dump_shape = true;
								}

								gameswf::key::code c = translate_key(key);
								if (c != gameswf::key::INVALID)
								{
									player->notify_key_event(c, true);
								}

								break;
							}

						case SDL_KEYUP:
							{
								SDLKey	key = event.key.keysym.sym;

								gameswf::key::code c = translate_key(key);
								if (c != gameswf::key::INVALID)
								{
									player->notify_key_event(c, false);
								}

								break;
							}

						case SDL_MOUSEMOTION:
							mouse_x = (int) (event.motion.x / scale_x);
							mouse_y = (int) (event.motion.y / scale_y);
							break;

						case SDL_MOUSEBUTTONDOWN:
						case SDL_MOUSEBUTTONUP:
							{
								int	mask = 1 << (event.button.button - 1);
								if (event.button.state == SDL_PRESSED)
								{
									mouse_buttons |= mask;
								}
								else
								{
									mouse_buttons &= ~mask;
								}
								break;
							}

						case SDL_QUIT:
							goto done;
							break;

						default:
							break;
						}
					}
				}

				if (do_render)
				{
					glDisable(GL_DEPTH_TEST);	// Disable depth testing.
				}

				m = player->get_root();
			if (m == NULL)
			{
				fprintf(stderr, "error: player->get_root() returned NULL\n");
				exit(1);
			}
			m->set_display_viewport(0, 0, width, height);
			m->set_background_alpha(s_background ? 1.0f : 0.05f);

				// Test hook (-sc): move the mouse over a grid of points and click
			// each one, so mouse/click dispatch runs without a real user.
			if (auto_click_scan)
			{
				const int GRID_W = 12;
				const int GRID_H = 12;
				static int s_scan_frame = 0;
				s_scan_frame++;

				const int first_frame = 150;	// let the intro play first
				if (s_scan_frame > first_frame)
				{
					const int cycle = s_scan_frame - first_frame;
					const int point = cycle / 4;	// 4 frames per grid point
					const int phase = cycle % 4;	// 0 move, 1 press, 2 release, 3 idle

					if (point >= GRID_W * GRID_H)
					{
						// Scan finished; exit the headless test loop.
						fprintf(stderr, "[SCAN] finished; exiting\n");
						fflush(stderr);
						break;
					}

					if (point < GRID_W * GRID_H)
					{
						const int gx = point % GRID_W;
						const int gy = point / GRID_W;
						const int mw = (int) m->get_movie_width();
						const int mh = (int) m->get_movie_height();

						mouse_x = (int) ((gx + 0.5f) * mw / GRID_W);
						mouse_y = (int) ((gy + 0.5f) * mh / GRID_H);

						switch (phase)
						{
						case 0:
							mouse_buttons = 0;
							fprintf(stderr, "[SCAN] point=%d gx=%d gy=%d mouse=%d,%d\n",
								point, gx, gy, mouse_x, mouse_y);
							break;
						case 1:
							mouse_buttons = 1;	// press (left button)
							break;
						case 2:
							mouse_buttons = 0;	// release -> should fire click
							break;
						default:
							mouse_buttons = 0;
							break;
						}
					}
				}
			}

			// Test hook (-clk): move to the target point, press on the given
			// loop frame and release two frames later, so root::on_mouse_event
			// sees a real press -> release on the same character (which is what
			// turns into an AS3 "click").  Multiple -clk options run in order.
			if (auto_click_count > 0)
			{
				static int s_click_frame = 0;
				s_click_frame++;

				for (int ci = 0; ci < auto_click_count; ci++)
				{
					const scheduled_click_t& c = auto_clicks[ci];

					// Hover a few frames before pressing.  root::generate_mouse_button_events
					// only records m_active_entity while the button is UP (ROLL_OVER), so a
					// press on the very frame the cursor arrives would see active==NULL and
					// neither PRESS nor RELEASE would ever reach the character.
					if (s_click_frame >= c.frame - 4 && s_click_frame <= c.frame + 2)
					{
						mouse_x = c.x;
						mouse_y = c.y;
					}

					if (s_click_frame == c.frame)
					{
						mouse_buttons = 1;
						fprintf(stderr, "[CLK] press at %d,%d loop_frame=%d\n",
							mouse_x, mouse_y, s_click_frame);
						fflush(stderr);
					}
					else if (s_click_frame == c.frame + 2)
					{
						mouse_buttons = 0;
						fprintf(stderr, "[CLK] release at %d,%d loop_frame=%d\n",
							mouse_x, mouse_y, s_click_frame);
						fflush(stderr);
					}
				}
			}

			m->notify_mouse_state(mouse_x, mouse_y, mouse_buttons);

				Uint32 t_advance = tu_timer::get_ticks();
				m->advance(delta_t * speed_scale);
				t_advance = tu_timer::get_ticks() - t_advance;

				if (do_sound && sound)
				{
					sound->advance(delta_t * speed_scale);
				}

				Uint32 t_display = tu_timer::get_ticks();
				if (do_render)
				{
					m->display();
				}
				t_display = tu_timer::get_ticks() - t_display;

			if (do_render)
			{
				// PRE-SWAP DIAGNOSTIC: Check framebuffer state
				{
					static int diag_count = 0;
					if (diag_count < 5) {
						GLint fbo_binding = 0, draw_buf = 0, read_buf = 0;
						glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo_binding);
						glGetIntegerv(GL_DRAW_BUFFER, &draw_buf);
						glGetIntegerv(GL_READ_BUFFER, &read_buf);
						// Check color mask
						GLboolean cm[4];
						glGetBooleanv(GL_COLOR_WRITEMASK, cm);
						// Read a pixel right before swap
						unsigned char pixel[4] = {0,0,0,0};
						glReadPixels(width/2, height/2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
						GLenum rerr = glGetError();
						fprintf(stderr, "[PRESWAP] FBO=%d DRAW=0x%X READ=0x%X ColorMask=%d%d%d%d pixel=(%d,%d,%d,%d) GLerr=%d\n",
							fbo_binding, draw_buf, read_buf,
							cm[0], cm[1], cm[2], cm[3],
							pixel[0], pixel[1], pixel[2], pixel[3], rerr);
						fprintf(stderr, "[PRESWAP] g_sdl_window=%p context=%p\n",
							(void*)g_sdl_window, (void*)g_gl_context);
						fflush(stderr);
						diag_count++;
					}
				}

				// DIAG: dump the framebuffer to a PPM so we can see the real frame.
				{
					extern int g_mesh_diag_frames;
					static int s_fb_dumps = 0;
					bool want = (g_mesh_diag_frames <= 3)
						|| ((g_mesh_diag_frames % 60) == 0);
					if (want && s_fb_dumps < 40)
					{
						s_fb_dumps++;
						int nw = width, nh = height;
						unsigned char* buf = new unsigned char[(size_t) nw * nh * 4];
						glReadPixels(0, 0, nw, nh, GL_RGBA, GL_UNSIGNED_BYTE, buf);
						char fn[256];
						sprintf(fn, "temp/fb_%04d.ppm", g_mesh_diag_frames);
						FILE* f = fopen(fn, "wb");
						if (f)
						{
							fprintf(f, "P6\n%d %d\n255\n", nw, nh);
							for (int y = nh - 1; y >= 0; y--)
							{
								const unsigned char* row = buf + (size_t) y * nw * 4;
								for (int x = 0; x < nw; x++)
								{
									fputc(row[x * 4 + 0], f);
									fputc(row[x * 4 + 1], f);
									fputc(row[x * 4 + 2], f);
								}
							}
							fclose(f);
							fprintf(stderr, "[FBDUMP] %s\n", fn);
						}
						else
						{
							fprintf(stderr, "[FBDUMP] FAILED %s\n", fn);
						}
						delete[] buf;
						fflush(stderr);
					}
				}

				Uint32 t_swap = tu_timer::get_ticks();
				glFinish();	// ensure all GL commands complete before swap
				// Bypass SDL_GL_SwapBuffers macro - call SDL_GL_SwapWindow directly
				SDL_GL_SwapWindow(g_sdl_window);
				{
					static int swap_count = 0;
					if (swap_count < 3) {
						fprintf(stderr, "[SWAP] SwapWindow called, g_sdl_window=%p\n", (void*)g_sdl_window);
						fflush(stderr);
						swap_count++;
					}
				}
					t_swap = tu_timer::get_ticks() - t_swap;
					//glPopAttrib ();


					frame_counter++;

					// Log the frame rate every second or so.
					if (last_ticks - last_logged_fps > 1000)
					{
						float	delta = (last_ticks - last_logged_fps) / 1000.f;
						fps = (int) ((float) frame_counter / delta);
						last_logged_fps = last_ticks;
						frame_counter = 0;
					}

					if (s_measure_performance == false)
					{
						// Don't hog the CPU.
						SDL_Delay(s_delay);
					}
					else
					{
						printf("fps = %d\n", fps);
					}

					// for perfomance testing
//					printf("advance time: %d, display time %d, swap buffers time = %d\n",
//						t_advance, t_display, t_swap);

#ifdef HAVE_PERFOMANCE_INFO
					char buffer[8];
					snprintf(buffer, 8, "%03d", t_advance);
					m->set_variable("t_Advance", buffer);
					snprintf(buffer, 8, "%03d", t_display);
					m->set_variable("t_Display", buffer);
					snprintf(buffer, 8, "%03d", t_swap);
					m->set_variable("t_SwapBuffers", buffer);
					snprintf(buffer, 8, "%d", fps);
					m->set_variable("FPS", buffer);
#endif
				}

				// TODO: clean up this interface and re-enable.
				//		gameswf_tesselate_dump_shape = false;  ///xxxxx

				// See if we should exit.
				if (do_loop == false && m->get_current_frame() + 1 == m->get_frame_count())
				{
					// We're reached the end of the movie; exit.
					break;
				}

				// Debug: check if movie is still alive
				if (m == NULL)
				{
					fprintf(stderr, "ERROR: root movie became NULL!\n");
					break;
				}
			}

	done:


			gameswf::set_sound_handler(NULL);
			delete sound;

			gameswf::set_render_handler(NULL);
			delete render;

			SDL_Quit();
		}

	}	// for testing memory leaks

	tu_memdebug::close();

	return 0;
}

// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:
