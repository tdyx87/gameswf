// gameswf_sound.cpp	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Code to handle SWF sound-related tags.


#include "gameswf/gameswf_sound.h"
#include "gameswf/gameswf_stream.h"
#include "gameswf/gameswf_impl.h"
#include "gameswf/gameswf_log.h"
#include "gameswf/gameswf_movie_def.h"


namespace gameswf
{
	// Callback interface to host, for handling sounds.  If it's NULL,
	// sound is ignored.
	static sound_handler*	s_sound_handler = 0;

	static int	s_sample_rate_table[] = { 5512, 11025, 22050, 44100 };

	int get_sample_rate(int index)
	{
		assert(index >=0 && index < 4);
		return s_sample_rate_table[index];
	}

	// Forward declaration
	void	process_pending_sounds();

	void	set_sound_handler(sound_handler* s)
	// Called by host, to set a handler for all sounds.
	// Can pass in 0 to disable sound.
	{
		s_sound_handler = s;
		// Process any pending sounds that were loaded before handler was set
		process_pending_sounds();
	}


	sound_handler*	get_sound_handler()
	{
		if (s_sound_handler)
		{
			if (s_sound_handler->is_open() == false)
			{
				return NULL;
			}
			return s_sound_handler;
		}
		return NULL;
	}


	sound_sample::~sound_sample()
	{
		if (s_sound_handler)
		{
			s_sound_handler->delete_sound(m_sound_handler_id);
		}
	}


	// Utility function to uncompress ADPCM.
	static void	adpcm_expand(
		void* data_out,
		stream* in,
		int sample_count,	// in stereo, this is number of *pairs* of samples
		bool stereo);


	// Structure to hold pending sound data when s_sound_handler is NULL
	struct pending_sound
	{
		Uint16 character_id;
		sound_handler::format_type format;
		int sample_rate;
		bool stereo;
		int sample_count;
		int data_bytes;
		unsigned char* data;
		movie_definition_sub* movie;
	};
	static array<pending_sound> s_pending_sounds;

	void	define_sound_loader(stream* in, int tag_type, movie_definition_sub* m)
	// Load a DefineSound tag.
	{
		assert(tag_type == 14);

		Uint16	character_id = in->read_u16();

		sound_handler::format_type	format = (sound_handler::format_type) in->read_uint(4);
		int	sample_rate = in->read_uint(2);	// multiples of 5512.5
		bool	sample_16bit = in->read_uint(1) ? true : false;
		bool	stereo = in->read_uint(1) ? true : false;
		int	sample_count = in->read_u32();

		IF_VERBOSE_PARSE(log_msg("define_sound_loader: ch=%d, format=%d, rate=%d, 16=%d, stereo=%d, ct=%d\n",
					 character_id, int(format), sample_rate, int(sample_16bit), int(stereo), sample_count));

		// Always read sound data
		int	data_bytes = 0;
		unsigned char*	data = NULL;

		if (format == sound_handler::FORMAT_ADPCM)
		{
			// Uncompress the ADPCM before handing data to host.
			data_bytes = sample_count * (stereo ? 4 : 2);
			data = new unsigned char[data_bytes];
			adpcm_expand(data, in, sample_count, stereo);
			format = sound_handler::FORMAT_NATIVE16;
		}
		else
		{
			// @@ This is pretty awful -- lots of copying, slow reading.
			data_bytes = in->get_tag_end_position() - in->get_position();
			data = new unsigned char[data_bytes];
			for (int i = 0; i < data_bytes; i++)
			{
				data[i] = in->read_u8();
			}

			// Swap bytes on behalf of the host, to make it easier for the handler.
			// @@ I'm assuming this is a good idea?	 Most sound handlers will prefer native endianness?
			if (format == sound_handler::FORMAT_UNCOMPRESSED)
			{
				if (sample_16bit)
				{
					#ifndef _TU_LITTLE_ENDIAN_
					// Swap sample bytes to get big-endian format.
					for (int i = 0; i < data_bytes - 1; i += 2)
					{
						swap(&data[i], &data[i+1]);
					}
					#endif // not _TU_LITTLE_ENDIAN_

					format = sound_handler::FORMAT_NATIVE16;
				}
				else
				{
					// Convert 8-bit samples to 16-bit NATIVE16
					int new_data_bytes = data_bytes * 2;
					unsigned char* new_data = new unsigned char[new_data_bytes];
					for (int i = 0; i < data_bytes; i++)
					{
						// 8-bit unsigned [0,255] -> 16-bit signed [-32768,32767]
						int16_t sample = (int16_t)((int)(data[i]) - 128) * 256;
						((int16_t*)new_data)[i] = sample;
					}
					delete [] data;
					data = new_data;
					data_bytes = new_data_bytes;
					format = sound_handler::FORMAT_NATIVE16;
				}
			}
		}

		// If we have a sound_handler, create the sound immediately
		if (s_sound_handler)
		{
			int	handler_id = s_sound_handler->create_sound(
				data,
				data_bytes,
				sample_count,
				format,
				get_sample_rate(sample_rate),
				stereo);
			sound_sample*	sam = new sound_sample(m->get_player(), handler_id);
			m->add_sound_sample(character_id, sam);
			delete [] data;
		}
		else
		{
			// Cache the sound data for later
			// Caching sound for later playback
			pending_sound ps;
			ps.character_id = character_id;
			ps.format = format;
			ps.sample_rate = sample_rate;
			ps.stereo = stereo;
			ps.sample_count = sample_count;
			ps.data_bytes = data_bytes;
			ps.data = data;
			ps.movie = m;
			s_pending_sounds.push_back(ps);
		}
	}

	void	process_pending_sounds()
	// Process all pending sounds after sound_handler is set
	{
		if (!s_sound_handler || s_pending_sounds.size() == 0)
		{
			return;
		}

		for (int i = 0; i < s_pending_sounds.size(); i++)
		{
			pending_sound& ps = s_pending_sounds[i];
			
			int	handler_id = s_sound_handler->create_sound(
				ps.data,
				ps.data_bytes,
				ps.sample_count,
				ps.format,
				get_sample_rate(ps.sample_rate),
				ps.stereo);
			
			sound_sample*	sam = new sound_sample(ps.movie->get_player(), handler_id);
			ps.movie->add_sound_sample(ps.character_id, sam);

			delete [] ps.data;
		}

		s_pending_sounds.clear();
	}


	struct start_sound_tag : public execute_tag
	{
		int	m_handler_id;
		int	m_loop_count;
		bool	m_stop_playback;
		array<sound_envelope> m_envelopes;

		start_sound_tag()
			:
			m_handler_id(0),
			m_loop_count(0),
			m_stop_playback(false)
		{
		}


		void	read(stream* in, int tag_type, movie_definition_sub* m, const sound_sample* sam)
		// Initialize this StartSound tag from the stream & given sample.
		// Insert ourself into the movie.
		{
			assert(sam);

			in->read_uint(2);	// skip reserved bits.
			m_stop_playback = in->read_uint(1) ? true : false;
			bool	no_multiple = in->read_uint(1) ? true : false;
			bool	has_envelope = in->read_uint(1) ? true : false;
			bool	has_loops = in->read_uint(1) ? true : false;
			bool	has_out_point = in->read_uint(1) ? true : false;
			bool	has_in_point = in->read_uint(1) ? true : false;

			UNUSED(no_multiple);
			UNUSED(has_envelope);
			
			Uint32	in_point = 0;
			Uint32	out_point = 0;
			if (has_in_point) { in_point = in->read_u32(); }
			if (has_out_point) { out_point = in->read_u32(); }
			if (has_loops) { m_loop_count = in->read_u16(); }
			if (has_envelope) 
			{ 
				int env_count = in->read_u8();
				
				m_envelopes.resize(env_count);

				for(int i=0;i<env_count;++i)
				{
					m_envelopes[i].m_mark44 = in->read_u32();
					m_envelopes[i].m_level0 = in->read_u16();
					m_envelopes[i].m_level1 = in->read_u16();
				}
			}

			m_handler_id = sam->m_sound_handler_id;
			m->add_execute_tag(this);
		}


		void	execute(character* m)
		{
			if (s_sound_handler)
			{
				if (m_stop_playback)
				{
					s_sound_handler->stop_sound(m_handler_id);
				}
				else
				{
					s_sound_handler->play_sound(NULL, m_handler_id, m_loop_count);
				}
			}
		}
	};




	// Delayed start_sound tag - executes when sound becomes available
	struct delayed_start_sound_tag : public execute_tag
	{
		Uint16 m_sound_id;
		movie_definition_sub* m_movie;
		int m_tag_type;
		Uint8* m_data;
		int m_data_size;

		delayed_start_sound_tag(Uint16 sound_id, movie_definition_sub* m, int tag_type, Uint8* data, int data_size)
			: m_sound_id(sound_id), m_movie(m), m_tag_type(tag_type), m_data(data), m_data_size(data_size)
		{
		}

		~delayed_start_sound_tag()
		{
			delete [] m_data;
		}

		void	execute(character* ch)
		{
			sound_sample* sam = m_movie->get_sound_sample(m_sound_id);
			if (sam && s_sound_handler)
			{
				// Re-parse the StartSound data to get loop_count and other params
				// StartSound format: sound_id(u16) + flags(u8) + [loop_count(u16)]
				int loop_count = 0;
				bool stop_playback = false;
				if (m_data_size >= 4)
				{
					Uint8 flags = m_data[2];
					stop_playback = (flags >> 7) & 1;
					bool has_loops = (flags >> 4) & 1;
					if (has_loops && m_data_size >= 6)
					{
						loop_count = m_data[4] | (m_data[5] << 8);
					}
				}

				if (stop_playback)
				{
					s_sound_handler->stop_sound(sam->m_sound_handler_id);
				}
				else
				{
					s_sound_handler->play_sound(NULL, sam->m_sound_handler_id, loop_count);
				}
			}
			// Remove ourselves after execution
			// Note: This is a memory leak since we can't remove from movie's execute list
			// But it's better than not playing the sound
		}
	};

	void	start_sound_loader(stream* in, int tag_type, movie_definition_sub* m)
	// Load a StartSound tag.
	{
		assert(tag_type == 15);

		Uint16	sound_id = in->read_u16();

		sound_sample*	sam = m->get_sound_sample(sound_id);
		if (sam)
		{
			start_sound_tag*	sst = new start_sound_tag();
			sst->read(in, tag_type, m, sam);

			IF_VERBOSE_PARSE(log_msg("start_sound tag: id=%d, stop = %d, loop ct = %d\n",
						 sound_id, int(sst->m_stop_playback), sst->m_loop_count));
		}
		else
		{
			// Sound not yet defined, create a delayed tag
			int data_size = in->get_tag_end_position() - in->get_position();
			Uint8* data = new Uint8[data_size];
			for (int i = 0; i < data_size; i++)
			{
				data[i] = in->read_u8();
			}
			
			delayed_start_sound_tag* dst = new delayed_start_sound_tag(sound_id, m, tag_type, data, data_size);
			m->add_execute_tag(dst);
			
			IF_VERBOSE_PARSE(log_msg("start_sound tag: id=%d - sound not yet defined, creating delayed tag\n", sound_id));
		}
	}



	//
	// ADPCM
	//

	// Data from Alexis' SWF reference
	static int	s_index_update_table_2bits[2] = { -1,  2 };
	static int	s_index_update_table_3bits[4] = { -1, -1,  2,  4 };
	static int	s_index_update_table_4bits[8] = { -1, -1, -1, -1,  2,  4,  6,  8 };
	static int	s_index_update_table_5bits[16] = { -1, -1, -1, -1, -1, -1, -1, -1, 1,  2,  4,  6,  8, 10, 13, 16 };

	static int*	s_index_update_tables[4] = {
		s_index_update_table_2bits,
		s_index_update_table_3bits,
		s_index_update_table_4bits,
		s_index_update_table_5bits,
	};

	// Data from Jansen.  http://homepages.cwi.nl/~jack/
	// Check out his Dutch retro punk songs, heh heh :)
	const int STEPSIZE_CT = 89;
	static int s_stepsize[STEPSIZE_CT] = {
		7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
		19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
		50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
		130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
		337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
		876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
		2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
		5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
		15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
	};


	// Algo from http://www.circuitcellar.com/pastissues/articles/richey110/text.htm
	// And also Jansen.
	// Here's another reference: http://www.geocities.com/SiliconValley/8682/aud3.txt
	// Original IMA spec doesn't seem to be on the web :(


	// @@ lots of macros here!  It seems that VC6 can't correctly
	// handle integer template args, although it's happy to
	// compile them?!

//	void DO_SAMPLE(int n_bits, int& sample, int& stepsize_index, int raw_code)
#define DO_SAMPLE(n_bits, sample, stepsize_index, raw_code)									\
	{															\
		assert(raw_code >= 0 && raw_code < (1 << n_bits));								\
																\
		static const int	HI_BIT = (1 << (n_bits - 1));								\
		int*	index_update_table = s_index_update_tables[n_bits - 2];							\
																\
		/* Core of ADPCM. */												\
																\
		int	code_mag = raw_code & (HI_BIT - 1);									\
		bool	code_sign_bit = (raw_code & HI_BIT) ? 1 : 0;								\
		int	mag = (code_mag << 1) + 1;	/* shift in LSB (they do this so that pos & neg zero are different)*/	\
																\
		int	stepsize = s_stepsize[stepsize_index];									\
																\
		/* Compute the new sample.  It's the predicted value			*/					\
		/* (i.e. the previous value), plus a delta.  The delta			*/					\
		/* comes from the code times the stepsize.  going for			*/					\
		/* something like: delta = stepsize * (code * 2 + 1) >> code_bits	*/					\
		int	delta = (stepsize * mag) >> (n_bits - 1);								\
		if (code_sign_bit) delta = -delta;										\
																\
		sample += delta;												\
		sample = iclamp(sample, -32768, 32767);										\
																\
		/* Update our stepsize index.  Use a lookup table. */								\
		stepsize_index += index_update_table[code_mag];									\
		stepsize_index = iclamp(stepsize_index, 0, STEPSIZE_CT - 1);							\
	}


	struct in_stream
	{
		const unsigned char*	m_in_data;
		int	m_current_bits;
		int	m_unused_bits;

		in_stream(const unsigned char* data)
			:
			m_in_data(data),
			m_current_bits(0),
			m_unused_bits(0)
		{
		}
	};


//	void DO_MONO_BLOCK(Sint16** out_data, int n_bits, int sample_count, stream* in, int sample, int stepsize_index)
#define DO_MONO_BLOCK(out_data, n_bits, sample_count, in, sample, stepsize_index)						\
	{															\
		/* First sample doesn't need to be decompressed. */								\
		sample_count--;													\
		*(*out_data)++ = (Sint16) sample;										\
																\
		while (sample_count--)												\
		{														\
			int	raw_code = in->read_uint(n_bits);								\
			DO_SAMPLE(n_bits, sample, stepsize_index, raw_code);	/* sample & stepsize_index are in/out params */	\
			*(*out_data)++ = (Sint16) sample;									\
		}														\
	}


//	void do_stereo_block(
//		Sint16** out_data,	// in/out param
//		int n_bits,
//		int sample_count,
//		stream* in,
//		int left_sample,
//		int left_stepsize_index,
//		int right_sample,
//		int right_stepsize_index
//		)
#define DO_STEREO_BLOCK(out_data, n_bits, sample_count, in, left_sample, left_stepsize_index, right_sample, right_stepsize_index) \
	/* Uncompress 4096 stereo sample pairs of ADPCM. */									  \
	{															  \
		/* First samples don't need to be decompressed. */								  \
		sample_count--;													  \
		*(*out_data)++ = (Sint16) left_sample;										  \
		*(*out_data)++ = (Sint16) right_sample;										  \
																  \
		while (sample_count--)												  \
		{														  \
			int	left_raw_code = in->read_uint(n_bits);								  \
			DO_SAMPLE(n_bits, left_sample, left_stepsize_index, left_raw_code);					  \
			*(*out_data)++ = (Sint16) left_sample;									  \
																  \
			int	right_raw_code = in->read_uint(n_bits);								  \
			DO_SAMPLE(n_bits, right_sample, right_stepsize_index, right_raw_code);					  \
			*(*out_data)++ = (Sint16) right_sample;									  \
		}														  \
	}


	void	adpcm_expand(
		void* out_data_void,
		stream* in,
		int sample_count,	// in stereo, this is number of *pairs* of samples
		bool stereo)
	// Utility function: uncompress ADPCM data from in stream to
	// out_data[].	The output buffer must have (sample_count*2)
	// bytes for mono, or (sample_count*4) bytes for stereo.
	{
		Sint16*	out_data = (Sint16*) out_data_void;

		// Read header.
		int	n_bits = in->read_uint(2) + 2;	// 2 to 5 bits

		while (sample_count)
		{
			// Read initial sample & index values.
			int	sample = in->read_sint(16);

			int	stepsize_index = in->read_uint(6);
			assert(STEPSIZE_CT >= (1 << 6));	// ensure we don't need to clamp.

			int	samples_this_block = imin(sample_count, 4096);
			sample_count -= samples_this_block;

			if (stereo == false)
			{
#define DO_MONO(n) DO_MONO_BLOCK(&out_data, n, samples_this_block, in, sample, stepsize_index)

				switch (n_bits)
				{
				default: assert(0); break;
				case 2: DO_MONO(2); break;
				case 3: DO_MONO(3); break;
				case 4: DO_MONO(4); break;
				case 5: DO_MONO(5); break;
				}
			}
			else
			{
				// Stereo.

				// Got values for left channel; now get initial sample
				// & index for right channel.
				int	right_sample = in->read_sint(16);

				int	right_stepsize_index = in->read_uint(6);
				assert(STEPSIZE_CT >= (1 << 6));	// ensure we don't need to clamp.

#define DO_STEREO(n)					\
	DO_STEREO_BLOCK(				\
		&out_data, n, samples_this_block,	\
		in, sample, stepsize_index,		\
		right_sample, right_stepsize_index)
			
				switch (n_bits)
				{
				default: assert(0); break;
				case 2: DO_STEREO(2); break;
				case 3: DO_STEREO(3); break;
				case 4: DO_STEREO(4); break;
				case 5: DO_STEREO(5); break;
				}
			}
		}
	}


};	// end namespace gameswf


// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:
