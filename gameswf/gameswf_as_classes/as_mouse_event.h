// as_mouse_event.h	-- Vitaly Alexeev <tishka92@yahoo.com> 2008

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// flash9

#ifndef GAMESWF_AS_MOUSE_EVENT_H
#define GAMESWF_AS_MOUSE_EVENT_H

#include "gameswf/gameswf_action.h"	// for as_object
#include "gameswf/gameswf_as_classes/as_event.h"

namespace gameswf
{
	// as_mouse_event is now defined in as_event.h
	// This header just provides the init function declaration

	// creates 'MouseEvent' object
	as_mouse_event* mouse_event_init(player* player);

}	// end namespace gameswf


#endif // GAMESWF_AS_MOUSE_EVENT_H
