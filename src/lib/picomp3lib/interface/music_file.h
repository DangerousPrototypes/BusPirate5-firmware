#pragma once
#include "mp3dec.h"

typedef enum MusicType
{
	MUSIC_WAV = 1,
	MUSIC_MP3
} MusicType;

// Music file header format
typedef struct music_file 
{
	FIL      fil;							// FatFS file block
	bool     init;							// True if initialised
	MusicType	type;						// The file format
	HMP3Decoder hMP3Decoder;
	char*    working;						// Pointer to working buffer
	uint32_t working_len;					// Size of working buffer in bytes
	uint32_t sample_rate;					// sampling rate (blocks per second)
	uint32_t data_size;						// NumSamples * NumChannels * BitsPerSample/8 - number of bytes of data
	uint32_t sample_size;                   // mf->channels * mf->bits_per_sample / 8
	uint32_t file_offset;                   // Offset of start of data in file (to allow skip initial tags on wrap)
	uint32_t file_pos;					    // Current read position in data in file, offset from start of data chunk
	uint16_t channels;						// no.of channels
	uint16_t bits_per_sample;				// bits per sample, 8- 8bits, 16- 16 bits etc
	uint32_t work_offset;			        // Current read position in working buffer
	uint32_t working_filled;				// Number of bytes of data in total in the the working buffer

} music_file;

// Function declarations
extern bool musicFileCreate(music_file* mf, const char* filename, unsigned char* working, uint32_t len);
extern void musicFileClose(music_file* mf);

// Populate destination from the circular buffer
extern bool musicFileRead(music_file* mf, int16_t* dest, uint32_t len, uint32_t* written);

// inline functions
static inline uint32_t musicFileGetSampleRate(music_file* mf){return mf->sample_rate;}
static inline uint16_t musicFileGetChannels(music_file* mf){return mf->channels;}
static inline bool musicFileIsStereo(music_file* mf){return (mf->channels == 2);}

static inline uint32_t musicFileBytesLeft(music_file* mf){return mf->working_filled - mf->work_offset;}
