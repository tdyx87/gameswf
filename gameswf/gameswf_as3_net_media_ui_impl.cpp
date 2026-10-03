// as3_net_media_ui_impl.cpp -- AS3 network, media, UI, text class implementations
// This source code has been donated to the Public Domain.

#include "gameswf/gameswf_as3_classes.h"
#include "gameswf/gameswf_as_classes/as_array.h"
#include "gameswf/gameswf_avm2.h"
#include <stdio.h>

namespace gameswf
{

// ========================================================================
// URLRequest implementation
// ========================================================================

static void urlrequest_get_url(const fn_call& fn)
{
	as_value val;
	fn.this_ptr->get_member("__url__", &val);
	*fn.result = val;
}

static void urlrequest_set_url(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	fn.this_ptr->set_member("__url__", fn.arg(0));
	fn.this_ptr->set_member("url", fn.arg(0));
}

static void urlrequest_get_method(const fn_call& fn)
{
	as_value val;
	fn.this_ptr->get_member("__method__", &val);
	if (val.is_undefined()) fn.result->set_string("GET");
	else *fn.result = val;
}

static void urlrequest_set_method(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	fn.this_ptr->set_member("__method__", fn.arg(0));
}

static void urlrequest_get_data(const fn_call& fn)
{
	as_value val;
	fn.this_ptr->get_member("__data__", &val);
	*fn.result = val;
}

static void urlrequest_set_data(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	fn.this_ptr->set_member("__data__", fn.arg(0));
}

// ========================================================================
// URLLoader implementation
// ========================================================================

static void urlloader_load(const fn_call& fn)
{
	if (fn.nargs < 1) return;

	as_value url_val;
	as_object* request = fn.arg(0).to_object();
	if (request)
	{
		request->get_member("__url__", &url_val);
		if (url_val.is_undefined())
		{
			// AS3 writes the address through the public `url` property; only
			// the native setter mirrors it into `__url__`.
			request->get_member("url", &url_val);
		}
		fn.this_ptr->set_member("__url__", url_val);
	}

	fn.this_ptr->set_member("data", as_value(""));
	fn.this_ptr->set_member("bytesLoaded", as_value(0));
	fn.this_ptr->set_member("bytesTotal", as_value(0));

	// This build has no network, so the request can never complete.  Fire
	// ioError synchronously: dispatching from inside load() means the AS3
	// error listener is already attached (it is registered just before the
	// call), so the application's error path runs instead of waiting for an
	// event that would never arrive.
	as_object* evt = new as_object(fn.get_player());
	evt->set_member("type", as_value("ioError"));
	evt->set_member("bubbles", as_value(false));
	evt->set_member("cancelable", as_value(false));
	evt->set_member("target", as_value(fn.this_ptr));
	evt->set_member("text", as_value("The URL cannot be loaded (offline)"));

	fprintf(stderr, "[URLLOAD] url='%s' dispatch ioError\n", url_val.to_string());
	fflush(stderr);

	as_environment local_env(fn.get_player());
	avm2_dispatch_event(fn.this_ptr, as_value(evt), fn.env ? fn.env : &local_env);
}

static void urlloader_close(const fn_call& fn)
{
	// Simplified - no-op
}

// ========================================================================
// TextField implementation
// ========================================================================

static void textfield_get_text(const fn_call& fn)
{
	as_value val;
	fn.this_ptr->get_member("__text__", &val);
	if (val.is_undefined()) fn.result->set_string("");
	else *fn.result = val;
}

static void textfield_set_text(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	fn.this_ptr->set_member("__text__", fn.arg(0));
	fn.this_ptr->set_member("text", fn.arg(0));
}

static void textfield_get_htmlText(const fn_call& fn)
{
	as_value val;
	fn.this_ptr->get_member("__text__", &val);
	if (val.is_undefined()) fn.result->set_string("");
	else *fn.result = val;
}

static void textfield_set_htmlText(const fn_call& fn)
{
	textfield_set_text(fn);
}

static void textfield_get_textWidth(const fn_call& fn)
{
	as_value val;
	fn.this_ptr->get_member("__text__", &val);
	const char* text = val.to_string();
	fn.result->set_double((double)strlen(text) * 7.0);  // Approximate width
}

static void textfield_get_textHeight(const fn_call& fn)
{
	fn.result->set_double(16.0);  // Approximate height
}

static void textfield_get_type(const fn_call& fn)
{
	as_value val;
	fn.this_ptr->get_member("__type__", &val);
	if (val.is_undefined()) fn.result->set_string("DYNAMIC");
	else *fn.result = val;
}

static void textfield_set_type(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	fn.this_ptr->set_member("__type__", fn.arg(0));
}

static void textfield_get_selectable(const fn_call& fn)
{
	as_value val;
	fn.this_ptr->get_member("__selectable__", &val);
	if (val.is_undefined()) fn.result->set_bool(true);
	else *fn.result = val;
}

static void textfield_set_selectable(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	fn.this_ptr->set_member("__selectable__", fn.arg(0));
}

static void textfield_get_autoSize(const fn_call& fn)
{
	as_value val;
	fn.this_ptr->get_member("__autoSize__", &val);
	if (val.is_undefined()) fn.result->set_string("NONE");
	else *fn.result = val;
}

static void textfield_set_autoSize(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	fn.this_ptr->set_member("__autoSize__", fn.arg(0));
}

static void textfield_get_wordWrap(const fn_call& fn)
{
	as_value val;
	fn.this_ptr->get_member("__wordWrap__", &val);
	if (val.is_undefined()) fn.result->set_bool(false);
	else *fn.result = val;
}

static void textfield_set_wordWrap(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	fn.this_ptr->set_member("__wordWrap__", fn.arg(0));
}

static void textfield_get_multiline(const fn_call& fn)
{
	as_value val;
	fn.this_ptr->get_member("__multiline__", &val);
	if (val.is_undefined()) fn.result->set_bool(false);
	else *fn.result = val;
}

static void textfield_set_multiline(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	fn.this_ptr->set_member("__multiline__", fn.arg(0));
}

static void textfield_appendText(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	as_value current;
	fn.this_ptr->get_member("__text__", &current);
	tu_string new_text = current.to_string();
	new_text += fn.arg(0).to_string();
	fn.this_ptr->set_member("__text__", as_value(new_text));
	fn.this_ptr->set_member("text", as_value(new_text));
}

static void textfield_get_defaultTextFormat(const fn_call& fn)
{
	gc_ptr<as_object> fmt = new as_object(fn.get_player());
	fmt->set_member("font", as_value("_sans"));
	fmt->set_member("size", as_value(12.0));
	fmt->set_member("color", as_value(0x000000));
	fmt->set_member("bold", as_value(false));
	fmt->set_member("italic", as_value(false));
	fmt->set_member("align", as_value("left"));
	fn.result->set_as_object(fmt.get_ptr());
}

static void textfield_set_defaultTextFormat(const fn_call& fn)
{
	// Simplified - store the format
	if (fn.nargs < 1) return;
	fn.this_ptr->set_member("__textFormat__", fn.arg(0));
}

static void textfield_setTextFormat(const fn_call& fn)
{
	textfield_set_defaultTextFormat(fn);
}

// ========================================================================
// TextFormat implementation
// ========================================================================

static void textformat_get_font(const fn_call& fn)
{
	as_value val;
	fn.this_ptr->get_member("__font__", &val);
	if (val.is_undefined()) fn.result->set_string("_sans");
	else *fn.result = val;
}

static void textformat_set_font(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	fn.this_ptr->set_member("__font__", fn.arg(0));
}

static void textformat_get_size(const fn_call& fn)
{
	as_value val;
	fn.this_ptr->get_member("__size__", &val);
	if (val.is_undefined()) fn.result->set_double(12.0);
	else *fn.result = val;
}

static void textformat_set_size(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	fn.this_ptr->set_member("__size__", fn.arg(0));
}

static void textformat_get_color(const fn_call& fn)
{
	as_value val;
	fn.this_ptr->get_member("__color__", &val);
	if (val.is_undefined()) fn.result->set_int(0);
	else *fn.result = val;
}

static void textformat_set_color(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	fn.this_ptr->set_member("__color__", fn.arg(0));
}

static void textformat_get_bold(const fn_call& fn)
{
	as_value val;
	fn.this_ptr->get_member("__bold__", &val);
	if (val.is_undefined()) fn.result->set_bool(false);
	else *fn.result = val;
}

static void textformat_set_bold(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	fn.this_ptr->set_member("__bold__", fn.arg(0));
}

static void textformat_get_italic(const fn_call& fn)
{
	as_value val;
	fn.this_ptr->get_member("__italic__", &val);
	if (val.is_undefined()) fn.result->set_bool(false);
	else *fn.result = val;
}

static void textformat_set_italic(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	fn.this_ptr->set_member("__italic__", fn.arg(0));
}

static void textformat_get_align(const fn_call& fn)
{
	as_value val;
	fn.this_ptr->get_member("__align__", &val);
	if (val.is_undefined()) fn.result->set_string("left");
	else *fn.result = val;
}

static void textformat_set_align(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	fn.this_ptr->set_member("__align__", fn.arg(0));
}

// ========================================================================
// Sound implementation
// ========================================================================

static void sound_load(const fn_call& fn)
{
	// Simplified - mark as loaded
	fn.this_ptr->set_member("__loaded__", as_value(true));
}

static void sound_play(const fn_call& fn)
{
	// Simplified - return null sound channel
	fn.result->set_null();
}

static void sound_close(const fn_call& fn)
{
	// Simplified - no-op
}

// ========================================================================
// SoundChannel implementation
// ========================================================================

static void soundchannel_stop(const fn_call& fn)
{
	// Simplified - no-op
}

// ========================================================================
// SoundTransform implementation
// ========================================================================

static void soundtransform_get_volume(const fn_call& fn)
{
	as_value val;
	fn.this_ptr->get_member("__volume__", &val);
	if (val.is_undefined()) fn.result->set_double(1.0);
	else *fn.result = val;
}

static void soundtransform_set_volume(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	fn.this_ptr->set_member("__volume__", fn.arg(0));
}

// ========================================================================
// Mouse class (static properties)
// ========================================================================

static void mouse_get_x(const fn_call& fn)
{
	// Simplified - return 0
	fn.result->set_double(0);
}

static void mouse_get_y(const fn_call& fn)
{
	// Simplified - return 0
	fn.result->set_double(0);
}

static void mouse_hide(const fn_call& fn)
{
	// Simplified - no-op
}

static void mouse_show(const fn_call& fn)
{
	// Simplified - no-op
}

// ========================================================================
// Keyboard class (static properties)
// ========================================================================

// Keyboard constants
static void keyboard_get_capsLock(const fn_call& fn)
{
	fn.result->set_bool(false);
}

static void keyboard_get_numLock(const fn_call& fn)
{
	fn.result->set_bool(false);
}

// ========================================================================
// ExternalInterface implementation
// ========================================================================

static void externalinterface_call(const fn_call& fn)
{
	// Simplified - return undefined
	fn.result->set_undefined();
}

static void externalinterface_addCallback(const fn_call& fn)
{
	// Simplified - no-op
}

static void externalinterface_available(const fn_call& fn)
{
	fn.result->set_bool(false);
}

// ========================================================================
// Capabilities implementation
// ========================================================================

static void capabilities_get_playerType(const fn_call& fn)
{
	fn.result->set_string("Desktop");
}

static void capabilities_get_os(const fn_call& fn)
{
	fn.result->set_string("Windows");
}

static void capabilities_get_version(const fn_call& fn)
{
	fn.result->set_string("WIN 11,0,0,0");
}

static void capabilities_get_screenResolutionX(const fn_call& fn)
{
	fn.result->set_double(1920);
}

static void capabilities_get_screenResolutionY(const fn_call& fn)
{
	fn.result->set_double(1080);
}

static void capabilities_get_language(const fn_call& fn)
{
	fn.result->set_string("en");
}

// ========================================================================
// Registration function
// ========================================================================

void as3_register_net_media_ui_classes(player* player)
{
	as_object* global = player->get_global();

	// Get flash package
	as_value flash_val;
	as_object* flash_pkg = NULL;
	if (global->get_member("flash", &flash_val) && flash_val.is_object())
	{
		flash_pkg = flash_val.to_object();
	}

	// Get sub-packages
	as_object* net_pkg = NULL;
	as_object* media_pkg = NULL;
	as_object* text_pkg = NULL;
	as_object* ui_pkg = NULL;
	as_object* system_pkg = NULL;

	if (flash_pkg)
	{
		as_value net_val, media_val, text_val, ui_val, system_val;
		if (flash_pkg->get_member("net", &net_val) && net_val.is_object())
			net_pkg = net_val.to_object();
		if (flash_pkg->get_member("media", &media_val) && media_val.is_object())
			media_pkg = media_val.to_object();
		if (flash_pkg->get_member("text", &text_val) && text_val.is_object())
			text_pkg = text_val.to_object();
		if (flash_pkg->get_member("ui", &ui_val) && ui_val.is_object())
			ui_pkg = ui_val.to_object();
		if (flash_pkg->get_member("system", &system_val) && system_val.is_object())
			system_pkg = system_val.to_object();
	}

	// ===== URLRequest =====
	{
		as_object* cls = new as_object(player);
		cls->set_member("url", as_value(""));
		cls->set_member("method", as_value("GET"));
		cls->set_member("data", as_value());
		cls->set_member("contentType", as_value("application/x-www-form-urlencoded"));
		cls->set_member("requestHeaders", as_value());

		cls->builtin_member("url", as_value(urlrequest_get_url));
		cls->builtin_member("method", as_value(urlrequest_get_method));
		cls->builtin_member("data", as_value(urlrequest_get_data));

		global->set_member("URLRequest", as_value(cls));
		if (net_pkg) net_pkg->set_member("URLRequest", as_value(cls));
	}

	// ===== URLLoader =====
	{
		as_object* cls = new as_object(player);
		cls->set_member("data", as_value());
		cls->set_member("bytesLoaded", as_value(0));
		cls->set_member("bytesTotal", as_value(0));

		cls->builtin_member("load", as_value(urlloader_load));
		cls->builtin_member("close", as_value(urlloader_close));

		global->set_member("URLLoader", as_value(cls));
		if (net_pkg) net_pkg->set_member("URLLoader", as_value(cls));
	}

	// ===== URLLoaderDataFormat =====
	{
		as_object* cls = new as_object(player);
		cls->set_member("BINARY", as_value("binary"));
		cls->set_member("TEXT", as_value("text"));
		cls->set_member("VARIABLES", as_value("variables"));
		global->set_member("URLLoaderDataFormat", as_value(cls));
		if (net_pkg) net_pkg->set_member("URLLoaderDataFormat", as_value(cls));
	}

	// ===== URLVariables =====
	{
		as_object* cls = new as_object(player);
		global->set_member("URLVariables", as_value(cls));
		if (net_pkg) net_pkg->set_member("URLVariables", as_value(cls));
	}

	// ===== TextField =====
	{
		as_object* cls = new as_object(player);
		cls->set_member("text", as_value(""));
		cls->set_member("htmlText", as_value(""));
		cls->set_member("textWidth", as_value(0.0));
		cls->set_member("textHeight", as_value(0.0));
		cls->set_member("type", as_value("DYNAMIC"));
		cls->set_member("selectable", as_value(true));
		cls->set_member("autoSize", as_value("NONE"));
		cls->set_member("wordWrap", as_value(false));
		cls->set_member("multiline", as_value(false));
		cls->set_member("border", as_value(false));
		cls->set_member("background", as_value(false));
		cls->set_member("backgroundColor", as_value(0xFFFFFF));
		cls->set_member("displayAsPassword", as_value(false));
		cls->set_member("maxChars", as_value(0));
		cls->set_member("length", as_value(0));

		cls->builtin_member("text", as_value(textfield_get_text));
		cls->builtin_member("htmlText", as_value(textfield_get_htmlText));
		cls->builtin_member("textWidth", as_value(textfield_get_textWidth));
		cls->builtin_member("textHeight", as_value(textfield_get_textHeight));
		cls->builtin_member("type", as_value(textfield_get_type));
		cls->builtin_member("selectable", as_value(textfield_get_selectable));
		cls->builtin_member("autoSize", as_value(textfield_get_autoSize));
		cls->builtin_member("wordWrap", as_value(textfield_get_wordWrap));
		cls->builtin_member("multiline", as_value(textfield_get_multiline));

		cls->builtin_member("appendText", as_value(textfield_appendText));
		cls->builtin_member("getTextFormat", as_value(textfield_get_defaultTextFormat));
		cls->builtin_member("setTextFormat", as_value(textfield_setTextFormat));
		cls->builtin_member("getDefaultTextFormat", as_value(textfield_get_defaultTextFormat));
		cls->builtin_member("setDefaultTextFormat", as_value(textfield_set_defaultTextFormat));

		global->set_member("TextField", as_value(cls));
		if (text_pkg) text_pkg->set_member("TextField", as_value(cls));
	}

	// ===== TextFormat =====
	{
		as_object* cls = new as_object(player);
		cls->set_member("font", as_value("_sans"));
		cls->set_member("size", as_value(12.0));
		cls->set_member("color", as_value(0));
		cls->set_member("bold", as_value(false));
		cls->set_member("italic", as_value(false));
		cls->set_member("underline", as_value(false));
		cls->set_member("align", as_value("left"));
		cls->set_member("leftMargin", as_value(0.0));
		cls->set_member("rightMargin", as_value(0.0));
		cls->set_member("indent", as_value(0.0));
		cls->set_member("leading", as_value(0.0));
		cls->set_member("blockIndent", as_value(0.0));
		cls->set_member("bullet", as_value(false));
		cls->set_member("kerning", as_value(true));
		cls->set_member("letterSpacing", as_value(0.0));
		cls->set_member("tabStops", as_value());

		cls->builtin_member("font", as_value(textformat_get_font));
		cls->builtin_member("size", as_value(textformat_get_size));
		cls->builtin_member("color", as_value(textformat_get_color));
		cls->builtin_member("bold", as_value(textformat_get_bold));
		cls->builtin_member("italic", as_value(textformat_get_italic));
		cls->builtin_member("align", as_value(textformat_get_align));

		global->set_member("TextFormat", as_value(cls));
		if (text_pkg) text_pkg->set_member("TextFormat", as_value(cls));
	}

	// ===== Sound =====
	{
		as_object* cls = new as_object(player);
		cls->builtin_member("load", as_value(sound_load));
		cls->builtin_member("play", as_value(sound_play));
		cls->builtin_member("close", as_value(sound_close));
		global->set_member("Sound", as_value(cls));
		if (media_pkg) media_pkg->set_member("Sound", as_value(cls));
	}

	// ===== SoundChannel =====
	{
		as_object* cls = new as_object(player);
		cls->builtin_member("stop", as_value(soundchannel_stop));
		global->set_member("SoundChannel", as_value(cls));
		if (media_pkg) media_pkg->set_member("SoundChannel", as_value(cls));
	}

	// ===== SoundTransform =====
	{
		as_object* cls = new as_object(player);
		cls->set_member("volume", as_value(1.0));
		cls->set_member("pan", as_value(0.0));
		cls->builtin_member("volume", as_value(soundtransform_get_volume));
		global->set_member("SoundTransform", as_value(cls));
		if (media_pkg) media_pkg->set_member("SoundTransform", as_value(cls));
	}

	// ===== Mouse =====
	{
		as_object* cls = new as_object(player);
		cls->builtin_member("hide", as_value(mouse_hide));
		cls->builtin_member("show", as_value(mouse_show));
		global->set_member("Mouse", as_value(cls));
		if (ui_pkg) ui_pkg->set_member("Mouse", as_value(cls));
	}

	// ===== Keyboard =====
	{
		as_object* cls = new as_object(player);
		// Keyboard constants
		cls->set_member("BACKSPACE", as_value(8));
		cls->set_member("CONTROL", as_value(17));
		cls->set_member("DELETE", as_value(46));
		cls->set_member("DOWN", as_value(40));
		cls->set_member("END", as_value(35));
		cls->set_member("ENTER", as_value(13));
		cls->set_member("ESCAPE", as_value(27));
		cls->set_member("F1", as_value(112));
		cls->set_member("F2", as_value(113));
		cls->set_member("F3", as_value(114));
		cls->set_member("F4", as_value(115));
		cls->set_member("F5", as_value(116));
		cls->set_member("F6", as_value(117));
		cls->set_member("F7", as_value(118));
		cls->set_member("F8", as_value(119));
		cls->set_member("F9", as_value(120));
		cls->set_member("F10", as_value(121));
		cls->set_member("F11", as_value(122));
		cls->set_member("F12", as_value(123));
		cls->set_member("HOME", as_value(36));
		cls->set_member("INSERT", as_value(45));
		cls->set_member("LEFT", as_value(37));
		cls->set_member("PAGE_DOWN", as_value(34));
		cls->set_member("PAGE_UP", as_value(33));
		cls->set_member("RIGHT", as_value(39));
		cls->set_member("SHIFT", as_value(16));
		cls->set_member("SPACE", as_value(32));
		cls->set_member("TAB", as_value(9));
		cls->set_member("UP", as_value(38));
		cls->set_member("A", as_value(65));
		cls->set_member("B", as_value(66));
		cls->set_member("C", as_value(67));
		cls->set_member("D", as_value(68));
		cls->set_member("E", as_value(69));
		cls->set_member("F", as_value(70));
		cls->set_member("G", as_value(71));
		cls->set_member("H", as_value(72));
		cls->set_member("I", as_value(73));
		cls->set_member("J", as_value(74));
		cls->set_member("K", as_value(75));
		cls->set_member("L", as_value(76));
		cls->set_member("M", as_value(77));
		cls->set_member("N", as_value(78));
		cls->set_member("NUMBER_0", as_value(48));
		cls->set_member("NUMBER_1", as_value(49));
		cls->set_member("NUMBER_2", as_value(50));
		cls->set_member("NUMBER_3", as_value(51));
		cls->set_member("NUMBER_4", as_value(52));
		cls->set_member("NUMBER_5", as_value(53));
		cls->set_member("NUMBER_6", as_value(54));
		cls->set_member("NUMBER_7", as_value(55));
		cls->set_member("NUMBER_8", as_value(56));
		cls->set_member("NUMBER_9", as_value(57));
		cls->set_member("O", as_value(79));
		cls->set_member("P", as_value(80));
		cls->set_member("Q", as_value(81));
		cls->set_member("R", as_value(82));
		cls->set_member("S", as_value(83));
		cls->set_member("T", as_value(84));
		cls->set_member("U", as_value(85));
		cls->set_member("V", as_value(86));
		cls->set_member("W", as_value(87));
		cls->set_member("X", as_value(88));
		cls->set_member("Y", as_value(89));
		cls->set_member("Z", as_value(90));
		global->set_member("Keyboard", as_value(cls));
		if (ui_pkg) ui_pkg->set_member("Keyboard", as_value(cls));
	}

	// ===== ExternalInterface =====
	{
		as_object* cls = new as_object(player);
		cls->set_member("available", as_value(false));
		cls->set_member("objectID", as_value(""));
		cls->builtin_member("call", as_value(externalinterface_call));
		cls->builtin_member("addCallback", as_value(externalinterface_addCallback));
		cls->builtin_member("available", as_value(externalinterface_available));
		global->set_member("ExternalInterface", as_value(cls));
	}

	// ===== Capabilities =====
	{
		as_object* cls = new as_object(player);
		cls->set_member("playerType", as_value("Desktop"));
		cls->set_member("os", as_value("Windows"));
		cls->set_member("version", as_value("WIN 11,0,0,0"));
		cls->set_member("screenResolutionX", as_value(1920));
		cls->set_member("screenResolutionY", as_value(1080));
		cls->set_member("language", as_value("en"));
		cls->set_member("isDebugger", as_value(false));
		cls->set_member("hasAudio", as_value(true));
		cls->set_member("hasMP3", as_value(true));
		cls->set_member("hasPrinting", as_value(true));
		cls->set_member("hasScreenBroadcast", as_value(false));
		cls->set_member("hasScreenPlayback", as_value(false));
		cls->set_member("hasStreamingAudio", as_value(true));
		cls->set_member("hasStreamingVideo", as_value(true));
		cls->set_member("hasVideoEncoder", as_value(false));
		cls->set_member("supports32BitProcesses", as_value(true));
		cls->set_member("supports64BitProcesses", as_value(true));
		cls->set_member("cpuArchitecture", as_value("x86"));
		global->set_member("Capabilities", as_value(cls));
		if (system_pkg) system_pkg->set_member("Capabilities", as_value(cls));
	}

	// ===== Security =====
	{
		as_object* cls = new as_object(player);
		cls->set_member("sandboxType", as_value("remote"));
		cls->set_member("LOCAL_TRUSTED", as_value("localTrusted"));
		cls->set_member("LOCAL_WITH_FILE", as_value("localWithFile"));
		cls->set_member("LOCAL_WITH_NETWORK", as_value("localWithNetwork"));
		cls->set_member("REMOTE", as_value("remote"));
		global->set_member("Security", as_value(cls));
		if (system_pkg) system_pkg->set_member("Security", as_value(cls));
	}

	// ===== StageQuality =====
	{
		as_object* cls = new as_object(player);
		cls->set_member("LOW", as_value("LOW"));
		cls->set_member("MEDIUM", as_value("MEDIUM"));
		cls->set_member("HIGH", as_value("HIGH"));
		cls->set_member("BEST", as_value("BEST"));
		global->set_member("StageQuality", as_value(cls));
	}

	// ===== StageAlign =====
	{
		as_object* cls = new as_object(player);
		cls->set_member("TOP", as_value("T"));
		cls->set_member("BOTTOM", as_value("B"));
		cls->set_member("LEFT", as_value("L"));
		cls->set_member("RIGHT", as_value("R"));
		cls->set_member("TOP_LEFT", as_value("TL"));
		cls->set_member("TOP_RIGHT", as_value("TR"));
		cls->set_member("BOTTOM_LEFT", as_value("BL"));
		cls->set_member("BOTTOM_RIGHT", as_value("BR"));
		global->set_member("StageAlign", as_value(cls));
	}

	// ===== StageScaleMode =====
	{
		as_object* cls = new as_object(player);
		cls->set_member("SHOW_ALL", as_value("showAll"));
		cls->set_member("NO_BORDER", as_value("noBorder"));
		cls->set_member("EXACT_FIT", as_value("exactFit"));
		cls->set_member("NO_SCALE", as_value("noScale"));
		global->set_member("StageScaleMode", as_value(cls));
	}
}

} // end namespace gameswf
