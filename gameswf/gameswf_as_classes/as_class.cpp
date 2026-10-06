// as_class.cpp	-- Julien Hamaide <julien.hamaide@gmail.com> 2008

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Action Script 3 Class object

#include "as_class.h"
#include <stdio.h>

namespace gameswf
{

	// Search class traits for a named property/method.
	// This is the shared logic used by both get_member and find_property.
	static bool search_class_traits(as_class* self, const tu_stringi& name, as_value* val)
	{
		class_info* ci = NULL;
		// Access m_class through find_property to avoid needing a friend accessor.
		// Instead, we inline the trait search here.
		// We need to access m_class, so we do it through a helper.
		// Actually, since this is a static function in the same .cpp, we can't access m_class directly.
		// We'll call find_property which already has this logic.
		// But find_property calls get_member which calls us... circular!
		// So we need a different approach: put the trait search logic here.

		// Since we can't access m_class from a static function, we'll restructure:
		// get_member calls as_object::get_member first, then falls through to trait search.
		// The trait search must be in the member function where m_class is accessible.
		return false; // placeholder - actual search is in get_member below
	}

	bool as_class::get_member( const tu_stringi & name, as_value * val )
	{
		// First try the base implementation (m_members + m_proto chain)
		if ( as_object::get_member( name, val ) )
		{
			return true;
		}

		// Not found in m_members or prototype chain.
		// Search class traits for methods, slots, getters, etc.
		if ( m_class == NULL )
		{
			return false;
		}

		for ( int i = 0; i < m_class->m_trait.size(); i++ )
		{
			traits_info* ti = m_class->m_trait[i].get();
			const char* traits_name = m_class->m_abc->get_multiname( ti->m_name );

			if ( name == traits_name )
			{
				if ( ti->m_kind == traits_info::Trait_Slot )
				{
					as_object * object = new as_object( get_player() );
					set_member( name, object );
					val->set_as_object( object );
					return true;
				}
				else if ( ti->m_kind == traits_info::Trait_Const )
				{
					// Static const declared on the class but not assigned yet.
					// cinit compiles to `findproperty X; push <default>;
					// initproperty X`, so find_property() runs *before* the
					// value exists.  Report the trait as present (undefined)
					// so the class object is selected as the owner -- otherwise
					// the constant is written to _global and reads such as
					// `GameType.MENU_CHANGE` / `event.SERVICE_READY` come back
					// undefined forever.
					*val = as_value();
					return true;
				}
				else if ( ti->m_kind == traits_info::Trait_Method
					|| ti->m_kind == traits_info::Trait_Getter
					|| ti->m_kind == traits_info::Trait_Setter )
				{
					int method_index = ti->trait_method.m_method;
					as_function* func = m_class->m_abc->get_method( method_index );
					if ( func )
					{
						as_value method_val( func );
						set_member( name, method_val );  // cache for future lookups
						*val = method_val;
						return true;
					}
					return false;
				}
				else if ( ti->m_kind == traits_info::Trait_Function )
				{
					int func_index = ti->trait_function.m_function;
					as_function* func = m_class->m_abc->get_method( func_index );
					if ( func )
					{
						as_value method_val( func );
						set_member( name, method_val );  // cache for future lookups
						*val = method_val;
						return true;
					}
					return false;
				}
				else if ( ti->m_kind == traits_info::Trait_Class )
				{
					int class_index = ti->trait_class.m_classi;
					const char* class_name = m_class->m_abc->get_multiname(
						m_class->m_abc->get_instance_info_by_index( class_index )->m_name );
					if ( class_name )
					{
						as_value class_val;
						if ( get_global()->get_member( class_name, &class_val ) )
						{
							set_member( name, class_val );  // cache for future lookups
							*val = class_val;
							return true;
						}
					}
					return false;
				}
				return false;
			}
		}

		fprintf(stderr, "[CLASS_MISS] '%s' m_class=%p trait_count=%d\n",
			name.c_str(), (void*)m_class.get_ptr(),
			m_class != NULL ? (int)m_class->m_trait.size() : -1);
		fflush(stderr);
		return false;
	}

	bool	as_class::find_property( const tu_stringi & name, as_value * val )
	{
		// find_property() must leave *val holding the OWNING object, not the
		// property's value: vm_stack::find_property() returns val->to_object()
		// as the scope entry that owns the name, and initproperty() then
		// writes through that pointer.  Returning the raw value here made
		// every `findproperty X` inside a cinit fall back to _global, so class
		// constants (GameType.*, event.*, ...) landed on the global object and
		// read back as undefined off the class.
		//
		// as_object::find_property() already routes through the virtual
		// get_member(), which searches class traits, then sets *val = this.
		return as_object::find_property( name, val );
	}

	bool	as_class::is_getter_trait(const char* name) const
	{
		if ( as_object::is_getter_trait( name ) )
		{
			return true;
		}

		if ( name == NULL || m_class == NULL )
		{
			return false;
		}

		for ( int i = 0; i < m_class->m_trait.size(); i++ )
		{
			traits_info* ti = m_class->m_trait[i].get();
			if ( ti == NULL )
			{
				continue;
			}
			const char* traits_name = m_class->m_abc != NULL
				? m_class->m_abc->get_multiname( ti->m_name ) : NULL;
			if ( traits_name == NULL || strcmp( traits_name, name ) != 0 )
			{
				continue;
			}
			if ( ti->m_kind == traits_info::Trait_Getter )
			{
				return true;
			}
		}
		return false;
	}
}
