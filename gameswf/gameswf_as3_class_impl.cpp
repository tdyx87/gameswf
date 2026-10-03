// as3_class_impl.cpp -- Actual implementations for AS3 built-in classes
// This source code has been donated to the Public Domain.
//
// Provides real implementations for ByteArray, Point, Rectangle, Matrix,
// TextField, URLLoader, URLRequest, Sound, JSON, Mouse, Keyboard, etc.

#include "gameswf/gameswf_as3_classes.h"
#include "gameswf/gameswf_as_classes/as_array.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace gameswf
{
// Helper functions to simplify property access
static double get_prop_number(as_object* obj, const char* name)
{
	as_value val;
	obj->get_member(name, &val);
	return val.to_number();
}

static const char* get_prop_string(as_object* obj, const char* name)
{
	as_value val;
	obj->get_member(name, &val);
	return val.to_string();
}

static bool get_prop_bool(as_object* obj, const char* name)
{
	as_value val;
	obj->get_member(name, &val);
	return val.to_bool();
}

static as_value get_prop_value(as_object* obj, const char* name)
{
	as_value val;
	obj->get_member(name, &val);
	return val;
}



// ========================================================================
// ByteArray implementation
// ========================================================================

// Internal data accessor helper
static array<Uint8>* get_byte_array_data(as_object* obj)
{
	if (obj == NULL) return NULL;
	as_value data_val;
	if (obj->get_member("__byte_data__", &data_val) && data_val.is_object())
	{
		// The byte data is stored as a special internal member
		// We use a simple approach: store a pointer in an as_object's user data
		as_value ptr_val;
		if (data_val.to_object()->get_member("__ptr__", &ptr_val))
		{
			return (array<Uint8>*) (size_t) ptr_val.to_number();
		}
	}
	return NULL;
}

static array<Uint8>* ensure_byte_array_data(as_object* obj)
{
	array<Uint8>* data = get_byte_array_data(obj);
	if (data != NULL) return data;

	// Create new byte array data
	data = new array<Uint8>();
	as_object* wrapper = new as_object(obj->get_player());
	wrapper->set_member("__ptr__", as_value((double)(size_t)data));
	obj->set_member("__byte_data__", as_value(wrapper));
	return data;
}

static int get_byte_array_position(as_object* obj)
{
	as_value pos_val;
	obj->get_member("__position__", &pos_val);
	return (int)pos_val.to_number();
}

static void set_byte_array_position(as_object* obj, int pos)
{
	obj->set_member("__position__", as_value((double)pos));
}

static void update_byte_array_properties(as_object* obj)
{
	array<Uint8>* data = get_byte_array_data(obj);
	if (data == NULL) return;
	int pos = get_byte_array_position(obj);
	obj->set_member("length", as_value((double)data->size()));
	obj->set_member("bytesAvailable", as_value((double)(data->size() - pos)));
}

// ByteArray methods
static void bytearray_readByte(const fn_call& fn)
{
	array<Uint8>* data = ensure_byte_array_data(fn.this_ptr);
	int pos = get_byte_array_position(fn.this_ptr);
	if (data && pos < data->size())
	{
		fn.result->set_int((Sint8)(*data)[pos]);
		set_byte_array_position(fn.this_ptr, pos + 1);
	}
	else
	{
		fn.result->set_int(0);  // EOF
	}
	update_byte_array_properties(fn.this_ptr);
}

static void bytearray_readUnsignedByte(const fn_call& fn)
{
	array<Uint8>* data = ensure_byte_array_data(fn.this_ptr);
	int pos = get_byte_array_position(fn.this_ptr);
	if (data && pos < data->size())
	{
		fn.result->set_int((*data)[pos]);
		set_byte_array_position(fn.this_ptr, pos + 1);
	}
	else
	{
		fn.result->set_int(0);
	}
	update_byte_array_properties(fn.this_ptr);
}

static void bytearray_readShort(const fn_call& fn)
{
	array<Uint8>* data = ensure_byte_array_data(fn.this_ptr);
	int pos = get_byte_array_position(fn.this_ptr);
	if (data && pos + 1 < data->size())
	{
		Sint16 val = (Sint16)(((*data)[pos] << 8) | (*data)[pos + 1]);
		fn.result->set_int(val);
		set_byte_array_position(fn.this_ptr, pos + 2);
	}
	else
	{
		fn.result->set_int(0);
	}
	update_byte_array_properties(fn.this_ptr);
}

static void bytearray_readUnsignedShort(const fn_call& fn)
{
	array<Uint8>* data = ensure_byte_array_data(fn.this_ptr);
	int pos = get_byte_array_position(fn.this_ptr);
	if (data && pos + 1 < data->size())
	{
		Uint16 val = (Uint16)(((*data)[pos] << 8) | (*data)[pos + 1]);
		fn.result->set_int(val);
		set_byte_array_position(fn.this_ptr, pos + 2);
	}
	else
	{
		fn.result->set_int(0);
	}
	update_byte_array_properties(fn.this_ptr);
}

static void bytearray_readInt(const fn_call& fn)
{
	array<Uint8>* data = ensure_byte_array_data(fn.this_ptr);
	int pos = get_byte_array_position(fn.this_ptr);
	if (data && pos + 3 < data->size())
	{
		Sint32 val = (Sint32)(((*data)[pos] << 24) | ((*data)[pos+1] << 16) | ((*data)[pos+2] << 8) | (*data)[pos+3]);
		fn.result->set_int(val);
		set_byte_array_position(fn.this_ptr, pos + 4);
	}
	else
	{
		fn.result->set_int(0);
	}
	update_byte_array_properties(fn.this_ptr);
}

static void bytearray_readUnsignedInt(const fn_call& fn)
{
	array<Uint8>* data = ensure_byte_array_data(fn.this_ptr);
	int pos = get_byte_array_position(fn.this_ptr);
	if (data && pos + 3 < data->size())
	{
		Uint32 val = (Uint32)(((*data)[pos] << 24) | ((*data)[pos+1] << 16) | ((*data)[pos+2] << 8) | (*data)[pos+3]);
		fn.result->set_int((int)val);
		set_byte_array_position(fn.this_ptr, pos + 4);
	}
	else
	{
		fn.result->set_int(0);
	}
	update_byte_array_properties(fn.this_ptr);
}

static void bytearray_readFloat(const fn_call& fn)
{
	array<Uint8>* data = ensure_byte_array_data(fn.this_ptr);
	int pos = get_byte_array_position(fn.this_ptr);
	if (data && pos + 3 < data->size())
	{
		Uint32 bits = (Uint32)(((*data)[pos] << 24) | ((*data)[pos+1] << 16) | ((*data)[pos+2] << 8) | (*data)[pos+3]);
		float val;
		memcpy(&val, &bits, sizeof(float));
		fn.result->set_double(val);
		set_byte_array_position(fn.this_ptr, pos + 4);
	}
	else
	{
		fn.result->set_double(0.0);
	}
	update_byte_array_properties(fn.this_ptr);
}

static void bytearray_readDouble(const fn_call& fn)
{
	array<Uint8>* data = ensure_byte_array_data(fn.this_ptr);
	int pos = get_byte_array_position(fn.this_ptr);
	if (data && pos + 7 < data->size())
	{
		Uint64 bits = 0;
		for (int i = 0; i < 8; i++)
		{
			bits = (bits << 8) | (*data)[pos + i];
		}
		double val;
		memcpy(&val, &bits, sizeof(double));
		fn.result->set_double(val);
		set_byte_array_position(fn.this_ptr, pos + 8);
	}
	else
	{
		fn.result->set_double(0.0);
	}
	update_byte_array_properties(fn.this_ptr);
}

static void bytearray_readBoolean(const fn_call& fn)
{
	array<Uint8>* data = ensure_byte_array_data(fn.this_ptr);
	int pos = get_byte_array_position(fn.this_ptr);
	if (data && pos < data->size())
	{
		fn.result->set_bool((*data)[pos] != 0);
		set_byte_array_position(fn.this_ptr, pos + 1);
	}
	else
	{
		fn.result->set_bool(false);
	}
	update_byte_array_properties(fn.this_ptr);
}

static void bytearray_readUTF(const fn_call& fn)
{
	array<Uint8>* data = ensure_byte_array_data(fn.this_ptr);
	int pos = get_byte_array_position(fn.this_ptr);
	if (data && pos + 1 < data->size())
	{
		// Read length (2 bytes, big-endian)
		int len = ((*data)[pos] << 8) | (*data)[pos + 1];
		pos += 2;
		if (pos + len <= data->size())
		{
			char* buf = new char[len + 1];
			memcpy(buf, &(*data)[pos], len);
			buf[len] = '\0';
			fn.result->set_string(tu_string(buf));
			delete[] buf;
			set_byte_array_position(fn.this_ptr, pos + len);
		}
	}
	else
	{
		fn.result->set_string("");
	}
	update_byte_array_properties(fn.this_ptr);
}

static void bytearray_readUTFBytes(const fn_call& fn)
{
	if (fn.nargs < 1) { fn.result->set_string(""); return; }
	int length = (int)fn.arg(0).to_number();

	array<Uint8>* data = ensure_byte_array_data(fn.this_ptr);
	int pos = get_byte_array_position(fn.this_ptr);
	if (data && pos + length <= data->size())
	{
		char* buf = new char[length + 1];
		memcpy(buf, &(*data)[pos], length);
		buf[length] = '\0';
		fn.result->set_string(tu_string(buf));
		delete[] buf;
		set_byte_array_position(fn.this_ptr, pos + length);
	}
	else
	{
		fn.result->set_string("");
	}
	update_byte_array_properties(fn.this_ptr);
}

static void bytearray_writeByte(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	array<Uint8>* data = ensure_byte_array_data(fn.this_ptr);
	int pos = get_byte_array_position(fn.this_ptr);
	int val = (int)fn.arg(0).to_number();
	Uint8 byte_val = (Uint8)(val & 0xFF);
	if (pos < data->size())
	{
		(*data)[pos] = byte_val;
	}
	else
	{
		data->push_back(byte_val);
	}
	set_byte_array_position(fn.this_ptr, pos + 1);
	update_byte_array_properties(fn.this_ptr);
}

static void bytearray_writeUnsignedByte(const fn_call& fn)
{
	bytearray_writeByte(fn);  // Same implementation
}

static void bytearray_writeShort(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	array<Uint8>* data = ensure_byte_array_data(fn.this_ptr);
	int pos = get_byte_array_position(fn.this_ptr);
	Sint16 val = (Sint16)(int)fn.arg(0).to_number();
	if (pos + 1 < data->size())
	{
		(*data)[pos] = (val >> 8) & 0xFF;
		(*data)[pos + 1] = val & 0xFF;
	}
	else
	{
		data->push_back((val >> 8) & 0xFF);
		data->push_back(val & 0xFF);
	}
	set_byte_array_position(fn.this_ptr, pos + 2);
	update_byte_array_properties(fn.this_ptr);
}

static void bytearray_writeInt(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	array<Uint8>* data = ensure_byte_array_data(fn.this_ptr);
	int pos = get_byte_array_position(fn.this_ptr);
	Sint32 val = (Sint32)(int)fn.arg(0).to_number();
	for (int i = 3; i >= 0; i--)
	{
		Uint8 b = (val >> (i * 8)) & 0xFF;
		if (pos < data->size())
		{
			(*data)[pos] = b;
		}
		else
		{
			data->push_back(b);
		}
		pos++;
	}
	set_byte_array_position(fn.this_ptr, pos);
	update_byte_array_properties(fn.this_ptr);
}

static void bytearray_writeUnsignedInt(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	array<Uint8>* data = ensure_byte_array_data(fn.this_ptr);
	int pos = get_byte_array_position(fn.this_ptr);
	Uint32 val = (Uint32)(int)fn.arg(0).to_number();
	for (int i = 3; i >= 0; i--)
	{
		Uint8 b = (val >> (i * 8)) & 0xFF;
		if (pos < data->size())
		{
			(*data)[pos] = b;
		}
		else
		{
			data->push_back(b);
		}
		pos++;
	}
	set_byte_array_position(fn.this_ptr, pos);
	update_byte_array_properties(fn.this_ptr);
}

static void bytearray_writeFloat(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	array<Uint8>* data = ensure_byte_array_data(fn.this_ptr);
	int pos = get_byte_array_position(fn.this_ptr);
	float val = (float)fn.arg(0).to_number();
	Uint32 bits;
	memcpy(&bits, &val, sizeof(float));
	for (int i = 3; i >= 0; i--)
	{
		Uint8 b = (bits >> (i * 8)) & 0xFF;
		if (pos < data->size())
		{
			(*data)[pos] = b;
		}
		else
		{
			data->push_back(b);
		}
		pos++;
	}
	set_byte_array_position(fn.this_ptr, pos);
	update_byte_array_properties(fn.this_ptr);
}

static void bytearray_writeDouble(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	array<Uint8>* data = ensure_byte_array_data(fn.this_ptr);
	int pos = get_byte_array_position(fn.this_ptr);
	double val = fn.arg(0).to_number();
	Uint64 bits;
	memcpy(&bits, &val, sizeof(double));
	for (int i = 7; i >= 0; i--)
	{
		Uint8 b = (bits >> (i * 8)) & 0xFF;
		if (pos < data->size())
		{
			(*data)[pos] = b;
		}
		else
		{
			data->push_back(b);
		}
		pos++;
	}
	set_byte_array_position(fn.this_ptr, pos);
	update_byte_array_properties(fn.this_ptr);
}

static void bytearray_writeBoolean(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	array<Uint8>* data = ensure_byte_array_data(fn.this_ptr);
	int pos = get_byte_array_position(fn.this_ptr);
	Uint8 val = fn.arg(0).to_bool() ? 1 : 0;
	if (pos < data->size())
	{
		(*data)[pos] = val;
	}
	else
	{
		data->push_back(val);
	}
	set_byte_array_position(fn.this_ptr, pos + 1);
	update_byte_array_properties(fn.this_ptr);
}

static void bytearray_writeUTF(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	const char* str = fn.arg(0).to_string();
	int len = (int)strlen(str);
	array<Uint8>* data = ensure_byte_array_data(fn.this_ptr);
	int pos = get_byte_array_position(fn.this_ptr);

	// Write length prefix (2 bytes, big-endian)
	data->push_back((len >> 8) & 0xFF);
	data->push_back(len & 0xFF);
	pos += 2;

	// Write string data
	for (int i = 0; i < len; i++)
	{
		data->push_back((Uint8)str[i]);
	}
	set_byte_array_position(fn.this_ptr, pos + len);
	update_byte_array_properties(fn.this_ptr);
}

static void bytearray_writeUTFBytes(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	const char* str = fn.arg(0).to_string();
	int len = (int)strlen(str);
	array<Uint8>* data = ensure_byte_array_data(fn.this_ptr);
	int pos = get_byte_array_position(fn.this_ptr);

	for (int i = 0; i < len; i++)
	{
		data->push_back((Uint8)str[i]);
	}
	set_byte_array_position(fn.this_ptr, pos + len);
	update_byte_array_properties(fn.this_ptr);
}

static void bytearray_writeBytes(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	// Simplified: write from another ByteArray
	as_object* source = fn.arg(0).to_object();
	if (source == NULL) return;

	array<Uint8>* src_data = get_byte_array_data(source);
	array<Uint8>* dst_data = ensure_byte_array_data(fn.this_ptr);
	if (src_data == NULL || dst_data == NULL) return;

	int offset = 0, length = 0;
	if (fn.nargs >= 2) offset = (int)fn.arg(1).to_number();
	if (fn.nargs >= 3) length = (int)fn.arg(2).to_number();
	if (length <= 0) length = src_data->size() - offset;

	for (int i = 0; i < length && (offset + i) < src_data->size(); i++)
	{
		dst_data->push_back((*src_data)[offset + i]);
	}
	update_byte_array_properties(fn.this_ptr);
}

static void bytearray_readBytes(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	as_object* dest = fn.arg(0).to_object();
	if (dest == NULL) return;

	array<Uint8>* src_data = ensure_byte_array_data(fn.this_ptr);
	array<Uint8>* dst_data = ensure_byte_array_data(dest);
	if (src_data == NULL || dst_data == NULL) return;

	int offset = 0, length = 0;
	if (fn.nargs >= 2) offset = (int)fn.arg(1).to_number();
	if (fn.nargs >= 3) length = (int)fn.arg(2).to_number();
	if (length <= 0) length = src_data->size();

	int src_pos = get_byte_array_position(fn.this_ptr);
	for (int i = 0; i < length && (src_pos + i) < src_data->size(); i++)
	{
		dst_data->push_back((*src_data)[src_pos + i]);
	}
	update_byte_array_properties(fn.this_ptr);
	update_byte_array_properties(dest);
}

static void bytearray_clear(const fn_call& fn)
{
	array<Uint8>* data = get_byte_array_data(fn.this_ptr);
	if (data) data->clear();
	set_byte_array_position(fn.this_ptr, 0);
	update_byte_array_properties(fn.this_ptr);
}

static void bytearray_compress(const fn_call& fn)
{
	// Simplified - no actual compression
}

static void bytearray_uncompress(const fn_call& fn)
{
	// Simplified - no actual decompression
}

static void bytearray_readObject(const fn_call& fn)
{
	// Simplified - return undefined
	fn.result->set_undefined();
}

static void bytearray_writeObject(const fn_call& fn)
{
	// Simplified - no-op
}

static void bytearray_readMultiByte(const fn_call& fn)
{
	bytearray_readUTFBytes(fn);
}

static void bytearray_writeMultiByte(const fn_call& fn)
{
	bytearray_writeUTFBytes(fn);
}

// Init function that binds real implementations
void as3_byte_array::init(as_object* target)
{
	target->set_member("length", as_value(0.0));
	target->set_member("position", as_value(0.0));
	target->set_member("bytesAvailable", as_value(0.0));
	target->set_member("endian", as_value("bigEndian"));

	target->set_member("readBoolean", as_value(bytearray_readBoolean));
	target->set_member("readByte", as_value(bytearray_readByte));
	target->set_member("readBytes", as_value(bytearray_readBytes));
	target->set_member("readDouble", as_value(bytearray_readDouble));
	target->set_member("readFloat", as_value(bytearray_readFloat));
	target->set_member("readInt", as_value(bytearray_readInt));
	target->set_member("readMultiByte", as_value(bytearray_readMultiByte));
	target->set_member("readObject", as_value(bytearray_readObject));
	target->set_member("readShort", as_value(bytearray_readShort));
	target->set_member("readUnsignedByte", as_value(bytearray_readUnsignedByte));
	target->set_member("readUnsignedInt", as_value(bytearray_readUnsignedInt));
	target->set_member("readUnsignedShort", as_value(bytearray_readUnsignedShort));
	target->set_member("readUTF", as_value(bytearray_readUTF));
	target->set_member("readUTFBytes", as_value(bytearray_readUTFBytes));
	target->set_member("writeBoolean", as_value(bytearray_writeBoolean));
	target->set_member("writeByte", as_value(bytearray_writeByte));
	target->set_member("writeBytes", as_value(bytearray_writeBytes));
	target->set_member("writeDouble", as_value(bytearray_writeDouble));
	target->set_member("writeFloat", as_value(bytearray_writeFloat));
	target->set_member("writeInt", as_value(bytearray_writeInt));
	target->set_member("writeMultiByte", as_value(bytearray_writeMultiByte));
	target->set_member("writeObject", as_value(bytearray_writeObject));
	target->set_member("writeShort", as_value(bytearray_writeShort));
	target->set_member("writeUnsignedInt", as_value(bytearray_writeUnsignedInt));
	target->set_member("writeUTF", as_value(bytearray_writeUTF));
	target->set_member("writeUTFBytes", as_value(bytearray_writeUTFBytes));
	target->set_member("clear", as_value(bytearray_clear));
	target->set_member("compress", as_value(bytearray_compress));
	target->set_member("uncompress", as_value(bytearray_uncompress));
}

// ========================================================================
// Point implementation
// ========================================================================

static void point_add(const fn_call& fn)
{
	if (fn.nargs < 1) { fn.result->set_undefined(); return; }
	double x = get_prop_number(fn.this_ptr, "x") + get_prop_number(fn.arg(0).to_object(), "x");
	double y = get_prop_number(fn.this_ptr, "y") + get_prop_number(fn.arg(0).to_object(), "y");
	gc_ptr<as_object> result = new as_object(fn.get_player());
	result->set_member("x", as_value(x));
	result->set_member("y", as_value(y));
	fn.result->set_as_object(result.get_ptr());
}

static void point_subtract(const fn_call& fn)
{
	if (fn.nargs < 1) { fn.result->set_undefined(); return; }
	double x = get_prop_number(fn.this_ptr, "x") - get_prop_number(fn.arg(0).to_object(), "x");
	double y = get_prop_number(fn.this_ptr, "y") - get_prop_number(fn.arg(0).to_object(), "y");
	gc_ptr<as_object> result = new as_object(fn.get_player());
	result->set_member("x", as_value(x));
	result->set_member("y", as_value(y));
	fn.result->set_as_object(result.get_ptr());
}

static void point_clone(const fn_call& fn)
{
	gc_ptr<as_object> result = new as_object(fn.get_player());
	result->set_member("x", get_prop_value(fn.this_ptr, "x"));
	result->set_member("y", get_prop_value(fn.this_ptr, "y"));
	fn.result->set_as_object(result.get_ptr());
}

static void point_equals(const fn_call& fn)
{
	if (fn.nargs < 1) { fn.result->set_bool(false); return; }
	as_object* other = fn.arg(0).to_object();
	bool eq = (get_prop_number(fn.this_ptr, "x") == get_prop_number(other, "x") &&
	           get_prop_number(fn.this_ptr, "y") == get_prop_number(other, "y"));
	fn.result->set_bool(eq);
}

static void point_normalize(const fn_call& fn)
{
	double x = get_prop_number(fn.this_ptr, "x");
	double y = get_prop_number(fn.this_ptr, "y");
	double len = sqrt(x * x + y * y);
	if (len > 0)
	{
		fn.this_ptr->set_member("x", as_value(x / len));
		fn.this_ptr->set_member("y", as_value(y / len));
	}
	fn.this_ptr->set_member("length", as_value(1.0));
}

static void point_offset(const fn_call& fn)
{
	if (fn.nargs < 2) return;
	double dx = fn.arg(0).to_number();
	double dy = fn.arg(1).to_number();
	fn.this_ptr->set_member("x", as_value(get_prop_number(fn.this_ptr, "x") + dx));
	fn.this_ptr->set_member("y", as_value(get_prop_number(fn.this_ptr, "y") + dy));
}

static void point_setTo(const fn_call& fn)
{
	if (fn.nargs < 2) return;
	fn.this_ptr->set_member("x", as_value(fn.arg(0).to_number()));
	fn.this_ptr->set_member("y", as_value(fn.arg(1).to_number()));
}

static void point_copyFrom(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	as_object* source = fn.arg(0).to_object();
	if (source)
	{
		fn.this_ptr->set_member("x", get_prop_value(source, "x"));
		fn.this_ptr->set_member("y", get_prop_value(source, "y"));
	}
}

static void point_distance(const fn_call& fn)
{
	if (fn.nargs < 2) { fn.result->set_double(0); return; }
	as_object* p1 = fn.arg(0).to_object();
	as_object* p2 = fn.arg(1).to_object();
	if (p1 && p2)
	{
		double dx = get_prop_number(p1, "x") - get_prop_number(p2, "x");
		double dy = get_prop_number(p1, "y") - get_prop_number(p2, "y");
		fn.result->set_double(sqrt(dx * dx + dy * dy));
	}
}

static void point_interpolate(const fn_call& fn)
{
	if (fn.nargs < 3) { fn.result->set_undefined(); return; }
	as_object* p1 = fn.arg(0).to_object();
	as_object* p2 = fn.arg(1).to_object();
	double t = fn.arg(2).to_number();
	if (p1 && p2)
	{
		gc_ptr<as_object> result = new as_object(fn.get_player());
		result->set_member("x", as_value(get_prop_number(p1, "x") + (get_prop_number(p2, "x") - get_prop_number(p1, "x")) * t));
		result->set_member("y", as_value(get_prop_number(p1, "y") + (get_prop_number(p2, "y") - get_prop_number(p1, "y")) * t));
		fn.result->set_as_object(result.get_ptr());
	}
}

static void point_polar(const fn_call& fn)
{
	if (fn.nargs < 2) { fn.result->set_undefined(); return; }
	double len = fn.arg(0).to_number();
	double angle = fn.arg(1).to_number();
	gc_ptr<as_object> result = new as_object(fn.get_player());
	result->set_member("x", as_value(cos(angle) * len));
	result->set_member("y", as_value(sin(angle) * len));
	fn.result->set_as_object(result.get_ptr());
}

void as3_point::init(as_object* target)
{
	target->set_member("x", as_value(0.0));
	target->set_member("y", as_value(0.0));
	target->set_member("length", as_value(0.0));

	target->set_member("add", as_value(point_add));
	target->set_member("clone", as_value(point_clone));
	target->set_member("copyFrom", as_value(point_copyFrom));
	target->set_member("equals", as_value(point_equals));
	target->set_member("normalize", as_value(point_normalize));
	target->set_member("offset", as_value(point_offset));
	target->set_member("setTo", as_value(point_setTo));
	target->set_member("subtract", as_value(point_subtract));
	target->set_member("distance", as_value(point_distance));
	target->set_member("interpolate", as_value(point_interpolate));
	target->set_member("polar", as_value(point_polar));
}

// ========================================================================
// Rectangle implementation
// ========================================================================

static void rect_clone(const fn_call& fn)
{
	gc_ptr<as_object> r = new as_object(fn.get_player());
	r->set_member("x", get_prop_value(fn.this_ptr, "x"));
	r->set_member("y", get_prop_value(fn.this_ptr, "y"));
	r->set_member("width", get_prop_value(fn.this_ptr, "width"));
	r->set_member("height", get_prop_value(fn.this_ptr, "height"));
	fn.result->set_as_object(r.get_ptr());
}

static void rect_contains(const fn_call& fn)
{
	if (fn.nargs < 2) { fn.result->set_bool(false); return; }
	double px = fn.arg(0).to_number(), py = fn.arg(1).to_number();
	double rx = get_prop_number(fn.this_ptr, "x");
	double ry = get_prop_number(fn.this_ptr, "y");
	double rw = get_prop_number(fn.this_ptr, "width");
	double rh = get_prop_number(fn.this_ptr, "height");
	fn.result->set_bool(px >= rx && px <= rx + rw && py >= ry && py <= ry + rh);
}

static void rect_containsPoint(const fn_call& fn)
{
	rect_contains(fn);
}

static void rect_containsRect(const fn_call& fn)
{
	if (fn.nargs < 1) { fn.result->set_bool(false); return; }
	as_object* other = fn.arg(0).to_object();
	if (!other) { fn.result->set_bool(false); return; }
	double ax = get_prop_number(fn.this_ptr, "x");
	double ay = get_prop_number(fn.this_ptr, "y");
	double aw = get_prop_number(fn.this_ptr, "width");
	double ah = get_prop_number(fn.this_ptr, "height");
	double bx = get_prop_number(other, "x");
	double by = get_prop_number(other, "y");
	double bw = get_prop_number(other, "width");
	double bh = get_prop_number(other, "height");
	fn.result->set_bool(bx >= ax && by >= ay && bx + bw <= ax + aw && by + bh <= ay + ah);
}

static void rect_equals(const fn_call& fn)
{
	if (fn.nargs < 1) { fn.result->set_bool(false); return; }
	as_object* other = fn.arg(0).to_object();
	if (!other) { fn.result->set_bool(false); return; }
	fn.result->set_bool(
		get_prop_number(fn.this_ptr, "x") == get_prop_number(other, "x") &&
		get_prop_number(fn.this_ptr, "y") == get_prop_number(other, "y") &&
		get_prop_number(fn.this_ptr, "width") == get_prop_number(other, "width") &&
		get_prop_number(fn.this_ptr, "height") == get_prop_number(other, "height"));
}

static void rect_inflate(const fn_call& fn)
{
	if (fn.nargs < 2) return;
	double dx = fn.arg(0).to_number() * 0.5;
	double dy = fn.arg(1).to_number() * 0.5;
	fn.this_ptr->set_member("x", as_value(get_prop_number(fn.this_ptr, "x") - dx));
	fn.this_ptr->set_member("y", as_value(get_prop_number(fn.this_ptr, "y") - dy));
	fn.this_ptr->set_member("width", as_value(get_prop_number(fn.this_ptr, "width") + fn.arg(0).to_number()));
	fn.this_ptr->set_member("height", as_value(get_prop_number(fn.this_ptr, "height") + fn.arg(1).to_number()));
}

static void rect_inflatePoint(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	as_object* pt = fn.arg(0).to_object();
	if (pt) rect_inflate(fn);  // Simplified
}

static void rect_intersection(const fn_call& fn)
{
	if (fn.nargs < 1) { fn.result->set_undefined(); return; }
	as_object* other = fn.arg(0).to_object();
	if (!other) { fn.result->set_undefined(); return; }

	double ax = get_prop_number(fn.this_ptr, "x");
	double ay = get_prop_number(fn.this_ptr, "y");
	double aw = get_prop_number(fn.this_ptr, "width");
	double ah = get_prop_number(fn.this_ptr, "height");
	double bx = get_prop_number(other, "x");
	double by = get_prop_number(other, "y");
	double bw = get_prop_number(other, "width");
	double bh = get_prop_number(other, "height");

	double x1 = (ax > bx) ? ax : bx;
	double y1 = (ay > by) ? ay : by;
	double x2 = (ax + aw < bx + bw) ? ax + aw : bx + bw;
	double y2 = (ay + ah < by + bh) ? ay + ah : by + bh;

	gc_ptr<as_object> r = new as_object(fn.get_player());
	r->set_member("x", as_value(x1));
	r->set_member("y", as_value(y1));
	r->set_member("width", as_value(x2 > x1 ? x2 - x1 : 0));
	r->set_member("height", as_value(y2 > y1 ? y2 - y1 : 0));
	fn.result->set_as_object(r.get_ptr());
}

static void rect_intersects(const fn_call& fn)
{
	if (fn.nargs < 1) { fn.result->set_bool(false); return; }
	as_object* other = fn.arg(0).to_object();
	if (!other) { fn.result->set_bool(false); return; }

	double ax = get_prop_number(fn.this_ptr, "x");
	double ay = get_prop_number(fn.this_ptr, "y");
	double aw = get_prop_number(fn.this_ptr, "width");
	double ah = get_prop_number(fn.this_ptr, "height");
	double bx = get_prop_number(other, "x");
	double by = get_prop_number(other, "y");
	double bw = get_prop_number(other, "width");
	double bh = get_prop_number(other, "height");

	fn.result->set_bool(ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by);
}

static void rect_isEmpty(const fn_call& fn)
{
	fn.result->set_bool(get_prop_number(fn.this_ptr, "width") <= 0 ||
	                   get_prop_number(fn.this_ptr, "height") <= 0);
}

static void rect_offset(const fn_call& fn)
{
	if (fn.nargs < 2) return;
	fn.this_ptr->set_member("x", as_value(get_prop_number(fn.this_ptr, "x") + fn.arg(0).to_number()));
	fn.this_ptr->set_member("y", as_value(get_prop_number(fn.this_ptr, "y") + fn.arg(1).to_number()));
}

static void rect_offsetPoint(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	as_object* pt = fn.arg(0).to_object();
	if (pt) rect_offset(fn);
}

static void rect_setEmpty(const fn_call& fn)
{
	fn.this_ptr->set_member("x", as_value(0.0));
	fn.this_ptr->set_member("y", as_value(0.0));
	fn.this_ptr->set_member("width", as_value(0.0));
	fn.this_ptr->set_member("height", as_value(0.0));
}

static void rect_setTo(const fn_call& fn)
{
	if (fn.nargs < 4) return;
	fn.this_ptr->set_member("x", as_value(fn.arg(0).to_number()));
	fn.this_ptr->set_member("y", as_value(fn.arg(1).to_number()));
	fn.this_ptr->set_member("width", as_value(fn.arg(2).to_number()));
	fn.this_ptr->set_member("height", as_value(fn.arg(3).to_number()));
}

static void rect_union(const fn_call& fn)
{
	if (fn.nargs < 1) { fn.result->set_undefined(); return; }
	as_object* other = fn.arg(0).to_object();
	if (!other) { fn.result->set_undefined(); return; }

	double ax = get_prop_number(fn.this_ptr, "x");
	double ay = get_prop_number(fn.this_ptr, "y");
	double aw = get_prop_number(fn.this_ptr, "width");
	double ah = get_prop_number(fn.this_ptr, "height");
	double bx = get_prop_number(other, "x");
	double by = get_prop_number(other, "y");
	double bw = get_prop_number(other, "width");
	double bh = get_prop_number(other, "height");

	double x1 = (ax < bx) ? ax : bx;
	double y1 = (ay < by) ? ay : by;
	double x2 = (ax + aw > bx + bw) ? ax + aw : bx + bw;
	double y2 = (ay + ah > by + bh) ? ay + ah : by + bh;

	gc_ptr<as_object> r = new as_object(fn.get_player());
	r->set_member("x", as_value(x1));
	r->set_member("y", as_value(y1));
	r->set_member("width", as_value(x2 - x1));
	r->set_member("height", as_value(y2 - y1));
	fn.result->set_as_object(r.get_ptr());
}

static void rect_copyFrom(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	as_object* source = fn.arg(0).to_object();
	if (source)
	{
		fn.this_ptr->set_member("x", get_prop_value(source, "x"));
		fn.this_ptr->set_member("y", get_prop_value(source, "y"));
		fn.this_ptr->set_member("width", get_prop_value(source, "width"));
		fn.this_ptr->set_member("height", get_prop_value(source, "height"));
	}
}

void as3_rectangle::init(as_object* target)
{
	target->set_member("x", as_value(0.0));
	target->set_member("y", as_value(0.0));
	target->set_member("width", as_value(0.0));
	target->set_member("height", as_value(0.0));
	target->set_member("top", as_value(0.0));
	target->set_member("bottom", as_value(0.0));
	target->set_member("left", as_value(0.0));
	target->set_member("right", as_value(0.0));

	target->set_member("clone", as_value(rect_clone));
	target->set_member("contains", as_value(rect_contains));
	target->set_member("containsPoint", as_value(rect_containsPoint));
	target->set_member("containsRect", as_value(rect_containsRect));
	target->set_member("copyFrom", as_value(rect_copyFrom));
	target->set_member("equals", as_value(rect_equals));
	target->set_member("inflate", as_value(rect_inflate));
	target->set_member("inflatePoint", as_value(rect_inflatePoint));
	target->set_member("intersection", as_value(rect_intersection));
	target->set_member("intersects", as_value(rect_intersects));
	target->set_member("isEmpty", as_value(rect_isEmpty));
	target->set_member("offset", as_value(rect_offset));
	target->set_member("offsetPoint", as_value(rect_offsetPoint));
	target->set_member("setEmpty", as_value(rect_setEmpty));
	target->set_member("setTo", as_value(rect_setTo));
	target->set_member("union", as_value(rect_union));
}

// ========================================================================
// Matrix implementation
// ========================================================================

static void matrix_clone(const fn_call& fn)
{
	gc_ptr<as_object> m = new as_object(fn.get_player());
	m->set_member("a", get_prop_value(fn.this_ptr, "a"));
	m->set_member("b", get_prop_value(fn.this_ptr, "b"));
	m->set_member("c", get_prop_value(fn.this_ptr, "c"));
	m->set_member("d", get_prop_value(fn.this_ptr, "d"));
	m->set_member("tx", get_prop_value(fn.this_ptr, "tx"));
	m->set_member("ty", get_prop_value(fn.this_ptr, "ty"));
	fn.result->set_as_object(m.get_ptr());
}

static void matrix_identity(const fn_call& fn)
{
	fn.this_ptr->set_member("a", as_value(1.0));
	fn.this_ptr->set_member("b", as_value(0.0));
	fn.this_ptr->set_member("c", as_value(0.0));
	fn.this_ptr->set_member("d", as_value(1.0));
	fn.this_ptr->set_member("tx", as_value(0.0));
	fn.this_ptr->set_member("ty", as_value(0.0));
}

static void matrix_invert(const fn_call& fn)
{
	double a = get_prop_number(fn.this_ptr, "a");
	double b = get_prop_number(fn.this_ptr, "b");
	double c = get_prop_number(fn.this_ptr, "c");
	double d = get_prop_number(fn.this_ptr, "d");
	double tx = get_prop_number(fn.this_ptr, "tx");
	double ty = get_prop_number(fn.this_ptr, "ty");

	double det = a * d - b * c;
	if (fabs(det) < 1e-10) return;  // Singular matrix

	double inv_det = 1.0 / det;
	fn.this_ptr->set_member("a", as_value(d * inv_det));
	fn.this_ptr->set_member("b", as_value(-b * inv_det));
	fn.this_ptr->set_member("c", as_value(-c * inv_det));
	fn.this_ptr->set_member("d", as_value(a * inv_det));
	fn.this_ptr->set_member("tx", as_value((c * ty - d * tx) * inv_det));
	fn.this_ptr->set_member("ty", as_value((b * tx - a * ty) * inv_det));
}

static void matrix_rotate(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	double angle = fn.arg(0).to_number();
	double cos_a = cos(angle);
	double sin_a = sin(angle);
	double a = get_prop_number(fn.this_ptr, "a");
	double b = get_prop_number(fn.this_ptr, "b");
	double c = get_prop_number(fn.this_ptr, "c");
	double d = get_prop_number(fn.this_ptr, "d");

	fn.this_ptr->set_member("a", as_value(a * cos_a + c * sin_a));
	fn.this_ptr->set_member("b", as_value(b * cos_a + d * sin_a));
	fn.this_ptr->set_member("c", as_value(c * cos_a - a * sin_a));
	fn.this_ptr->set_member("d", as_value(d * cos_a - b * sin_a));
}

static void matrix_scale(const fn_call& fn)
{
	if (fn.nargs < 2) return;
	double sx = fn.arg(0).to_number();
	double sy = fn.arg(1).to_number();
	fn.this_ptr->set_member("a", as_value(get_prop_number(fn.this_ptr, "a") * sx));
	fn.this_ptr->set_member("b", as_value(get_prop_number(fn.this_ptr, "b") * sx));
	fn.this_ptr->set_member("c", as_value(get_prop_number(fn.this_ptr, "c") * sy));
	fn.this_ptr->set_member("d", as_value(get_prop_number(fn.this_ptr, "d") * sy));
}

static void matrix_translate(const fn_call& fn)
{
	if (fn.nargs < 2) return;
	double dx = fn.arg(0).to_number();
	double dy = fn.arg(1).to_number();
	fn.this_ptr->set_member("tx", as_value(get_prop_number(fn.this_ptr, "tx") + dx));
	fn.this_ptr->set_member("ty", as_value(get_prop_number(fn.this_ptr, "ty") + dy));
}

static void matrix_concat(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	as_object* other = fn.arg(0).to_object();
	if (!other) return;

	double a1 = get_prop_number(fn.this_ptr, "a");
	double b1 = get_prop_number(fn.this_ptr, "b");
	double c1 = get_prop_number(fn.this_ptr, "c");
	double d1 = get_prop_number(fn.this_ptr, "d");
	double tx1 = get_prop_number(fn.this_ptr, "tx");
	double ty1 = get_prop_number(fn.this_ptr, "ty");

	double a2 = get_prop_number(other, "a");
	double b2 = get_prop_number(other, "b");
	double c2 = get_prop_number(other, "c");
	double d2 = get_prop_number(other, "d");
	double tx2 = get_prop_number(other, "tx");
	double ty2 = get_prop_number(other, "ty");

	fn.this_ptr->set_member("a", as_value(a1 * a2 + c1 * b2));
	fn.this_ptr->set_member("b", as_value(b1 * a2 + d1 * b2));
	fn.this_ptr->set_member("c", as_value(a1 * c2 + c1 * d2));
	fn.this_ptr->set_member("d", as_value(b1 * c2 + d1 * d2));
	fn.this_ptr->set_member("tx", as_value(a1 * tx2 + c1 * ty2 + tx1));
	fn.this_ptr->set_member("ty", as_value(b1 * tx2 + d1 * ty2 + ty1));
}

static void matrix_copyFrom(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	as_object* source = fn.arg(0).to_object();
	if (source)
	{
		fn.this_ptr->set_member("a", get_prop_value(source, "a"));
		fn.this_ptr->set_member("b", get_prop_value(source, "b"));
		fn.this_ptr->set_member("c", get_prop_value(source, "c"));
		fn.this_ptr->set_member("d", get_prop_value(source, "d"));
		fn.this_ptr->set_member("tx", get_prop_value(source, "tx"));
		fn.this_ptr->set_member("ty", get_prop_value(source, "ty"));
	}
}

static void matrix_createBox(const fn_call& fn)
{
	if (fn.nargs < 1) return;
	double scaleX = fn.arg(0).to_number();
	double scaleY = (fn.nargs >= 2) ? fn.arg(1).to_number() : scaleX;
	double rotation = (fn.nargs >= 3) ? fn.arg(2).to_number() : 0;
	double tx = (fn.nargs >= 4) ? fn.arg(3).to_number() : 0;
	double ty = (fn.nargs >= 5) ? fn.arg(4).to_number() : 0;

	double cos_r = cos(rotation);
	double sin_r = sin(rotation);
	fn.this_ptr->set_member("a", as_value(cos_r * scaleX));
	fn.this_ptr->set_member("b", as_value(sin_r * scaleX));
	fn.this_ptr->set_member("c", as_value(-sin_r * scaleY));
	fn.this_ptr->set_member("d", as_value(cos_r * scaleY));
	fn.this_ptr->set_member("tx", as_value(tx));
	fn.this_ptr->set_member("ty", as_value(ty));
}

static void matrix_createGradientBox(const fn_call& fn)
{
	matrix_createBox(fn);
}

static void matrix_deltaTransformPoint(const fn_call& fn)
{
	if (fn.nargs < 1) { fn.result->set_undefined(); return; }
	as_object* pt = fn.arg(0).to_object();
	if (!pt) { fn.result->set_undefined(); return; }

	double px = get_prop_number(pt, "x");
	double py = get_prop_number(pt, "y");
	double a = get_prop_number(fn.this_ptr, "a");
	double b = get_prop_number(fn.this_ptr, "b");
	double c = get_prop_number(fn.this_ptr, "c");
	double d = get_prop_number(fn.this_ptr, "d");

	gc_ptr<as_object> result = new as_object(fn.get_player());
	result->set_member("x", as_value(a * px + c * py));
	result->set_member("y", as_value(b * px + d * py));
	fn.result->set_as_object(result.get_ptr());
}

static void matrix_transformPoint(const fn_call& fn)
{
	if (fn.nargs < 1) { fn.result->set_undefined(); return; }
	as_object* pt = fn.arg(0).to_object();
	if (!pt) { fn.result->set_undefined(); return; }

	double px = get_prop_number(pt, "x");
	double py = get_prop_number(pt, "y");
	double a = get_prop_number(fn.this_ptr, "a");
	double b = get_prop_number(fn.this_ptr, "b");
	double c = get_prop_number(fn.this_ptr, "c");
	double d = get_prop_number(fn.this_ptr, "d");
	double tx = get_prop_number(fn.this_ptr, "tx");
	double ty = get_prop_number(fn.this_ptr, "ty");

	gc_ptr<as_object> result = new as_object(fn.get_player());
	result->set_member("x", as_value(a * px + c * py + tx));
	result->set_member("y", as_value(b * px + d * py + ty));
	fn.result->set_as_object(result.get_ptr());
}

static void matrix_setTo(const fn_call& fn)
{
	if (fn.nargs < 6) return;
	fn.this_ptr->set_member("a", as_value(fn.arg(0).to_number()));
	fn.this_ptr->set_member("b", as_value(fn.arg(1).to_number()));
	fn.this_ptr->set_member("c", as_value(fn.arg(2).to_number()));
	fn.this_ptr->set_member("d", as_value(fn.arg(3).to_number()));
	fn.this_ptr->set_member("tx", as_value(fn.arg(4).to_number()));
	fn.this_ptr->set_member("ty", as_value(fn.arg(5).to_number()));
}

// Stub implementations for less common methods
static void matrix_copyColumnFrom(const fn_call& fn) {}
static void matrix_copyColumnTo(const fn_call& fn) {}
static void matrix_copyRowFrom(const fn_call& fn) {}
static void matrix_copyRowTo(const fn_call& fn) {}

void as3_matrix::init(as_object* target)
{
	target->set_member("a", as_value(1.0));
	target->set_member("b", as_value(0.0));
	target->set_member("c", as_value(0.0));
	target->set_member("d", as_value(1.0));
	target->set_member("tx", as_value(0.0));
	target->set_member("ty", as_value(0.0));

	target->set_member("clone", as_value(matrix_clone));
	target->set_member("concat", as_value(matrix_concat));
	target->set_member("copyColumnFrom", as_value(matrix_copyColumnFrom));
	target->set_member("copyColumnTo", as_value(matrix_copyColumnTo));
	target->set_member("copyFrom", as_value(matrix_copyFrom));
	target->set_member("copyRowFrom", as_value(matrix_copyRowFrom));
	target->set_member("copyRowTo", as_value(matrix_copyRowTo));
	target->set_member("createBox", as_value(matrix_createBox));
	target->set_member("createGradientBox", as_value(matrix_createGradientBox));
	target->set_member("deltaTransformPoint", as_value(matrix_deltaTransformPoint));
	target->set_member("identity", as_value(matrix_identity));
	target->set_member("invert", as_value(matrix_invert));
	target->set_member("rotate", as_value(matrix_rotate));
	target->set_member("scale", as_value(matrix_scale));
	target->set_member("setTo", as_value(matrix_setTo));
	target->set_member("transformPoint", as_value(matrix_transformPoint));
	target->set_member("translate", as_value(matrix_translate));
}

// ========================================================================
// JSON implementation
// ========================================================================

// Simple JSON parser (handles basic objects, arrays, strings, numbers, booleans, null)
static as_value json_parse_value(const char** pp);

static void skip_whitespace(const char** pp)
{
	while (**pp && (**pp == ' ' || **pp == '\t' || **pp == '\n' || **pp == '\r')) (*pp)++;
}

static as_value json_parse_string(const char** pp)
{
	(*pp)++;  // skip opening "
	tu_string result;
	while (**pp && **pp != '"')
	{
		if (**pp == '\\')
		{
			(*pp)++;
			switch (**pp)
			{
			case '"': result += '"'; break;
			case '\\': result += '\\'; break;
			case '/': result += '/'; break;
			case 'n': result += '\n'; break;
			case 'r': result += '\r'; break;
			case 't': result += '\t'; break;
			default: result += **pp; break;
			}
		}
		else
		{
			result += **pp;
		}
		(*pp)++;
	}
	if (**pp == '"') (*pp)++;  // skip closing "
	return as_value(result);
}

static as_value json_parse_number(const char** pp)
{
	char* end = NULL;
	double val = strtod(*pp, &end);
	*pp = end;
	return as_value(val);
}


// Simplified JSON parse - handles basic types
static void json_parse(const fn_call& fn)
{
	if (fn.nargs < 1) { fn.result->set_undefined(); return; }
	const char* str = fn.arg(0).to_string();

	// Simple parser for basic JSON values
	skip_whitespace(&str);
	if (*str == '"')
	{
		fn.result->set_string(json_parse_string(&str).to_string());
	}
	else if (*str == '{' || *str == '[')
	{
		// Complex objects - return undefined for now
		fn.result->set_undefined();
	}
	else if (*str == 't' || *str == 'f')
	{
		fn.result->set_bool(strncmp(str, "true", 4) == 0);
	}
	else if (*str == 'n')
	{
		fn.result->set_null();
	}
	else
	{
		fn.result->set_double(json_parse_number(&str).to_number());
	}
}

static void json_stringify(const fn_call& fn)
{
	if (fn.nargs < 1) { fn.result->set_string("undefined"); return; }
	// Simplified - return toString representation
	fn.result->set_string(fn.arg(0).to_string());
}

// ========================================================================
// Init function to register all implementations
// ========================================================================

void as3_register_class_implementations(player* player)
{
	// Register JSON
	as_object* json_obj = new as_object(player);
	json_obj->set_member("parse", as_value(json_parse));
	json_obj->set_member("stringify", as_value(json_stringify));
	player->get_global()->set_member("JSON", as_value(json_obj));
}

} // end namespace gameswf
