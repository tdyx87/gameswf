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
#include <math.h>
#include <vector>
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
#include "gameswf/gameswf_sprite.h"
#include "gameswf/gameswf_movie_def.h"
#include "gameswf/gameswf.h"
#include "base/container.h"

#if TU_ENABLE_NETWORK == 1
#	include "net/tu_net_file.h"
#endif

#ifdef _WIN32
#	include <Winsock.h>
#	include <windows.h>
#	include <mmsystem.h>
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
// Antialiasing: 4x MSAA by default.  Vector art is the bulk of what this
	// player draws and without MSAA every edge is hard-aliased, which is the
	// single biggest reason our output looks coarser than the Flash Player's.
	// Measured against flashplayer.exe, 4x MSAA cut the "content present in
	// Flash but flat in ours" pixels by ~3.6x.  Pass "-a 1" to turn it off,
	// or "-a N" to pick a different sample count.
	static int s_aa_level = 4;

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

	struct aim_cache_t { char name[64]; int x, y; };
static aim_cache_t s_aim_cache[32];
static int s_aim_cache_count = 0;
static int s_aim_last_search = -1000000;

static bool aim_cached(const char* name, int* px, int* py)
{
	for (int i = 0; i < s_aim_cache_count; i++)
	{
		if (strcmp(s_aim_cache[i].name, name) == 0)
		{
			*px = s_aim_cache[i].x;
			*py = s_aim_cache[i].y;
			return true;
		}
	}
	return false;
}

static void aim_store(const char* name, int x, int y)
{
	for (int i = 0; i < s_aim_cache_count; i++)
	{
		if (strcmp(s_aim_cache[i].name, name) == 0)
		{
			s_aim_cache[i].x = x;
			s_aim_cache[i].y = y;
			return;
		}
	}
	if (s_aim_cache_count < 32)
	{
		strncpy(s_aim_cache[s_aim_cache_count].name, name, 63);
		s_aim_cache[s_aim_cache_count].name[63] = 0;
		s_aim_cache[s_aim_cache_count].x = x;
		s_aim_cache[s_aim_cache_count].y = y;
		s_aim_cache_count++;
	}
}

// When GAMESWF_IGNORE_REAL_MOUSE=1 the harness drops OS mouse motion and
// button events, so a test driven purely by -clkms/-clickname/-dragto is
// reproducible even if a real pointer is moving over the window.
static bool ignore_real_mouse_input()
{
	static int s_ignore = -1;
	if (s_ignore < 0)
	{
		const char* e = getenv("GAMESWF_IGNORE_REAL_MOUSE");
		s_ignore = (e != NULL && e[0] == '1') ? 1 : 0;
	}
	return s_ignore == 1;
}

// Optional stderr tracing of raw SDL mouse events, for harness debugging.
static bool log_sdl_mouse_events()
{
	static int s_log = -1;
	if (s_log < 0)
	{
		const char* e = getenv("GAMESWF_TRACE_SDL_MOUSE");
		s_log = (e != NULL && e[0] == '1') ? 1 : 0;
	}
	return s_log == 1;
}

// -gestures <file>: a scripted, fully-automated gesture sequence.
//
// The -clk*/-drag* tasks all run concurrently every loop and share one
// pointer state, so the later task in the frame clobbers the earlier one
// and tricky interactions (press, hold, travel, release over a moving
// target) cannot be expressed.  A gesture script instead runs strictly in
// order, one entry at a time, and fixes the pointer for as many frames as
// each entry needs.  That is enough to replay an entire drag-driven level
// without a real mouse (combine with GAMESWF_IGNORE_REAL_MOUSE=1).
//
// One command per line; '#' starts a comment; blank lines are ignored:
//
//   waitms   <ms>                 wait until <ms> since start
//   waitframe <n>                 wait until the frame of the -framechar clip
//                                  (default stepMC) reaches n
//   waitname  <name>              wait until a clip named <name> exists
//   waithit   <x> <y> <name>      wait until the topmost clip at x,y is <name>
//   moveto    <x> <y>             move the pointer (1 frame, button unchanged)
//   press                         hold the left button (1 frame, current pos)
//   release                       release the left button (1 frame)
//   click     <x> <y> [hover]     hover, press, release at x,y
//   clickname <name> [hover]      like click, but aim at where a press would
//                                 really land on the clip named <name>
//   drag      <x1> <y1> <x2> <y2> [dur]
//                                 hover on the grab point, press, travel in a
//                                 straight line over dur frames, release
//   dragpath  <dur> <x1> <y1> <x2> <y2> ...
//                                 press on the first point, travel through the
//                                 remaining waypoints over dur frames, release
//   movepath  <dur> <x1> <y1> <x2> <y2> ...
//                                 like dragpath but with the button up (for
//                                 steps that only watch mouse movement)
//   home                          move the pointer to 0,0 and drop the button
//   quit                          stop the player once earlier entries finish
//
// Every press is preceded by a short hover so root::generate_mouse_button_events
// has a roll-over frame to record the active entity on, exactly like a person
// moving the pointer onto the target before clicking.
enum gesture_kind_t
{
	GK_MOVETO, GK_PRESS, GK_RELEASE, GK_CLICK, GK_CLICKNAME, GK_DRAG, GK_DRAGPATH, GK_MOVEPATH,
	GK_WASH, GK_WAITMS, GK_WAITFRAME, GK_WAITNAME, GK_WAITHIT, GK_DUMP, GK_QUIT,
	GK_DRAGNAME, GK_LISTPRESS
};

struct scheduled_gesture_t
{
	int		kind;
	int		x, y;			// point (moveto/click; hit point for waithit)
	int		x1, y1, x2, y2;		// drag endpoints
	int		dur;			// frames for click hover / drag travel
	int		wait_ms;		// absolute ms for waitms
	int		wait_frame;		// target frame for waitframe
	tu_string	name;			// waitname / waithit target
	std::vector<int>	path;		// dragpath flat [x0,y0,x1,y1,...]
	// runtime
	int		step;			// frames elapsed inside this entry
};

static bool	gesture_load(const char* path, std::vector<scheduled_gesture_t>& out)
{
	FILE* f = fopen(path, "rb");
	if (f == NULL)
	{
		fprintf(stderr, "-gestures: cannot open '%s'\n", path);
		return false;
	}

	char	line[2048];
	int	lineno = 0;
	while (fgets(line, sizeof(line), f) != NULL)
	{
		lineno++;
		char* hash = strchr(line, '#');
		if (hash) { *hash = 0; }

		char*	tok[40];
		int	nt = 0;
		char* p = strtok(line, " \t\r\n");
		while (p != NULL && nt < 40) { tok[nt++] = p; p = strtok(NULL, " \t\r\n"); }
		if (nt == 0) { continue; }

		scheduled_gesture_t	g;
		g.kind = -1; g.x = g.y = g.x1 = g.y1 = g.x2 = g.y2 = 0;
		g.dur = 0; g.wait_ms = 0; g.wait_frame = 0; g.name = ""; g.step = 0;

		const char* c0 = tok[0];
		if (strcmp(c0, "waitms") == 0 && nt >= 2)		{ g.kind = GK_WAITMS; g.wait_ms = atoi(tok[1]); }
		else if (strcmp(c0, "waitframe") == 0 && nt >= 2)	{ g.kind = GK_WAITFRAME; g.wait_frame = atoi(tok[1]); }
		else if (strcmp(c0, "waitname") == 0 && nt >= 2)	{ g.kind = GK_WAITNAME; g.name = tok[1]; }
		else if (strcmp(c0, "waithit") == 0 && nt >= 4)		{ g.kind = GK_WAITHIT; g.x = atoi(tok[1]); g.y = atoi(tok[2]); g.name = tok[3]; }
		else if (strcmp(c0, "moveto") == 0 && nt >= 3)		{ g.kind = GK_MOVETO; g.x = atoi(tok[1]); g.y = atoi(tok[2]); }
		else if (strcmp(c0, "press") == 0)			{ g.kind = GK_PRESS; }
		else if (strcmp(c0, "release") == 0)			{ g.kind = GK_RELEASE; }
		else if (strcmp(c0, "click") == 0 && nt >= 3)		{ g.kind = GK_CLICK; g.x = atoi(tok[1]); g.y = atoi(tok[2]); if (nt >= 4) { g.dur = atoi(tok[3]); } }
		else if (strcmp(c0, "clickname") == 0 && nt >= 2)	{ g.kind = GK_CLICKNAME; g.name = tok[1]; if (nt >= 3) { g.dur = atoi(tok[2]); } }
		else if (strcmp(c0, "drag") == 0 && nt >= 5)
		{
			g.kind = GK_DRAG;
			g.x1 = atoi(tok[1]); g.y1 = atoi(tok[2]); g.x2 = atoi(tok[3]); g.y2 = atoi(tok[4]);
			if (nt >= 6) { g.dur = atoi(tok[5]); }
		}
		else if (strcmp(c0, "dragpath") == 0 && nt >= 5)
		{
			g.kind = GK_DRAGPATH; g.dur = atoi(tok[1]);
			for (int i = 2; i < nt; i++) { g.path.push_back(atoi(tok[i])); }
		}
		else if (strcmp(c0, "movepath") == 0 && nt >= 5)
		{
			g.kind = GK_MOVEPATH; g.dur = atoi(tok[1]);
			for (int i = 2; i < nt; i++) { g.path.push_back(atoi(tok[i])); }
		}
		else if (strcmp(c0, "wash") == 0 && nt >= 4)
		{
			// wash <cx> <cy> <radius> [frames]: rub a small circle around
			// (cx,cy), button up.  Dense, single-target pointer motion, so
			// "rub the soap/rinse" steps accumulate their per-mouseMove
			// progress instead of resetting.
			g.kind = GK_WASH;
			g.x = atoi(tok[1]); g.y = atoi(tok[2]); g.x1 = atoi(tok[3]);
			if (nt >= 5) { g.dur = atoi(tok[4]); }
		}
		else if (strcmp(c0, "dragname") == 0 && nt >= 4)
		{
			// dragname <name> <x> <y> [dur]
			// Grab the clip called <name> at a point the engine's own hit
			// test agrees belongs to it, then drag it to (x,y) and release.
			// Resolving the grab point this way is much more reliable than
			// hard-coded coordinates: the clips in these levels are scaled
			// and stacked, so a hand-picked pixel often lands on a
			// neighbour and the drag never starts.
			g.kind = GK_DRAGNAME;
			g.name = tok[1];
			g.x2 = atoi(tok[2]); g.y2 = atoi(tok[3]);
			if (nt >= 5) { g.dur = atoi(tok[4]); }
		}
		else if (strcmp(c0, "listpress") == 0)		{ g.kind = GK_LISTPRESS; }
		else if (strcmp(c0, "dumptree") == 0)			{ g.kind = GK_DUMP; }
		else if (strcmp(c0, "home") == 0)			{ g.kind = GK_MOVETO; g.x = 0; g.y = 0; }
		else if (strcmp(c0, "quit") == 0)			{ g.kind = GK_QUIT; }
		else
		{
			fprintf(stderr, "-gestures: %s:%d: unknown command '%s'\n", path, lineno, c0);
			continue;
		}

		out.push_back(g);
	}
	fclose(f);
	return true;
}


int g_render_frames = 0;

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
	// pair at movie coordinates on a given loop frame.  When time_ms is
	// >= 0 the trigger is wall-clock milliseconds since start instead of a
	// loop frame: the movie timeline advances on real time (delta_t comes
	// from tu_timer), so loop-frame schedules land at a different movie
	// state on every machine / build / trace level and are not reproducible.
struct scheduled_click_t { int frame, x, y; int time_ms; int press_frame; bool fired; bool aim; tu_string want_name; int hover_frames; int hover_frame; };
	scheduled_click_t	auto_clicks[24];
	int	auto_click_count = 0;
bool	dump_tree = false;
bool	lv_trace = false;
int	dump_tree_ms = -1;
	// -drag: scheduled drag gestures (repeatable); hover, press at
	// (x1,y1) on <frame>, interpolate to (x2,y2) over <dur> frames
	// while held, then release.  time_ms >= 0 triggers on wall clock.
	struct scheduled_drag_t { int frame, x1, y1, x2, y2, dur; int time_ms; int press_frame; bool fired; tu_string want_name; };
	scheduled_drag_t	auto_drags[12];
	int	auto_drag_count = 0;
	// -gestures: strictly-ordered scripted gestures (see gesture_load above).
	std::vector<scheduled_gesture_t>	gestures;
	bool	gestures_on = false;
	tu_string	frame_char = "stepMC";	// clip watched by 'waitframe'
		float	tex_lod_bias;
		bool	force_realtime_framerate = true;

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
				if (auto_click_count >= 24)
				{
					fprintf(stderr, "-clk: too many clicks (max 24)\n");
					exit(1);
				}
				auto_clicks[auto_click_count].frame = atoi(argv[arg + 1]);
				auto_clicks[auto_click_count].x = atoi(argv[arg + 2]);
				auto_clicks[auto_click_count].y = atoi(argv[arg + 3]);
				auto_clicks[auto_click_count].time_ms = -1;
				auto_clicks[auto_click_count].press_frame = -1;
				auto_clicks[auto_click_count].fired = false;
				auto_clicks[auto_click_count].want_name = "";
				auto_clicks[auto_click_count].hover_frames = 0;
				auto_clicks[auto_click_count].hover_frame = -1;
				auto_click_count++;
				arg += 3;
				continue;
			}

			if (strcmp(argv[arg], "-clkms") == 0)
			{
				// Wall-clock variant of -clk: press/release at (x,y) once the
				// movie has been running for <ms> milliseconds.  Use this
				// instead of -clk whenever the expected result depends on how
				// far the movie timeline has progressed.
				if (arg + 3 >= argc)
				{
					fprintf(stderr, "-clkms needs three args: <ms> <x> <y>\n");
					exit(1);
				}
				if (auto_click_count >= 24)
				{
					fprintf(stderr, "-clkms: too many clicks (max 24)\n");
					exit(1);
				}
				auto_clicks[auto_click_count].frame = -1;
				auto_clicks[auto_click_count].x = atoi(argv[arg + 2]);
				auto_clicks[auto_click_count].y = atoi(argv[arg + 3]);
				auto_clicks[auto_click_count].time_ms = atoi(argv[arg + 1]);
				auto_clicks[auto_click_count].press_frame = -1;
				auto_clicks[auto_click_count].fired = false;
				auto_clicks[auto_click_count].want_name = "";
				auto_clicks[auto_click_count].hover_frames = 0;
				auto_clicks[auto_click_count].hover_frame = -1;
				auto_click_count++;
				arg += 3;
				continue;
			}

			if (strcmp(argv[arg], "-drag") == 0)
			{
				// Test hook: drag gesture.  Usage:
				//   -drag <frame> <x1> <y1> <x2> <y2> [durFrames]
				// Hover before <frame>, press at (x1,y1), move toward
				// (x2,y2) over durFrames (default 15) while held, release.
				if (arg + 5 >= argc)
				{
					fprintf(stderr, "-drag needs args: <frame> <x1> <y1> <x2> <y2> [dur]\n");
					exit(1);
				}
				if (auto_drag_count >= 12)
				{
					fprintf(stderr, "-drag: too many drags (max 12)\n");
					exit(1);
				}
				auto_drags[auto_drag_count].frame = atoi(argv[arg + 1]);
				auto_drags[auto_drag_count].x1 = atoi(argv[arg + 2]);
				auto_drags[auto_drag_count].y1 = atoi(argv[arg + 3]);
				auto_drags[auto_drag_count].x2 = atoi(argv[arg + 4]);
				auto_drags[auto_drag_count].y2 = atoi(argv[arg + 5]);
				auto_drags[auto_drag_count].dur = 15;
				auto_drags[auto_drag_count].time_ms = -1;
				auto_drags[auto_drag_count].press_frame = -1;
				auto_drags[auto_drag_count].fired = false;
				auto_drags[auto_drag_count].want_name = "";
				if (arg + 6 < argc && argv[arg + 6][0] >= '0' && argv[arg + 6][0] <= '9')
				{
					auto_drags[auto_drag_count].dur = atoi(argv[arg + 6]);
					arg += 6;
				}
				else
				{
					arg += 5;
				}
				auto_drag_count++;
				continue;
			}

			if (strcmp(argv[arg], "-dragms") == 0)
			{
				// Wall-clock variant of -drag.  Usage:
				//   -dragms <ms> <x1> <y1> <x2> <y2> [durFrames]
				if (arg + 5 >= argc)
				{
					fprintf(stderr, "-dragms needs args: <ms> <x1> <y1> <x2> <y2> [dur]\n");
					exit(1);
				}
				if (auto_drag_count >= 12)
				{
					fprintf(stderr, "-dragms: too many drags (max 12)\n");
					exit(1);
				}
				auto_drags[auto_drag_count].frame = -1;
				auto_drags[auto_drag_count].x1 = atoi(argv[arg + 2]);
				auto_drags[auto_drag_count].y1 = atoi(argv[arg + 3]);
				auto_drags[auto_drag_count].x2 = atoi(argv[arg + 4]);
				auto_drags[auto_drag_count].y2 = atoi(argv[arg + 5]);
				auto_drags[auto_drag_count].dur = 15;
				auto_drags[auto_drag_count].time_ms = atoi(argv[arg + 1]);
				auto_drags[auto_drag_count].press_frame = -1;
				auto_drags[auto_drag_count].fired = false;
				auto_drags[auto_drag_count].want_name = "";
				if (arg + 6 < argc && argv[arg + 6][0] >= '0' && argv[arg + 6][0] <= '9')
				{
					auto_drags[auto_drag_count].dur = atoi(argv[arg + 6]);
					arg += 6;
				}
				else
				{
					arg += 5;
				}
				auto_drag_count++;
				continue;
			}

			if (strcmp(argv[arg], "-dragto") == 0)
			{
				// State-triggered drag: hover at (x1,y1) and only press once
				// the hit test there resolves to the display object named
				// <name>; then drag to (x2,y2).  Frame- and time-numbered
				// triggers are both unreliable here: the movie timeline runs
				// on clamped real time, so how far the movie has progressed
				// at a given loop frame (or wall-clock second) depends on
				// how fast the host renders, which changes with tracing,
				// frame dumping and machine speed.  Waiting for the actual
				// object makes the gesture reproducible.  Usage:
				//   -dragto <name> <x1> <y1> <x2> <y2> [durFrames]
				if (arg + 5 >= argc)
				{
					fprintf(stderr, "-dragto needs args: <name> <x1> <y1> <x2> <y2> [dur]\n");
					exit(1);
				}
				if (auto_drag_count >= 12)
				{
					fprintf(stderr, "-dragto: too many drags (max 12)\n");
					exit(1);
				}
				auto_drags[auto_drag_count].frame = -1;
				auto_drags[auto_drag_count].x1 = atoi(argv[arg + 2]);
				auto_drags[auto_drag_count].y1 = atoi(argv[arg + 3]);
				auto_drags[auto_drag_count].x2 = atoi(argv[arg + 4]);
				auto_drags[auto_drag_count].y2 = atoi(argv[arg + 5]);
				auto_drags[auto_drag_count].dur = 15;
				auto_drags[auto_drag_count].time_ms = -1;
				auto_drags[auto_drag_count].press_frame = -1;
				auto_drags[auto_drag_count].fired = false;
				auto_drags[auto_drag_count].want_name = argv[arg + 1];
				if (arg + 6 < argc && argv[arg + 6][0] >= '0' && argv[arg + 6][0] <= '9')
				{
					auto_drags[auto_drag_count].dur = atoi(argv[arg + 6]);
					arg += 6;
				}
				else
				{
					arg += 5;
				}
				auto_drag_count++;
				continue;
			}

			if (strcmp(argv[arg], "-clickto") == 0)
			{
				// State-triggered click: click (x,y) only once the hit test
				// there resolves to the display object named <name>, then
				// release.  Same rationale as -dragto: the movie timeline
				// runs on clamped real time, so a frame- or time-numbered
				// click lands at a different game state on every host.
				// Usage: -clickto <name> <x> <y>
				if (arg + 3 >= argc)
				{
					fprintf(stderr, "-clickto needs args: <name> <x> <y>\n");
					exit(1);
				}
				if (auto_click_count >= 24)
				{
					fprintf(stderr, "-clickto: too many clicks (max 24)\n");
					exit(1);
				}
				auto_clicks[auto_click_count].frame = -1;
				auto_clicks[auto_click_count].x = atoi(argv[arg + 2]);
				auto_clicks[auto_click_count].y = atoi(argv[arg + 3]);
				auto_clicks[auto_click_count].time_ms = -1;
				auto_clicks[auto_click_count].press_frame = -1;
				auto_clicks[auto_click_count].fired = false;
				auto_clicks[auto_click_count].aim = false;
				auto_clicks[auto_click_count].want_name = argv[arg + 1];
				auto_clicks[auto_click_count].hover_frames = 0;
				auto_clicks[auto_click_count].hover_frame = -1;
				auto_click_count++;
				arg += 3;
				continue;
			}

			if (strcmp(argv[arg], "-clickname") == 0)
			{
				// Click the object itself: find the display object called
				// <name> anywhere in the tree, aim at the centre of where it
				// is actually drawn, and press once it is reachable there.
				//
				// This is the difference between a test that plays the game
				// and one that replays three magic coordinates: everything
				// that moves, scales or sits at another depth is followed
				// automatically, exactly like a person aiming at a button.
				// Usage: -clickname <name>
				if (arg + 1 >= argc)
				{
					fprintf(stderr, "-clickname needs arg: <name>\n");
					exit(1);
				}
				if (auto_click_count >= 24)
				{
					fprintf(stderr, "-clickname: too many clicks (max 24)\n");
					exit(1);
				}
				auto_clicks[auto_click_count].frame = -1;
				auto_clicks[auto_click_count].x = -1;
				auto_clicks[auto_click_count].y = -1;
				auto_clicks[auto_click_count].time_ms = -1;
				auto_clicks[auto_click_count].press_frame = -1;
				auto_clicks[auto_click_count].fired = false;
				auto_clicks[auto_click_count].aim = true;
				auto_clicks[auto_click_count].want_name = argv[arg + 1];
				auto_clicks[auto_click_count].hover_frames = 0;
				auto_clicks[auto_click_count].hover_frame = -1;
				auto_click_count++;
				arg += 1;
				continue;
			}

			if (strcmp(argv[arg], "-lvtrace") == 0)
			{
				// Log the doll's garment layers (frame + play state) whenever
				// they change, for a whole play-through.
				// Usage: -lvtrace
				lv_trace = true;
				arg += 0;
				continue;
			}

			if (strcmp(argv[arg], "-dumptree") == 0)
			{
				// Print every named display object with the position it is
				// drawn at.  Used to find out what the game actually calls its
				// controls instead of guessing coordinates.
				// Usage: -dumptree <ms>
				if (arg + 1 >= argc)
				{
					fprintf(stderr, "-dumptree needs arg: <ms>\n");
					exit(1);
				}
				dump_tree_ms = atoi(argv[arg + 1]);
				dump_tree = true;
				arg += 1;
				continue;
			}

			if (strcmp(argv[arg], "-gestures") == 0)
			{
				// Scripted, fully-automated gesture sequence; see gesture_load.
				// Usage: -gestures <file>
				if (arg + 1 >= argc)
				{
					fprintf(stderr, "-gestures needs arg: <file>\n");
					exit(1);
				}
				if (!gesture_load(argv[arg + 1], gestures))
				{
					exit(1);
				}
				gestures_on = !gestures.empty();
				fprintf(stderr, "-gestures: loaded %d entries from %s\n",
					(int) gestures.size(), argv[arg + 1]);
				fflush(stderr);
				arg += 1;
				continue;
			}

			if (strcmp(argv[arg], "-framechar") == 0)
			{
				// Clip whose timeline 'waitframe' watches.  Default stepMC.
				// Usage: -framechar <name>
				if (arg + 1 >= argc)
				{
					fprintf(stderr, "-framechar needs arg: <name>\n");
					exit(1);
				}
				frame_char = argv[arg + 1];
				arg += 1;
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

			// --- Deterministic self-test for the gensui 730/733 resolution bug ---
			// Build an isolated child sprite (acting like stepMC) and place two
			// DIFFERENT character ids at the SAME depth with the SAME name
			// ("gensui"), exactly like the SWF does at frames 146/148.  Then
			// resolve this["gensui"] and assert it points at the LATEST cid.
			if (getenv("GW_SELFTEST"))
			{
				gameswf::character* rm = m->get_root_movie();
				gameswf::sprite_instance* test = new gameswf::sprite_instance(
					player,
					static_cast<gameswf::movie_definition_sub*>(m->get_movie_definition()),
					m.get_ptr(), rm, 9999);
				array<gameswf::swf_event*> eh;
				gameswf::cxform cx;
				gameswf::matrix mat;
				test->add_display_object(716, "gensui", eh, 488, true, cx, mat, 0, 0, 0);
				gameswf::as_value v1; test->get_member("gensui", &v1);
				int after716 = -1;
				if (v1.is_object())
				{
					gameswf::character* c1 = static_cast<gameswf::character*>(v1.to_object());
					if (c1) after716 = c1->get_id();
				}
				test->add_display_object(733, "gensui", eh, 488, true, cx, mat, 0, 0, 0);
				gameswf::as_value v2; test->get_member("gensui", &v2);
				int after733 = -1;
				if (v2.is_object())
				{
					gameswf::character* c2 = static_cast<gameswf::character*>(v2.to_object());
					if (c2) after733 = c2->get_id();
				}
				fprintf(stderr, "[SELFTEST-RESOLVE] gensui after place 716 -> cid=%d ; after place 733 -> cid=%d (expect 733)\n",
					after716, after733);
				fprintf(stderr, "[SELFTEST-RESULT] %s\n", (after733 == 733) ? "PASS" : "FAIL");
			}

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

				// Background music: the SWF itself carries no audio, so the
				// music the site plays comes from an external file.  If a
				// bgm.wav sits next to the movie (or is named via
				// GAMESWF_BGM), loop it by default.  GAMESWF_BGM=0 or
				// GAMESWF_NO_BGM=1 disables it.
				if (do_sound)
				{
					const char* no_bgm = getenv("GAMESWF_NO_BGM");
					const char* bgm_path = getenv("GAMESWF_BGM");
					const char* candidates[3];
					int ncand = 0;
					static char override_bgm[600];
					static char dir_bgm[600];
					static char exe_bgm[600];
					if (bgm_path && bgm_path[0] && strcmp(bgm_path, "0") != 0)
					{
						strcpy(override_bgm, bgm_path);
						candidates[ncand++] = override_bgm;
					}
					if (infile != NULL && infile[0])
					{
						strcpy(dir_bgm, infile);
						char* s1 = strrchr(dir_bgm, '/');
						char* s2 = strrchr(dir_bgm, '\\');
						char* last = (s1 > s2) ? s1 : s2;
						if (last != NULL)
							last[1] = '\0';
						else
							strcpy(dir_bgm, ".\\");
						strcat(dir_bgm, "bgm.wav");
						candidates[ncand++] = dir_bgm;
					}
					{
						char exe_dir_buf[520];
						DWORD exelen = GetModuleFileNameA(NULL, exe_dir_buf, sizeof(exe_dir_buf) - 10);
						char* slash = (exelen > 0) ? strrchr(exe_dir_buf, '\\') : NULL;
						if (slash != NULL)
						{
							slash[1] = '\0';
							strcat(exe_dir_buf, "bgm.wav");
							strcpy(exe_bgm, exe_dir_buf);
							candidates[ncand++] = exe_bgm;
						}
					}
					bool want_bgm = (no_bgm == NULL || no_bgm[0] != '1');
					if (want_bgm)
					{
						for (int ci = 0; ci < ncand; ci++)
						{
							DWORD attr = GetFileAttributesA(candidates[ci]);
							if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
							{
								PlaySoundA(candidates[ci], NULL, SND_FILENAME | SND_ASYNC | SND_LOOP | SND_NODEFAULT);
								break;
							}
						}
					}
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
				// rendering �?producing a black screen despite valid GL calls.
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

			// -gestures playback cursor: only the entry at gestures[g_index]
			// acts, and it must finish before the next one starts.
			int	g_index = 0;
			bool	g_quit = false;

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
					fprintf(stderr, "[EXIT] timeout\n");
					fflush(stderr);
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
								fprintf(stderr, "[EXIT] window close\n");
								fflush(stderr);
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
									fprintf(stderr, "[EXIT] key %d\n", (int) key);
									fflush(stderr);
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
							if (ignore_real_mouse_input()) break;
							mouse_x = (int) (event.motion.x / scale_x);
							mouse_y = (int) (event.motion.y / scale_y);
							if (log_sdl_mouse_events())
							{
								fprintf(stderr, "[SDLMOT] %d,%d\n", mouse_x, mouse_y);
								fflush(stderr);
							}
							break;

					case SDL_MOUSEBUTTONDOWN:
					case SDL_MOUSEBUTTONUP:
							{
								if (ignore_real_mouse_input()) break;
								if (log_sdl_mouse_events())
								{
									fprintf(stderr, "[SDLBTN] button=%d state=%d at %d,%d\n",
										(int) event.button.button, (int) event.button.state,
										(int) event.button.x, (int) event.button.y);
									fflush(stderr);
								}
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
							fprintf(stderr, "[EXIT] SDL_QUIT\n");
							fflush(stderr);
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

			int elapsed_ms = (int) (ticks - start_ticks);

			// Test hook (-clk): move to the target point, press on the given
			// loop frame and release two frames later, so root::on_mouse_event
			// sees a real press -> release on the same character (which is what
			// turns into an AS3 "click").  Multiple -clk options run in order.
			if (auto_click_count > 0 && !gestures_on)
			{
				static int s_click_frame = 0;
				s_click_frame++;

				for (int ci = 0; ci < auto_click_count; ci++)
				{
					scheduled_click_t& c = auto_clicks[ci];
					int frame = c.frame;

					if (c.want_name.size() > 0)
					{
						// State-triggered (-clickto / -clickname): wait until the
						// click can actually land on the requested object.
						// The pointer is not touched while waiting, so other
						// scheduled gestures in the same frame are unaffected.
						if (!c.fired)
						{
							int want_x = c.x;
							int want_y = c.y;

							if (c.aim)
							{
								// Every one of the game's controls is already
								// on screen at the same time, so "wait until
								// it is clickable" fires them all on a single
								// frame - each press then lands before the game
								// has reacted to the previous one.  Hold off
								// until the game has had time to settle.
								static int s_last_aim_fire = -1000000;
								if (elapsed_ms - s_last_aim_fire < 1500)
								{
									continue;
								}

								// Track the object wherever it is drawn right
								// now, and only take the shot once we have
								// found a point a press would really land on
								// it - i.e. exactly where a person would
								// click.  The search is expensive, so the
								// result is cached and only redone (rate
								// limited) if the cached point stops being
								// valid, e.g. after the screen changed.
								int cx = 0, cy = 0;
								bool have = aim_cached(c.want_name.c_str(), &cx, &cy);
								if (have)
								{
									gameswf::character* te =
										m->hit_test_pixel((float) cx, (float) cy);
									have = te != NULL
										&& tu_string(te->get_name().c_str()) == c.want_name;
								}
								if (!have)
								{
									if (elapsed_ms - s_aim_last_search < 300)
									{
										continue;
									}
									s_aim_last_search = elapsed_ms;

									float fx = 0, fy = 0;
									gameswf::character* ch =
										m->find_named_in_tree(c.want_name.c_str());
									if (ch == NULL
										|| !m->find_click_point(ch, &fx, &fy))
									{
										continue;
									}
									cx = (int) fx;
									cy = (int) fy;
								}
								want_x = cx;
								want_y = cy;
								aim_store(c.want_name.c_str(), cx, cy);
								s_last_aim_fire = elapsed_ms;
							}
							else
							{
								gameswf::character* target_te =
									m->hit_test_pixel((float) want_x, (float) want_y);
								if (target_te == NULL
									|| tu_string(target_te->get_name().c_str()) != c.want_name)
								{
									continue;
								}
							}

							// Freeze the aim point now: the object may keep
							// moving after the press, and the release has to
							// land where the press did.
							c.x = want_x;
							c.y = want_y;
							c.fired = true;
							c.press_frame = s_click_frame;
						}
						frame = c.press_frame;
				}

				if (c.time_ms >= 0)
				{
					// Wall-clock scheduled (-clkms): wait until the movie has
					// been running long enough, then press; release two
					// loop frames later.
					int now_ms = (int) (ticks - start_ticks);
					if (!c.fired)
					{
						// Park the pointer on the target as soon as we get
						// near the trigger time, and keep it there so the
						// roll-over update can pick the entity up.  Count
						// FRAMES, not milliseconds: a fixed 100 ms lead is
						// shorter than a single frame whenever the movie runs
						// slowly (heavy FB dumps, a loaded machine), and the
						// hover then never lands at all before the press --
						// which is why -clkms clicks used to silently do
						// nothing.
						if (now_ms >= c.time_ms - 100)
						{
							mouse_x = c.x;
							mouse_y = c.y;
							if (s_click_frame != c.hover_frame)
							{
								c.hover_frames++;
								c.hover_frame = s_click_frame;
							}
						}
						if (now_ms < c.time_ms)
						{
							continue;
						}
						if (c.hover_frames < 2)
						{
							// Not enough hover frames yet; wait rather than
							// pressing into a movie that has no target.
							continue;
						}
						// Arm on this frame and fall through, so the
						// press really happens now.  Continuing here
						// used to skip the press and still fire the
						// release two frames later, so -clkms clicks
						// never reached the game at all.
						c.fired = true;
						c.press_frame = s_click_frame;
					}
					frame = c.press_frame;
				}

					// Hover a few frames before pressing.  root::generate_mouse_button_events
					// only records m_active_entity while the button is UP (ROLL_OVER), so a
					// press on the very frame the cursor arrives would see active==NULL and
					// neither PRESS nor RELEASE would ever reach the character.
					if (c.time_ms < 0
						&& s_click_frame >= frame - 4 && s_click_frame <= frame + 2)
					{
						mouse_x = c.x;
						mouse_y = c.y;
					}

					if (s_click_frame == frame)
					{
						mouse_x = c.x;
						mouse_y = c.y;
						mouse_buttons = 1;
						fprintf(stderr, "[CLK] press at %d,%d loop_frame=%d%s\n",
							mouse_x, mouse_y, s_click_frame,
							c.time_ms >= 0 ? " (wall-clock)" : "");
						fflush(stderr);
					}
					else if (s_click_frame == frame + 2)
					{
						mouse_buttons = 0;
						fprintf(stderr, "[CLK] release at %d,%d loop_frame=%d%s\n",
							mouse_x, mouse_y, s_click_frame,
							c.time_ms >= 0 ? " (wall-clock)" : "");
						fflush(stderr);
					}
				}
			}

			// Test hook (-lvtrace): follow the doll's garment layers and log each one
			// when its frame or play state changes.  Cheap enough to leave on
			// for a whole play-through done by hand.
			if (lv_trace)
			{
				m->trace_outfit_layers();
			}

			// Test hook (-dumptree): list the named display objects and where they are.
			if (dump_tree)
			{
				static int s_last_dump = -100000;
				if (elapsed_ms >= dump_tree_ms && elapsed_ms - s_last_dump >= 4000)
				{
					s_last_dump = elapsed_ms;
					fprintf(stderr, "\n=== tree @%dms ===\n", elapsed_ms);
					m->dump_named_tree();
				}
			}

			// Test hook (-drag): hover, press, interpolated move while
			// held, release.  Independent of -clk.
			if (auto_drag_count > 0 && !gestures_on)
			{
				static int s_drag_frame = 0;
				s_drag_frame++;

				for (int di = 0; di < auto_drag_count; di++)
				{
					scheduled_drag_t& d = auto_drags[di];
					int dur = d.dur > 0 ? d.dur : 15;
					int start = d.frame;

					if (d.want_name.size() > 0)
					{
						// State-triggered (-dragto): keep the cursor parked on
						// the grab point and press only once the hit test
						// there resolves to the requested object.  Until then
						// the button stays up so the press frame itself still
						// sees the right roll-over target.
						if (d.fired)
						{
							start = d.press_frame;
						}
						else
						{
							// Wait for the target to appear under the grab
							// point.  The cursor is deliberately NOT moved
							// while waiting: the hit test is done directly
							// against (x1,y1), and stealing the pointer
							// would clobber any -clk/-drag scheduled in the
							// same frame (this block runs after them).
							gameswf::character* target_te =
								m->hit_test_pixel((float) d.x1, (float) d.y1);
							if (target_te != NULL)
							{
								if (tu_string(target_te->get_name().c_str()) == d.want_name)
								{
									d.fired = true;
									d.press_frame = s_drag_frame;
									start = s_drag_frame;
								}
								else
								{
									continue;
								}
							}
							else
							{
								continue;
							}
						}
					}

					if (d.time_ms >= 0)
					{
						// Wall-clock scheduled (-dragms): wait until the movie
						// has run long enough, then pin the drag to loop frames
						// starting now.  The movie timeline advances on real time
						// while these triggers used to count loop frames, so a
						// frame-numbered drag lands at a different movie state
						// on every machine and is not reproducible.
						int now_ms = (int) (ticks - start_ticks);
						if (!d.fired)
						{
							if (now_ms >= d.time_ms - 100)
							{
								mouse_x = d.x1;
								mouse_y = d.y1;
							}
							if (now_ms >= d.time_ms)
							{
								d.fired = true;
								d.press_frame = s_drag_frame;
								start = s_drag_frame;
							}
							else
							{
								continue;
							}
						}
						else
						{
							start = d.press_frame;
						}
					}
					int end = start + dur;

					if (s_drag_frame >= start - 4 && s_drag_frame < start)
					{
						mouse_x = d.x1;
						mouse_y = d.y1;
					}
					else if (s_drag_frame == start)
					{
						mouse_x = d.x1;
						mouse_y = d.y1;
						mouse_buttons = 1;
						fprintf(stderr, "[DRAG] press at %d,%d loop_frame=%d%s\n",
							mouse_x, mouse_y, s_drag_frame,
							d.time_ms >= 0 ? " (wall-clock)" : "");
						fflush(stderr);
					}
					else if (s_drag_frame > start && s_drag_frame <= end)
					{
						int step = s_drag_frame - start;
						mouse_x = d.x1 + (d.x2 - d.x1) * step / dur;
						mouse_y = d.y1 + (d.y2 - d.y1) * step / dur;
						mouse_buttons = 1;
						if (step == 1 || step == dur / 2 || step == dur)
						{
							fprintf(stderr, "[DRAG] move to %d,%d loop_frame=%d%s\n",
								mouse_x, mouse_y, s_drag_frame,
								d.time_ms >= 0 ? " (wall-clock)" : "");
							fflush(stderr);
						}
					}
					else if (s_drag_frame == end + 1)
					{
						mouse_x = d.x2;
						mouse_y = d.y2;
						mouse_buttons = 0;
						fprintf(stderr, "[DRAG] release at %d,%d loop_frame=%d%s\n",
							mouse_x, mouse_y, s_drag_frame,
							d.time_ms >= 0 ? " (wall-clock)" : "");
						fflush(stderr);
					}
				}
			}

			// -gestures playback: run exactly one scripted entry at a time.
			// Wait entries block (the pointer is left untouched) until their
			// condition holds; action entries then own the pointer for the
			// frames they need.  This is what lets a press/travel/release be
			// replayed without a real mouse.
			if (gestures_on && !g_quit && g_index < (int) gestures.size())
			{
				scheduled_gesture_t& g = gestures[g_index];
				bool done = false;

				switch (g.kind)
				{
				case GK_WAITMS:
					if ((int) (ticks - start_ticks) >= g.wait_ms) { done = true; }
					break;

				case GK_WAITFRAME:
					{
						gameswf::character* fc = m->find_named_in_tree(frame_char.c_str());
						if (fc != NULL && fc->get_current_frame() >= g.wait_frame) { done = true; }
					}
					break;

				case GK_WAITNAME:
					if (m->find_named_in_tree(g.name.c_str()) != NULL) { done = true; }
					break;

				case GK_WAITHIT:
					{
						gameswf::character* te = m->hit_test_pixel((float) g.x, (float) g.y);
						if (te != NULL && tu_string(te->get_name().c_str()) == g.name) { done = true; }
					}
					break;

				case GK_DUMP:
					m->dump_named_tree();
					done = true;
					break;

				case GK_MOVETO:
					mouse_x = g.x;
					mouse_y = g.y;
					done = true;
					break;

				case GK_PRESS:
					mouse_buttons |= 1;
					done = true;
					break;

				case GK_RELEASE:
					mouse_buttons &= ~1;
					done = true;
					break;

				case GK_CLICKNAME:
					{
						// Resolve the aim point lazily (the clip may not be
						// pressable yet), then behave exactly like GK_CLICK.
						// find_click_point sweeps the clip's area, so it is
						// rate limited while waiting.
						if (g.step == 0 || (g.step % 20) == 0)
						{
							gameswf::character* ch = m->find_named_in_tree(g.name.c_str());
							float cx = 0, cy = 0;
							if (ch != NULL && m->find_click_point(ch, &cx, &cy))
							{
								g.x = (int) cx;
								g.y = (int) cy;
								g.step = 0;
								g.kind = GK_CLICK;
							}
						}
						if (g.kind == GK_CLICKNAME)
						{
							g.step++;
							break;
						}
					}
					// fall through
				case GK_CLICK:
					{
						// hover (button up) -> press for two frames -> release
						int hover = g.dur > 0 ? g.dur : 4;
						int t = g.step;
						mouse_x = g.x;
						mouse_y = g.y;
						if (t < hover)		{ mouse_buttons &= ~1; }
						else if (t < hover + 2)	{ mouse_buttons |= 1; }
						else			{ mouse_buttons &= ~1; }
						g.step++;
						if (g.step >= hover + 3) { done = true; }
					}
					break;

				case GK_DRAG:
					{
						// hover on the grab point -> press -> travel in a
						// straight line -> release
						int hover = 4;
						int dur = g.dur > 0 ? g.dur : 15;
						int t = g.step;
						if (t < hover)
						{
							mouse_x = g.x1; mouse_y = g.y1; mouse_buttons &= ~1;
						}
						else if (t == hover)
						{
							mouse_x = g.x1; mouse_y = g.y1; mouse_buttons |= 1;
						}
						else if (t <= hover + dur)
						{
							int s = t - hover;
							mouse_x = g.x1 + (g.x2 - g.x1) * s / dur;
							mouse_y = g.y1 + (g.y2 - g.y1) * s / dur;
							mouse_buttons |= 1;
						}
						else
						{
							mouse_x = g.x2; mouse_y = g.y2; mouse_buttons &= ~1;
						}
						g.step++;
						if (g.step > hover + dur + 1) { done = true; }
					}
					break;

				case GK_DRAGNAME:
					{
						// Resolve the grab point once, then behave like GK_DRAG.
						// find_click_point() probes the clip's area and keeps
						// the first pixel whose hit test resolves back to that
						// clip, so the press really lands on it.
						if (g.step == 0)
						{
							gameswf::character* ch = m->find_named_in_tree(g.name.c_str());
							float gx = 0, gy = 0;
							if (ch != NULL && m->find_click_point(ch, &gx, &gy))
							{
								g.x1 = (int) gx;
								g.y1 = (int) gy;
								fprintf(stderr, "[GEST] dragname '%s' grab=(%d,%d) drop=(%d,%d)\n",
									g.name.c_str(), g.x1, g.y1, g.x2, g.y2);
								fflush(stderr);
							}
							else
							{
								fprintf(stderr, "[GEST] dragname '%s' could not resolve a grab point\n",
									g.name.c_str());
								fflush(stderr);
								done = true;
								break;
							}
						}
						if (done) { break; }
						int hover = 4;
						int dur = g.dur > 0 ? g.dur : 15;
						int t = g.step;
						if (t < hover)
						{
							mouse_x = g.x1; mouse_y = g.y1; mouse_buttons &= ~1;
						}
						else if (t == hover)
						{
							mouse_x = g.x1; mouse_y = g.y1; mouse_buttons |= 1;
						}
						else if (t <= hover + dur)
						{
							int s = t - hover;
							mouse_x = g.x1 + (g.x2 - g.x1) * s / dur;
							mouse_y = g.y1 + (g.y2 - g.y1) * s / dur;
							mouse_buttons |= 1;
						}
						else
						{
							mouse_x = g.x2; mouse_y = g.y2; mouse_buttons &= ~1;
						}
						g.step++;
						if (g.step > hover + dur + 1) { done = true; }
					}
					break;

				case GK_DRAGPATH:
					{
						int npts = (int) g.path.size() / 2;
						if (npts < 2) { done = true; break; }
						int hover = 4;
						int dur = g.dur > 0 ? g.dur : (npts - 1) * 10;
						int t = g.step;
						int px0 = g.path[0], py0 = g.path[1];
						int pxn = g.path[(npts - 1) * 2], pyn = g.path[(npts - 1) * 2 + 1];
						if (t < hover)
						{
							mouse_x = px0; mouse_y = py0; mouse_buttons &= ~1;
						}
						else if (t == hover)
						{
							mouse_x = px0; mouse_y = py0; mouse_buttons |= 1;
						}
						else if (t <= hover + dur)
						{
							// Walk the polyline by arc length so unevenly
							// spaced waypoints do not change the speed.
							double total = 0;
							for (int i = 1; i < npts; i++)
							{
								double dx = g.path[i * 2] - g.path[(i - 1) * 2];
								double dy = g.path[i * 2 + 1] - g.path[(i - 1) * 2 + 1];
								total += sqrt(dx * dx + dy * dy);
							}
							double target = ((double) (t - hover) / (double) dur) * total;
							double acc = 0;
							mouse_x = pxn; mouse_y = pyn;
							for (int i = 1; i < npts; i++)
							{
								double ax = g.path[(i - 1) * 2], ay = g.path[(i - 1) * 2 + 1];
								double bx = g.path[i * 2], by = g.path[i * 2 + 1];
								double dx = bx - ax, dy = by - ay;
								double seg = sqrt(dx * dx + dy * dy);
								if (acc + seg >= target && seg > 0)
								{
									double u = (target - acc) / seg;
									mouse_x = (int) (ax + dx * u);
									mouse_y = (int) (ay + dy * u);
									break;
								}
								acc += seg;
							}
							mouse_buttons |= 1;
						}
						else
						{
							mouse_x = pxn; mouse_y = pyn; mouse_buttons &= ~1;
						}
						g.step++;
						if (g.step > hover + dur + 1) { done = true; }
					}
					break;

				case GK_MOVEPATH:
					{
						// Like dragpath but the button stays up: some steps
						// only watch MOUSE_MOVE (the game has already called
						// startDrag on the tool itself).
						int npts = (int) g.path.size() / 2;
						if (npts < 2) { done = true; break; }
						int dur = g.dur > 0 ? g.dur : (npts - 1) * 10;
						int t = g.step;
						int px0 = g.path[0], py0 = g.path[1];
						int pxn = g.path[(npts - 1) * 2], pyn = g.path[(npts - 1) * 2 + 1];
						mouse_buttons &= ~1;
						if (t <= 0)
						{
							mouse_x = px0; mouse_y = py0;
						}
						else if (t <= dur)
						{
							double total = 0;
							for (int i = 1; i < npts; i++)
							{
								double dx = g.path[i * 2] - g.path[(i - 1) * 2];
								double dy = g.path[i * 2 + 1] - g.path[(i - 1) * 2 + 1];
								total += sqrt(dx * dx + dy * dy);
							}
							double target = ((double) t / (double) dur) * total;
							double acc = 0;
							mouse_x = pxn; mouse_y = pyn;
							for (int i = 1; i < npts; i++)
							{
								double ax = g.path[(i - 1) * 2], ay = g.path[(i - 1) * 2 + 1];
								double bx = g.path[i * 2], by = g.path[i * 2 + 1];
								double dx = bx - ax, dy = by - ay;
								double seg = sqrt(dx * dx + dy * dy);
								if (acc + seg >= target && seg > 0)
								{
									double u = (target - acc) / seg;
									mouse_x = (int) (ax + dx * u);
									mouse_y = (int) (ay + dy * u);
									break;
								}
								acc += seg;
							}
						}
						else
						{
							mouse_x = pxn; mouse_y = pyn;
						}
						g.step++;
						if (g.step > dur + 1) { done = true; }
					}
					break;

				case GK_WASH:
					{
						// Rub a small circle around (g.x,g.y).  The pointer
						// stays inside a few pixels, i.e. on a single target,
						// so a game that fades a mask / advances a frame per
						// mouseMove accumulates progress instead of resetting
						// whenever the cursor leaves the object.
						int dur = g.dur > 0 ? g.dur : 300;
						int r = g.x1 > 0 ? g.x1 : 10;
						int t = g.step;
						double ang = t * 0.6;
						mouse_x = g.x + (int) (r * cos(ang));
						mouse_y = g.y + (int) (r * sin(ang));
						mouse_buttons &= ~1;
						g.step++;
						if (g.step > dur) { done = true; }
					}
					break;

				case GK_LISTPRESS:
					m->list_pressable();
					done = true;
					break;

				case GK_QUIT:
					g_quit = true;
					done = true;
					break;

				default:
					done = true;
					break;
				}

				if (done)
				{
					fprintf(stderr, "[GEST] done #%d kind=%d mouse=%d,%d\n",
						g_index, g.kind, mouse_x, mouse_y);
					fflush(stderr);
					g_index++;
				}
			}

			if (g_quit)
			{
				// Let the current frame render, then leave on the next one.
				exit_timeout = 0.001f;
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

				// Temporary diagnostic: GAMESWF_WATCH=name1,name2,... dumps the
				// world bounds / frame / alpha of a handful of named clips so the
				// state machine of a scripted sequence can be followed without a
				// debugger.  Costs nothing when unset.
				{
					const char* watch = getenv("GAMESWF_WATCH");
					extern int g_render_frames;
					if (watch != NULL && watch[0] != 0 && (g_render_frames % 10) == 0)
					{
						char list[512];
						strncpy(list, watch, sizeof(list) - 1);
						list[sizeof(list) - 1] = 0;
						fprintf(stderr, "[WATCH] f=%d mouse=%d,%d", g_render_frames, mouse_x, mouse_y);
						char* p = list;
						while (p != NULL && *p != 0)
						{
							char* comma = strchr(p, ',');
							if (comma != NULL) { *comma = 0; }
							gameswf::character* ch = m->find_named_in_tree(p);
							if (ch == NULL)
							{
								fprintf(stderr, " %s=<none>", p);
							}
							else
							{
								gameswf::rect b;
								ch->get_bound(&b);
								// get_bound() is in the parent's space and already
								// contains this clip's own matrix, so use the
								// parent's world matrix - not the clip's own -
								// otherwise the box lands off stage.
								gameswf::matrix wm;
								gameswf::character* parent = ch->get_parent();
								if (parent != NULL)
								{
									wm = parent->get_world_matrix();
								}
								else
								{
									wm = ch->get_world_matrix();
								}
								wm.transform(&b);
								gameswf::sprite_instance* sp =
									gameswf::cast_to<gameswf::sprite_instance>(ch);
								fprintf(stderr, " %s[z=%d vis=%d a=%.2f (%.0f,%.0f)-(%.0f,%.0f) fr=%d/%d]",
									p, (int) ch->m_depth, (int) ch->m_visible, ch->get_cxform().m_[3][0],
									b.m_x_min * 0.05f, b.m_y_min * 0.05f,
									b.m_x_max * 0.05f, b.m_y_max * 0.05f,
									sp ? sp->get_current_frame() : -1,
									sp ? sp->get_frame_count() : -1);
							}
							p = (comma != NULL) ? comma + 1 : NULL;
						}
						fprintf(stderr, "\n");
						fflush(stderr);
					}
				}

		if (do_render)
		{
			// Framebuffer capture used for animation verification.
			// IMPORTANT: this must run BEFORE the swap.  The context is
			// double buffered (SDL_GL_DOUBLEBUFFER=1), so once we swap the
			// contents of the buffer we drew into are undefined and the dump
			// would be a stale/garbage frame rather than the frame we drew.
			{
				static int s_fb_frame = 0;
				extern int g_render_frames;
				g_render_frames++;
				s_fb_frame = g_render_frames;
				if ((g_render_frames % 500) == 0)
				{
					fprintf(stderr, "[RF] %d\n", g_render_frames);
					fflush(stderr);
				}
				static int s_fb_dumps = 0;
				s_fb_frame++;

				// Frame dumping is opt-in: a glReadPixels plus a PPM write every 20th
				// frame is slow enough to hold the movie down to a couple of
				// frames per second, which makes any timed test unreliable.
				// Set GAMESWF_FB_DIR to turn it on.
				const char* fb_dir = getenv("GAMESWF_FB_DIR");
				bool fb_enabled = (fb_dir != NULL && fb_dir[0] != 0);
				const char* every_s = getenv("GAMESWF_FB_EVERY");
				int every = (every_s != NULL && every_s[0] != 0) ? atoi(every_s) : 20;
				if (every < 1) { every = 1; }

				const char* max_s = getenv("GAMESWF_FB_MAX");
				int fb_max = (max_s != NULL && max_s[0] != 0) ? atoi(max_s) : 400;
				const char* start_s = getenv("GAMESWF_FB_START");
				int fb_start = (start_s != NULL && start_s[0] != 0) ? atoi(start_s) : 0;
				const char* startname_s = getenv("GAMESWF_FB_START_NAME");
				static bool s_fb_named_seen = false;
				bool named_ok = true;
				if (startname_s != NULL && startname_s[0] != 0)
				{
					named_ok = s_fb_named_seen;
					if (!s_fb_named_seen && m->find_named_in_tree(startname_s) != NULL)
					{
						s_fb_named_seen = true;
						named_ok = true;
					}
				}

				bool want = fb_enabled
					&& (s_fb_frame >= fb_start)
					&& named_ok
					&& ((s_fb_frame <= 3) || ((s_fb_frame % every) == 0));

				// GAMESWF_PICK=x,y dumps the display-list path drawn at that
				// screen pixel, so we can identify what renders a particular pixel.
				// It runs on every capture so we can see the tree once the
				// screen we care about is up.
				const char* pick_s = getenv("GAMESWF_PICK");
				bool do_pick = (pick_s != NULL && pick_s[0] != 0 && want);

				if (want && s_fb_dumps < fb_max)
				{
					s_fb_dumps++;
					int nw = width, nh = height;
					unsigned char* buf = new unsigned char[(size_t) nw * nh * 4];
					glReadPixels(0, 0, nw, nh, GL_RGBA, GL_UNSIGNED_BYTE, buf);

					char fn[512];
					sprintf(fn, "%s/fb_%04d.ppm", fb_dir, s_fb_frame);
					FILE* f = fopen(fn, "wb");
					if (f)
					{
						// Announce the capture so a log line can be matched to
						// the exact image: the render-frame counter here is not
						// the harness loop-frame counter.
						fprintf(stderr, "[FB] n=%d mouse=%d,%d\n", s_fb_frame, mouse_x, mouse_y);
						fflush(stderr);
						if (do_pick)
						{
							float pxp = 0.0f, pyp = 0.0f;
							if (sscanf(pick_s, "%f,%f", &pxp, &pyp) == 2)
							{
								fprintf(stderr, "[PICK] probing %f,%f at frame %d\n", pxp, pyp, s_fb_frame);
								fflush(stderr);
								m->pick_report_at(pxp, pyp);
								fflush(stderr);
							}
						}
						// GAMESWF_TREE=n dumps the full display tree at frame n.
						// GAMESWF_TREE_EVERY=k dumps it every k frames from n on.
						{
							const char* tree_s = getenv("GAMESWF_TREE");
							const char* tree_every_s = getenv("GAMESWF_TREE_EVERY");
							int tree_at = (tree_s && tree_s[0]) ? atoi(tree_s) : -1;
							int tree_every = (tree_every_s && tree_every_s[0]) ? atoi(tree_every_s) : 0;
							bool tree_now = false;
							if (tree_at >= 0)
							{
								tree_now = (tree_every > 0)
									? (s_fb_frame >= tree_at && ((s_fb_frame - tree_at) % tree_every) == 0)
									: (s_fb_frame == tree_at);
							}
							if (tree_now)
							{
								fprintf(stderr, "[TREE] dump at frame %d\n", s_fb_frame);
								fflush(stderr);
								m->dump_tree();
								fflush(stderr);
							}
						}
						fprintf(f, "P6\n%d %d\n255\n", nw, nh);
						// GL origin is bottom-left, PPM is top-left: flip rows.
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
					}
					delete[] buf;
				}
			}

			Uint32 t_swap = tu_timer::get_ticks();
			glFinish();	// ensure all GL commands complete before swap
			// Bypass SDL_GL_SwapBuffers macro - call SDL_GL_SwapWindow directly
			SDL_GL_SwapWindow(g_sdl_window);
			t_swap = tu_timer::get_ticks() - t_swap;

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
					fprintf(stderr, "[EXIT] movie end\n");
					fflush(stderr);
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
			fprintf(stderr, "[EXIT] done label reached\n");
			fflush(stderr);

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
