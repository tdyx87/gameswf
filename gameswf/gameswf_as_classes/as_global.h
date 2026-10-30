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
		// Strong references on purpose.  Flash keeps both the callback closure
		// and its receiver alive until the interval is cleared; with weak
		// pointers the collector could reclaim a closure that the SWF only
		// referenced through this timer, and the timer would then keep firing
		// while the call did nothing at all (silently stalling whatever the
		// game was waiting for).
		gc_ptr<as_object> m_this_ptr;
		gc_ptr<as_function> m_func;
		float m_interval;	// sec
		float m_time_remainder;
		array<as_value> m_arg;
		bool m_do_once;

		// Note: a timer must not hold a gc_ptr to itself to stay alive.  The
		// collector is mark/sweep, so a self-reference is an unreachable
		// cycle and is swept; the strong reference lives in
		// player::m_timers instead (see player::add_timer).

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
