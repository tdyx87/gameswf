// as_loader_info.cpp -- AS3 LoaderInfo class implementation
// This source code has been donated to the Public Domain.

#include "gameswf/gameswf_as_classes/as_loader_info.h"

namespace gameswf
{

	// Property getter/setter wrappers
	static void as_loader_info_url_get(const fn_call& fn)
	{
		as_loader_info* info = cast_to<as_loader_info>(fn.this_ptr);
		if (info == NULL)
		{
			fn.result->set_undefined();
			return;
		}
		as_value url_val;
		if (info->get_member("__url__", &url_val))
		{
			*fn.result = url_val;
		}
		else
		{
			fn.result->set_string("");
		}
	}

	static void as_loader_info_bytesLoaded_get(const fn_call& fn)
	{
		as_loader_info* info = cast_to<as_loader_info>(fn.this_ptr);
		if (info == NULL)
		{
			fn.result->set_int(0);
			return;
		}
		as_value val;
		if (info->get_member("__bytesLoaded__", &val))
		{
			*fn.result = val;
		}
		else
		{
			fn.result->set_int(0);
		}
	}

	static void as_loader_info_bytesTotal_get(const fn_call& fn)
	{
		as_loader_info* info = cast_to<as_loader_info>(fn.this_ptr);
		if (info == NULL)
		{
			fn.result->set_int(0);
			return;
		}
		as_value val;
		if (info->get_member("__bytesTotal__", &val))
		{
			*fn.result = val;
		}
		else
		{
			fn.result->set_int(0);
		}
	}

	static void as_loader_info_contentType_get(const fn_call& fn)
	{
		as_loader_info* info = cast_to<as_loader_info>(fn.this_ptr);
		if (info == NULL)
		{
			fn.result->set_string("");
			return;
		}
		as_value val;
		if (info->get_member("__contentType__", &val))
		{
			*fn.result = val;
		}
		else
		{
			fn.result->set_string("application/x-shockwave-flash");
		}
	}

	as_loader_info::as_loader_info(player* player) :
		as_object(player),
		m_bytesLoaded(0),
		m_bytesTotal(0)
	{
		m_contentType = "application/x-shockwave-flash";

		// Plain data properties.  These used to be registered as native getter
		// functions, but AVM2 `getproperty` hands back the member's value as-is
		// -- for a function member that is the function itself -- so
		// `loaderInfo.bytesTotal` evaluated to a function object and every
		// arithmetic use of it (the loading bar's percentage) produced NaN.
		set_member("url", as_value(""));
		set_member("bytesLoaded", as_value(0));
		set_member("bytesTotal", as_value(0));
		set_member("contentType", as_value(m_contentType.c_str()));
	}

	void as_loader_info::set_url(const char* url)
	{
		m_url = url;
		set_member("__url__", as_value(url));
		set_member("url", as_value(url));
	}

	void as_loader_info::set_bytesLoaded(int bytes)
	{
		m_bytesLoaded = bytes;
		set_member("__bytesLoaded__", as_value(bytes));
		set_member("bytesLoaded", as_value(bytes));
	}

	void as_loader_info::set_bytesTotal(int bytes)
	{
		m_bytesTotal = bytes;
		set_member("__bytesTotal__", as_value(bytes));
		set_member("bytesTotal", as_value(bytes));
	}

	void as_loader_info::set_contentType(const char* type)
	{
		m_contentType = type;
		set_member("__contentType__", as_value(type));
		set_member("contentType", as_value(type));
	}

	void as_global_loader_info_ctor(const fn_call& fn)
	{
		gc_ptr<as_loader_info> obj = new as_loader_info(fn.get_player());
		fn.result->set_as_object(obj.get_ptr());
	}

} // end namespace gameswf
