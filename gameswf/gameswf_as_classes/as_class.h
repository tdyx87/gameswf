// as_class.h	-- Julien Hamaide <julien.hamaide@gmail.com> 2008

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Action Script 3 Class object

#ifndef GAMESWF_AS_CLASS_H
#define GAMESWF_AS_CLASS_H

#include "gameswf/gameswf_object.h"
#include "gameswf/gameswf_abc.h"

namespace gameswf
{
	class as_class : public as_object
	{
	public:
		enum { m_class_id = AS_CLASS };

		virtual bool is(int class_id) const
		{
			if (m_class_id == class_id) return true;
			else return as_object::is(class_id);
		}

		as_class( player * player) : 
			as_object( player ),
			m_class_index(-1)
		{
		}

		void set_class( class_info * info, int class_index = -1 )
		{
			m_class = info;
			m_class_index = class_index;
		}

		class_info * get_class_info() const
		{
			return m_class.get_ptr();
		}

		int get_class_index() const
		{
			return m_class_index;
		}

		exported_module virtual bool	find_property( const tu_stringi & name, as_value * val );

		// Override get_member so that prototype chain lookups can find
		// methods/properties defined in class traits.
		// Without this, as_object::get_member only searches m_members + proto chain
		// and never triggers find_property's trait search.
		exported_module virtual bool	get_member( const tu_stringi & name, as_value * val );

		// Static getters (Trait_Getter in class_info traits) live beside the
		// instance/prototype search so property reads on class objects also
		// resolve to the getter's value.
		exported_module virtual bool	is_getter_trait(const char* name) const;

	private:

		gc_ptr<class_info> m_class;
		int m_class_index;

	};

}


#endif
