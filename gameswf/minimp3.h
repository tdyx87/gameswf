// minimp3.h - Minimal MP3 decoder
// Single header library for MP3 decoding
// Based on minimp3 by lion2010

#ifndef MINIMP3_H
#define MINIMP3_H

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MP3_MAX_SAMPLES_PER_FRAME 1152
#define MP3_MAX_FRAME_SIZE 2880

typedef struct {
    int sample_rate;
    int channels;
    int frame_size;
    int bit_rate;
    int samples;
} mp3_info_t;

// Decode MP3 data to PCM
// Returns number of samples decoded, or -1 on error
int mp3_decode(const uint8_t* mp3_data, int mp3_size, 
               int16_t* pcm_data, int pcm_size,
               mp3_info_t* info);

// Find sync word in MP3 data
const uint8_t* mp3_find_sync(const uint8_t* data, int size);

// Get MP3 frame info
int mp3_get_frame_info(const uint8_t* data, int size, mp3_info_t* info);

#ifdef __cplusplus
}
#endif

#endif // MINIMP3_H
