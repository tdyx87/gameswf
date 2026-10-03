// as_event.cpp -- AS3 Event class implementation
// This source code has been donated to the Public Domain.

#include "gameswf/gameswf_as_classes/as_event.h"

namespace gameswf
{

	// Event constructor
	as_event::as_event(player* player, const char* type, bool bubbles, bool cancelable) :
		as_object(player)
	{
		set_member("type", as_value(type));
		set_member("bubbles", as_value(bubbles));
		set_member("cancelable", as_value(cancelable));
		set_member("eventPhase", as_value(0));
		set_member("target", as_value());
		set_member("currentTarget", as_value());

		// Register methods
		builtin_member("stopPropagation", as_event_stopPropagation);
		builtin_member("stopImmediatePropagation", as_event_stopImmediatePropagation);
		builtin_member("preventDefault", as_event_preventDefault);
		builtin_member("clone", as_event_clone);
		builtin_member("toString", as_event_toString);
		builtin_member("formatToString", as_event_formatToString);
	}

	void as_event::set_type(const char* type)
	{
		set_member("type", as_value(type));
	}

	void as_event::set_target(as_object* target)
	{
		set_member("target", as_value(target));
	}

	void as_event::set_currentTarget(as_object* currentTarget)
	{
		set_member("currentTarget", as_value(currentTarget));
	}

	void as_event::set_eventPhase(int phase)
	{
		set_member("eventPhase", as_value(phase));
	}

	// Register all Event constants
	void as_event::register_event_constants(as_object* target)
	{
		// Event types
		target->set_member("ACTIVATE", as_value("activate"));
		target->set_member("ADDED", as_value("added"));
		target->set_member("ADDED_TO_STAGE", as_value("addedToStage"));
		target->set_member("CANCEL", as_value("cancel"));
		target->set_member("CHANGE", as_value("change"));
		target->set_member("CLEAR", as_value("clear"));
		target->set_member("CLOSE", as_value("close"));
		target->set_member("CLOSING", as_value("closing"));
		target->set_member("COMPLETE", as_value("complete"));
		target->set_member("CONNECT", as_value("connect"));
		target->set_member("COPY", as_value("copy"));
		target->set_member("CUT", as_value("cut"));
		target->set_member("DEACTIVATE", as_value("deactivate"));
		target->set_member("DISPLAYING", as_value("displaying"));
		target->set_member("ENTER_FRAME", as_value("enterFrame"));
		target->set_member("EXIT_FRAME", as_value("exitFrame"));
		target->set_member("FRAME_CONSTRUCTED", as_value("frameConstructed"));
		target->set_member("FRAME_LABEL", as_value("frameLabel"));
		target->set_member("FULLSCREEN", as_value("fullScreen"));
		target->set_member("HTML_BOUNDS_CHANGE", as_value("htmlBoundsChange"));
		target->set_member("HTML_DOM_INITIALIZE", as_value("htmlDOMInitialize"));
		target->set_member("HTML_RENDER", as_value("htmlRender"));
		target->set_member("ID3", as_value("id3"));
		target->set_member("INIT", as_value("init"));
		target->set_member("LOCATION_CHANGE", as_value("locationChange"));
		target->set_member("MOUSE_LEAVE", as_value("mouseLeave"));
		target->set_member("NETWORK_CHANGE", as_value("networkChange"));
		target->set_member("OPEN", as_value("open"));
		target->set_member("PASTE", as_value("paste"));
		target->set_member("PREPARING", as_value("preparing"));
		target->set_member("REMOVED", as_value("removed"));
		target->set_member("REMOVED_FROM_STAGE", as_value("removedFromStage"));
		target->set_member("RENDER", as_value("render"));
		target->set_member("RESIZE", as_value("resize"));
		target->set_member("SCROLL", as_value("scroll"));
		target->set_member("SELECT", as_value("select"));
		target->set_member("SELECT_ALL", as_value("selectAll"));
		target->set_member("SOUND_COMPLETE", as_value("soundComplete"));
		target->set_member("STANDARD_ERROR_CLOSE", as_value("standardErrorClose"));
		target->set_member("STANDARD_INPUT_CLOSE", as_value("standardInputClose"));
		target->set_member("STANDARD_OUTPUT_CLOSE", as_value("standardOutputClose"));
		target->set_member("TAB_CHILDREN_CHANGE", as_value("tabChildrenChange"));
		target->set_member("TAB_ENABLED_CHANGE", as_value("tabEnabledChange"));
		target->set_member("TAB_INDEX_CHANGE", as_value("tabIndexChange"));
		target->set_member("TEXT_INTERACTION_MODE_CHANGE", as_value("textInteractionModeChange"));
		target->set_member("TEXTURE_READY", as_value("textureReady"));
		target->set_member("UNLOAD", as_value("unload"));
		target->set_member("USER_IDLE", as_value("userIdle"));
		target->set_member("USER_PRESENT", as_value("userPresent"));
		target->set_member("VIDEO_FRAME", as_value("videoFrame"));
		target->set_member("WORKER_STATE", as_value("workerState"));

		// Event phases
		target->set_member("CAPTURING_PHASE", as_value(1));
		target->set_member("AT_TARGET", as_value(2));
		target->set_member("BUBBLING_PHASE", as_value(3));
	}

	// Event methods
	void as_event_stopPropagation(const fn_call& fn)
	{
		as_event* evt = cast_to<as_event>(fn.this_ptr);
		if (evt)
		{
			evt->set_member("isPropagationStopped", as_value(true));
		}
	}

	void as_event_stopImmediatePropagation(const fn_call& fn)
	{
		as_event* evt = cast_to<as_event>(fn.this_ptr);
		if (evt)
		{
			evt->set_member("isImmediatePropagationStopped", as_value(true));
			evt->set_member("isPropagationStopped", as_value(true));
		}
	}

	void as_event_preventDefault(const fn_call& fn)
	{
		as_event* evt = cast_to<as_event>(fn.this_ptr);
		if (evt)
		{
			evt->set_member("isDefaultPrevented", as_value(true));
		}
	}

	void as_event_clone(const fn_call& fn)
	{
		as_event* evt = cast_to<as_event>(fn.this_ptr);
		if (evt == NULL)
		{
			fn.result->set_undefined();
			return;
		}

		as_value type, bubbles, cancelable;
		evt->get_member("type", &type);
		evt->get_member("bubbles", &bubbles);
		evt->get_member("cancelable", &cancelable);

		gc_ptr<as_event> clone = new as_event(fn.get_player(), type.to_string(), bubbles.to_bool(), cancelable.to_bool());
		fn.result->set_as_object(clone.get_ptr());
	}

	void as_event_toString(const fn_call& fn)
	{
		as_event* evt = cast_to<as_event>(fn.this_ptr);
		if (evt == NULL)
		{
			fn.result->set_string("[object Event]");
			return;
		}

		as_value type;
		evt->get_member("type", &type);

		char buf[256];
		snprintf(buf, sizeof(buf), "[Event type=\"%s\"]", type.to_string());
		fn.result->set_string(buf);
	}

	void as_event_formatToString(const fn_call& fn)
	{
		// Simplified implementation
		as_event_toString(fn);
	}

	// Global Event constructor
	void as_global_event_ctor(const fn_call& fn)
	{
		const char* type = "";
		bool bubbles = false;
		bool cancelable = false;

		if (fn.nargs >= 1) type = fn.arg(0).to_string();
		if (fn.nargs >= 2) bubbles = fn.arg(1).to_bool();
		if (fn.nargs >= 3) cancelable = fn.arg(2).to_bool();

		gc_ptr<as_event> evt = new as_event(fn.get_player(), type, bubbles, cancelable);
		fn.result->set_as_object(evt.get_ptr());
	}

	// ===== MouseEvent =====

	as_mouse_event::as_mouse_event(player* player, const char* type, bool bubbles, bool cancelable) :
		as_event(player, type, bubbles, cancelable)
	{
		// MouseEvent specific properties
		set_member("localX", as_value(0.0));
		set_member("localY", as_value(0.0));
		set_member("stageX", as_value(0.0));
		set_member("stageY", as_value(0.0));
		set_member("relatedObject", as_value());
		set_member("ctrlKey", as_value(false));
		set_member("altKey", as_value(false));
		set_member("shiftKey", as_value(false));
		set_member("buttonDown", as_value(false));
		set_member("delta", as_value(0));
	}

	void as_mouse_event::register_mouse_event_constants(as_object* target)
	{
		target->set_member("CLICK", as_value("click"));
		target->set_member("DOUBLE_CLICK", as_value("doubleClick"));
		target->set_member("MOUSE_DOWN", as_value("mouseDown"));
		target->set_member("MOUSE_MOVE", as_value("mouseMove"));
		target->set_member("MOUSE_OUT", as_value("mouseOut"));
		target->set_member("MOUSE_OVER", as_value("mouseOver"));
		target->set_member("MOUSE_UP", as_value("mouseUp"));
		target->set_member("MOUSE_WHEEL", as_value("mouseWheel"));
		target->set_member("ROLL_OUT", as_value("rollOut"));
		target->set_member("ROLL_OVER", as_value("rollOver"));
		target->set_member("MIDDLE_CLICK", as_value("middleClick"));
		target->set_member("MIDDLE_MOUSE_DOWN", as_value("middleMouseDown"));
		target->set_member("MIDDLE_MOUSE_UP", as_value("middleMouseUp"));
		target->set_member("RIGHT_CLICK", as_value("rightClick"));
		target->set_member("RIGHT_MOUSE_DOWN", as_value("rightMouseDown"));
		target->set_member("RIGHT_MOUSE_UP", as_value("rightMouseUp"));
		target->set_member("CONTEXT_MENU", as_value("contextMenu"));
	}

	// ===== KeyboardEvent =====

	as_keyboard_event::as_keyboard_event(player* player, const char* type, bool bubbles, bool cancelable) :
		as_event(player, type, bubbles, cancelable)
	{
		// KeyboardEvent specific properties
		set_member("charCode", as_value(0));
		set_member("keyCode", as_value(0));
		set_member("keyLocation", as_value(0));
		set_member("ctrlKey", as_value(false));
		set_member("altKey", as_value(false));
		set_member("shiftKey", as_value(false));
		set_member("commandKey", as_value(false));
		set_member("controlKey", as_value(false));
	}

	void as_keyboard_event::register_keyboard_event_constants(as_object* target)
	{
		target->set_member("KEY_DOWN", as_value("keyDown"));
		target->set_member("KEY_UP", as_value("keyUp"));
	}

	// ===== TimerEvent =====

	as_timer_event::as_timer_event(player* player, const char* type, bool bubbles, bool cancelable) :
		as_event(player, type, bubbles, cancelable)
	{
	}

	void as_timer_event::register_timer_event_constants(as_object* target)
	{
		target->set_member("TIMER", as_value("timer"));
		target->set_member("TIMER_COMPLETE", as_value("timerComplete"));
	}

} // end namespace gameswf
