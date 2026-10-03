// as_graphics.h	-- gameswf AVM2 Graphics implementation

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

#ifndef GAMESWF_AS_GRAPHICS_H
#define GAMESWF_AS_GRAPHICS_H

#include "gameswf/gameswf_action.h"
#include "gameswf/gameswf_character.h"
#include "gameswf/gameswf_shape.h"
#include "base/image.h"

namespace gameswf
{

	struct sprite_instance;
	struct canvas;

	// Graphics class for AVM2 (AS3)
	// Handles vector drawing commands: beginFill, lineTo, drawRect, etc.
	// All drawing is delegated to the canvas object (shape_character_def subclass)
	// which handles the actual rendering through the gameswf shape pipeline.
	struct as_graphics : public as_object
	{
		enum { m_class_id = AS_GRAPHICS };
		virtual bool is(int class_id) const
		{
			if (m_class_id == class_id) return true;
			else return as_object::is(class_id);
		}

		as_graphics(player* player, sprite_instance* target_sprite);

		// Drawing state
		struct fill_style
		{
			bool m_active;
			rgba m_color;
			fill_style() : m_active(false), m_color(0, 0, 0, 255) {}
		};

		struct line_style
		{
			bool m_active;
			float m_thickness;
			rgba m_color;
			line_style() : m_active(false), m_thickness(0), m_color(0, 0, 0, 255) {}
		};

		fill_style m_fill_style;
		line_style m_line_style;
		float m_current_x;
		float m_current_y;
		bool m_in_path;

		// Target sprite to draw into
		weak_ptr<sprite_instance> m_target_sprite;

		// Get or create the canvas for the target sprite
		canvas* get_canvas();

		// AS3 Drawing methods
		void beginFill(const rgba& color);
		void endFill();
		void lineStyle(float thickness, const rgba& color);
		void moveTo(float x, float y);
		void lineTo(float x, float y);
		void curveTo(float controlX, float controlY, float anchorX, float anchorY);
		void drawRect(float x, float y, float width, float height);
		void drawCircle(float x, float y, float radius);
		void drawEllipse(float x, float y, float width, float height);
		void drawRoundRect(float x, float y, float width, float height, float ellipseWidth, float ellipseHeight);
		void clear();

		// Legacy (no-op, drawing is immediate)
		void clear_drawing_commands();
		void execute_drawing_commands();
	};

	void as_global_graphics_ctor(const fn_call& fn);

	void as_graphics_beginFill(const fn_call& fn);
	void as_graphics_endFill(const fn_call& fn);
	void as_graphics_lineStyle(const fn_call& fn);
	void as_graphics_moveTo(const fn_call& fn);
	void as_graphics_lineTo(const fn_call& fn);
	void as_graphics_curveTo(const fn_call& fn);
	void as_graphics_drawRect(const fn_call& fn);
	void as_graphics_drawCircle(const fn_call& fn);
	void as_graphics_drawEllipse(const fn_call& fn);
	void as_graphics_drawRoundRect(const fn_call& fn);
	void as_graphics_clear(const fn_call& fn);

} // end namespace gameswf

#endif // GAMESWF_AS_GRAPHICS_H
