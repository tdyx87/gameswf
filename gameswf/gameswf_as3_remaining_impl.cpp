// gameswf_as3_remaining_impl.cpp -- AS3 remaining class implementations
// This source code has been donated to the Public Domain.
//
// Implements: XML/E4X, trace, SharedObject, Socket, filters, Video, etc.

#include "gameswf/gameswf_as3_classes.h"
#include "gameswf/gameswf_as_classes/as_array.h"
#include <stdio.h>
#include <string.h>

namespace gameswf
{

// ========================================================================
// trace function implementation
// ========================================================================

static void as3_trace(const fn_call& fn)
{
	// Build trace message from all arguments
	tu_string msg;
	for (int i = 0; i < fn.nargs; i++)
	{
		if (i > 0) msg += " ";
		msg += fn.arg(i).to_string();
	}
	// Output to stderr (or could be redirected to a log)
	fprintf(stderr, "%s\n", msg.c_str());
	fflush(stderr);
}

// ========================================================================
// XML implementation (simplified E4X)
// ========================================================================

static void xml_constructor(const fn_call& fn)
{
	gc_ptr<as_object> xml_obj = new as_object(fn.get_player());

	if (fn.nargs >= 1)
	{
		// Store the XML string
		xml_obj->set_member("__xml_string__", fn.arg(0));
	}

	// Set up XML properties
	xml_obj->set_member("nodeKind", as_value("element"));
	xml_obj->set_member("localName", as_value(""));
	xml_obj->set_member("uri", as_value(""));
	xml_obj->set_member("prefix", as_value(""));

	fn.result->set_as_object(xml_obj.get_ptr());
}

static void xml_toString(const fn_call& fn)
{
	as_value val;
	fn.this_ptr->get_member("__xml_string__", &val);
	*fn.result = val;
}

static void xml_toXMLString(const fn_call& fn)
{
	xml_toString(fn);
}

static void xml_appendChild(const fn_call& fn)
{
	// Simplified - no-op
}

static void xml_prependChild(const fn_call& fn)
{
	// Simplified - no-op
}

static void xml_insertChildAfter(const fn_call& fn)
{
	// Simplified - no-op
}

static void xml_insertChildBefore(const fn_call& fn)
{
	// Simplified - no-op
}

static void xml_removeChild(const fn_call& fn)
{
	// Simplified - no-op
}

static void xml_replace(const fn_call& fn)
{
	// Simplified - no-op
}

static void xml_copy(const fn_call& fn)
{
	// Return a copy
	gc_ptr<as_object> copy = new as_object(fn.get_player());
	as_value val;
	fn.this_ptr->get_member("__xml_string__", &val);
	copy->set_member("__xml_string__", val);
	fn.result->set_as_object(copy.get_ptr());
}

static void xml_length(const fn_call& fn)
{
	fn.result->set_int(1);
}

static void xml_hasSimpleContent(const fn_call& fn)
{
	fn.result->set_bool(true);
}

static void xml_hasComplexContent(const fn_call& fn)
{
	fn.result->set_bool(false);
}

static void xml_child(const fn_call& fn)
{
	fn.result->set_undefined();
}

static void xml_children(const fn_call& fn)
{
	gc_ptr<as_array> arr = new as_array(fn.get_player());
	fn.result->set_as_object(arr.get_ptr());
}

static void xml_descendants(const fn_call& fn)
{
	gc_ptr<as_array> arr = new as_array(fn.get_player());
	fn.result->set_as_object(arr.get_ptr());
}

static void xml_attribute(const fn_call& fn)
{
	fn.result->set_undefined();
}

static void xml_attributes(const fn_call& fn)
{
	gc_ptr<as_array> arr = new as_array(fn.get_player());
	fn.result->set_as_object(arr.get_ptr());
}

static void xml_text(const fn_call& fn)
{
	as_value val;
	fn.this_ptr->get_member("__xml_string__", &val);
	*fn.result = val;
}

static void xml_elements(const fn_call& fn)
{
	gc_ptr<as_array> arr = new as_array(fn.get_player());
	fn.result->set_as_object(arr.get_ptr());
}

static void xml_normalize(const fn_call& fn)
{
	// Simplified - no-op
}

static void xml_setName(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	fn.this_ptr->set_member("localName", fn.arg(0));
}

static void xml_setLocalName(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	fn.this_ptr->set_member("localName", fn.arg(0));
}

static void xml_setNamespace(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	fn.this_ptr->set_member("uri", fn.arg(0));
}

// ========================================================================
// XMLList implementation
// ========================================================================

static void xmllist_constructor(const fn_call& fn)
{
	gc_ptr<as_array> list = new as_array(fn.get_player());
	fn.result->set_as_object(list.get_ptr());
}

static void xmllist_length(const fn_call& fn)
{
	as_array* arr = cast_to<as_array>(fn.this_ptr);
	if (arr) fn.result->set_int(arr->size());
	else fn.result->set_int(0);
}

static void xmllist_toString(const fn_call& fn)
{
	fn.result->set_string("");
}

// ========================================================================
// Namespace implementation
// ========================================================================

static void namespace_constructor(const fn_call& fn)
{
	gc_ptr<as_object> ns = new as_object(fn.get_player());
	if (fn.nargs >= 1)
	{
		ns->set_member("uri", fn.arg(0));
	}
	if (fn.nargs >= 2)
	{
		ns->set_member("prefix", fn.arg(1));
	}
	fn.result->set_as_object(ns.get_ptr());
}

// ========================================================================
// QName implementation
// ========================================================================

static void qname_constructor(const fn_call& fn)
{
	gc_ptr<as_object> qn = new as_object(fn.get_player());
	if (fn.nargs >= 1)
	{
		qn->set_member("localName", fn.arg(0));
	}
	if (fn.nargs >= 2)
	{
		qn->set_member("uri", fn.arg(1));
	}
	fn.result->set_as_object(qn.get_ptr());
}

// ========================================================================
// SharedObject implementation
// ========================================================================

static hash<tu_string, gc_ptr<as_object>> s_shared_objects;

static void sharedobject_getLocal(const fn_call& fn)
{
	if (fn.nargs < 1)
	{
		fn.result->set_null();
		return;
	}

	tu_string name = fn.arg(0).to_string();

	// Check if already exists
	gc_ptr<as_object> existing;
	if (s_shared_objects.get(name, &existing))
	{
		fn.result->set_as_object(existing.get_ptr());
		return;
	}

	// Create new SharedObject
	gc_ptr<as_object> so = new as_object(fn.get_player());
	so->set_member("data", as_value(new as_object(fn.get_player())));

	// Store in global cache
	s_shared_objects.add(name, so);

	fn.result->set_as_object(so.get_ptr());
}

static void sharedobject_flush(const fn_call& fn)
{
	// Simplified - mark as flushed but don't actually persist
	fn.result->set_string("flushed");
}

static void sharedobject_clear(const fn_call& fn)
{
	fn.this_ptr->set_member("data", as_value(new as_object(fn.get_player())));
}

static void sharedobject_getSize(const fn_call& fn)
{
	// Return approximate size
	fn.result->set_int(1024);
}

// ========================================================================
// Socket implementation (AS3)
// ========================================================================

static void socket_constructor(const fn_call& fn)
{
	gc_ptr<as_object> socket = new as_object(fn.get_player());
	socket->set_member("connected", as_value(false));
	socket->set_member("bytesAvailable", as_value(0));
	socket->set_member("endian", as_value("bigEndian"));
	fn.result->set_as_object(socket.get_ptr());
}

static void socket_connect(const fn_call& fn)
{
	if (fn.nargs < 2) return;
	const char* host = fn.arg(0).to_string();
	int port = (int)fn.arg(1).to_number();

	// Simplified - just mark as connected
	fn.this_ptr->set_member("connected", as_value(true));

	// Dispatch connect event
	fprintf(stderr, "Socket connected to %s:%d\n", host, port);
}

static void socket_close(const fn_call& fn)
{
	fn.this_ptr->set_member("connected", as_value(false));
}

static void socket_flush(const fn_call& fn)
{
	// Simplified - no-op
}

static void socket_readBytes(const fn_call& fn)
{
	// Simplified - no-op
}

static void socket_writeBytes(const fn_call& fn)
{
	// Simplified - no-op
}

static void socket_readByte(const fn_call& fn)
{
	fn.result->set_int(0);
}

static void socket_writeByte(const fn_call& fn)
{
	// Simplified - no-op
}

static void socket_readUTF(const fn_call& fn)
{
	fn.result->set_string("");
}

static void socket_writeUTF(const fn_call& fn)
{
	// Simplified - no-op
}

static void socket_readUTFBytes(const fn_call& fn)
{
	fn.result->set_string("");
}

static void socket_writeUTFBytes(const fn_call& fn)
{
	// Simplified - no-op
}

// ========================================================================
// XMLSocket implementation (AS3)
// ========================================================================

static void xmlsocket_constructor(const fn_call& fn)
{
	gc_ptr<as_object> socket = new as_object(fn.get_player());
	socket->set_member("connected", as_value(false));
	fn.result->set_as_object(socket.get_ptr());
}

static void xmlsocket_connect(const fn_call& fn)
{
	if (fn.nargs < 2) return;
	const char* host = fn.arg(0).to_string();
	int port = (int)fn.arg(1).to_number();

	fn.this_ptr->set_member("connected", as_value(true));
	fprintf(stderr, "XMLSocket connected to %s:%d\n", host, port);
}

static void xmlsocket_close(const fn_call& fn)
{
	fn.this_ptr->set_member("connected", as_value(false));
}

static void xmlsocket_send(const fn_call& fn)
{
	// Simplified - no-op
}

// ========================================================================
// Filter implementations (simplified)
// ========================================================================

static void blurfilter_constructor(const fn_call& fn)
{
	gc_ptr<as_object> filter = new as_object(fn.get_player());
	filter->set_member("blurX", as_value(4.0));
	filter->set_member("blurY", as_value(4.0));
	filter->set_member("quality", as_value(1));
	fn.result->set_as_object(filter.get_ptr());
}

static void dropshadowfilter_constructor(const fn_call& fn)
{
	gc_ptr<as_object> filter = new as_object(fn.get_player());
	filter->set_member("distance", as_value(4.0));
	filter->set_member("angle", as_value(45.0));
	filter->set_member("color", as_value(0));
	filter->set_member("alpha", as_value(1.0));
	filter->set_member("blurX", as_value(4.0));
	filter->set_member("blurY", as_value(4.0));
	filter->set_member("strength", as_value(1.0));
	filter->set_member("quality", as_value(1));
	filter->set_member("inner", as_value(false));
	filter->set_member("knockout", as_value(false));
	filter->set_member("hideObject", as_value(false));
	fn.result->set_as_object(filter.get_ptr());
}

static void glowfilter_constructor(const fn_call& fn)
{
	gc_ptr<as_object> filter = new as_object(fn.get_player());
	filter->set_member("color", as_value(0xFF0000));
	filter->set_member("alpha", as_value(1.0));
	filter->set_member("blurX", as_value(6.0));
	filter->set_member("blurY", as_value(6.0));
	filter->set_member("strength", as_value(2.0));
	filter->set_member("quality", as_value(1));
	filter->set_member("inner", as_value(false));
	filter->set_member("knockout", as_value(false));
	fn.result->set_as_object(filter.get_ptr());
}

static void bevelfilter_constructor(const fn_call& fn)
{
	gc_ptr<as_object> filter = new as_object(fn.get_player());
	filter->set_member("distance", as_value(4.0));
	filter->set_member("angle", as_value(45.0));
	filter->set_member("highlightColor", as_value(0xFFFFFF));
	filter->set_member("highlightAlpha", as_value(1.0));
	filter->set_member("shadowColor", as_value(0x000000));
	filter->set_member("shadowAlpha", as_value(1.0));
	filter->set_member("blurX", as_value(4.0));
	filter->set_member("blurY", as_value(4.0));
	filter->set_member("strength", as_value(1.0));
	filter->set_member("quality", as_value(1));
	filter->set_member("type", as_value("inner"));
	filter->set_member("knockout", as_value(false));
	fn.result->set_as_object(filter.get_ptr());
}

static void colormatrixfilter_constructor(const fn_call& fn)
{
	gc_ptr<as_object> filter = new as_object(fn.get_player());
	// Identity matrix
	gc_ptr<as_array> matrix = new as_array(fn.get_player());
	for (int i = 0; i < 20; i++)
	{
		matrix->push(as_value(i % 6 == 0 ? 1.0 : 0.0));
	}
	filter->set_member("matrix", as_value(matrix.get_ptr()));
	fn.result->set_as_object(filter.get_ptr());
}

// ========================================================================
// Video implementation (simplified)
// ========================================================================

static void video_constructor(const fn_call& fn)
{
	gc_ptr<as_object> video = new as_object(fn.get_player());
	video->set_member("width", as_value(320.0));
	video->set_member("height", as_value(240.0));
	video->set_member("videoWidth", as_value(0));
	video->set_member("videoHeight", as_value(0));
	video->set_member("deblocking", as_value(0));
	video->set_member("smoothing", as_value(false));
	fn.result->set_as_object(video.get_ptr());
}

static void video_attachNetStream(const fn_call& fn)
{
	// Simplified - no-op
}

static void video_attachCamera(const fn_call& fn)
{
	// Simplified - no-op
}

static void video_clear(const fn_call& fn)
{
	// Simplified - no-op
}

// ========================================================================
// Camera implementation (simplified)
// ========================================================================

static void camera_getCamera(const fn_call& fn)
{
	// Return null (no camera available)
	fn.result->set_null();
}

static void camera_constructor(const fn_call& fn)
{
	gc_ptr<as_object> camera = new as_object(fn.get_player());
	camera->set_member("name", as_value(""));
	camera->set_member("width", as_value(0));
	camera->set_member("height", as_value(0));
	camera->set_member("fps", as_value(0));
	camera->set_member("currentFPS", as_value(0));
	camera->set_member("bandwidth", as_value(0));
	camera->set_member("quality", as_value(0));
	camera->set_member("activityLevel", as_value(-1));
	camera->set_member("motionLevel", as_value(0));
	camera->set_member("motionTimeout", as_value(0));
	fn.result->set_as_object(camera.get_ptr());
}

// ========================================================================
// Microphone implementation (simplified)
// ========================================================================

static void microphone_getMicrophone(const fn_call& fn)
{
	// Return null (no microphone available)
	fn.result->set_null();
}

static void microphone_constructor(const fn_call& fn)
{
	gc_ptr<as_object> mic = new as_object(fn.get_player());
	mic->set_member("name", as_value(""));
	mic->set_member("gain", as_value(50));
	mic->set_member("rate", as_value(8));
	mic->set_member("silenceLevel", as_value(0));
	mic->set_member("silenceTimeout", as_value(0));
	mic->set_member("activityLevel", as_value(-1));
	mic->set_member("muted", as_value(true));
	fn.result->set_as_object(mic.get_ptr());
}

// ========================================================================
// StyleSheet implementation (simplified)
// ========================================================================

static void stylesheet_constructor(const fn_call& fn)
{
	gc_ptr<as_object> ss = new as_object(fn.get_player());
	ss->set_member("styleNames", as_value(new as_array(fn.get_player())));
	fn.result->set_as_object(ss.get_ptr());
}

static void stylesheet_parseCSS(const fn_call& fn)
{
	// Simplified - no-op
	fn.result->set_bool(true);
}

static void stylesheet_getStyle(const fn_call& fn)
{
	fn.result->set_undefined();
}

static void stylesheet_setStyle(const fn_call& fn)
{
	// Simplified - no-op
}

static void stylesheet_clear(const fn_call& fn)
{
	// Simplified - no-op
}

// ========================================================================
// Font implementation (simplified)
// ========================================================================

static void font_enumerateFonts(const fn_call& fn)
{
	gc_ptr<as_array> fonts = new as_array(fn.get_player());
	// Return empty list
	fn.result->set_as_object(fonts.get_ptr());
}

static void font_registerFont(const fn_call& fn)
{
	// Simplified - no-op
}

static void font_constructor(const fn_call& fn)
{
	gc_ptr<as_object> font = new as_object(fn.get_player());
	font->set_member("fontName", as_value(""));
	font->set_member("fontStyle", as_value("regular"));
	font->set_member("fontType", as_value("device"));
	fn.result->set_as_object(font.get_ptr());
}

static void font_hasGlyphs(const fn_call& fn)
{
	fn.result->set_bool(true);
}

// ========================================================================
// ApplicationDomain implementation
// ========================================================================

static void applicationdomain_get_currentDomain(const fn_call& fn)
{
	gc_ptr<as_object> domain = new as_object(fn.get_player());
	domain->set_member("parentDomain", as_value());
	fn.result->set_as_object(domain.get_ptr());
}

static void applicationdomain_constructor(const fn_call& fn)
{
	gc_ptr<as_object> domain = new as_object(fn.get_player());
	if (fn.nargs >= 1)
	{
		domain->set_member("parentDomain", fn.arg(0));
	}
	fn.result->set_as_object(domain.get_ptr());
}

static void applicationdomain_getDefinition(const fn_call& fn)
{
	if (fn.nargs < 1)
	{
		fn.result->set_undefined();
		return;
	}
	// Try to get from global
	as_value val;
	fn.get_player()->get_global()->get_member(fn.arg(0).to_string(), &val);
	*fn.result = val;
}

static void applicationdomain_hasDefinition(const fn_call& fn)
{
	if (fn.nargs < 1)
	{
		fn.result->set_bool(false);
		return;
	}
	as_value val;
	bool has = fn.get_player()->get_global()->get_member(fn.arg(0).to_string(), &val);
	fn.result->set_bool(has);
}

// ========================================================================
// ContextMenu implementation
// ========================================================================

static void contextmenu_constructor(const fn_call& fn)
{
	gc_ptr<as_object> menu = new as_object(fn.get_player());
	menu->set_member("customItems", as_value(new as_array(fn.get_player())));
	menu->set_member("builtInItems", as_value(new as_object(fn.get_player())));
	menu->set_member("hideBuiltInItems", as_value(false));
	fn.result->set_as_object(menu.get_ptr());
}

static void contextmenu_hideBuiltInItems(const fn_call& fn)
{
	fn.this_ptr->set_member("hideBuiltInItems", as_value(true));
}

// ========================================================================
// ContextMenuItem implementation
// ========================================================================

static void contextmenuitem_constructor(const fn_call& fn)
{
	gc_ptr<as_object> item = new as_object(fn.get_player());
	if (fn.nargs >= 1) item->set_member("caption", fn.arg(0));
	else item->set_member("caption", as_value(""));
	if (fn.nargs >= 2) item->set_member("separatorBefore", fn.arg(1));
	else item->set_member("separatorBefore", as_value(false));
	if (fn.nargs >= 3) item->set_member("enabled", fn.arg(2));
	else item->set_member("enabled", as_value(true));
	if (fn.nargs >= 4) item->set_member("visible", fn.arg(3));
	else item->set_member("visible", as_value(true));
	fn.result->set_as_object(item.get_ptr());
}

// ========================================================================
// Registration function
// ========================================================================

void as3_register_remaining_classes(player* player)
{
	as_object* global = player->get_global();

	// Get flash package and sub-packages
	as_object* flash_pkg = NULL;
	as_object* media_pkg = NULL;
	as_object* text_pkg = NULL;
	as_object* system_pkg = NULL;
	as_object* ui_pkg = NULL;
	as_object* net_pkg = NULL;

	as_value flash_val;
	if (global->get_member("flash", &flash_val) && flash_val.is_object())
	{
		flash_pkg = flash_val.to_object();
		as_value media_val, text_val, system_val, ui_val, net_val;
		if (flash_pkg->get_member("media", &media_val) && media_val.is_object())
			media_pkg = media_val.to_object();
		if (flash_pkg->get_member("text", &text_val) && text_val.is_object())
			text_pkg = text_val.to_object();
		if (flash_pkg->get_member("system", &system_val) && system_val.is_object())
			system_pkg = system_val.to_object();
		if (flash_pkg->get_member("ui", &ui_val) && ui_val.is_object())
			ui_pkg = ui_val.to_object();
		if (flash_pkg->get_member("net", &net_val) && net_val.is_object())
			net_pkg = net_val.to_object();
	}

	// ===== trace function =====
	global->set_member("trace", as_value(as3_trace));

	// ===== XML =====
	{
		as_object* xml_class = new as_object(player);
		xml_class->set_member("ignoreWhitespace", as_value(true));
		xml_class->set_member("ignoreComments", as_value(false));
		xml_class->set_member("ignoreProcessingInstructions", as_value(false));
		xml_class->set_member("prettyPrinting", as_value(true));
		xml_class->set_member("prettyIndent", as_value(2));

		// Static methods
		gc_ptr<as_object> xml_proto = new as_object(player);
		xml_proto->builtin_member("toString", as_value(xml_toString));
		xml_proto->builtin_member("toXMLString", as_value(xml_toXMLString));
		xml_proto->builtin_member("appendChild", as_value(xml_appendChild));
		xml_proto->builtin_member("prependChild", as_value(xml_prependChild));
		xml_proto->builtin_member("insertChildAfter", as_value(xml_insertChildAfter));
		xml_proto->builtin_member("insertChildBefore", as_value(xml_insertChildBefore));
		xml_proto->builtin_member("removeChild", as_value(xml_removeChild));
		xml_proto->builtin_member("replace", as_value(xml_replace));
		xml_proto->builtin_member("copy", as_value(xml_copy));
		xml_proto->builtin_member("length", as_value(xml_length));
		xml_proto->builtin_member("hasSimpleContent", as_value(xml_hasSimpleContent));
		xml_proto->builtin_member("hasComplexContent", as_value(xml_hasComplexContent));
		xml_proto->builtin_member("child", as_value(xml_child));
		xml_proto->builtin_member("children", as_value(xml_children));
		xml_proto->builtin_member("descendants", as_value(xml_descendants));
		xml_proto->builtin_member("attribute", as_value(xml_attribute));
		xml_proto->builtin_member("attributes", as_value(xml_attributes));
		xml_proto->builtin_member("text", as_value(xml_text));
		xml_proto->builtin_member("elements", as_value(xml_elements));
		xml_proto->builtin_member("normalize", as_value(xml_normalize));
		xml_proto->builtin_member("setName", as_value(xml_setName));
		xml_proto->builtin_member("setLocalName", as_value(xml_setLocalName));
		xml_proto->builtin_member("setNamespace", as_value(xml_setNamespace));

		global->set_member("XML", as_value(xml_constructor));
		global->set_member("XML_proto", as_value(xml_proto.get_ptr()));
	}

	// ===== XMLList =====
	{
		gc_ptr<as_object> xmllist_proto = new as_object(player);
		xmllist_proto->builtin_member("length", as_value(xmllist_length));
		xmllist_proto->builtin_member("toString", as_value(xmllist_toString));
		global->set_member("XMLList", as_value(xmllist_constructor));
	}

	// ===== Namespace =====
	{
		global->set_member("Namespace", as_value(namespace_constructor));
	}

	// ===== QName =====
	{
		global->set_member("QName", as_value(qname_constructor));
	}

	// ===== SharedObject =====
	{
		gc_ptr<as_object> so_proto = new as_object(player);
		so_proto->builtin_member("flush", as_value(sharedobject_flush));
		so_proto->builtin_member("clear", as_value(sharedobject_clear));
		so_proto->builtin_member("getSize", as_value(sharedobject_getSize));

		as_object* so_class = new as_object(player);
		so_class->set_member("prototype", as_value(so_proto.get_ptr()));
		so_class->builtin_member("getLocal", as_value(sharedobject_getLocal));
		global->set_member("SharedObject", as_value(so_class));
		if (net_pkg) net_pkg->set_member("SharedObject", as_value(so_class));
	}

	// ===== Socket =====
	{
		gc_ptr<as_object> socket_proto = new as_object(player);
		socket_proto->builtin_member("connect", as_value(socket_connect));
		socket_proto->builtin_member("close", as_value(socket_close));
		socket_proto->builtin_member("flush", as_value(socket_flush));
		socket_proto->builtin_member("readBytes", as_value(socket_readBytes));
		socket_proto->builtin_member("writeBytes", as_value(socket_writeBytes));
		socket_proto->builtin_member("readByte", as_value(socket_readByte));
		socket_proto->builtin_member("writeByte", as_value(socket_writeByte));
		socket_proto->builtin_member("readUTF", as_value(socket_readUTF));
		socket_proto->builtin_member("writeUTF", as_value(socket_writeUTF));
		socket_proto->builtin_member("readUTFBytes", as_value(socket_readUTFBytes));
		socket_proto->builtin_member("writeUTFBytes", as_value(socket_writeUTFBytes));

		as_object* socket_class = new as_object(player);
		socket_class->set_member("prototype", as_value(socket_proto.get_ptr()));
		global->set_member("Socket", as_value(socket_constructor));
		if (net_pkg) net_pkg->set_member("Socket", as_value(socket_constructor));
	}

	// ===== XMLSocket =====
	{
		gc_ptr<as_object> xmlsocket_proto = new as_object(player);
		xmlsocket_proto->builtin_member("connect", as_value(xmlsocket_connect));
		xmlsocket_proto->builtin_member("close", as_value(xmlsocket_close));
		xmlsocket_proto->builtin_member("send", as_value(xmlsocket_send));

		as_object* xmlsocket_class = new as_object(player);
		xmlsocket_class->set_member("prototype", as_value(xmlsocket_proto.get_ptr()));
		global->set_member("XMLSocket", as_value(xmlsocket_constructor));
		if (net_pkg) net_pkg->set_member("XMLSocket", as_value(xmlsocket_constructor));
	}

	// ===== Filters =====
	{
		as_object* filters_pkg = new as_object(player);
		filters_pkg->set_member("BlurFilter", as_value(blurfilter_constructor));
		filters_pkg->set_member("DropShadowFilter", as_value(dropshadowfilter_constructor));
		filters_pkg->set_member("GlowFilter", as_value(glowfilter_constructor));
		filters_pkg->set_member("BevelFilter", as_value(bevelfilter_constructor));
		filters_pkg->set_member("ColorMatrixFilter", as_value(colormatrixfilter_constructor));

		// Add to flash package
		as_value flash_val;
		global->get_member("flash", &flash_val);
		if (flash_val.is_object())
		{
			flash_val.to_object()->set_member("filters", as_value(filters_pkg));
		}
	}

	// ===== Video =====
	{
		gc_ptr<as_object> video_proto = new as_object(player);
		video_proto->builtin_member("attachNetStream", as_value(video_attachNetStream));
		video_proto->builtin_member("attachCamera", as_value(video_attachCamera));
		video_proto->builtin_member("clear", as_value(video_clear));

		as_object* video_class = new as_object(player);
		video_class->set_member("prototype", as_value(video_proto.get_ptr()));
		global->set_member("Video", as_value(video_constructor));
		if (media_pkg) media_pkg->set_member("Video", as_value(video_constructor));
	}

	// ===== Camera =====
	{
		gc_ptr<as_object> camera_class = new as_object(player);
		camera_class->builtin_member("getCamera", as_value(camera_getCamera));
		global->set_member("Camera", as_value(camera_class));
		if (media_pkg) media_pkg->set_member("Camera", as_value(camera_class));
	}

	// ===== Microphone =====
	{
		gc_ptr<as_object> mic_class = new as_object(player);
		mic_class->builtin_member("getMicrophone", as_value(microphone_getMicrophone));
		global->set_member("Microphone", as_value(mic_class));
		if (media_pkg) media_pkg->set_member("Microphone", as_value(mic_class));
	}

	// ===== StyleSheet =====
	{
		gc_ptr<as_object> ss_proto = new as_object(player);
		ss_proto->builtin_member("parseCSS", as_value(stylesheet_parseCSS));
		ss_proto->builtin_member("getStyle", as_value(stylesheet_getStyle));
		ss_proto->builtin_member("setStyle", as_value(stylesheet_setStyle));
		ss_proto->builtin_member("clear", as_value(stylesheet_clear));

		as_object* ss_class = new as_object(player);
		ss_class->set_member("prototype", as_value(ss_proto.get_ptr()));
		global->set_member("StyleSheet", as_value(stylesheet_constructor));
		if (text_pkg) text_pkg->set_member("StyleSheet", as_value(stylesheet_constructor));
	}

	// ===== Font =====
	{
		gc_ptr<as_object> font_class = new as_object(player);
		font_class->builtin_member("enumerateFonts", as_value(font_enumerateFonts));
		font_class->builtin_member("registerFont", as_value(font_registerFont));
		font_class->set_member("prototype", as_value(new as_object(player)));
		{
			as_value proto_val;
			font_class->get_member("prototype", &proto_val);
			proto_val.to_object()->builtin_member("hasGlyphs", as_value(font_hasGlyphs));
		}
		global->set_member("Font", as_value(font_class));
		if (text_pkg) text_pkg->set_member("Font", as_value(font_class));
	}

	// ===== ApplicationDomain =====
	{
		gc_ptr<as_object> ad_proto = new as_object(player);
		ad_proto->builtin_member("getDefinition", as_value(applicationdomain_getDefinition));
		ad_proto->builtin_member("hasDefinition", as_value(applicationdomain_hasDefinition));

		as_object* ad_class = new as_object(player);
		ad_class->set_member("prototype", as_value(ad_proto.get_ptr()));
		ad_class->builtin_member("currentDomain", as_value(applicationdomain_get_currentDomain));
		global->set_member("ApplicationDomain", as_value(ad_class));
		if (system_pkg) system_pkg->set_member("ApplicationDomain", as_value(ad_class));
	}

	// ===== ContextMenu =====
	{
		gc_ptr<as_object> cm_proto = new as_object(player);
		cm_proto->builtin_member("hideBuiltInItems", as_value(contextmenu_hideBuiltInItems));

		as_object* cm_class = new as_object(player);
		cm_class->set_member("prototype", as_value(cm_proto.get_ptr()));
		global->set_member("ContextMenu", as_value(cm_class));
		if (ui_pkg) ui_pkg->set_member("ContextMenu", as_value(cm_class));
	}

	// ===== ContextMenuItem =====
	{
		as_object* cmi_class = new as_object(player);
		global->set_member("ContextMenuItem", as_value(contextmenuitem_constructor));
		if (ui_pkg) ui_pkg->set_member("ContextMenuItem", as_value(contextmenuitem_constructor));
	}
}

} // end namespace gameswf
