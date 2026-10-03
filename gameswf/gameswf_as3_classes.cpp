// gameswf_as3_classes.cpp -- AS3 built-in classes support
// This source code has been donated to the Public Domain.

#include "gameswf_as3_classes.h"
#include "gameswf/gameswf_log.h"
#include "gameswf/gameswf_abc.h"
#include "gameswf/gameswf_as_classes/as_timer.h"
#include "gameswf/gameswf_as_classes/as_event.h"

namespace gameswf
{

	// Helper to create a class object
	static as_object* create_class(player* player, const char* name, as_object* prototype)
	{
		as_object* cls = new as_object(player);
		cls->set_member("prototype", as_value(prototype));
		prototype->set_member("constructor", as_value(cls));
		return cls;
	}

	// Helper to get prototype from a class object
	static as_object* get_prototype(as_object* cls)
	{
		if (cls == NULL) return NULL;
		as_value proto_val;
		cls->get_member("prototype", &proto_val);
		return proto_val.to_object();
	}

	// ===== AS3 Utility Functions =====

	// getTimer():int - Returns the number of milliseconds since the SWF started
	static void as3_getTimer(const fn_call& fn)
	{
		// Return milliseconds since player started
		// This is a simplified implementation
		fn.result->set_int(0);  // TODO: Implement actual timer
	}

	// describeType(value:*):XML - Returns XML description of the value's type
	static void as3_describeType(const fn_call& fn)
	{
		// Simplified implementation - return a basic XML structure
		// Full implementation would introspect the object's class
		if (fn.nargs < 1)
		{
			fn.result->set_undefined();
			return;
		}

		as_value val = fn.arg(0);
		const char* type_name = "Object";

		if (val.is_number()) type_name = "Number";
		else if (val.is_string()) type_name = "String";
		else if (val.is_bool()) type_name = "Boolean";
		else if (val.is_object())
		{
			as_object* obj = val.to_object();
			if (obj)
			{
				// Try to get class name from constructor
				as_value ctor_val;
				if (obj->get_member("constructor", &ctor_val))
				{
					// This is a simplified approach
					type_name = "Object";
				}
			}
		}

		// Return a simple XML-like string (simplified)
		char xml_buf[256];
		snprintf(xml_buf, sizeof(xml_buf), "<type name=\"%s\"/>", type_name);
		fn.result->set_string(xml_buf);
	}

	// getDefinitionByName(name:String):Object - Returns a reference to the class with the given name
	static void as3_getDefinitionByName(const fn_call& fn)
	{
		if (fn.nargs < 1)
		{
			fn.result->set_undefined();
			return;
		}

		tu_string class_name = fn.arg(0).to_string();

		// Look up the class in the global object
		as_value class_val;
		if (fn.get_player()->get_global()->get_member(class_name.c_str(), &class_val))
		{
			*fn.result = class_val;
		}
		else
		{
			fn.result->set_undefined();
		}
	}

	// getQualifiedClassName(value:*):String - Returns the fully qualified class name
	static void as3_getQualifiedClassName(const fn_call& fn)
	{
		if (fn.nargs < 1)
		{
			fn.result->set_string("void");
			return;
		}

		as_value val = fn.arg(0);
		const char* type_name = "Object";

		if (val.is_number()) type_name = "Number";
		else if (val.is_string()) type_name = "String";
		else if (val.is_bool()) type_name = "Boolean";
		else if (val.is_null()) type_name = "null";
		else if (val.is_undefined()) type_name = "void";

		fn.result->set_string(type_name);
	}

	// getQualifiedSuperclassName(value:*):String - Returns the superclass name
	static void as3_getQualifiedSuperclassName(const fn_call& fn)
	{
		// Simplified - always return "Object" for objects
		if (fn.nargs < 1)
		{
			fn.result->set_undefined();
			return;
		}

		as_value val = fn.arg(0);
		if (val.is_object() && val.to_object() != NULL)
		{
			fn.result->set_string("Object");
		}
		else
		{
			fn.result->set_undefined();
		}
	}

	// Note: setInterval/setTimeout/clearInterval/clearTimeout are now implemented in as_timer.cpp

	void as3_init_classes(player* player, as_object* global)
	{
		if (global == NULL) return;

		// Top-level classes
		as_object* object_class = create_class(player, "Object", new as_object(player));
		global->set_member("Object", as_value(object_class));

		as_object* function_class = create_class(player, "Function", new as_object(player));
		global->set_member("Function", as_value(function_class));

		as_object* class_class = create_class(player, "Class", new as_object(player));
		global->set_member("Class", as_value(class_class));

		as_object* boolean_class = create_class(player, "Boolean", new as_object(player));
		global->set_member("Boolean", as_value(boolean_class));

		as_object* number_class = create_class(player, "Number", new as_object(player));
		global->set_member("Number", as_value(number_class));

		as_object* int_class = create_class(player, "int", new as_object(player));
		global->set_member("int", as_value(int_class));

		as_object* uint_class = create_class(player, "uint", new as_object(player));
		global->set_member("uint", as_value(uint_class));

		as_object* string_class = create_class(player, "String", new as_object(player));
		global->set_member("String", as_value(string_class));

		as_object* array_class = create_class(player, "Array", new as_object(player));
		global->set_member("Array", as_value(array_class));

		// flash.events package
		as_object* flash_pkg = new as_object(player);
		as_object* events_pkg = new as_object(player);

		as_object* event_class = create_class(player, "Event", new as_object(player));
		as3_event::init(get_prototype(event_class));
		as_event::register_event_constants(event_class);
		events_pkg->set_member("Event", as_value(event_class));
		global->set_member("Event", as_value(event_class));

		as_object* event_dispatcher_class = create_class(player, "EventDispatcher", new as_object(player));
		as3_event_dispatcher::init(get_prototype(event_dispatcher_class));
		events_pkg->set_member("EventDispatcher", as_value(event_dispatcher_class));
		global->set_member("EventDispatcher", as_value(event_dispatcher_class));

		// MouseEvent, KeyboardEvent, etc.
		as_object* mouse_event_class = create_class(player, "MouseEvent", new as_object(player));
		as_mouse_event::register_mouse_event_constants(mouse_event_class);
		events_pkg->set_member("MouseEvent", as_value(mouse_event_class));
		global->set_member("MouseEvent", as_value(mouse_event_class));

		as_object* keyboard_event_class = create_class(player, "KeyboardEvent", new as_object(player));
		as_keyboard_event::register_keyboard_event_constants(keyboard_event_class);
		events_pkg->set_member("KeyboardEvent", as_value(keyboard_event_class));
		global->set_member("KeyboardEvent", as_value(keyboard_event_class));

		as_object* timer_event_class = create_class(player, "TimerEvent", new as_object(player));
		as_timer_event::register_timer_event_constants(timer_event_class);
		events_pkg->set_member("TimerEvent", as_value(timer_event_class));
		global->set_member("TimerEvent", as_value(timer_event_class));

		// Error / menu event types (constants only; no instances are constructed here)
		as_object* io_error_event_class = create_class(player, "IOErrorEvent", new as_object(player));
		io_error_event_class->set_member("IO_ERROR", as_value("ioError"));
		io_error_event_class->set_member("ERROR", as_value("ioError"));
		events_pkg->set_member("IOErrorEvent", as_value(io_error_event_class));
		global->set_member("IOErrorEvent", as_value(io_error_event_class));

		as_object* security_error_event_class = create_class(player, "SecurityErrorEvent", new as_object(player));
		security_error_event_class->set_member("SECURITY_ERROR", as_value("securityError"));
		events_pkg->set_member("SecurityErrorEvent", as_value(security_error_event_class));
		global->set_member("SecurityErrorEvent", as_value(security_error_event_class));

		as_object* context_menu_event_class = create_class(player, "ContextMenuEvent", new as_object(player));
		context_menu_event_class->set_member("MENU_ITEM_SELECT", as_value("menuItemSelect"));
		context_menu_event_class->set_member("MENU_ITEM_ENABLE", as_value("menuItemEnable"));
		context_menu_event_class->set_member("MENU_ITEM_SELECT_ENABLE", as_value("menuItemSelectEnable"));
		events_pkg->set_member("ContextMenuEvent", as_value(context_menu_event_class));
		global->set_member("ContextMenuEvent", as_value(context_menu_event_class));

		flash_pkg->set_member("events", as_value(events_pkg));

		// flash.display package
		as_object* display_pkg = new as_object(player);

		as_object* display_object_class = create_class(player, "DisplayObject", new as_object(player));
		as3_display_object::init(get_prototype(display_object_class));
		display_pkg->set_member("DisplayObject", as_value(display_object_class));
		global->set_member("DisplayObject", as_value(display_object_class));

		as_object* interactive_object_class = create_class(player, "InteractiveObject", new as_object(player));
		display_pkg->set_member("InteractiveObject", as_value(interactive_object_class));
		global->set_member("InteractiveObject", as_value(interactive_object_class));

		as_object* display_object_container_class = create_class(player, "DisplayObjectContainer", new as_object(player));
		as3_display_object_container::init(get_prototype(display_object_container_class));
		display_pkg->set_member("DisplayObjectContainer", as_value(display_object_container_class));
		global->set_member("DisplayObjectContainer", as_value(display_object_container_class));

		as_object* sprite_class = create_class(player, "Sprite", new as_object(player));
		as3_sprite::init(get_prototype(sprite_class));
		display_pkg->set_member("Sprite", as_value(sprite_class));
		global->set_member("Sprite", as_value(sprite_class));

		as_object* movie_clip_class = create_class(player, "MovieClip", new as_object(player));
		as3_movie_clip::init(get_prototype(movie_clip_class));
		display_pkg->set_member("MovieClip", as_value(movie_clip_class));
		global->set_member("MovieClip", as_value(movie_clip_class));

		as_object* stage_class = create_class(player, "Stage", new as_object(player));
		as3_stage::init(get_prototype(stage_class));
		display_pkg->set_member("Stage", as_value(stage_class));
		global->set_member("Stage", as_value(stage_class));

		as_object* shape_class = create_class(player, "Shape", new as_object(player));
		display_pkg->set_member("Shape", as_value(shape_class));
		global->set_member("Shape", as_value(shape_class));

		as_object* bitmap_class = create_class(player, "Bitmap", new as_object(player));
		display_pkg->set_member("Bitmap", as_value(bitmap_class));
		global->set_member("Bitmap", as_value(bitmap_class));

		as_object* text_field_class = create_class(player, "TextField", new as_object(player));
		display_pkg->set_member("TextField", as_value(text_field_class));
		global->set_member("TextField", as_value(text_field_class));

		as_object* loader_class = create_class(player, "Loader", new as_object(player));
		display_pkg->set_member("Loader", as_value(loader_class));
		global->set_member("Loader", as_value(loader_class));

		flash_pkg->set_member("display", as_value(display_pkg));

		// flash.utils package
		as_object* utils_pkg = new as_object(player);

		as_object* timer_class = create_class(player, "Timer", new as_object(player));
		as3_timer::init(get_prototype(timer_class));
		utils_pkg->set_member("Timer", as_value(timer_class));

		as_object* byte_array_class = create_class(player, "ByteArray", new as_object(player));
		as3_byte_array::init(get_prototype(byte_array_class));
		utils_pkg->set_member("ByteArray", as_value(byte_array_class));

		as_object* dictionary_class = create_class(player, "Dictionary", new as_object(player));
		as3_dictionary::init(get_prototype(dictionary_class));
		utils_pkg->set_member("Dictionary", as_value(dictionary_class));

		// getTimer, setInterval, setTimeout, etc.
		utils_pkg->set_member("getTimer", as_value(as3_getTimer));
		utils_pkg->set_member("setInterval", as_value(gameswf::as3_setInterval));
		utils_pkg->set_member("setTimeout", as_value(gameswf::as3_setTimeout));
		utils_pkg->set_member("clearInterval", as_value(gameswf::as3_clearInterval));
		utils_pkg->set_member("clearTimeout", as_value(gameswf::as3_clearTimeout));
		utils_pkg->set_member("describeType", as_value(as3_describeType));
		utils_pkg->set_member("getDefinitionByName", as_value(as3_getDefinitionByName));
		utils_pkg->set_member("getQualifiedClassName", as_value(as3_getQualifiedClassName));
		utils_pkg->set_member("getQualifiedSuperclassName", as_value(as3_getQualifiedSuperclassName));

		flash_pkg->set_member("utils", as_value(utils_pkg));

		// flash.geom package
		as_object* geom_pkg = new as_object(player);

		as_object* point_class = create_class(player, "Point", new as_object(player));
		as3_point::init(get_prototype(point_class));
		geom_pkg->set_member("Point", as_value(point_class));

		as_object* rectangle_class = create_class(player, "Rectangle", new as_object(player));
		as3_rectangle::init(get_prototype(rectangle_class));
		geom_pkg->set_member("Rectangle", as_value(rectangle_class));

		as_object* matrix_class = create_class(player, "Matrix", new as_object(player));
		as3_matrix::init(get_prototype(matrix_class));
		geom_pkg->set_member("Matrix", as_value(matrix_class));

		as_object* color_transform_class = create_class(player, "ColorTransform", new as_object(player));
		geom_pkg->set_member("ColorTransform", as_value(color_transform_class));

		as_object* transform_class = create_class(player, "Transform", new as_object(player));
		geom_pkg->set_member("Transform", as_value(transform_class));

		flash_pkg->set_member("geom", as_value(geom_pkg));

		// flash.net package
		as_object* net_pkg = new as_object(player);
		net_pkg->set_member("URLLoader", as_value());
		net_pkg->set_member("URLRequest", as_value());
		net_pkg->set_member("URLVariables", as_value());
		net_pkg->set_member("SharedObject", as_value());
		net_pkg->set_member("LocalConnection", as_value());
		net_pkg->set_member("Socket", as_value());
		net_pkg->set_member("XMLSocket", as_value());
		flash_pkg->set_member("net", as_value(net_pkg));

		// flash.media package
		as_object* media_pkg = new as_object(player);
		media_pkg->set_member("Sound", as_value());
		media_pkg->set_member("SoundChannel", as_value());
		media_pkg->set_member("SoundTransform", as_value());
		media_pkg->set_member("Video", as_value());
		media_pkg->set_member("Camera", as_value());
		media_pkg->set_member("Microphone", as_value());
		flash_pkg->set_member("media", as_value(media_pkg));

		// flash.text package
		as_object* text_pkg = new as_object(player);
		text_pkg->set_member("TextField", as_value());
		text_pkg->set_member("TextFormat", as_value());
		text_pkg->set_member("StyleSheet", as_value());
		text_pkg->set_member("Font", as_value());
		flash_pkg->set_member("text", as_value(text_pkg));

		// flash.system package
		as_object* system_pkg = new as_object(player);
		system_pkg->set_member("Capabilities", as_value());
		system_pkg->set_member("Security", as_value());
		system_pkg->set_member("ApplicationDomain", as_value());
		flash_pkg->set_member("system", as_value(system_pkg));

		// flash.ui package
		as_object* ui_pkg = new as_object(player);
		ui_pkg->set_member("Mouse", as_value());
		ui_pkg->set_member("Keyboard", as_value());
		ui_pkg->set_member("ContextMenu", as_value());
		ui_pkg->set_member("ContextMenuItem", as_value());
		flash_pkg->set_member("ui", as_value(ui_pkg));

		// flash.external package
		as_object* external_pkg = new as_object(player);
		external_pkg->set_member("ExternalInterface", as_value());
		flash_pkg->set_member("external", as_value(external_pkg));

		// flash.filters package
		as_object* filters_pkg = new as_object(player);
		filters_pkg->set_member("BlurFilter", as_value());
		filters_pkg->set_member("DropShadowFilter", as_value());
		filters_pkg->set_member("GlowFilter", as_value());
		filters_pkg->set_member("BevelFilter", as_value());
		filters_pkg->set_member("ColorMatrixFilter", as_value());
		flash_pkg->set_member("filters", as_value(filters_pkg));

		// Register flash package
		global->set_member("flash", as_value(flash_pkg));

		// XML/E4X support
		as_object* xml_class = create_class(player, "XML", new as_object(player));
		global->set_member("XML", as_value(xml_class));

		as_object* xml_list_class = create_class(player, "XMLList", new as_object(player));
		global->set_member("XMLList", as_value(xml_list_class));

		// QName, Namespace
		as_object* qname_class = create_class(player, "QName", new as_object(player));
		global->set_member("QName", as_value(qname_class));

		as_object* namespace_class = create_class(player, "Namespace", new as_object(player));
		global->set_member("Namespace", as_value(namespace_class));

		// Error classes
		as_object* error_class = create_class(player, "Error", new as_object(player));
		global->set_member("Error", as_value(error_class));

		as_object* type_error_class = create_class(player, "TypeError", new as_object(player));
		global->set_member("TypeError", as_value(type_error_class));

		as_object* argument_error_class = create_class(player, "ArgumentError", new as_object(player));
		global->set_member("ArgumentError", as_value(argument_error_class));

		as_object* range_error_class = create_class(player, "RangeError", new as_object(player));
		global->set_member("RangeError", as_value(range_error_class));

		as_object* security_error_class = create_class(player, "SecurityError", new as_object(player));
		global->set_member("SecurityError", as_value(security_error_class));

		as_object* io_error_class = create_class(player, "IOError", new as_object(player));
		global->set_member("IOError", as_value(io_error_class));

		// RegExp
		as_object* regexp_class = create_class(player, "RegExp", new as_object(player));
		global->set_member("RegExp", as_value(regexp_class));

		// trace function
		global->set_member("trace", as_value());

		// Vector (AS3 generics)
		as_object* vector_class = create_class(player, "Vector", new as_object(player));
		global->set_member("Vector", as_value(vector_class));

		IF_VERBOSE_ACTION(log_msg("AS3 classes initialized\n"));

		// Register class method implementations (ByteArray, Point, Rectangle, Matrix, JSON)
		as3_register_class_implementations(player);

		// Register network, media, UI, and text class implementations
		as3_register_net_media_ui_classes(player);

		// Register remaining AS3 classes (XML, SharedObject, Socket, Filters, Video, etc.)
		as3_register_remaining_classes(player);
	}

} // namespace gameswf
