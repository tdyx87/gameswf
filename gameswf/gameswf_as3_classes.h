// gameswf_as3_classes.h -- AS3 built-in classes support
// This source code has been donated to the Public Domain.

#ifndef GAMESWF_AS3_CLASSES_H
#define GAMESWF_AS3_CLASSES_H

#include "gameswf/gameswf_object.h"
#include "gameswf/gameswf_sprite.h"
#include "gameswf/gameswf_player.h"

namespace gameswf
{

	// AS3 Event class
	class as3_event : public as_object
	{
	public:
		as3_event(player* player) : as_object(player) {}

		static void init(as_object* target)
		{
			target->set_member("type", as_value());
			target->set_member("target", as_value());
			target->set_member("currentTarget", as_value());
			target->set_member("bubbles", as_value(false));
			target->set_member("cancelable", as_value(false));
			target->set_member("eventPhase", as_value(0.0));
		}
	};

	// AS3 EventDispatcher class
	class as3_event_dispatcher : public as_object
	{
	public:
		as3_event_dispatcher(player* player) : as_object(player) {}

		static void init(as_object* target)
		{
			// addEventListener(type:String, listener:Function, useCapture:Boolean = false, priority:int = 0, useWeakReference:Boolean = false)
			target->set_member("addEventListener", as_value());
			target->set_member("removeEventListener", as_value());
			target->set_member("dispatchEvent", as_value());
			target->set_member("hasEventListener", as_value());
			target->set_member("willTrigger", as_value());
		}
	};

	// AS3 DisplayObject class
	class as3_display_object : public as_object
	{
	public:
		as3_display_object(player* player) : as_object(player) {}

		static void init(as_object* target)
		{
			// Properties
			target->set_member("x", as_value(0.0));
			target->set_member("y", as_value(0.0));
			target->set_member("width", as_value(0.0));
			target->set_member("height", as_value(0.0));
			target->set_member("scaleX", as_value(1.0));
			target->set_member("scaleY", as_value(1.0));
			target->set_member("rotation", as_value(0.0));
			target->set_member("alpha", as_value(1.0));
			target->set_member("visible", as_value(true));
			target->set_member("name", as_value(""));
			target->set_member("parent", as_value());
			target->set_member("root", as_value());
			target->set_member("stage", as_value());
			target->set_member("mouseX", as_value(0.0));
			target->set_member("mouseY", as_value(0.0));

			// Methods
			target->set_member("getBounds", as_value());
			target->set_member("getRect", as_value());
			target->set_member("globalToLocal", as_value());
			target->set_member("localToGlobal", as_value());
			target->set_member("hitTestPoint", as_value());
			target->set_member("hitTestObject", as_value());
		}
	};

	// AS3 DisplayObjectContainer class
	class as3_display_object_container : public as_object
	{
	public:
		as3_display_object_container(player* player) : as_object(player) {}

		static void init(as_object* target)
		{
			// Properties
			target->set_member("numChildren", as_value(0.0));
			target->set_member("mouseChildren", as_value(true));
			target->set_member("tabChildren", as_value(true));

			// Methods are handled in AVM2 interpreter
		}
	};

	// AS3 Sprite class
	class as3_sprite : public as_object
	{
	public:
		as3_sprite(player* player) : as_object(player) {}

		static void init(as_object* target)
		{
			// Inherit from DisplayObjectContainer
			as3_display_object::init(target);
			as3_display_object_container::init(target);

			// Sprite specific
			target->set_member("graphics", as_value());
			target->set_member("buttonMode", as_value(false));
			target->set_member("useHandCursor", as_value(true));
			target->set_member("hitArea", as_value());
			target->set_member("dropTarget", as_value());

			// Methods
			target->set_member("startDrag", as_value());
			target->set_member("stopDrag", as_value());
		}
	};

	// AS3 MovieClip class
	class as3_movie_clip : public as_object
	{
	public:
		as3_movie_clip(player* player) : as_object(player) {}

		static void init(as_object* target)
		{
			// Inherit from Sprite
			as3_sprite::init(target);

			// MovieClip specific
			target->set_member("currentFrame", as_value(1.0));
			target->set_member("totalFrames", as_value(1.0));
			target->set_member("framesLoaded", as_value(1.0));
			target->set_member("currentLabel", as_value());
			target->set_member("currentScene", as_value());
			target->set_member("scenes", as_value());
			target->set_member("isPlaying", as_value(false));

			// Methods
			target->set_member("play", as_value());
			target->set_member("stop", as_value());
			target->set_member("gotoAndPlay", as_value());
			target->set_member("gotoAndStop", as_value());
			target->set_member("nextFrame", as_value());
			target->set_member("prevFrame", as_value());
			target->set_member("addFrameScript", as_value());
		}
	};

	// AS3 Stage class
	class as3_stage : public as_object
	{
	public:
		as3_stage(player* player) : as_object(player) {}

		static void init(as_object* target)
		{
			as3_display_object::init(target);
			as3_display_object_container::init(target);

			target->set_member("stageWidth", as_value(0.0));
			target->set_member("stageHeight", as_value(0.0));
			target->set_member("frameRate", as_value(24.0));
			target->set_member("backgroundColor", as_value(0xFFFFFF));
			target->set_member("align", as_value(""));
			target->set_member("scaleMode", as_value("noScale"));
			target->set_member("displayState", as_value("normal"));
			target->set_member("quality", as_value("high"));
			target->set_member("showDefaultContextMenu", as_value(true));
		}
	};

	// AS3 Timer class
	class as3_timer : public as_object
	{
	public:
		as3_timer(player* player) : as_object(player) {}

		static void init(as_object* target)
		{
			target->set_member("currentCount", as_value(0.0));
			target->set_member("delay", as_value(1000.0));
			target->set_member("repeatCount", as_value(0.0));
			target->set_member("running", as_value(false));

			target->set_member("start", as_value());
			target->set_member("stop", as_value());
			target->set_member("reset", as_value());
		}
	};

	// AS3 ByteArray class
	class as3_byte_array : public as_object
	{
	public:
		as3_byte_array(player* player) : as_object(player) {}
		static void init(as_object* target);
	};

	// AS3 Dictionary class
	class as3_dictionary : public as_object
	{
	public:
		as3_dictionary(player* player) : as_object(player) {}

		static void init(as_object* target)
		{
			// Dictionary uses object keys, implemented via weak references
		}
	};

	// AS3 Point class
	class as3_point : public as_object
	{
	public:
		as3_point(player* player) : as_object(player) {}
		static void init(as_object* target);
	};

	// AS3 Rectangle class
	class as3_rectangle : public as_object
	{
	public:
		as3_rectangle(player* player) : as_object(player) {}
		static void init(as_object* target);
	};

	// AS3 Matrix class
	class as3_matrix : public as_object
	{
	public:
		as3_matrix(player* player) : as_object(player) {}
		static void init(as_object* target);
	};

	// Initialize all AS3 classes
	void as3_init_classes(player* player, as_object* global);

	// Register network, media, UI, and text class implementations
	void as3_register_net_media_ui_classes(player* player);

	// Register class method implementations (ByteArray, Point, Rectangle, Matrix)
	void as3_register_class_implementations(player* player);

	// Register remaining AS3 classes (XML, SharedObject, Socket, Filters, Video, etc.)
	void as3_register_remaining_classes(player* player);

} // namespace gameswf

#endif // GAMESWF_AS3_CLASSES_H
