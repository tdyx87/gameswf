// as_timer.cpp -- AS3 Timer class implementation
// This source code has been donated to the Public Domain.

#include "gameswf/gameswf_as_classes/as_timer.h"
#include "gameswf/gameswf_as_classes/as_event.h"

namespace gameswf
{

	// Timer constructor
	as_timer::as_timer(player* player, double delay, int repeatCount) :
		as_object(player),
		m_delay(delay),
		m_repeatCount(repeatCount),
		m_currentCount(0),
		m_running(false),
		m_elapsedTime(0)
	{
		// Register properties
		set_member("delay", as_value(delay));
		set_member("repeatCount", as_value(repeatCount));
		set_member("currentCount", as_value(0));
		set_member("running", as_value(false));

		// Register methods
		builtin_member("start", as_timer_start);
		builtin_member("stop", as_timer_stop);
		builtin_member("reset", as_timer_reset);
	}

	void as_timer::start()
	{
		if (!m_running)
		{
			m_running = true;
			set_member("running", as_value(true));
		}
	}

	void as_timer::stop()
	{
		if (m_running)
		{
			m_running = false;
			set_member("running", as_value(false));
		}
	}

	void as_timer::reset()
	{
		stop();
		m_currentCount = 0;
		m_elapsedTime = 0;
		set_member("currentCount", as_value(0));
	}

	void as_timer::update(double deltaTime)
	{
		if (!m_running) return;

		m_elapsedTime += deltaTime;

		while (m_elapsedTime >= m_delay)
		{
			m_elapsedTime -= m_delay;
			m_currentCount++;
			set_member("currentCount", as_value(m_currentCount));

			// Dispatch TIMER event
			as_value timer_event_val;
			if (get_member("__timer_event__", &timer_event_val) && timer_event_val.is_object())
			{
				// TODO: Dispatch event to listeners
			}

			// Check if we've reached repeat count
			if (m_repeatCount > 0 && m_currentCount >= m_repeatCount)
			{
				stop();
				// Dispatch TIMER_COMPLETE event
				break;
			}
		}
	}

	// Global Timer constructor
	void as_global_timer_ctor(const fn_call& fn)
	{
		double delay = 1000; // Default 1 second
		int repeatCount = 0; // Default infinite

		if (fn.nargs >= 1) delay = fn.arg(0).to_number();
		if (fn.nargs >= 2) repeatCount = (int)fn.arg(1).to_number();

		gc_ptr<as_timer> timer = new as_timer(fn.get_player(), delay, repeatCount);
		fn.result->set_as_object(timer.get_ptr());
	}

	// Timer methods
	void as_timer_start(const fn_call& fn)
	{
		as_timer* timer = cast_to<as_timer>(fn.this_ptr);
		if (timer) timer->start();
	}

	void as_timer_stop(const fn_call& fn)
	{
		as_timer* timer = cast_to<as_timer>(fn.this_ptr);
		if (timer) timer->stop();
	}

	void as_timer_reset(const fn_call& fn)
	{
		as_timer* timer = cast_to<as_timer>(fn.this_ptr);
		if (timer) timer->reset();
	}

	// Global timer functions for setInterval/setTimeout
	static int s_timer_id_counter = 1;
	static hash<int, gc_ptr<as_timer>> s_active_timers;

	void as3_setInterval(const fn_call& fn)
	{
		if (fn.nargs < 2)
		{
			fn.result->set_int(0);
			return;
		}

		as_value callback = fn.arg(0);
		double delay = fn.arg(1).to_number();

		// Create a timer that repeats indefinitely
		gc_ptr<as_timer> timer = new as_timer(fn.get_player(), delay, 0);
		timer->start();

		int timerId = s_timer_id_counter++;
		s_active_timers.add(timerId, timer);

		fn.result->set_int(timerId);
	}

	void as3_setTimeout(const fn_call& fn)
	{
		if (fn.nargs < 2)
		{
			fn.result->set_int(0);
			return;
		}

		as_value callback = fn.arg(0);
		double delay = fn.arg(1).to_number();

		// Create a timer that runs once
		gc_ptr<as_timer> timer = new as_timer(fn.get_player(), delay, 1);
		timer->start();

		int timerId = s_timer_id_counter++;
		s_active_timers.add(timerId, timer);

		fn.result->set_int(timerId);
	}

	void as3_clearInterval(const fn_call& fn)
	{
		if (fn.nargs < 1) return;

		int timerId = (int)fn.arg(0).to_number();
		s_active_timers.erase(timerId);
	}

	void as3_clearTimeout(const fn_call& fn)
	{
		if (fn.nargs < 1) return;

		int timerId = (int)fn.arg(0).to_number();
		s_active_timers.erase(timerId);
	}

} // end namespace gameswf
