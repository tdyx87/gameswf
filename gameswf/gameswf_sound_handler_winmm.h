// gameswf_sound_handler_winmm.h -- Windows waveOut audio handler

#ifndef SOUND_HANDLER_WINMM_H
#define SOUND_HANDLER_WINMM_H

#include "base/tu_config.h"
#include "gameswf/gameswf.h"
#include "base/container.h"
#include "gameswf/gameswf_log.h"
#include "gameswf/gameswf_mutex.h"
#include "gameswf/gameswf_listener.h"

#include <windows.h>
#include <mmsystem.h>

namespace gameswf
{
    struct sound;
    struct active_sound;

    struct winmm_sound_handler : public sound_handler
    {
        hash<int, gc_ptr<sound> > m_sound;
        float m_max_volume;
        tu_mutex m_mutex;
        HWAVEOUT m_hWaveOut;
        HANDLE m_hAudioThread;
        bool m_is_open;
        int m_sampleRate;
        int m_channels;

        winmm_sound_handler();
        virtual ~winmm_sound_handler();

        virtual bool is_open() { return m_is_open; };
        virtual int load_sound(const char* url);
        virtual int create_sound(void* data, int data_bytes, int sample_count, format_type format, int sample_rate, bool stereo);
        virtual void append_sound(int sound_handle, void* data, int data_bytes);
        virtual void play_sound(as_object* listener_obj, int sound_handle, int loop_count);
        virtual void set_max_volume(int vol);
        virtual void stop_sound(int sound_handle);
        virtual void delete_sound(int sound_handle);
        virtual void stop_all_sounds();
        virtual int get_volume(int sound_handle);
        virtual void set_volume(int sound_handle, int volume);
        virtual void attach_aux_streamer(gameswf::sound_handler::aux_streamer_ptr ptr, as_object* netstream);
        virtual void detach_aux_streamer(as_object* netstream);
        virtual void pause(int sound_handle, bool paused);
        virtual int get_position(int sound_handle);
    };

    struct sound : public gameswf::ref_counted
    {
        sound(int size, Uint8* data, sound_handler::format_type format, int sample_count, int sample_rate, bool stereo);
        ~sound();

        inline void clear_playlist() { m_playlist.clear(); }
        inline int get_volume() const { return (int)(m_volume * 100.0f); }
        inline void set_volume(int vol) { m_volume = (float)vol / 100.0f; }

        void play(int loops, winmm_sound_handler* handler);
        void pause(bool paused);
        int get_played_bytes();

        Uint8* m_data;
        int m_size;
        float m_volume;
        gameswf::sound_handler::format_type m_format;
        int m_sample_count;
        int m_sample_rate;
        bool m_stereo;
        array<gc_ptr<active_sound> > m_playlist;
        bool m_is_paused;
        gc_ptr<listener> m_listeners;
    };

    struct active_sound : public gameswf::ref_counted
    {
        active_sound(sound* parent, int loops);
        ~active_sound();

        inline int get_played_bytes() { return m_played_bytes; }
        bool mix(int16_t* mixbuf, int mixbuf_samples);

        int m_pos;
        int m_played_bytes;
        int m_loops;
        int m_size;
        int16_t* m_data;
        sound* m_parent;
        int m_decoded;
        winmm_sound_handler* m_handler;
    };

    sound_handler* create_sound_handler_winmm();
}

#endif // SOUND_HANDLER_WINMM_H
