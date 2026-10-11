// as_global.h	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Action Script global functions implementation


#ifndef GAMESWF_AS_GLOBAL_H
#define GAMESWF_AS_GLOBAL_H

namespace gameswf
{
	void	as_global_trace(const fn_call& fn);
	void	as_global_get_version(const fn_call& fn);
	void	as_global_assetpropflags(const fn_call& fn);
	void  as_global_setinterval(const fn_call& fn);
	void  as_global_clearinterval(const fn_call& fn);
	void  as_global_settimeout(const fn_call& fn);
	void  as_global_cleartimeout(const fn_call& fn);
	void	as_global_update_after_event(const fn_call& fn);

	// for setInterval and setTimeout
	struct as_timer : public as_object
	{
		weak_ptr<as_object> m_this_ptr;
		weak_ptr<as_function> m_func;
		float m_interval;	// sec
		float m_time_remainder;
		array<as_value> m_arg;
		bool m_do_once;

		// The GC is refcount-based and listener::m_listeners holds only
		// weak_ptrs.  A timer registered from callpropvoid (return value
		// discarded) therefore died before its first tick; m_keepalive is
		// a self-reference released when the timer is consumed (do_once
		// fire) or cleared, so it stays alive exactly while registered.
		gc_ptr<as_object> m_keepalive;

		// Unique id of a gameswf resource
		enum { m_class_id = AS_TIMER };
		virtual bool is(int class_id) const
		{
			if (m_class_id == class_id) return true;
			else return as_object::is(class_id);
		}

		as_timer(player* player) :
			as_object(player),
			m_interval(0.0f),
			m_time_remainder(0.0f),
			m_do_once(false)
		{
		}

		virtual ~as_timer();

		virtual void advance(float delta_time);
	};


}	// end namespace gameswf


#endif // GAMESWF_AS_GLOBAL_H


// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:
