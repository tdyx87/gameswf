// gameswf_sound_handler_winmm.cpp -- Windows waveOut audio handler

#include "gameswf/gameswf_sound_handler_winmm.h"

#ifdef _WIN32

#pragma comment(lib, "winmm.lib")

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

namespace gameswf
{
    tu_mutex& gameswf_engine_mutex();

    static DWORD WINAPI winmm_audio_thread(LPVOID param);

    winmm_sound_handler::winmm_sound_handler()
        : m_max_volume(1.0f), m_is_open(false), m_sampleRate(44100), m_channels(1), m_hWaveOut(NULL), m_hAudioThread(NULL)
    {
        log_msg("winmm_sound_handler: initializing...\n");

        WAVEFORMATEX wfx;
        wfx.wFormatTag = WAVE_FORMAT_PCM;
        wfx.nChannels = m_channels;
        wfx.nSamplesPerSec = m_sampleRate;
        wfx.wBitsPerSample = 16;
        wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
        wfx.cbSize = 0;

        MMRESULT result = waveOutOpen(&m_hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
        if (result != MMSYSERR_NOERROR)
        {
            log_error("winmm_sound_handler: waveOutOpen failed, error=%d\n", result);
            return;
        }

        m_is_open = true;
        log_msg("winmm_sound_handler: audio opened successfully, rate=%d, channels=%d\n", m_sampleRate, m_channels);

        // Start audio thread
        m_hAudioThread = CreateThread(NULL, 0, winmm_audio_thread, this, 0, NULL);
    }

    winmm_sound_handler::~winmm_sound_handler()
    {
        // Signal audio thread to stop and wait for it to finish FIRST
        // (thread accesses handler members, so handler must be alive while thread runs)
        m_is_open = false;
        if (m_hAudioThread)
        {
            WaitForSingleObject(m_hAudioThread, 5000);
            CloseHandle(m_hAudioThread);
            m_hAudioThread = NULL;
        }
        // Now safe to close waveOut - audio thread is done
        if (m_hWaveOut)
        {
            waveOutReset(m_hWaveOut);
            waveOutClose(m_hWaveOut);
            m_hWaveOut = NULL;
        }
        m_sound.clear();
    }

    int winmm_sound_handler::load_sound(const char* url)
    {
        // WAV loading not implemented for winmm
        return -1;
    }

    int winmm_sound_handler::create_sound(void* data, int data_bytes, int sample_count,
        format_type format, int sample_rate, bool stereo)
    {
        log_msg("create_sound: ENTER, data_bytes=%d, format=%d, rate=%d, stereo=%d\n",
            data_bytes, (int)format, sample_rate, stereo ? 1 : 0);
        m_mutex.lock();
        log_msg("create_sound: locked\n");

        int sound_id = m_sound.size();
        log_msg("create_sound: creating sound with id=%d\n", sound_id);
        m_sound[sound_id] = new sound(data_bytes, (Uint8*)data, format, sample_count, sample_rate, stereo);
        log_msg("create_sound: sound created, id=%d\n", sound_id);

        m_mutex.unlock();
        log_msg("create_sound: EXIT, returning %d\n", sound_id);
        return sound_id;
    }

    void winmm_sound_handler::append_sound(int sound_handle, void* data, int data_bytes)
    {
        m_mutex.lock();
        hash<int, gc_ptr<sound> >::iterator it = m_sound.find(sound_handle);
        if (it != m_sound.end())
        {
            // Not implemented
        }
        m_mutex.unlock();
    }

    void winmm_sound_handler::play_sound(as_object* listener_obj, int sound_handle, int loops)
    {
        m_mutex.lock();
        hash<int, gc_ptr<sound> >::iterator it = m_sound.find(sound_handle);
        if (it != m_sound.end())
        {
            if (listener_obj)
            {
                if (it->second->m_listeners == NULL)
                    it->second->m_listeners = new listener();
                it->second->m_listeners->add(listener_obj);
            }
            it->second->play(loops, this);
        }
        m_mutex.unlock();
    }

    void winmm_sound_handler::set_max_volume(int vol)
    {
        if (vol >= 0 && vol <= 100)
            m_max_volume = (float)vol / 100.0f;
    }

    void winmm_sound_handler::stop_sound(int sound_handle)
    {
        m_mutex.lock();
        hash<int, gc_ptr<sound> >::iterator it = m_sound.find(sound_handle);
        if (it != m_sound.end())
            it->second->clear_playlist();
        m_mutex.unlock();
    }

    void winmm_sound_handler::delete_sound(int sound_handle)
    {
        m_mutex.lock();
        m_sound.erase(sound_handle);
        m_mutex.unlock();
    }

    void winmm_sound_handler::stop_all_sounds()
    {
        m_mutex.lock();
        for (hash<int, gc_ptr<sound> >::iterator it = m_sound.begin(); it != m_sound.end(); ++it)
            it->second->clear_playlist();
        m_mutex.unlock();
    }

    int winmm_sound_handler::get_volume(int sound_handle)
    {
        m_mutex.lock();
        int vol = 0;
        hash<int, gc_ptr<sound> >::iterator it = m_sound.find(sound_handle);
        if (it != m_sound.end())
            vol = it->second->get_volume();
        m_mutex.unlock();
        return vol;
    }

    void winmm_sound_handler::set_volume(int sound_handle, int volume)
    {
        m_mutex.lock();
        hash<int, gc_ptr<sound> >::iterator it = m_sound.find(sound_handle);
        if (it != m_sound.end())
            it->second->set_volume(volume);
        m_mutex.unlock();
    }

    void winmm_sound_handler::attach_aux_streamer(gameswf::sound_handler::aux_streamer_ptr ptr, as_object* netstream)
    {
        // Not implemented
    }

    void winmm_sound_handler::detach_aux_streamer(as_object* netstream)
    {
        // Not implemented
    }

    void winmm_sound_handler::pause(int sound_handle, bool paused)
    {
        m_mutex.lock();
        hash<int, gc_ptr<sound> >::iterator it = m_sound.find(sound_handle);
        if (it != m_sound.end())
            it->second->pause(paused);
        m_mutex.unlock();
    }

    int winmm_sound_handler::get_position(int sound_handle)
    {
        return 0;
    }

    // sound implementation
    sound::sound(int size, Uint8* data, sound_handler::format_type format, int sample_count,
        int sample_rate, bool stereo)
        : m_data(data), m_size(size), m_volume(1.0f), m_format(format),
        m_sample_count(sample_count), m_sample_rate(sample_rate), m_stereo(stereo), m_is_paused(false)
    {
        if (data != NULL && size > 0)
        {
            m_data = (Uint8*)malloc(size);
            memcpy(m_data, data, size);
        }

        // Decode MP3 to PCM using dr_mp3
        if (m_format == sound_handler::FORMAT_MP3 && m_data != NULL && m_size > 0)
        {
            drmp3_config config;
            drmp3_uint64 totalPCMFrameCount;
            float* pPCMData = drmp3_open_memory_and_read_pcm_frames_f32(m_data, m_size, &config, &totalPCMFrameCount, NULL);

            if (pPCMData != NULL && totalPCMFrameCount > 0)
            {
                log_msg("MP3 decoded: %llu frames, %d channels, %d Hz\n",
                    totalPCMFrameCount, config.channels, config.sampleRate);

                int sampleCount = (int)(totalPCMFrameCount * config.channels);
                int16_t* pcmS16 = (int16_t*)malloc(sampleCount * sizeof(int16_t));
                drmp3dec_f32_to_s16(pPCMData, pcmS16, sampleCount);

                drmp3_free(pPCMData, NULL);

                free(m_data);
                m_data = (Uint8*)pcmS16;
                m_size = sampleCount * sizeof(int16_t);
                m_format = sound_handler::FORMAT_NATIVE16;
                m_sample_rate = config.sampleRate;
                m_stereo = config.channels > 1;
                m_sample_count = (int)totalPCMFrameCount;

                log_msg("MP3 converted to NATIVE16: size=%d\n", m_size);
            }
            else
            {
                log_error("Failed to decode MP3 data\n");
            }
        }
    }

    sound::~sound()
    {
        free(m_data);
    }

    void sound::play(int loops, winmm_sound_handler* handler)
    {
        if (m_size == 0)
        {
            log_error("the attempt to play the empty sound\n");
            return;
        }
        m_playlist.push_back(new active_sound(this, loops));
    }

    void sound::pause(bool paused)
    {
        m_is_paused = paused;
    }

    int sound::get_played_bytes()
    {
        if (m_playlist.size() > 0)
            return m_playlist[0]->get_played_bytes();
        return 0;
    }

    // Simple linear resample from source_rate to target_rate
    static int16_t* resample_audio(int16_t* src, int src_samples, int src_rate, int target_rate, int channels, int* out_samples)
    {
        if (src_rate == target_rate)
        {
            // No resampling needed
            *out_samples = src_samples;
            int16_t* dst = (int16_t*)malloc(src_samples * sizeof(int16_t));
            memcpy(dst, src, src_samples * sizeof(int16_t));
            return dst;
        }

        // src_samples includes all channels (e.g., 1000 samples = 1000 frames for mono, 500 frames for stereo)
        int src_frames = src_samples / channels;
        double ratio = (double)target_rate / (double)src_rate;
        int dst_frames = (int)(src_frames * ratio);
        int dst_samples = dst_frames * channels;
        *out_samples = dst_samples;

        int16_t* dst = (int16_t*)malloc(dst_samples * sizeof(int16_t));

        for (int frame = 0; frame < dst_frames; frame++)
        {
            double src_pos = frame / ratio;
            int src_idx = (int)src_pos;
            double frac = src_pos - src_idx;
            
            if (src_idx >= src_frames - 1)
            {
                // Last frame - just copy
                for (int ch = 0; ch < channels; ch++)
                {
                    dst[frame * channels + ch] = src[(src_frames - 1) * channels + ch];
                }
            }
            else
            {
                // Linear interpolation
                for (int ch = 0; ch < channels; ch++)
                {
                    int idx0 = src_idx * channels + ch;
                    int idx1 = (src_idx + 1) * channels + ch;

                    double s0 = src[idx0];
                    double s1 = src[idx1];
                    dst[frame * channels + ch] = (int16_t)(s0 + (s1 - s0) * frac);
                }
            }
        }

        return dst;
    }

    // active_sound implementation
    active_sound::active_sound(sound* parent, int loops)
        : m_pos(0), m_played_bytes(0), m_loops(loops), m_size(0), m_data(NULL),
        m_parent(parent), m_decoded(0)
    {
        m_handler = (winmm_sound_handler*)get_sound_handler();
        
        // Copy and resample data from parent immediately
        if (m_parent && m_parent->m_data && m_parent->m_size > 0)
        {
            int src_samples = m_parent->m_size / sizeof(int16_t);
            int channels = m_parent->m_stereo ? 2 : 1;
            int dst_samples = 0;

            // Resample to handler's output rate (44100 Hz)
            int target_rate = m_handler ? m_handler->m_sampleRate : 44100;
            
            m_data = resample_audio((int16_t*)m_parent->m_data, src_samples, 
                m_parent->m_sample_rate, target_rate, channels, &dst_samples);
            m_size = dst_samples * sizeof(int16_t);
            m_decoded = m_parent->m_size;
            
            // Sound resampled successfully
        }
    }

    active_sound::~active_sound()
    {
        free(m_data);
    }

    bool active_sound::mix(int16_t* mixbuf, int mixbuf_samples)
    {
        if (m_parent->m_is_paused)
            return true;

        if (!m_data || m_size == 0)
            return false;

        // mixbuf_samples is the number of mono samples to fill
        // m_data contains resampled data at the output sample rate
        int sound_channels = m_parent->m_stereo ? 2 : 1;
        int total_sound_samples = m_size / sizeof(int16_t);
        int current_sample = m_pos / sizeof(int16_t);
        int remaining_samples = total_sound_samples - current_sample;

        // Check if we've reached the end
        if (remaining_samples <= 0)
        {
            // End of sound - check for loop
            if (m_loops == 0)
                return false;  // Stop playing
            
            // Loop
            if (m_loops > 0)
                m_loops--;
            
            m_pos = 0;
            current_sample = 0;
            remaining_samples = total_sound_samples;
        }

        // Calculate how many output samples to mix
        // For mono sound: 1 output sample per input sample
        // For stereo sound: mix to mono (average left and right)
        int output_samples = (remaining_samples / sound_channels < mixbuf_samples) ? 
                             (remaining_samples / sound_channels) : mixbuf_samples;

        // Apply volume and mix
        float vol = m_parent->m_volume * (m_handler ? m_handler->m_max_volume : 1.0f);
        int16_t* src = (int16_t*)m_data + current_sample;
        
        if (sound_channels == 1)
        {
            // Mono sound -> Mono output
            for (int i = 0; i < output_samples; i++)
            {
                int sample = (int)mixbuf[i] + (int)(src[i] * vol);
                if (sample > 32767) sample = 32767;
                if (sample < -32768) sample = -32768;
                mixbuf[i] = (int16_t)sample;
            }
        }
        else
        {
            // Stereo sound -> Mono output (average left and right)
            for (int i = 0; i < output_samples; i++)
            {
                int left = src[i * 2];
                int right = src[i * 2 + 1];
                int mono = (left + right) / 2;
                int sample = (int)mixbuf[i] + (int)(mono * vol);
                if (sample > 32767) sample = 32767;
                if (sample < -32768) sample = -32768;
                mixbuf[i] = (int16_t)sample;
            }
        }

        m_pos += output_samples * sound_channels * sizeof(int16_t);
        m_played_bytes += output_samples * sound_channels * sizeof(int16_t);

        // Return true if there's more to play, false if we're done
        return (m_pos < m_size) || (m_loops != 0);
    }

    // Audio thread
    static DWORD WINAPI winmm_audio_thread(LPVOID param)
    {
        winmm_sound_handler* handler = (winmm_sound_handler*)param;

        const int BUFFER_SAMPLES = 2048;  // Samples per buffer
        const int BUFFER_BYTES = BUFFER_SAMPLES * sizeof(int16_t);
        const int NUM_BUFFERS = 3;

        WAVEHDR headers[NUM_BUFFERS];
        int16_t* buffers[NUM_BUFFERS];

        for (int i = 0; i < NUM_BUFFERS; i++)
        {
            buffers[i] = (int16_t*)malloc(BUFFER_BYTES);
            memset(buffers[i], 0, BUFFER_BYTES);
            memset(&headers[i], 0, sizeof(WAVEHDR));
            headers[i].lpData = (LPSTR)buffers[i];
            headers[i].dwBufferLength = BUFFER_BYTES;
        }

        int current_buffer = 0;
        bool first_run = true;

        while (handler->m_is_open)
        {
            WAVEHDR* hdr = &headers[current_buffer];
            int16_t* buf = buffers[current_buffer];

            // Wait for buffer to be done (skip on first run)
            if (!first_run)
            {
                while ((hdr->dwFlags & WHDR_DONE) == 0 && (hdr->dwFlags & WHDR_INQUEUE) != 0)
                {
                    Sleep(1);
                    if (!handler->m_is_open) goto cleanup;
                }

                if (hdr->dwFlags & WHDR_PREPARED)
                {
                    waveOutUnprepareHeader(handler->m_hWaveOut, hdr, sizeof(WAVEHDR));
                }
            }
            first_run = false;

            // Clear buffer
            memset(buf, 0, BUFFER_BYTES);

            // Mix audio
            handler->m_mutex.lock();

            array<gc_ptr<listener> > listeners;

            for (hash<int, gc_ptr<sound> >::iterator snd = handler->m_sound.begin();
                snd != handler->m_sound.end(); ++snd)
            {
                for (int i = 0; i < snd->second->m_playlist.size();)
                {
                    if (!snd->second->m_playlist[i]->mix(buf, BUFFER_SAMPLES))
                    {
                        if (snd->second->m_listeners != NULL)
                            listeners.push_back(snd->second->m_listeners);
                        snd->second->m_playlist.remove(i);
                    }
                    else
                    {
                        i++;
                    }
                }
            }

            handler->m_mutex.unlock();

            // Notify listeners
            if (listeners.size() > 0)
            {
                gameswf_engine_mutex().lock();
                for (int i = 0, n = listeners.size(); i < n; i++)
                    listeners[i]->notify(event_id::ON_SOUND_COMPLETE);
                gameswf_engine_mutex().unlock();
            }

            // Write buffer to audio device
            MMRESULT result = waveOutPrepareHeader(handler->m_hWaveOut, hdr, sizeof(WAVEHDR));
            if (result == MMSYSERR_NOERROR)
            {
                result = waveOutWrite(handler->m_hWaveOut, hdr, sizeof(WAVEHDR));
                if (result != MMSYSERR_NOERROR)
                {
                    waveOutUnprepareHeader(handler->m_hWaveOut, hdr, sizeof(WAVEHDR));
                }
            }

            current_buffer = (current_buffer + 1) % NUM_BUFFERS;
            
            // Sleep to maintain roughly 20ms buffer time
            Sleep(20);
        }

    cleanup:
        // Cleanup - handler may already be destroyed, use local copy of m_hWaveOut
        HWAVEOUT hOut = handler->m_hWaveOut;
        if (hOut)
        {
            waveOutReset(hOut);
            for (int i = 0; i < NUM_BUFFERS; i++)
            {
                if (headers[i].dwFlags & WHDR_PREPARED)
                    waveOutUnprepareHeader(hOut, &headers[i], sizeof(WAVEHDR));
                free(buffers[i]);
            }
        }
        else
        {
            for (int i = 0; i < NUM_BUFFERS; i++)
            {
                free(buffers[i]);
            }
        }

        return 0;
    }

    sound_handler* create_sound_handler_winmm()
    {
        return new winmm_sound_handler();
    }
}

#endif // _WIN32
