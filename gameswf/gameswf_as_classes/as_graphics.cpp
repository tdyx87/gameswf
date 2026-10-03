// as_graphics.cpp	-- gameswf AVM2 Graphics implementation

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// AS3 Graphics class - delegates all drawing to the canvas object
// which inherits shape_character_def and handles actual rendering.

#include "gameswf/gameswf_as_classes/as_graphics.h"
#include "gameswf/gameswf_sprite.h"
#include "gameswf/gameswf_canvas.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace gameswf
{

	// Helper: convert AS3 color value to rgba
	static rgba color_from_as3_value(const as_value& val)
	{
		Uint32 color_val = (Uint32)val.to_number();
		// AS3 color format: 0xRRGGBB (alpha defaults to 255)
		Uint8 r = (color_val >> 16) & 0xFF;
		Uint8 g = (color_val >> 8) & 0xFF;
		Uint8 b = color_val & 0xFF;
		Uint8 a = 255;
		if (color_val > 0xFFFFFF)
		{
			a = (color_val >> 24) & 0xFF;
		}
		return rgba(r, g, b, a);
	}

	// Constructor
	as_graphics::as_graphics(player* player, sprite_instance* target_sprite) :
		as_object(player),
		m_current_x(0),
		m_current_y(0),
		m_in_path(false)
	{
		m_target_sprite = target_sprite;

		// Register AS3 methods
		builtin_member("beginFill", as_graphics_beginFill);
		builtin_member("endFill", as_graphics_endFill);
		builtin_member("lineStyle", as_graphics_lineStyle);
		builtin_member("moveTo", as_graphics_moveTo);
		builtin_member("lineTo", as_graphics_lineTo);
		builtin_member("drawRect", as_graphics_drawRect);
		builtin_member("drawCircle", as_graphics_drawCircle);
		builtin_member("drawRoundRect", as_graphics_drawRoundRect);
		builtin_member("clear", as_graphics_clear);
		builtin_member("curveTo", as_graphics_curveTo);
		builtin_member("drawEllipse", as_graphics_drawEllipse);
	}

	// Get the canvas for the target sprite, or NULL if no target
	canvas* as_graphics::get_canvas()
	{
		sprite_instance* sprite = m_target_sprite.get_ptr();
		if (sprite == NULL) return NULL;
		return sprite->get_canvas();
	}

	// ===== Drawing methods that delegate to canvas =====

	// beginFill(color:uint, alpha:Number = 1.0):void
	void as_graphics::beginFill(const rgba& color)
	{
		m_fill_style.m_active = true;
		m_fill_style.m_color = color;
		m_in_path = true;

		canvas* cv = get_canvas();
		if (cv) cv->begin_fill(color);
	}

	// endFill():void
	void as_graphics::endFill()
	{
		m_fill_style.m_active = false;
		m_in_path = false;

		canvas* cv = get_canvas();
		if (cv) cv->end_fill();
	}

	// lineStyle(thickness:Number, color:uint = 0, alpha:Number = 1.0):void
	void as_graphics::lineStyle(float thickness, const rgba& color)
	{
		m_line_style.m_active = true;
		m_line_style.m_thickness = thickness;
		m_line_style.m_color = color;

		canvas* cv = get_canvas();
		if (cv)
		{
			// thickness is in pixels, canvas expects TWIPS
			cv->set_line_style((Uint16)(thickness * 20.0f), color);
		}
	}

	// moveTo(x:Number, y:Number):void
	void as_graphics::moveTo(float x, float y)
	{
		m_current_x = x;
		m_current_y = y;
		m_in_path = true;

		canvas* cv = get_canvas();
		if (cv)
		{
			cv->move_to(x * 20.0f, y * 20.0f);  // pixels -> TWIPS
		}
	}

	// lineTo(x:Number, y:Number):void
	void as_graphics::lineTo(float x, float y)
	{
		m_current_x = x;
		m_current_y = y;

		canvas* cv = get_canvas();
		if (cv)
		{
			cv->line_to(x * 20.0f, y * 20.0f);  // pixels -> TWIPS
		}
	}

	// curveTo(controlX:Number, controlY:Number, anchorX:Number, anchorY:Number):void
	void as_graphics::curveTo(float controlX, float controlY, float anchorX, float anchorY)
	{
		m_current_x = anchorX;
		m_current_y = anchorY;

		canvas* cv = get_canvas();
		if (cv)
		{
			cv->curve_to(
				controlX * 20.0f, controlY * 20.0f,
				anchorX * 20.0f, anchorY * 20.0f);
		}
	}

	// drawRect(x:Number, y:Number, width:Number, height:Number):void
	void as_graphics::drawRect(float x, float y, float width, float height)
	{
		canvas* cv = get_canvas();
		if (cv == NULL) return;

		// Apply fill style if active
		if (m_fill_style.m_active)
		{
			cv->begin_fill(m_fill_style.m_color);
		}
		// Apply line style if active
		if (m_line_style.m_active)
		{
			cv->set_line_style((Uint16)(m_line_style.m_thickness * 20.0f), m_line_style.m_color);
		}

		// Convert pixels to TWIPS and draw rectangle
		float x0 = x * 20.0f;
		float y0 = y * 20.0f;
		float x1 = (x + width) * 20.0f;
		float y1 = (y + height) * 20.0f;

		cv->move_to(x0, y0);
		cv->line_to(x1, y0);
		cv->line_to(x1, y1);
		cv->line_to(x0, y1);
		cv->line_to(x0, y0);  // close path

		if (m_fill_style.m_active)
		{
			cv->end_fill();
		}

		m_current_x = x + width;
		m_current_y = y + height;
	}

	// drawCircle(x:Number, y:Number, radius:Number):void
	void as_graphics::drawCircle(float x, float y, float radius)
	{
		canvas* cv = get_canvas();
		if (cv == NULL) return;

		if (m_fill_style.m_active)
		{
			cv->begin_fill(m_fill_style.m_color);
		}
		if (m_line_style.m_active)
		{
			cv->set_line_style((Uint16)(m_line_style.m_thickness * 20.0f), m_line_style.m_color);
		}

		// Convert to TWIPS
		float cx_t = x * 20.0f;
		float cy_t = y * 20.0f;
		float r_t = radius * 20.0f;

		// Approximate circle with 8 quadratic Bezier curves
		const int segments = 8;
		float angle_step = (2.0f * (float)M_PI) / segments;
		// Control point distance for quadratic Bezier approximation of circle arc
		float kappa = 0.5522847498f;

		// Start at rightmost point
		cv->move_to(cx_t + r_t, cy_t);

		for (int i = 0; i < segments; i++)
		{
			float angle0 = angle_step * i;
			float angle1 = angle_step * (i + 1);
			float mid = (angle0 + angle1) * 0.5f;

			// Control point: on the tangent at the midpoint of the arc
			float cpx = cx_t + cosf(mid) * r_t / cosf(angle_step * 0.5f);
			float cpy = cy_t + sinf(mid) * r_t / cosf(angle_step * 0.5f);

			// Anchor point: on the circle at angle1
			float ax = cx_t + cosf(angle1) * r_t;
			float ay = cy_t + sinf(angle1) * r_t;

			cv->curve_to(cpx, cpy, ax, ay);
		}

		if (m_fill_style.m_active)
		{
			cv->end_fill();
		}

		m_current_x = x + radius;
		m_current_y = y;
	}

	// drawEllipse(x:Number, y:Number, width:Number, height:Number):void
	void as_graphics::drawEllipse(float x, float y, float width, float height)
	{
		canvas* cv = get_canvas();
		if (cv == NULL) return;

		if (m_fill_style.m_active)
		{
			cv->begin_fill(m_fill_style.m_color);
		}
		if (m_line_style.m_active)
		{
			cv->set_line_style((Uint16)(m_line_style.m_thickness * 20.0f), m_line_style.m_color);
		}

		// Convert to TWIPS
		float cx_t = (x + width * 0.5f) * 20.0f;
		float cy_t = (y + height * 0.5f) * 20.0f;
		float rx_t = (width * 0.5f) * 20.0f;
		float ry_t = (height * 0.5f) * 20.0f;

		// Approximate ellipse with 8 quadratic Bezier curves
		const int segments = 8;
		float angle_step = (2.0f * (float)M_PI) / segments;

		// Start at rightmost point
		cv->move_to(cx_t + rx_t, cy_t);

		for (int i = 0; i < segments; i++)
		{
			float angle0 = angle_step * i;
			float angle1 = angle_step * (i + 1);
			float mid = (angle0 + angle1) * 0.5f;

			// Control point for ellipse
			float cpx = cx_t + cosf(mid) * rx_t / cosf(angle_step * 0.5f);
			float cpy = cy_t + sinf(mid) * ry_t / cosf(angle_step * 0.5f);

			// Anchor point
			float ax = cx_t + cosf(angle1) * rx_t;
			float ay = cy_t + sinf(angle1) * ry_t;

			cv->curve_to(cpx, cpy, ax, ay);
		}

		if (m_fill_style.m_active)
		{
			cv->end_fill();
		}

		m_current_x = x + width;
		m_current_y = y + height;
	}

	// drawRoundRect(x, y, w, h, ellipseWidth, ellipseHeight):void
	void as_graphics::drawRoundRect(float x, float y, float width, float height,
		float ellipseWidth, float ellipseHeight)
	{
		canvas* cv = get_canvas();
		if (cv == NULL) return;

		if (m_fill_style.m_active)
		{
			cv->begin_fill(m_fill_style.m_color);
		}
		if (m_line_style.m_active)
		{
			cv->set_line_style((Uint16)(m_line_style.m_thickness * 20.0f), m_line_style.m_color);
		}

		// Convert to TWIPS
		float x0 = x * 20.0f;
		float y0 = y * 20.0f;
		float x1 = (x + width) * 20.0f;
		float y1 = (y + height) * 20.0f;
		float ew = (ellipseWidth * 0.5f) * 20.0f;  // corner radius X
		float eh = (ellipseHeight * 0.5f) * 20.0f; // corner radius Y

		// Clamp corner radii to half the rect dimensions
		float max_ew = (x1 - x0) * 0.5f;
		float max_eh = (y1 - y0) * 0.5f;
		if (ew > max_ew) ew = max_ew;
		if (eh > max_eh) eh = max_eh;

		// Approximate rounded corners with quadratic Bezier curves
		// canvas::curve_to(cx, cy, ax, ay) takes 4 params: control point + anchor point
		// Each corner uses two quadratic curves to approximate a quarter circle
		float kappa = 0.5522847498f;

		// Start at top-left + corner offset
		cv->move_to(x0 + ew, y0);

		// Top edge
		cv->line_to(x1 - ew, y0);
		// Top-right corner: two quadratic curves
		cv->curve_to(x1 - ew + ew * kappa, y0, x1, y0 + eh * (1 - kappa));
		cv->curve_to(x1, y0 + eh * kappa, x1, y0 + eh);
		// Right edge
		cv->line_to(x1, y1 - eh);
		// Bottom-right corner
		cv->curve_to(x1, y1 - eh + eh * kappa, x1 - ew * (1 - kappa), y1);
		cv->curve_to(x1 - ew * kappa, y1, x1 - ew, y1);
		// Bottom edge
		cv->line_to(x0 + ew, y1);
		// Bottom-left corner
		cv->curve_to(x0 + ew * (1 - kappa), y1, x0, y1 - eh * kappa);
		cv->curve_to(x0, y1 - eh * (1 - kappa), x0, y1 - eh);
		// Left edge
		cv->line_to(x0, y0 + eh);
		// Top-left corner
		cv->curve_to(x0, y0 + eh * (1 - kappa), x0 + ew * (1 - kappa), y0);
		cv->curve_to(x0 + ew * kappa, y0, x0 + ew, y0);

		if (m_fill_style.m_active)
		{
			cv->end_fill();
		}

		m_current_x = x + width;
		m_current_y = y + height;
	}

	// clear():void
	void as_graphics::clear()
	{
		m_fill_style.m_active = false;
		m_line_style.m_active = false;
		m_current_x = 0;
		m_current_y = 0;
		m_in_path = false;

		// Remove the canvas display object from the target sprite
		sprite_instance* sprite = m_target_sprite.get_ptr();
		if (sprite && sprite->m_canvas != NULL)
		{
			sprite->remove_display_object(sprite->m_canvas.get_ptr());
			sprite->m_canvas = NULL;
		}
	}

	void as_graphics::clear_drawing_commands()
	{
		// Drawing commands are directly applied to canvas, no separate queue needed
	}

	void as_graphics::execute_drawing_commands()
	{
		// Drawing commands are directly applied to canvas, no separate queue needed
	}

	// ===== AS3 Method wrappers (fn_call interface) =====

	void as_graphics_beginFill(const fn_call& fn)
	{
		as_graphics* gfx = cast_to<as_graphics>(fn.this_ptr);
		if (gfx == NULL) return;
		if (fn.nargs < 1) return;

		rgba color = color_from_as3_value(fn.arg(0));
		if (fn.nargs >= 2)
		{
			float alpha = fn.arg(1).to_float();
			color.m_a = (Uint8)(alpha * 255.0f);
		}
		gfx->beginFill(color);
	}

	void as_graphics_endFill(const fn_call& fn)
	{
		as_graphics* gfx = cast_to<as_graphics>(fn.this_ptr);
		if (gfx == NULL) return;
		gfx->endFill();
	}

	void as_graphics_lineStyle(const fn_call& fn)
	{
		as_graphics* gfx = cast_to<as_graphics>(fn.this_ptr);
		if (gfx == NULL) return;
		if (fn.nargs < 1) return;

		float thickness = fn.arg(0).to_float();
		rgba color(0, 0, 0, 255);
		if (fn.nargs >= 2) color = color_from_as3_value(fn.arg(1));
		if (fn.nargs >= 3)
		{
			float alpha = fn.arg(2).to_float();
			color.m_a = (Uint8)(alpha * 255.0f);
		}
		gfx->lineStyle(thickness, color);
	}

	void as_graphics_moveTo(const fn_call& fn)
	{
		as_graphics* gfx = cast_to<as_graphics>(fn.this_ptr);
		if (gfx == NULL) return;
		if (fn.nargs < 2) return;
		gfx->moveTo(fn.arg(0).to_float(), fn.arg(1).to_float());
	}

	void as_graphics_lineTo(const fn_call& fn)
	{
		as_graphics* gfx = cast_to<as_graphics>(fn.this_ptr);
		if (gfx == NULL) return;
		if (fn.nargs < 2) return;
		gfx->lineTo(fn.arg(0).to_float(), fn.arg(1).to_float());
	}

	void as_graphics_curveTo(const fn_call& fn)
	{
		as_graphics* gfx = cast_to<as_graphics>(fn.this_ptr);
		if (gfx == NULL) return;
		if (fn.nargs < 4) return;
		gfx->curveTo(
			fn.arg(0).to_float(), fn.arg(1).to_float(),
			fn.arg(2).to_float(), fn.arg(3).to_float());
	}

	void as_graphics_drawRect(const fn_call& fn)
	{
		as_graphics* gfx = cast_to<as_graphics>(fn.this_ptr);
		if (gfx == NULL) return;
		if (fn.nargs < 4) return;
		gfx->drawRect(
			fn.arg(0).to_float(), fn.arg(1).to_float(),
			fn.arg(2).to_float(), fn.arg(3).to_float());
	}

	void as_graphics_drawCircle(const fn_call& fn)
	{
		as_graphics* gfx = cast_to<as_graphics>(fn.this_ptr);
		if (gfx == NULL) return;
		if (fn.nargs < 3) return;
		gfx->drawCircle(
			fn.arg(0).to_float(), fn.arg(1).to_float(),
			fn.arg(2).to_float());
	}

	void as_graphics_drawEllipse(const fn_call& fn)
	{
		as_graphics* gfx = cast_to<as_graphics>(fn.this_ptr);
		if (gfx == NULL) return;
		if (fn.nargs < 4) return;
		gfx->drawEllipse(
			fn.arg(0).to_float(), fn.arg(1).to_float(),
			fn.arg(2).to_float(), fn.arg(3).to_float());
	}

	void as_graphics_drawRoundRect(const fn_call& fn)
	{
		as_graphics* gfx = cast_to<as_graphics>(fn.this_ptr);
		if (gfx == NULL) return;
		if (fn.nargs < 6) return;
		gfx->drawRoundRect(
			fn.arg(0).to_float(), fn.arg(1).to_float(),
			fn.arg(2).to_float(), fn.arg(3).to_float(),
			fn.arg(4).to_float(), fn.arg(5).to_float());
	}

	void as_graphics_clear(const fn_call& fn)
	{
		as_graphics* gfx = cast_to<as_graphics>(fn.this_ptr);
		if (gfx == NULL) return;
		gfx->clear();
	}

	// Global constructor
	void as_global_graphics_ctor(const fn_call& fn)
	{
		gc_ptr<as_graphics> obj = new as_graphics(fn.get_player(), NULL);
		fn.result->set_as_object(obj.get_ptr());
	}

} // end namespace gameswf
