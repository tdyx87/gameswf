// gameswf_styles.cpp	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Fill and line style types.


#include "gameswf/gameswf_styles.h"
#include "gameswf/gameswf_impl.h"
#include "gameswf/gameswf_log.h"
#include "gameswf/gameswf_render.h"
#include "gameswf/gameswf_stream.h"
#include "gameswf/gameswf_movie_def.h"


namespace gameswf
{
	//
	// gradient_record
	//

	gradient_record::gradient_record() :
		m_ratio(0)
	{
	}


	void	gradient_record::read(stream* in, int tag_type)
	{
		m_ratio = in->read_u8();
		m_color.read(in, tag_type);
	}


	//
	// fill_style
	//


	fill_style::fill_style() :
		m_type(0),
		m_focal(0.f),
		m_gradient_bitmap_info(0),
		m_bitmap_character(0)
	{
		assert(m_gradients.size() == 0);
	}


	fill_style::~fill_style()
	{
	}

	void	fill_style::read(stream* in, int tag_type, movie_definition_sub* md)
	{
		m_type = in->read_u8();

		IF_VERBOSE_PARSE(log_msg("  fill_style read type = 0x%X\n", m_type));

		if (m_type == 0x00)
		{
			// 0x00: solid fill
			if (tag_type <= 22) {
				m_color.read_rgb(in);
			} else {
				m_color.read_rgba(in);
			}

			IF_VERBOSE_PARSE(log_msg("  color: ");
					 m_color.print());
		}
		else if (m_type == 0x10 || m_type == 0x12)
		{
			// 0x10: linear gradient fill
			// 0x12: radial gradient fill

			matrix	input_matrix;
			input_matrix.read(in);

			if (m_type == 0x10)
			{
				m_gradient_matrix.set_identity();
				m_gradient_matrix.concatenate_translation(128.f, 0.f);
				m_gradient_matrix.concatenate_scale(1.0f / 128.0f);
			}
			else 
			{
				m_gradient_matrix.set_identity();
				m_gradient_matrix.concatenate_translation(32.f, 32.f);
				m_gradient_matrix.concatenate_scale(1.0f / 512.0f);
			}


			matrix	m;
			m.set_inverse(input_matrix);
			m_gradient_matrix.concatenate(m);
				
			// GRADIENT

			// 'spread_mode' and 'interpolation_mode' are not used for now
			// and so they are commented out
//			in->align();
//			int spread_mode = in->read_uint(2);
//			int interpolation_mode = in->read_uint(2);
//			int num_gradients = in->read_uint(4);

			// SWF 8 and later supports up to 15 gradient control points
			int num_gradients = in->read_u8() & 0x0F;

			m_gradients.resize(num_gradients);
			for (int i = 0; i < num_gradients; i++)
			{
				m_gradients[i].read(in, tag_type);
			}

			IF_VERBOSE_PARSE(log_msg("  gradients: num_gradients = %d\n", num_gradients));

			// @@ hack.
			if (num_gradients > 0)
			{
				m_color = m_gradients[0].m_color;
			}

			if (md->get_create_bitmaps() == DO_LOAD_BITMAPS)
			{
				m_gradient_bitmap_info = create_gradient_bitmap();
			}
			else
			{
				m_gradient_bitmap_info = render::create_bitmap_info_empty();
			}

			// Make sure our movie_def_impl knows about this bitmap.
			md->add_bitmap_info(m_gradient_bitmap_info.get_ptr());
		}
		else
		if (m_type == 0x13)
		{
			// 0x13: focal radial gradient (Flash 8, DefineShape4).
			// Store the gradient records and build a gradient bitmap the same
			// way we do for a plain radial fill, offset by the focal point.
			// Previously the records were parsed and thrown away, so apply()
			// pushed nothing into the renderer and the shape was painted with
			// whatever fill style was still active -- i.e. solid black.
			assert( tag_type == 83 );

			matrix	input_matrix;
			input_matrix.read(in);

			m_gradient_matrix.set_identity();
			m_gradient_matrix.concatenate_translation(32.f, 32.f);
			m_gradient_matrix.concatenate_scale(1.0f / 512.0f);

			matrix	m;
			m.set_inverse(input_matrix);
			m_gradient_matrix.concatenate(m);

			// SWF 8 and later supports up to 15 gradient control points
			int num_gradients = in->read_u8() & 0x0F;
			if (num_gradients < 1 || num_gradients > 15)
			{
				log_error("Invalid focal-gradient record count: %d\n", num_gradients);
				num_gradients = 1;
			}

			m_gradients.resize(num_gradients);
			for (int i = 0; i < num_gradients; i++)
			{
				m_gradients[i].read(in, tag_type);
			}

			// FIXED8 focal point, -1.0 .. 1.0 along the gradient's x axis
			m_focal = (float) (Sint16) in->read_u16() / 256.0f;
			if (m_focal < -1.0f) m_focal = -1.0f;
			if (m_focal > 1.0f) m_focal = 1.0f;

			m_color = m_gradients[0].m_color;

			if (md->get_create_bitmaps() == DO_LOAD_BITMAPS)
			{
				m_gradient_bitmap_info = create_gradient_bitmap();
			}
			else
			{
				m_gradient_bitmap_info = render::create_bitmap_info_empty();
			}

			// Make sure our movie_def_impl knows about this bitmap.
			md->add_bitmap_info(m_gradient_bitmap_info.get_ptr());

			IF_VERBOSE_PARSE(log_msg("  focal gradient: num_gradients = %d focal = %.3f\n",
				num_gradients, m_focal));
		}
		else
		if (m_type >= 0x40 && m_type <= 0x43)
		{
			// 0x40: tiled bitmap fill
			// 0x41: clipped bitmap fill
			// 0x42: non-smoothed repeating bitmap, Flash 8, TODO
			// 0x43: non-smoothed clipped bitmap, Flash 8, TODO

			int	bitmap_char_id = in->read_u16();
			IF_VERBOSE_PARSE(log_msg("  bitmap_char = %d\n", bitmap_char_id));

			// Look up the bitmap character.
			m_bitmap_character = md->get_bitmap_character(bitmap_char_id);

			matrix	m;
			m.read(in);

			// For some reason, it looks like they store the inverse of the
			// TWIPS-to-texcoords matrix.
			m_bitmap_matrix.set_inverse(m);
			IF_VERBOSE_PARSE(m_bitmap_matrix.print());
		}
		else
		{
			assert(0);
		}
	}


	rgba	fill_style::sample_gradient(int ratio) const
	// Return the color at the specified ratio into our gradient.
	// Ratio is in [0, 255].
	{
		assert(ratio >= 0 && ratio <= 255);
		assert(m_type == 0x10 || m_type == 0x12 || m_type == 0x13);
		assert(m_gradients.size() > 0);

		if (ratio < m_gradients[0].m_ratio)
			return m_gradients[0].m_color;		
		
		for (int i = 1; i < m_gradients.size(); i++)
		{
			if (m_gradients[i].m_ratio >= ratio)
			{
				const gradient_record& gr0 = m_gradients[i - 1];
				const gradient_record& gr1 = m_gradients[i];
				float	f = 0.0f;
				if (gr0.m_ratio != gr1.m_ratio)
				{
					f = (ratio - gr0.m_ratio) / float(gr1.m_ratio - gr0.m_ratio);
				}

				rgba	result;
				result.set_lerp(m_gradients[i - 1].m_color, m_gradients[i].m_color, f);
				return result;
			}
		}
		return m_gradients.back().m_color;
	}

	bitmap_info*	fill_style::create_gradient_bitmap() const
	// Make a bitmap_info* corresponding to our gradient.
	// We can use this to set the gradient fill style.
	{
		assert(m_type == 0x10 || m_type == 0x12 || m_type == 0x13);

		image::rgba*	im = NULL;

		if (m_type == 0x10)
		{
			// Linear gradient.
			im = image::create_rgba(256, 1);

			for (int i = 0; i < im->m_width; i++)
			{
				rgba	sample = sample_gradient(i);
				im->set_pixel(i, 0, sample.m_r, sample.m_g, sample.m_b, sample.m_a);
			}
		}
		else if (m_type == 0x12 || m_type == 0x13)
		{
			// Radial gradient; type 0x13 additionally has a focal point, i.e.
			// the centre of the ramp is pulled off-center by m_focal.
			im = image::create_rgba(64, 64);

			float	radius = (im->m_height - 1) / 2.0f;
			float	focal_x = (m_type == 0x13) ? m_focal : 0.0f;

			for (int j = 0; j < im->m_height; j++)
			{
				for (int i = 0; i < im->m_width; i++)
				{
					float	y = (j - radius) / radius;
					float	x = (i - radius) / radius;
					int	ratio;

					if (focal_x == 0.0f)
					{
						ratio = (int) floorf(255.5f * sqrt(x * x + y * y));
					}
					else
					{
						// Walk the ray from the focal point P=(focal_x,0)
						// through the sample; find where it leaves the unit
						// circle (parameter s) and ramp by 1/s, so the value
						// is 0 at P and 1 on the rim.
						float	dx = x - focal_x;
						float	dy = y;
						float	dd = dx * dx + dy * dy;
						if (dd < 1e-12f)
						{
							ratio = 0;
						}
						else
						{
							float	pd = focal_x * dx;
							float	disc = pd * pd - dd * (focal_x * focal_x - 1.0f);
							if (disc < 0.0f)
							{
								disc = 0.0f;
							}
							float	s = (-pd + sqrtf(disc)) / dd;
							if (s <= 1e-6f)
							{
								ratio = 255;
							}
							else
							{
								ratio = (int) floorf(255.5f / s);
							}
						}
					}

					if (ratio > 255)
					{
						ratio = 255;
					}
					if (ratio < 0)
					{
						ratio = 0;
					}
					rgba	sample = sample_gradient( ratio );
					im->set_pixel(i, j, sample.m_r, sample.m_g, sample.m_b, sample.m_a);
				}
			}
		}

		// Diagnostic: log the generated gradient's center/edge colours so we
		// can tell whether a pale gradient is a generation bug or a later
		// (texture/cxform) problem.
		{
			static int s_gd = -1;
			if (s_gd < 0)
			{
				const char* e = getenv("GAMESWF_GRADDIAG");
				s_gd = (e && e[0] == '1') ? 1 : 0;
			}
			if (s_gd && (m_type == 0x12 || m_type == 0x13))
			{
				rgba c0 = sample_gradient(0);
				rgba c128 = sample_gradient(128);
				rgba c255 = sample_gradient(255);
				fprintf(stderr, "[GRAD] type=0x%x stops=%d focal=%.3f center=(%d,%d,%d,%d) mid=(%d,%d,%d,%d) edge=(%d,%d,%d,%d)\n",
					m_type, m_gradients.size(), (double) m_focal,
					c0.m_r, c0.m_g, c0.m_b, c0.m_a,
					c128.m_r, c128.m_g, c128.m_b, c128.m_a,
					c255.m_r, c255.m_g, c255.m_b, c255.m_a);
				fflush(stderr);
			}
		}

		bitmap_info*	bi = render::create_bitmap_info_rgba(im);
		delete im;

		return bi;
	}


	void	fill_style::apply(int fill_side, float ratio, render_handler::bitmap_blend_mode bm) const
	// Push our style parameters into the renderer.
	{
		UNUSED(ratio);
		if (m_type == 0x00)
		{
			// 0x00: solid fill
			render::fill_style_color(fill_side, m_color);
		}
		else if (m_type == 0x10 || m_type == 0x12 || m_type == 0x13)
		{
			// 0x10: linear gradient fill
			// 0x12: radial gradient fill
			// 0x13: focal radial gradient fill

			if (m_gradient_bitmap_info == NULL)
			{
				// This can happen when morphing gradient styles.
				// assert(morphing???);
				// log an error?
				fill_style*	this_non_const = const_cast<fill_style*>(this);
				this_non_const->m_gradient_bitmap_info = create_gradient_bitmap();
			}

			if (m_gradient_bitmap_info != NULL)
			{
				render::fill_style_bitmap( fill_side, m_gradient_bitmap_info.get_ptr(), m_gradient_matrix,
					render_handler::WRAP_CLAMP, bm);
			}
		}
		else if (m_type >= 0x40 && m_type <= 0x43)
		{
			// bitmap fill (either tiled or clipped)
			bitmap_info*	bi = NULL;
			if (m_bitmap_character != NULL)
			{
				bi = m_bitmap_character->get_bitmap_info();
	
				if (bi != NULL)
				{
					switch (m_type)
					{
						case 0x40 :
						case 0x42 :
							render::fill_style_bitmap(fill_side, bi, m_bitmap_matrix,  render_handler::WRAP_REPEAT, bm);
							break;

						case 0x41 :
						case 0x43 :
							render::fill_style_bitmap(fill_side, bi, m_bitmap_matrix, render_handler::WRAP_CLAMP, bm);
							break;

						default:
							assert(0);
					}
				}
			}
			if (bi == NULL)
			{
				// We have nothing drawable for this style.  Pushing nothing
				// at all would leave whatever fill was last applied active
				// (that is what painted focal-gradient shapes black), so
				// explicitly turn this side off instead.
				static int s_warned[256] = { 0 };
				if (s_warned[m_type] == 0)
				{
					s_warned[m_type] = 1;
					log_error("fill style type 0x%X has no bitmap; fill side disabled\n", m_type);
				}
				render::fill_style_disable(fill_side);
			}
		}
		else
		{
			// Unknown style.  Never leave a stale colour in the renderer.
			static int s_unknown[256] = { 0 };
			if (s_unknown[m_type] == 0)
			{
				s_unknown[m_type] = 1;
				log_error("unsupported fill style type 0x%X; fill side disabled\n", m_type);
			}
			render::fill_style_disable(fill_side);
		}
	}


	void	fill_style::set_lerp(const fill_style& a, const fill_style& b, float t)
	// Sets this style to a blend of a and b.  t = [0,1]
	{
		assert(t >= 0 && t <= 1);

		// fill style type
		m_type = a.get_type();
		assert(m_type == b.get_type());

		// fill style color
		m_color.set_lerp(a.get_color(), b.get_color(), t);

		// fill style gradient matrix
		//
		// @@ TODO morphed gradients don't come out exactly
		// right; they shift around some.  Not sure where the
		// problem is.
		m_gradient_matrix.set_lerp(a.m_gradient_matrix, b.m_gradient_matrix, t);

		// fill style gradients
		assert(m_gradients.size() == a.m_gradients.size());
		assert(m_gradients.size() == b.m_gradients.size());
		for (int j=0; j < m_gradients.size(); j++)
		{
			m_gradients[j].m_ratio =
				(Uint8) frnd(
					flerp(a.m_gradients[j].m_ratio, b.m_gradients[j].m_ratio, t)
					);
			m_gradients[j].m_color.set_lerp(a.m_gradients[j].m_color, b.m_gradients[j].m_color, t);
		}
		m_gradient_bitmap_info = NULL;

		// focal radial gradient offset (type 0x13)
		m_focal = a.m_focal + (b.m_focal - a.m_focal) * t;

		// fill style bitmap ID
		m_bitmap_character = a.m_bitmap_character;
		assert(m_bitmap_character == b.m_bitmap_character);

		// fill style bitmap matrix
		m_bitmap_matrix.set_lerp(a.m_bitmap_matrix, b.m_bitmap_matrix, t);
	}


	//
	// line_style
	//

	
	line_style::line_style()
		:
		m_width(0),
		m_start_capstyle(0),
		m_joinstyle(0),
		m_has_fill_flag(false),
		m_no_hscale_flag(false),
		m_no_vscale_flag(false),
		m_pixelhinting_flag(false),
		m_noclose(false),
		m_end_capstyle(0),
		m_miter_limit_factor(0)
	{
	}


	void	line_style::read(stream* in, int tag_type, movie_definition_sub* m)
	{
		m_width = in->read_u16();
		if (tag_type == 83)	// SHAPE 4
		{
			// 0 = Round cap
			// 1 = No cap
			// 2 = Square cap
			m_start_capstyle = in->read_uint(2);

			// 0 = Round join
			// 1 = Bevel join
			// 2 = Miter join
			m_joinstyle = in->read_uint(2);

			// If 0, uses Color field.
			m_has_fill_flag  = in->read_uint(1) == 1 ? true : false;

			// If 1, stroke thickness will not scale if the object is scaled horizontally.
			m_no_hscale_flag = in->read_uint(1) == 1 ? true : false;

			// If 1, stroke thickness will not scale if the object is scaled vertically.
			m_no_vscale_flag = in->read_uint(1) == 1 ? true : false;

			// If 1, all anchors will be aligned to full pixels.
			m_pixelhinting_flag = in->read_uint(1) == 1 ? true : false;

			in->read_uint(5); // Reserved UB[5] Must be 0.

			// If 1, stroke will not be closed if the stroke�s last point
			// matches its first point. Flash Player will apply caps instead of a join.
			m_noclose = in->read_uint(1) == 1 ? true : false;

			// 0 = Round cap
			// 1 = No cap
			// 2 = Square cap
			m_end_capstyle = in->read_uint(2);

			if (m_joinstyle == 2)
			{
				// Miter limit factor is an 8.8 fixed-point value
				m_miter_limit_factor = in->read_u16();
			}

			if (m_has_fill_flag)
			{
				m_fill_style.read(in, tag_type, m);
			}
			else
			{
				m_color.read(in, tag_type);
			}
		}
		else
		{
			m_color.read(in, tag_type);
		}
	}


	void	line_style::apply(float ratio) const
	{
		UNUSED(ratio);
		if (m_has_fill_flag == false)
		{
			render::line_style_color(m_color);
		}
		else
		{
			// LineStyle2/Shape4 strokes can carry a full fill style (solid,
			// gradient or bitmap).  Push it into the renderer the same way
			// fill styles are, otherwise the stroke falls back to whatever
			// colour was last active.
			m_fill_style.apply(render_handler::FILL_SIDE_LINE, ratio, render_handler::BLEND_NORMAL);
		}
		render::line_style_width(m_width);
	}

}


// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:
