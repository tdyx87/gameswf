// as_timer.h -- AS3 Timer class implementation
// This source code has been donated to the Public Domain.

#ifndef GAMESWF_AS_TIMER_H
#define GAMESWF_AS_TIMER_H

#include "gameswf/gameswf_action.h"
#include "gameswf/gameswf_character.h"

namespace gameswf
{

	// AS3 Timer class
	struct as_timer : public as_object
	{
		enum { m_class_id = AS_TIMER };
		virtual bool is(int class_id) const
		{
			if (m_class_id == class_id) return true;
			else return as_object::is(class_id);
		}

		as_timer(player* player, double delay, int repeatCount = 0);

		// Timer control
		void start();
		void stop();
		void reset();

		// Properties
		double get_delay() const { return m_delay; }
		void set_delay(double delay) { m_delay = delay; }
		int get_repeatCount() const { return m_repeatCount; }
		void set_repeatCount(int count) { m_repeatCount = count; }
		int get_currentCount() const { return m_currentCount; }
		bool get_running() const { return m_running; }

		// Called by player each frame to update timer
		void update(double deltaTime);

	private:
		double m_delay;
		int m_repeatCount;
		int m_currentCount;
		bool m_running;
		double m_elapsedTime;
	};

	// Global timer functions
	void as_global_timer_ctor(const fn_call& fn);
	void as_timer_start(const fn_call& fn);
	void as_timer_stop(const fn_call& fn);
	void as_timer_reset(const fn_call& fn);

	// setInterval/setTimeout/clearInterval/clearTimeout implementations
	void as3_setInterval(const fn_call& fn);
	void as3_setTimeout(const fn_call& fn);
	void as3_clearInterval(const fn_call& fn);
	void as3_clearTimeout(const fn_call& fn);

} // end namespace gameswf

#endif // GAMESWF_AS_TIMER_H
