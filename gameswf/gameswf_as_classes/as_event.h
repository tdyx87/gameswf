// as_event.h -- AS3 Event class implementation
// This source code has been donated to the Public Domain.

#ifndef GAMESWF_AS_EVENT_H
#define GAMESWF_AS_EVENT_H

#include "gameswf/gameswf_action.h"
#include "gameswf/gameswf_character.h"

namespace gameswf
{

	// AS3 Event class with all standard event constants
	struct as_event : public as_object
	{
		enum { m_class_id = AS_EVENT };
		virtual bool is(int class_id) const
		{
			if (m_class_id == class_id) return true;
			else return as_object::is(class_id);
		}

		as_event(player* player, const char* type = "", bool bubbles = false, bool cancelable = false);

		// Event properties
		void set_type(const char* type);
		void set_target(as_object* target);
		void set_currentTarget(as_object* currentTarget);
		void set_eventPhase(int phase);

		// Static method to register Event constants on a target object
		static void register_event_constants(as_object* target);
	};

	// Event constructor
	void as_global_event_ctor(const fn_call& fn);

	// Event methods
	void as_event_stopPropagation(const fn_call& fn);
	void as_event_stopImmediatePropagation(const fn_call& fn);
	void as_event_preventDefault(const fn_call& fn);
	void as_event_formatToString(const fn_call& fn);
	void as_event_clone(const fn_call& fn);
	void as_event_toString(const fn_call& fn);

	// MouseEvent class
	struct as_mouse_event : public as_event
	{
		as_mouse_event(player* player, const char* type = "", bool bubbles = true, bool cancelable = false);
		static void register_mouse_event_constants(as_object* target);
	};

	// KeyboardEvent class
	struct as_keyboard_event : public as_event
	{
		as_keyboard_event(player* player, const char* type = "", bool bubbles = true, bool cancelable = false);
		static void register_keyboard_event_constants(as_object* target);
	};

	// TimerEvent class
	struct as_timer_event : public as_event
	{
		as_timer_event(player* player, const char* type = "", bool bubbles = false, bool cancelable = false);
		static void register_timer_event_constants(as_object* target);
	};

} // end namespace gameswf

#endif // GAMESWF_AS_EVENT_H
