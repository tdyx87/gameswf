// gameswf_root.cpp	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Some implementation for SWF player.

// Useful links:
//
// http://sswf.sourceforge.net/SWFalexref.html
// http://www.openswf.org

#include "gameswf/gameswf_character.h"
#include "gameswf/gameswf_movie_def.h"
#include "gameswf/gameswf_render.h"
#include "gameswf/gameswf_root.h"
#include "gameswf/gameswf_shape.h"
#include "gameswf/gameswf_sprite.h"
#include "gameswf/gameswf_impl.h"
#include "gameswf/gameswf_avm2.h"
#include "base/tu_random.h"

#ifdef _WIN32
#	define stricmp _stricmp
#else
#	define stricmp strcasecmp
#endif                                  

namespace gameswf
{

	static tu_mutex s_gameswf_engine;
	tu_mutex& gameswf_engine_mutex()
	{
		return s_gameswf_engine;
	}

	// Mouse tracing used to run unconditionally, and the drag trace fires on
	// every frame of a drag with an fflush -- enough to starve the frame rate
	// that AS3 timers depend on.  Off unless asked for.
	static bool mouse_diag()
	{
		static int s = -1;
		if (s < 0)
		{
			const char* e = getenv("GAMESWF_MOUSEDIAG");
			s = (e != NULL && e[0] != 0 && strcmp(e, "0") != 0) ? 1 : 0;
		}
		return s == 1;
	}

	root::root(player* player, movie_def_impl* def)	:
		m_def(def),
		m_movie(NULL),
		m_viewport_x0(0),
		m_viewport_y0(0),
		m_viewport_width(1),
		m_viewport_height(1),
		m_pixel_scale(1.0f),
		m_background_color(0, 0, 0, 255),
		m_mouse_x(0),
		m_mouse_y(0),
		m_mouse_buttons(0),
		m_userdata(NULL),
		m_on_event_load_called(false),
		m_shift_key_state(false),
		m_current_active_entity(NULL),

		// the first time we needs do advance() and
		// then display() certainly
		m_time_remainder(1.0f),

		m_frame_time(1.0f),
		m_player(player)
	{
		assert(m_def != NULL);
		set_display_viewport(0, 0, (int) m_def->get_width_pixels(), (int) m_def->get_height_pixels());
		m_frame_time = 1.0f / get_frame_rate();
		player->set_root(this);
	}

	root::~root()
	{
		assert(m_movie != NULL);
		m_movie = NULL;

		assert(m_def != NULL);
		m_def = NULL;
	}

	void root::set_active_entity(character* ch)
	{
		m_current_active_entity = ch;
	}

	void	root::generate_mouse_button_events(mouse_button_state* ms)
	{
		gc_ptr<character> active_entity  = ms->m_active_entity; // entity that currently owns the mouse pointer
		gc_ptr<character> topmost_entity = ms->m_topmost_entity;// what's underneath the mouse right now (updated every frame at the beginning)

		// set cursor as passive/active object
		if( active_entity != NULL && active_entity->can_handle_mouse_event() )
		{
			render::set_cursor(render_handler::ACTIVE_CURSOR);
		}
		else
		{
			render::set_cursor(render_handler::SYSTEM_CURSOR);
		}

		if( ms->m_mouse_button_state_last == 1 )
		{
			// Mouse button was down.

			// Handle trackAsMenu dragOver
			if( active_entity == NULL || active_entity->get_track_as_menu() )
			{
				if( (topmost_entity != NULL) && (topmost_entity != active_entity) && topmost_entity->get_track_as_menu() )
				{
					// Transfer to topmost entity, dragOver
					active_entity = topmost_entity;
					active_entity->on_event( event_id::DRAG_OVER );
					ms->m_mouse_inside_entity_last = true;
				}
			}

			// Handle onDragOut, onDragOver
			if( !ms->m_mouse_inside_entity_last && (topmost_entity == active_entity) )
			{
				// onDragOver
				if (active_entity != NULL)
				{
					active_entity->on_event( event_id::DRAG_OVER );
				}
				ms->m_mouse_inside_entity_last = true;
			}
			else if( ms->m_mouse_inside_entity_last && (topmost_entity != active_entity) )
			{
				// onDragOut
				if (active_entity != NULL)
				{
					active_entity->on_event( event_id::DRAG_OUT );
				}
				ms->m_mouse_inside_entity_last = false;
			}

			// Handle onRelease, onReleaseOutside
			if (ms->m_mouse_button_state_current == 0)
			{
				m_mouse_listener.notify(event_id::MOUSE_UP);

				// AS3 MouseEvent.MOUSE_UP on the stage: the stage is the
				// root movie, and games use `stage.addEventListener(
				// MouseEvent.MOUSE_UP, ...)` to end a drag gesture that
				// started on one of their own objects.  The release has to
				// reach the stage no matter which object was under the
				// cursor.
				if (m_movie != NULL)
				{
					fprintf(stderr, "[STAGEUP] movie=%p name=%s\n",
						(void*) m_movie, m_movie->get_name().c_str());
					fflush(stderr);
					m_movie->on_event(event_id::MOUSE_UP);
				}

				if( topmost_entity == active_entity )
				{
					// onRelease
					if( active_entity != NULL )
					{
						active_entity->on_event( event_id::RELEASE );
					}
				}
				else if( active_entity != NULL && !active_entity->get_track_as_menu() )
				{
					// onReleaseOutside
					active_entity->on_event( event_id::RELEASE_OUTSIDE );

					// Flash still delivers mouseUp to whatever sits under
					// the cursor at release time (bubbling upward).  This
					// matters when the target was replaced or covered
					// between press and release: without it, root-level
					// mouseUp listeners never see the release at all.
					if (topmost_entity != NULL)
					{
						topmost_entity->on_event( event_id::MOUSE_UP );
					}
				}
			}
		}

		if (ms->m_mouse_button_state_last == 0)
		{
			// Mouse button was up.

			// mouse up
			if( ms->m_mouse_button_state_current == 0 )
			{
				// New active entity is whatever is below the mouse right now.
				if (topmost_entity != active_entity)
				{
					// onRollOut
					if (active_entity != NULL && ms->m_mouse_inside_entity_last)
					{
						active_entity->on_event( event_id::ROLL_OUT );
					}

					active_entity = topmost_entity;

					// onRollOver
					if (active_entity != NULL)
					{
						active_entity->on_event( event_id::ROLL_OVER );
					}

					ms->m_mouse_inside_entity_last = true;
				}
			}
			else
			// mouse button press
			{
				// onPress
				{
				character* pe = active_entity.get_ptr();
				fprintf(stderr, "[PRESSBR] raw=%d active='%s' mouse=%.1f,%.1f\n",
					m_mouse_buttons, pe != NULL ? pe->get_name().c_str() : "-",
					(float) m_mouse_x, (float) m_mouse_y);
					fflush(stderr);
				}

				// The mouse may have reached a new object since the last
				// roll update (movement across a frame boundary or a
				// synthetic warp+press).  The roll branch above is the
				// "else" of this press branch, so it never runs on the
				// press frame itself.  Sync first so the press lands on
				// what is actually under the cursor; otherwise press and
				// release target a stale entity and the click is lost.
				if (topmost_entity != active_entity)
				{
					if (active_entity != NULL && ms->m_mouse_inside_entity_last)
					{
						active_entity->on_event(event_id::ROLL_OUT);
					}
					active_entity = topmost_entity;
					if (active_entity != NULL)
					{
						active_entity->on_event(event_id::ROLL_OVER);
					}
					ms->m_mouse_inside_entity_last = true;
				}

				m_mouse_listener.notify(event_id::MOUSE_DOWN);

				// set/kill focus
				// It's another entity ?
				if (m_current_active_entity != active_entity)
				{
					// First to clean focus
					if (m_current_active_entity != NULL)
					{
						m_current_active_entity->on_event(event_id::KILLFOCUS);
						m_current_active_entity = NULL;
					}

					// Then to set focus
					if (active_entity != NULL)
					{
						if (active_entity->on_event(event_id::SETFOCUS))
						{
							m_current_active_entity = active_entity;
						}
					}
				}

				if (active_entity != NULL)
				{
					active_entity->on_event(event_id::PRESS);
				}
				ms->m_mouse_inside_entity_last = true;
			}
		}

		if (ms->m_x != ms->m_x_last || ms->m_y != ms->m_y_last)
		{
			// Flash dispatches mouseMove to the object under the cursor
			// (e.target), honouring mouseEnabled/mouseChildren.  AS3 games
			// gate drag logic on it ("if (e.target.name == zone)"), so
			// target the deepest mouse-taking object rather than the
			// pressed/claimed entity.  AS2 onMouseMove has its own global
			// notify (m_mouse_listener) and is unaffected.
			{
				static character* s_last_move_te = NULL;
				character* move_te = NULL;
				if (m_movie->get_mouse_target(move_te, PIXELS_TO_TWIPS(ms->m_x), PIXELS_TO_TWIPS(ms->m_y)))
				{
					// deepest mouse-taking target found
				}
				else if (active_entity != NULL)
				{
					move_te = active_entity;
				}
				else
				if (topmost_entity != NULL)
				{
					move_te = topmost_entity;
				}
				if (move_te != NULL)
				{
					if (move_te != s_last_move_te)
					{
						s_last_move_te = move_te;
						sprite_instance* msp = cast_to<sprite_instance>(move_te);
						character* par = move_te->get_parent();
						rect mlb;
						move_te->get_bound(&mlb);
						{
							as_value mval;
							double malpha = -1.0;
							if (move_te->get_member("alpha", &mval) && mval.is_number())
							{
								malpha = mval.to_number();
							}
							fprintf(stderr, "[MMOVE] te='%s' id=%d ty='%s' par='%s' depth=%d vis=%d alpha=%.3f at %d,%d lb=(%.0f,%.0f)-(%.0f,%.0f) en=%d ch=%d\n",
								move_te->get_name().c_str(), move_te->get_id(), move_te->type_of(),
								par != NULL ? par->get_name().c_str() : "-",
								move_te->get_depth(), (int) move_te->get_visible(),
								malpha,
								ms->m_x, ms->m_y,
								(float) TWIPS_TO_PIXELS(mlb.m_x_min), (float) TWIPS_TO_PIXELS(mlb.m_y_min),
								(float) TWIPS_TO_PIXELS(mlb.m_x_max), (float) TWIPS_TO_PIXELS(mlb.m_y_max),
								msp ? (int) msp->get_mouse_flag("mouseEnabled", true) : -1,
								msp ? (int) msp->get_mouse_flag("mouseChildren", true) : -1);
						}
						fflush(stderr);
					}
					move_te->on_event(event_id::MOUSE_MOVE);
				}
			}

			// Flash dispatches mouseMove to the stage as well, so that
			// `stage.addEventListener(MouseEvent.MOUSE_MOVE, ...)` fires
			// even when the pointer is over empty space.  Without this,
			// AS3 games that track the pointer on the stage (e.g. to
			// follow the cursor with a spray nozzle) never see the move.
			if (m_movie != NULL)
			{
				m_movie->on_event(event_id::MOUSE_MOVE);
			}
		}

		// Write the (possibly modified) gc_ptr copies back
		// into the state struct.
		ms->m_active_entity = active_entity;
		ms->m_topmost_entity = topmost_entity;
		ms->m_mouse_button_state_last = ms->m_mouse_button_state_current;
		ms->m_x_last = ms->m_x;
		ms->m_y_last = ms->m_y;
	}

	// movie info

	int	root::get_movie_version()
	{
		return m_def->get_version();
	}

	int	root::get_movie_width()
	{
		return (int) m_def->get_width_pixels();
	}

	int	root::get_movie_height()
	{
		return (int) m_def->get_height_pixels();
	}

	float	root::get_movie_fps()
	{
		return m_def->get_frame_rate();
	}

	void notify_key_object(player* player, key::code k, bool down);

	void change_focus_character(character *movie, character *active_entity, bool shift)
	{
		bool killfocus = false;
		int  tab = 0;
		int  lastvalid = -1;

		// lets see if focus is in the root
		int i = 0;
		for (i = 0; movie->get_character(i) != NULL; ++i)
		{
			if (movie->get_character(i)->is(AS_EDIT_TEXT) == false)
			{
				continue;
			}

			if (movie->get_character(i)->can_handle_mouse_event() == false)
			{
				continue;
			}

			if (movie->get_character(i) == active_entity)
			{
				if (lastvalid != -1 && shift)
				{
					movie->get_character(i)->on_event(event_id::KILLFOCUS);
					movie->get_character(lastvalid)->on_event(event_id::SETFOCUS);
				}

				//now kill focus on current active event
				tab = i;

				//now i need to find which event to set focus on
				killfocus = true;
				continue;
			}

			lastvalid = i;

			if (killfocus)
			{
				movie->get_character(tab)->on_event(event_id::KILLFOCUS);
				movie->get_character(i)->on_event(event_id::SETFOCUS);
				killfocus = false;
			}
		}

		if (movie->get_character(i) == NULL)
		{
			for (i = 0; movie->get_character(i) != NULL; ++i)
			{
				if (stricmp(movie->get_character(i)->type_of(), "movieclip") != 0)
				{
					continue;
				}

				change_focus_character(movie->get_character(i), active_entity, shift);
			}
		}
	}

	void	root::notify_key_event(player* player, key::code k, bool down)
	{
		// multithread plugins can call gameswf core therefore we should 
		// use gameswf mutex to lock gameswf engine
		gameswf_engine_mutex().lock();

		// First notify global Key object
		// listeners that uses the last keypressed code
		player->notify_key_object(k, down);

		// Notify keypress listeners.
		if (down)
		{
			m_keypress_listener.notify(event_id(event_id::KEY_PRESS, (key::code) k));
		}

		// A bit of a hack huh
		if( k == key::SHIFT )
		{
			m_shift_key_state = down;
		}

		if (((key::code)k == key::TAB) && down)
		{
			bool killfocus = false;
			int	 tab = 0;
			int  lastvalid = -1;

			// lets see if focus is in the root
			int i = 0;
			for (i = 0; get_character(i) != NULL; ++i)
			{
				character* movie = get_character(i);
				assert(movie);

				if (movie->is(AS_EDIT_TEXT) == false)
				{
					continue;
				}

				if (movie->can_handle_mouse_event() == false)
				{
					continue;
				}

				if (movie == m_current_active_entity.get_ptr())
				{
					if (lastvalid != -1 && m_shift_key_state)
					{
						movie->on_event(event_id::KILLFOCUS);
						get_character(lastvalid)->on_event(event_id::SETFOCUS);
						break;
					}
					//we only want to kill focus if there is another event below this one
					tab = i;

					//now i need to find which event to set focus on
					killfocus = true;
					continue;
				}

				lastvalid = i;

				if (killfocus)
				{
					get_character(tab)->on_event(event_id::KILLFOCUS);
					movie->on_event(event_id::SETFOCUS);
					killfocus = false;
					break;
				}
			}

			if (get_character(i) == NULL)
			{
				for (int i = 0; get_character(i) != NULL; ++i)
				{
					if (stricmp(get_character(i)->type_of(), "movieclip") != 0)
					{
						continue;
					}

					change_focus_character(get_character(i), m_current_active_entity.get_ptr(), m_shift_key_state);
				}
			}
		}

		gameswf_engine_mutex().unlock();
	}

	void	root::set_root_movie(character* root_movie)
	{
		m_movie = root_movie;
		assert(m_movie != NULL);
	}

	void	root::set_display_viewport(int x0, int y0, int w, int h)
	{
		m_viewport_x0 = x0;
		m_viewport_y0 = y0;
		m_viewport_width = w;
		m_viewport_height = h;

		// Recompute pixel scale.
		float	scale_x = m_viewport_width / TWIPS_TO_PIXELS(m_def->m_frame_size.width());
		float	scale_y = m_viewport_height / TWIPS_TO_PIXELS(m_def->m_frame_size.height());
		m_pixel_scale = fmax(scale_x, scale_y);
	}


	void	root::notify_mouse_state(int x, int y, int buttons)
	// The host app uses this to tell the movie where the
	// user's mouse pointer is.
	{
		bool is_mouse_moved = (x !=  m_mouse_x) || (y != m_mouse_y);
		m_mouse_x = x;
		m_mouse_y = y;
		m_mouse_buttons = buttons;
		if (buttons != 0 && mouse_diag())
		{
			fprintf(stderr, "[NMS] buttons=%d at %d,%d\n", buttons, x, y);
			fflush(stderr);
		}

		if (is_mouse_moved)
		{
			m_mouse_listener.notify(event_id::MOUSE_MOVE);
		}
	}

	void	root::get_mouse_state(int* x, int* y, int* buttons)
	// Use this to retrieve the last state of the mouse, as set via
	// notify_mouse_state().  Coordinates are in PIXELS, NOT TWIPS.
	{
		assert(x);
		assert(y);
		assert(buttons);

		*x = m_mouse_x;
		*y = m_mouse_y;
		*buttons = m_mouse_buttons;
	}

	character*	root::get_root_movie() const { return m_movie.get_ptr(); }

	void	root::set_drag_state(character::drag_state& ds)
	{
		m_drag_state = ds;

		// do we need to calculate the offset?
		character* ch = ds.GetCharacter();
		if (ch == NULL || ds.IsLockCentered())
		{
			return;
		}

		// get the character's origin
		point origin(0, 0);
		matrix chMatrix = ch->get_world_matrix();
		point worldOrigin;
		chMatrix.transform( &worldOrigin, origin );

		// Get current mouse coordinates
		int x, y, buttons;
		get_mouse_state(&x, &y, &buttons);

		// ... in 'world' space (i.e. twips)
		point worldMouse(PIXELS_TO_TWIPS(x), PIXELS_TO_TWIPS(y));

		// we need the offset from the origin of the character
		float xOffset = worldMouse.m_x - worldOrigin.m_x;
		float yOffset = worldMouse.m_y - worldOrigin.m_y;
		m_drag_state.SetOffset(xOffset, yOffset);
	}

	void	root::stop_drag()
	{
		m_drag_state.Reset();
	}

	// Implement mouse-dragging for this movie.
	void	root::do_mouse_drag()		
	{
		character* draggingChar = m_drag_state.GetCharacter();
		if (draggingChar == NULL)
		{
			return;
		}

		// handle if the character isn't valid anymore
		if (draggingChar->is_alive() == false)
		{
			// no longer valid
			m_drag_state.Reset();
			return;
		}

		// get the current mouse
		int	x, y, buttons;
		get_mouse_state( &x, &y, &buttons );

		{
			static int s_last_dx = -99999, s_last_dy = -99999;
			if (x != s_last_dx || y != s_last_dy)
			{
				s_last_dx = x;
				s_last_dy = y;
			}
		}

		// ... in world coordinates (twips)
		point worldMouse( PIXELS_TO_TWIPS(x), PIXELS_TO_TWIPS(y) );

		matrix parentWorldMat;
		if (draggingChar->m_parent != NULL)
		{
			parentWorldMat = draggingChar->m_parent->get_world_matrix();
		}

		// if we're not locked to the center - adjust by the offset we started dragging by
		if(m_drag_state.IsLockCentered() == false)
		{
			worldMouse.m_x -= m_drag_state.OffsetX();
			worldMouse.m_y -= m_drag_state.OffsetY();
		}

		rect origBoundRect;
		if (m_drag_state.GetBounds(&origBoundRect))
		{
			// bounds are in local coordinate space
			rect bounds;
			bounds.enclose_transformed_rect( parentWorldMat, origBoundRect );

			// Clamp mouse coords within a defined rect.
			worldMouse.m_x = fclamp(worldMouse.m_x, bounds.m_x_min, bounds.m_x_max);
			worldMouse.m_y = fclamp(worldMouse.m_y, bounds.m_y_min, bounds.m_y_max);
		}

		point parentMouse;
		parentWorldMat.transform_by_inverse( &parentMouse, worldMouse );

		// Place our origin so that it coincides with the mouse coords
		// in our parent frame.
		matrix local = draggingChar->get_matrix();
		local.m_[0][2] = parentMouse.m_x;//set translation x
		local.m_[1][2] = parentMouse.m_y;//set translation y
		draggingChar->set_matrix( local );
		if (mouse_diag())
		{
			matrix wm = draggingChar->get_world_matrix();
			point wo(0, 0);
			wm.transform(&wo, wo);
			rect lb;
			draggingChar->get_bound(&lb);
			fprintf(stderr, "[DRAGTRK] name='%s' mouse=%d,%d local=%.1f,%.1f worigin=%.1f,%.1f lb=(%.0f,%.0f)-(%.0f,%.0f)\n",
				draggingChar->get_name().c_str(), x, y,
				(float) TWIPS_TO_PIXELS(parentMouse.m_x), (float) TWIPS_TO_PIXELS(parentMouse.m_y),
				(float) TWIPS_TO_PIXELS(wo.m_x), (float) TWIPS_TO_PIXELS(wo.m_y),
				(float) TWIPS_TO_PIXELS(lb.m_x_min), (float) TWIPS_TO_PIXELS(lb.m_y_min),
				(float) TWIPS_TO_PIXELS(lb.m_x_max), (float) TWIPS_TO_PIXELS(lb.m_y_max));
			fflush(stderr);
		}
		fflush(stderr);
	}


	movie_definition*	root::get_movie_definition() { return m_movie->get_movie_definition(); }

	// 0-based!!
	int	root::get_current_frame() const { return m_movie->get_current_frame(); }
	float	root::get_frame_rate() const { return m_def->get_frame_rate(); }
	void	root::set_frame_rate(float rate)
	{
		if (rate >= 1 && rate <= 120)
		{
			m_frame_time = 1.0f / rate;
			m_def->set_frame_rate(rate); 
		}
	}

	float	root::get_pixel_scale() const
	// Return the size of a logical movie pixel as
	// displayed on-screen, with the current device
	// coordinates.
	{
		return m_pixel_scale;
	}

	character*	root::hit_test_pixel(float x, float y)
	// Resolve the display object that would receive a mouse press at the
	// given pixel position.  Same call generate_mouse_button_events() uses
	// each frame, exposed so the harness can drive reproducible gestures.
	{
		character* te = NULL;
		m_movie->get_topmost_mouse_entity(te,
			PIXELS_TO_TWIPS((int) x), PIXELS_TO_TWIPS((int) y));
		return te;
	}

	static character* find_named(character* ch, const char* name)
	// Depth first search of the display tree for a clip with the given name.
	// Children are walked from the top of the stack downwards so that a
	// harness ends up with the same instance a user would actually click -
	// an object that is covered by another one is not reachable by hand.
	{
		if (ch == NULL || name == NULL)
		{
			return NULL;
		}

		if (ch->get_name().size() > 0 && strcmp(ch->get_name().c_str(), name) == 0)
		{
			return ch;
		}

		sprite_instance* sp =
			cast_to<sprite_instance>(ch);
		if (sp == NULL)
		{
			return NULL;
		}

		int n = sp->avm2_get_num_children();
		for (int i = n - 1; i >= 0; i--)
		{
			character* found = find_named(sp->avm2_get_child_at(i), name);
			if (found != NULL)
			{
				return found;
			}
		}
		return NULL;
	}

	character*	root::find_named_in_tree(const char* name)
	{
		return find_named(m_movie, name);
	}

	// Walk the display list and report every character whose screen-space
	// bounds contain the given pixel, in draw order (bottom first).  Test
	// harnesses use this to find out what actually paints a given pixel,
	// including the fill styles a shape carries.  Enabled with GAMESWF_PICK.
	static void pick_report(character* ch, float x, float y, int depth)
	{
		if (ch == NULL)
		{
			return;
		}

		rect local;
		ch->get_bound(&local);
		matrix m = ch->get_world_matrix();
		rect area = local;
		m.transform(&area);

		float x0 = (float) TWIPS_TO_PIXELS(area.m_x_min);
		float x1 = (float) TWIPS_TO_PIXELS(area.m_x_max);
		float y0 = (float) TWIPS_TO_PIXELS(area.m_y_min);
		float y1 = (float) TWIPS_TO_PIXELS(area.m_y_max);

		bool inside = (x >= x0 && x <= x1 && y >= y0 && y <= y1);
		if (inside && depth < 30)
		{
			// One line per character: what it is, where it is, and -- for
			// shapes -- the fill styles it carries.  Children are reported in
			// draw order (bottom first), so this shows what is painted on
			// top of what.
			const char* kind = "sprite";
			char detail_buf[512];
			detail_buf[0] = 0;
			const char* detail = detail_buf;

			if (cast_to<sprite_instance>(ch) == NULL)
			{
				shape_character_def* sh = cast_to<shape_character_def>(ch->get_character_def());
				if (sh != NULL)
				{
					kind = "SHAPE";
					const array<fill_style>& fs = sh->get_fill_styles();
					int pos = 0;
					for (int k = 0; k < fs.size() && k < 6; k++)
					{
						const rgba& c = fs[k].get_color();
						pos += snprintf(detail_buf + pos, sizeof(detail_buf) - pos,
							" fill[%d]{0x%X,%d,%d,%d,%d}", k, fs[k].get_type(),
							c.m_r, c.m_g, c.m_b, c.m_a);
					}
				}
				else
				{
					bitmap_character* bc = cast_to<bitmap_character>(ch->get_character_def());
					kind = (bc != NULL) ? "BITMAP" : "LEAF";
				}
			}
			detail = detail_buf;

			fprintf(stderr, "[PICK]%*sid=%-5d %-6s name='%s' z=%d vis=%d blend=%d ratio=%.2f bounds=(%.0f,%.0f)-(%.0f,%.0f)%s\n",
				depth, "", ch->get_id(), kind, ch->get_name().c_str(), (int) ch->m_depth,
				(int) ch->m_visible, (int) ch->m_blend_mode, ch->m_ratio, x0, y0, x1, y1, detail);
		}

		sprite_instance* sp = cast_to<sprite_instance>(ch);
		if (sp == NULL)
		{
			return;
		}
		int n = sp->avm2_get_num_children();
		for (int i = 0; i < n; i++)
		{
			pick_report(sp->avm2_get_child_at(i), x, y, inside ? depth + 1 : depth);
		}
	}

	void	root::pick_report_at(float x, float y)
	{
		pick_report(m_movie, x, y, 0);
	}

	// List every named clip in the display tree with its true stage-space
	// bounding box, and say whether a press at the centre of that box would
	// actually land on it.  A clip can be perfectly visible and still be
	// un-grabbable because something else is painted on top of it, and that
	// is exactly the question a drag gesture has to answer.
	void	root::list_pressable_walk(character* ch, int depth)
	{
		if (ch == NULL || depth > 24)
		{
			return;
		}

		if (ch->get_name().size() > 0)
		{
			rect bound;
			ch->get_bound(&bound);
			matrix m;
			character* parent = ch->get_parent();
			if (parent != NULL)
			{
				m = parent->get_world_matrix();
			}
			else
			{
				m = ch->get_world_matrix();
			}
			rect area = bound;
			m.transform(&area);
			float x0 = (float) TWIPS_TO_PIXELS(area.m_x_min);
			float x1 = (float) TWIPS_TO_PIXELS(area.m_x_max);
			float y0 = (float) TWIPS_TO_PIXELS(area.m_y_min);
			float y1 = (float) TWIPS_TO_PIXELS(area.m_y_max);
			float cx = (x0 + x1) * 0.5f;
			float cy = (y0 + y1) * 0.5f;

			// Ask the engine what a press at the centre resolves to, and
			// also probe the four corners of the box: a clip built from
			// several pieces often has an empty middle.
			character* hit = hit_test_pixel(cx, cy);
			const char* hit_name = (hit != NULL) ? hit->get_name().c_str() : "(null)";
			int self_hit = (hit == ch) ? 1 : 0;
			if (!self_hit)
			{
				character* h2 = hit_test_pixel(x0 + 2, y0 + 2);
				if (h2 == ch) { self_hit = 1; hit = h2; hit_name = hit->get_name().c_str(); }
				else
				{
					character* h3 = hit_test_pixel(x1 - 2, y0 + 2);
					if (h3 == ch) { self_hit = 1; hit = h3; hit_name = hit->get_name().c_str(); }
					else
					{
						character* h4 = hit_test_pixel(x0 + 2, y1 - 2);
						if (h4 == ch) { self_hit = 1; hit = h4; hit_name = hit->get_name().c_str(); }
					}
				}
			}
			fprintf(stderr, "[PRESS] %*s%-12s id=%-5d z=%-6d vis=%d box=(%.0f,%.0f)-(%.0f,%.0f) on_stage=%d press=%d hit='%s'\n",
				depth * 2, "", ch->get_name().c_str(), ch->get_id(), ch->get_depth(),
				(int) ch->m_visible,
				x0, y0, x1, y1,
				(int) (x0 >= 0 && y0 >= 0 && x1 <= get_movie_width() && y1 <= get_movie_height()),
				self_hit, hit_name);
		}

		sprite_instance* sp = cast_to<sprite_instance>(ch);
		if (sp == NULL)
		{
			return;
		}
		int n = sp->avm2_get_num_children();
		for (int i = 0; i < n; i++)
		{
			list_pressable_walk(sp->avm2_get_child_at(i), depth + 1);
		}
	}

	void	root::list_pressable()
	{
		list_pressable_walk(m_movie, 0);
		fflush(stderr);
	}

	// GAMESWF_TREE dump: print the whole display tree (every character, not
	// just bounds-matching ones) so instance names and structure can be
	// correlated with on-screen artifacts.
	static void dump_tree_walk(character* ch, int depth)
	{
		if (ch == NULL)
		{
			return;
		}

		rect local;
		ch->get_bound(&local);
		matrix m = ch->get_world_matrix();
		rect area = local;
		m.transform(&area);

		sprite_instance* sp = cast_to<sprite_instance>(ch);
		if (sp)
		{
			fprintf(stderr, "[TREE]%*sid=%-5d %-6s name='%s' z=%d vis=%d bounds=(%.0f,%.0f)-(%.0f,%.0f) frame=%d/%d\n",
				depth, "", ch->get_id(),
				"sprite",
				ch->get_name().c_str(), (int) ch->m_depth, (int) ch->m_visible,
				(float) TWIPS_TO_PIXELS(area.m_x_min), (float) TWIPS_TO_PIXELS(area.m_y_min),
				(float) TWIPS_TO_PIXELS(area.m_x_max), (float) TWIPS_TO_PIXELS(area.m_y_max),
				sp->get_current_frame(), sp->get_frame_count());
		}
		else
		{
			fprintf(stderr, "[TREE]%*sid=%-5d %-6s name='%s' z=%d vis=%d bounds=(%.0f,%.0f)-(%.0f,%.0f)\n",
				depth, "", ch->get_id(),
				"other",
				ch->get_name().c_str(), (int) ch->m_depth, (int) ch->m_visible,
				(float) TWIPS_TO_PIXELS(area.m_x_min), (float) TWIPS_TO_PIXELS(area.m_y_min),
				(float) TWIPS_TO_PIXELS(area.m_x_max), (float) TWIPS_TO_PIXELS(area.m_y_max));
		}

		if (sp == NULL)
		{
			return;
		}
		int n = sp->avm2_get_num_children();
		for (int i = 0; i < n; i++)
		{
			dump_tree_walk(sp->avm2_get_child_at(i), depth + 1);
		}
	}

	void	root::dump_tree()
	{
		dump_tree_walk(m_movie, 0);
	}

	static bool is_descendant_of(character* ch, character* ancestor)
	{
		for (character* c = ch; c != NULL; c = c->get_parent())
		{
			if (c == ancestor)
			{
				return true;
			}
		}
		return false;
	}

	bool	root::find_click_point(character* ch, float* px, float* py)
	// Find somewhere on this clip that a real press would land on it.
	//
	// The centre of the bounding box is no use as an aim point: buttons are
	// built out of several pieces, and the middle is frequently empty or is
	// covered by a child that swallows the press.  So probe the area the clip
	// occupies and keep the first position the hit test agrees belongs to it.
	//
	// The probe count is budgeted.  Every probe walks the display tree, and
	// this runs from inside the frame loop - an unbounded sweep of the whole
	// stage used to take seconds per click and stall the movie so badly that
	// the following scheduled click landed before the game had reacted to
	// the previous one.
	{
		if (ch == NULL)
		{
			return false;
		}

		rect local;
		ch->get_bound(&local);

		matrix m = ch->get_world_matrix();
		rect area = local;
		m.transform(&area);

		float x0 = (float) TWIPS_TO_PIXELS(area.m_x_min);
		float x1 = (float) TWIPS_TO_PIXELS(area.m_x_max);
		float y0 = (float) TWIPS_TO_PIXELS(area.m_y_min);
		float y1 = (float) TWIPS_TO_PIXELS(area.m_y_max);

		// A degenerate matrix (a zero scale during a transition) produces
		// NaN here; fall through to the coarse sweep instead.
		if (x1 > x0 && y1 > y0)
		{
			const int steps = 20;
			for (int pass = 0; pass < 2; pass++)
			{
				for (int j = 0; j <= steps; j++)
				{
					for (int i = 0; i <= steps; i++)
					{
						float x = x0 + (x1 - x0) * i / steps;
						float y = y0 + (y1 - y0) * j / steps;
						character* te = hit_test_pixel(x, y);
						if (te == ch
							|| (pass == 1 && is_descendant_of(te, ch)))
						{
							*px = x;
							*py = y;
							return true;
						}
					}
				}
			}
		}

		// Nothing inside the reported bounds answered.  The bound is often
		// much tighter (or offset) compared to the artwork that is really
		// drawn - a button can sit well away from its own bounding box - so
		// sweep the stage.  This runs once per name and the caller caches
		// the answer, so it does not have to be cheap, only bounded.
		{
			const int step = 16;
			int stage_w = get_movie_width();
			int stage_h = get_movie_height();
			for (int y = step; y < stage_h; y += step)
			{
				for (int x = step; x < stage_w; x += step)
				{
					character* te = hit_test_pixel((float) x, (float) y);
					if (te == ch || is_descendant_of(te, ch))
					{
						*px = (float) x;
						*py = (float) y;
						return true;
					}
				}
			}
		}

		return false;
	}

	static bool stage_center_of(character* ch, float* cx, float* cy)
	{
		if (ch == NULL)
		{
			return false;
		}

		rect bound;
		ch->get_bound(&bound);
		if (bound.m_x_max <= bound.m_x_min || bound.m_y_max <= bound.m_y_min)
		{
			return false;
		}

		// get_bound() returns the box in the PARENT's coordinate space, and it
		// already contains this character's own matrix.  Multiplying by our own
		// world matrix would apply that matrix a second time and report
		// positions far outside the stage, so use the parent's world matrix
		// instead (our own matrix for a root-level object).
		matrix m;
		character* parent = ch->get_parent();
		if (parent != NULL)
		{
			m = parent->get_world_matrix();
		}
		else
		{
			m = ch->get_world_matrix();
		}
		point p((bound.m_x_min + bound.m_x_max) / 2,
				(bound.m_y_min + bound.m_y_max) / 2);
		m.transform(&p, p);

		*cx = (float) TWIPS_TO_PIXELS(p.m_x);
		*cy = (float) TWIPS_TO_PIXELS(p.m_y);
		return true;
	}

	static void dump_named(character* ch, int depth)
	{
		if (ch == NULL || depth > 24)
		{
			return;
		}

		if (ch->get_name().size() > 0)
		{
			float cx = 0, cy = 0;
			if (stage_center_of(ch, &cx, &cy))
			{
				sprite_instance* sp = cast_to<sprite_instance>(ch);
				// Report the bounding box in STAGE pixels, so the numbers can
				// be fed straight back into a gesture.  get_bound() is in the
				// parent's space, so use the parent's world matrix (our own
				// matrix for a root-level object) - see stage_center_of().
				matrix pm;
				character* parent = ch->get_parent();
				if (parent != NULL)
				{
					pm = parent->get_world_matrix();
				}
				else
				{
					pm = ch->get_world_matrix();
				}
				point po(0, 0);
				pm.transform(&po, po);
				rect lb;
				ch->get_bound(&lb);
				pm.transform(&lb);
				fprintf(stderr, "[TREE] %*s%s id=%d depth=%d at %.0f,%.0f origin=%.0f,%.0f lb=(%.0f,%.0f)-(%.0f,%.0f) frame=%d\n",
					depth * 2, "", ch->get_name().c_str(), ch->get_id(),
					ch->get_depth(), cx, cy,
					(float) TWIPS_TO_PIXELS(po.m_x), (float) TWIPS_TO_PIXELS(po.m_y),
					(float) TWIPS_TO_PIXELS(lb.m_x_min), (float) TWIPS_TO_PIXELS(lb.m_y_min),
					(float) TWIPS_TO_PIXELS(lb.m_x_max), (float) TWIPS_TO_PIXELS(lb.m_y_max),
					sp ? sp->get_current_frame() : -1);
			}
		}

		sprite_instance* sp = cast_to<sprite_instance>(ch);
		if (sp == NULL)
		{
			return;
		}

		int n = sp->avm2_get_num_children();
		for (int i = 0; i < n; i++)
		{
			dump_named(sp->avm2_get_child_at(i), depth + 1);
		}
	}

	static void trace_outfit(character* ch, int depth)
	// The doll's garment layers, logged only when the frame or the play state
	// changes.  That is quiet enough to leave on while someone plays by hand,
	// and it still shows exactly when a garment is chosen, which frame it
	// lands on, and whether the layer was stopped or left running.
	{
		if (ch == NULL || depth > 24)
		{
			return;
		}

		const char* n = ch->get_name().c_str();
		if (n[0] == 'L' && n[1] == 'V')
		{
			sprite_instance* lsp = cast_to<sprite_instance>(ch);
			if (lsp != NULL)
			{
				// Key the "last seen" state on the id, so a layer that gets
				// rebuilt (a fresh instance on the show screen) reports its
				// first frame again instead of looking unchanged.
				int id = ch->get_id();
				static int s_last_id[32] = { 0 };
				static int s_last_frame[32] = { -1 };
				static int s_last_play[32] = { -1 };
				static int s_count = 0;

				int slot = -1;
				for (int i = 0; i < s_count; i++)
				{
					if (s_last_id[i] == id) { slot = i; break; }
				}
				if (slot < 0 && s_count < 32)
				{
					slot = s_count++;
					s_last_id[slot] = id;
				}

				if (slot >= 0)
				{
					int f = lsp->get_current_frame();
					int p = (int) lsp->get_play_state();
					if (f != s_last_frame[slot] || p != s_last_play[slot])
					{
						s_last_frame[slot] = f;
						s_last_play[slot] = p;
						float cx = 0, cy = 0;
						stage_center_of(ch, &cx, &cy);

						// Who is this layer hanging off?  On the dress-up
						// screen the doll is built by one symbol and on the
						// show screen by another, so the parent says which
						// display is actually supplying the visible outfit.
						character* par = ch->get_parent();
						char pname[40];
						if (par != NULL && par->get_name().size() > 0)
						{
							strncpy(pname, par->get_name().c_str(), 39);
							pname[39] = 0;
						}
						else
						{
							snprintf(pname, sizeof(pname), "(id=%d)", par ? par->get_id() : -1);
						}

						fprintf(stderr,
							"[OUTFIT] %-4s id=%-5d frame=%-3d %-4s parent=%s at %.0f,%.0f depth=%d\n",
							n, id, f, p == (int) character::STOP ? "STOP" : "PLAY",
							pname, cx, cy, ch->get_depth());
						fflush(stderr);
					}
				}
			}
		}

		sprite_instance* sp = cast_to<sprite_instance>(ch);
		if (sp == NULL)
		{
			return;
		}
		int nc = sp->avm2_get_num_children();
		for (int i = 0; i < nc; i++)
		{
			trace_outfit(sp->avm2_get_child_at(i), depth + 1);
		}
	}

	void	root::trace_outfit_layers()
	{
		trace_outfit(m_movie, 0);
	}

	void	root::dump_named_tree()
	{
		dump_named(m_movie, 0);
		fflush(stderr);
	}

	bool	root::get_stage_center(character* ch, float* cx, float* cy)
	{
		return stage_center_of(ch, cx, cy);
	}

	// @@ Is this one necessary?
	character*	root::get_character(int character_id)
	{
		return m_movie->get_character(character_id);
	}

	void	root::set_background_color(const rgba& color)
	{
		m_background_color = color;
	}

	void	root::set_background_alpha(float alpha)
	{
		m_background_color.m_a = iclamp(frnd(alpha * 255.0f), 0, 255);
	}

	float	root::get_background_alpha() const
	{
		return m_background_color.m_a / 255.0f;
	}

	void	root::advance(float delta_time)
	{
		if (m_movie == NULL)
		{
			log_error("root::advance() - m_movie is NULL!\n");
			return;
		}

		if (m_def == NULL)
		{
			log_error("root::advance() - m_def is NULL!\n");
			return;
		}

		// Lock gameswf engine. Video is running in separate thread and
		// it calls gameswf functions from separate thread to set
		// status of netstream object
		gameswf_engine_mutex().lock();

		// Handle the mouse.
		character *te;
		m_movie->get_topmost_mouse_entity(te, PIXELS_TO_TWIPS(m_mouse_x), PIXELS_TO_TWIPS(m_mouse_y));
		m_mouse_button_state.m_topmost_entity = te;

		m_mouse_button_state.m_mouse_button_state_current = (m_mouse_buttons & 1);
		m_mouse_button_state.m_x = m_mouse_x;
		m_mouse_button_state.m_y = m_mouse_y;
		generate_mouse_button_events(&m_mouse_button_state);

		// advance Action script objects (interval timers, xmlsocket, ...)
		m_listener.advance(delta_time);

		m_time_remainder += delta_time;
		if (m_time_remainder >= m_frame_time)
		{

			// mark all as garbage
			m_player->set_as_garbage();

			// this should be called infinitely to not repeat
			// the game situation after restart
			tu_random::next_random();

			if (m_on_event_load_called == false)
			{
				set_flash_vars(m_player->m_flash_vars);
				if (m_def->m_is_avm2)
				{
					const abc_def* adef  = m_def->get_abc();
					if (adef)
					{
						// AVM2 spec: every script's init method runs exactly
						// once, in script order (0..n-1).  We used to run only
						// the LAST script's init here; the other scripts were
						// initialized lazily, but only on getlex /
						// findpropstrict -- a plain `getproperty __showMenu`
						// never triggered it.  Script-level functions defined
						// by earlier scripts (the shell's __showMenu /
						// __moreGame / __showMid / readyGame) then never
						// appeared on the global object, the event-handler
						// registration failed with GPF_MISS and the game menu
						// could not be shown.
						int nscripts = adef->m_script.size();
						for (int s = 0; s < nscripts; s++)
						{
							script_info* si = adef->m_script[s].get();
							if (si == NULL || si->m_executed)
							{
								continue;
							}
							as_function* script_func =
								adef->get_method(si->m_init);
							if (script_func == NULL)
							{
								continue;
							}
							si->m_executed = true;
							as_environment env(m_player.get_ptr());
							// Script init runs with the global object as its
							// 'this' (ECMA-262: [[Scope]] of a script starts
							// with the global).  Passing the root movie here
							// made the prologue `getlocal_0; pushscope` place
							// the movie where the script expects _global --
							// unqualified global lookups then saw root members.
							gameswf::call_method(script_func,
								&env, as_value(m_player->get_global()), 0, 0);
						}
					}
				}
				
				// Must do loading events.  For child sprites this is
				// done by the dlist, but root movies don't get added
				// to a dlist, so we do it here.
				m_on_event_load_called = true;
				m_movie->on_event(event_id::LOAD);
			}

			if (m_player->get_force_realtime_framerate() == true)
			{
				while (m_time_remainder >= m_frame_time)
				{
					m_movie->advance(m_frame_time);
					m_time_remainder -= m_frame_time;
				}
			}
			else
			{
				m_movie->advance(delta_time);
				m_time_remainder = fmod(m_time_remainder - m_frame_time, m_frame_time);
			}

			m_player->clear_garbage();

			// Handle mouse dragging — apply after frame advance so drag position
			// takes precedence over PlaceObject / frame-script matrix resets.
			do_mouse_drag();

			// Dispatch Event.ENTER_FRAME to plain as_objects that registered a
			// listener but are not characters (AS3 `new Sprite()` is an
			// as_object, not a character, so nothing advances it).  TweenLite
			// parks its whole ticker on such a sprite, so without this every
			// tween in a game freezes.  Characters get the event from
			// sprite_instance::advance() and are never in this list.
			if (m_player->m_frame_listeners.size() > 0)
			{
				static int s_efl_diag = -1;
				static int s_efl_count = 0;
				if (s_efl_diag < 0)
				{
					const char* e = getenv("GAMESWF_EFRAMEDIAG");
					s_efl_diag = (e != NULL && e[0] == '1') ? 1 : 0;
				}
				s_efl_count++;
				as_environment env(m_player.get_ptr());
				array<gc_ptr<as_object> > listeners = m_player->m_frame_listeners;
				for (int i = 0; i < listeners.size(); i++)
				{
					as_object* obj = listeners[i].get_ptr();
					if (obj == NULL)
					{
						continue;
					}
					// Lazy prune: the listener array is gone when the game
					// removed its last enterFrame listener (removeEventListener
					// clears the member and unregisters, but a game may also
					// just drop the object).
					as_value lv;
					if (!obj->get_member("__events_enterFrame", &lv) || !lv.is_object())
					{
						m_player->remove_frame_listener(obj);
						i--;
						continue;
					}
					gc_ptr<as_object> evt = new as_object(m_player.get_ptr());
					evt->set_member("type", as_value("enterFrame"));
					evt->set_member("bubbles", as_value(false));
					evt->set_member("target", as_value(obj));
					avm2_dispatch_event(obj, as_value(evt.get_ptr()), &env);
					if (s_efl_diag && (s_efl_count % 60) == 0)
					{
						fprintf(stderr, "[EFRAME] dispatched to %d object(s) tick=%d\n",
							(int) listeners.size(), s_efl_count);
						fflush(stderr);
					}
				}
			}
		}

		gameswf_engine_mutex().unlock();
	}

	// 0-based!!
	void	root::goto_frame(int target_frame_number) { m_movie->goto_frame(target_frame_number); }

	bool	root::has_looped() const { return m_movie->has_looped(); }

	void	root::display()
	{
		if (m_movie == NULL)
		{
			log_error("root::display() - m_movie is NULL!\n");
			return;
		}
		if (m_movie->get_visible() == false)
		{
			// Don't display.
			return;
		}

		gameswf::render::begin_display(
			m_background_color,
			m_viewport_x0, m_viewport_y0,
			m_viewport_width, m_viewport_height,
			m_def->m_frame_size.m_x_min, m_def->m_frame_size.m_x_max,
			m_def->m_frame_size.m_y_min, m_def->m_frame_size.m_y_max);

		m_movie->display();

		gameswf::render::end_display();
	}

	bool	root::goto_labeled_frame(const char* label)
	{
		int	target_frame = -1;
		if (m_def->get_labeled_frame(label, &target_frame))
		{
			goto_frame(target_frame);
			return true;
		}
		else
		{
			IF_VERBOSE_ACTION(
				log_error("error: movie_impl::goto_labeled_frame('%s') unknown label\n", label));
			return false;
		}
	}

	void	root::set_play_state(character::play_state s) { m_movie->set_play_state(s); }
	character::play_state	root::get_play_state() const { return m_movie->get_play_state(); }

	void	root::set_variable(const char* path_to_var, const char* new_value)
	{
		m_movie->set_variable(path_to_var, new_value);
	}

	void	root::set_variable(const char* path_to_var, const wchar_t* new_value)
	{
		m_movie->set_variable(path_to_var, new_value);
	}

	const char*	root::get_variable(const char* path_to_var) const
	{
		return m_movie->get_variable(path_to_var);
	}

	// For ActionScript interfacing convenience.
	const char*	root::call_method(const char* method_name, const char* method_arg_fmt, ...)
	{
		assert(m_movie != NULL);

		va_list	args;
		va_start(args, method_arg_fmt);
		const char*	result = m_movie->call_method_args(method_name, method_arg_fmt, args);
		va_end(args);

		return result;
	}

	const char*	root::call_method_args(const char* method_name, const char* method_arg_fmt, va_list args)
	{
		assert(m_movie != NULL);
		return m_movie->call_method_args(method_name, method_arg_fmt, args);
	}
	
	tu_string root::call_method(const char* method_name, as_value * arguments, int argument_count )
	{
		assert(m_movie != NULL);
		return m_movie->call_method(method_name, arguments, argument_count);
	}

	void	root::set_visible(bool visible) { m_movie->set_visible(visible); }
	bool	root::get_visible() const { return m_movie->get_visible(); }

	void* root::get_userdata() { return m_userdata; }
	void root::set_userdata(void * ud ) { m_userdata = ud;  }

	void	root::attach_display_callback(const char* path_to_object, void (*callback)(void* user_ptr), void* user_ptr)
	{
		m_movie->attach_display_callback(path_to_object, callback, user_ptr);
	}

	// parse Flash vars and create _root variables
	// Flash vars come in the format:
	// myvar1=value1,myvar2=value2,myvar3=value3,...
	void root::set_flash_vars(const tu_string& vars)
	{
		for (const char* word = vars.c_str(); *word; )
		{
			bool nullDel = false;
			const char* delimiter = strchr(word, '=');
			if (delimiter == NULL)
			{
				// no value
				return;
			}
			tu_string varname(word, int(delimiter - word));

			word = delimiter + 1;
			delimiter = strchr(word, ',');
			if (delimiter == NULL)
			{
				delimiter = vars.c_str() + vars.size();
				nullDel = true;
			}
			tu_string value(word, int(delimiter - word));
		
			get_root_movie()->set_member(varname, value.c_str());

			if (nullDel)
				return;

			word = delimiter + 1;
			
		}

	}

	bool root::is_avm2() const
	{
		// This can be reached with a torn-down root: player::~player()
		// clears m_current_root before walking the heap.
		if (this == NULL || m_def == NULL)
		{
			return false;
		}
		return m_def->m_is_avm2;
	}

}
