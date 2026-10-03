// as_loader_info.h -- AS3 LoaderInfo class
// This source code has been donated to the Public Domain.

#ifndef GAMESWF_AS_LOADER_INFO_H
#define GAMESWF_AS_LOADER_INFO_H

#include "gameswf/gameswf_action.h"
#include "gameswf/gameswf_character.h"

namespace gameswf
{
	// AS3 LoaderInfo class - provides information about the loaded SWF
	struct as_loader_info : public as_object
	{
		enum { m_class_id = AS_LOADER_INFO };
		virtual bool is(int class_id) const
		{
			if (m_class_id == class_id) return true;
			else return as_object::is(class_id);
		}

		as_loader_info(player* player);

		// Properties
		void set_url(const char* url);
		void set_bytesLoaded(int bytes);
		void set_bytesTotal(int bytes);
		void set_contentType(const char* type);

	private:
		tu_string m_url;
		int m_bytesLoaded;
		int m_bytesTotal;
		tu_string m_contentType;
	};

	void as_global_loader_info_ctor(const fn_call& fn);

} // end namespace gameswf

#endif // GAMESWF_AS_LOADER_INFO_H
