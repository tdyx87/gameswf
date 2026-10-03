// as_mouse_event.cpp	-- Vitaly Alexeev <tishka92@yahoo.com> 2008

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// flash9

#include "gameswf/gameswf_as_classes/as_mouse_event.h"

namespace gameswf
{

	as_mouse_event* mouse_event_init(player* player)
	{
		// Create built-in mouse event object.
		as_mouse_event*	obj = new as_mouse_event(player, "mouseEvent", true, false);

		// methods
		obj->builtin_member("CLICK", "click");

		return obj;
	}

};
