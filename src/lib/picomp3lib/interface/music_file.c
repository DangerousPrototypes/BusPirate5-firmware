/**
 * Read and parse a Mp3 file
 *
 **/
#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include <string.h>
#include "fatfs/ff.h"
#include <stdbool.h>
#include "music_file.h"

// Type definitions
typedef enum mp3FrameError
{
    ID_OK = 0,
    ID_EOF,
    ID_FULL,
    ID_FATAL,
    ID_INVALID_FILE_TYPE,
    ID_NO_SYNC
} mp3FrameError;

// Static Function declarations
static bool mp3FileRead(music_file* mf, int16_t* dest, uint32_t len, uint32_t* written);
static bool waveFileRead(music_file* mf, uint16_t* dest, uint32_t len, uint32_t* written);

static void mp3FileFillReadBuffer(music_file* mf);
static uint32_t mp3FileSkipId3v2(music_file* mf);
static mp3FrameError mp3FileDecodeSyncWord(music_file* mf, MP3FrameInfo* mp3_info, uint16_t* encode_len, uint16_t* decode_len);
static mp3FrameError mp3FileMoveToSyncWord(music_file* mf);

static bool waveFileCheck(music_file* mf);
static bool file_read(FIL* fil, void* buffer, uint size, const char* msg);


// Defines
#define MPG_FILE_MAX_EMPTY_FRAMES 1

//#define DEBUG_STATUS

// Extra debug
#ifdef DEBUG_STATUS
  #define STATUS(a) printf a
#else
  #define STATUS(a) (void)0
#endif

/*
 * musicFileCreate
 * mf           structure containing info for the music_file instance
 * filename     path to the mp3 file
 * working      An allocated cache buffer, used internally to store data read from file
 * len          Size (in bytes) of the cache buffer
 * 
 * Open the file and determine whether a valid wav or mp3
 * 
 */
bool musicFileCreate(music_file* mf, const char* filename, unsigned char* working, uint32_t len)
{
    mf->init = false;

    // open file
    STATUS(("\nOpening  file: %s\n", filename));
    FRESULT fr = f_open(&mf->fil, filename, FA_OPEN_EXISTING | FA_READ);

    if (FR_OK != fr)
    {
        printf("Error opening file\n");
        return false;
    }
    else
    {
        mf->working = working;
        mf->working_len = len;

        // Working buffer starts empty
        mf->work_offset = 0;
        mf->working_filled = 0;
        mf->init = true;

        // Determine the file type - see if it is a wav file
        if (waveFileCheck(mf))
        {
            mf->type = MUSIC_WAV;
        }
        else
        {
            // Not a wav file, so assume mp3
            mf->type = MUSIC_MP3;

            // Initialise the mp3 decoder
            mf->hMP3Decoder = MP3InitDecoder();

            if (mf->hMP3Decoder == 0)
            {
                printf("Error opening decoder\n");
                return false;
            }

            // Back to the start of the file
            if (f_lseek(&mf->fil, 0) != FR_OK)
            {
                printf("Cannot seek to start of file in musicFileCreate\n");
            }

            // Populate the working buffer
            mp3FileFillReadBuffer(mf);

            // Search for id v2 tags
            uint32_t skip = mp3FileSkipId3v2(mf);

            if (skip)
            {
                if (skip <= musicFileBytesLeft(mf))
                {
                    mf->work_offset += skip;
                }
                else
                {
                    if (f_lseek(&mf->fil, skip) == FR_OK)
                    {
                        // The buffer is now used
                        mf->work_offset = mf->working_filled;
                    }
                    else
                    {
                        printf("f_lseek error in mp3FileCreate\n");
                        return false;
                    }
                }
            }
            // Decode the first frame to get sample rate and number of channels
            MP3FrameInfo mp3_info;
            uint16_t encode;
            uint16_t decode;

            if (mp3FileDecodeSyncWord(mf, &mp3_info, &encode, &decode) != ID_OK)
            {
                printf("Cannot decode first frame\n");
                return false;
            }

            // Save the position of the first frame
            mf->file_offset = mf->work_offset;

            // Populate info
            mf->sample_rate = (uint32_t)mp3_info.samprate;
            mf->channels = (uint16_t)mp3_info.nChans;
            mf->bits_per_sample = (uint16_t)mp3_info.bitsPerSample;
        }
    }
    return true;
 }

/*
 * musicFileClose
 * 
 * Close the music file
 * 
 */
void musicFileClose(music_file* mf)
{
    //Close the file, if have a valid handle
    if (mf->init)
    {
        if (mf->type == MUSIC_MP3)
        {
            MP3FreeDecoder(mf->hMP3Decoder);
        }
            
        STATUS(("Closing file..\n"));
        FRESULT fr = f_close(&mf->fil);

        if (FR_OK != fr) 
        {
            //printf("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
        }
        mf->init = false;
    }
}

/* 
 * musicFileRead 
 * mf           structure containing info for the music file instance
 * dest         Destination buffer to fill
 * len          max number of 16 bit samples that will fit in dest
 * written      number of 16 bit samples written
 * 
 * Fill the destination buffer
 *      If mono source, just fill with single samples
 *      If stereo source, fill with with left and right interleaved samples
 * 
 */
bool musicFileRead(music_file* mf, int16_t* dest, uint32_t len, uint32_t* written)
{
    if (mf->type == MUSIC_MP3)
    {
        return mp3FileRead(mf, dest, len, written);
    }
    else
    {
        return waveFileRead(mf, dest, len, written);
    }
}

/* 
 * mp3FileRead 
 * mf           structure containing info for the music_file instance
 * dest         Destination buffer to fill
 * len          max number of 16 bit samples that will fit in dest
 * written      number of 16 bit samples written
 * 
 * Fill the destination buffer
 *      If mono source, just fill with single samples
 *      If stereo source, fill with with left and right interleaved samples
 * 
 */
static bool mp3FileRead(music_file* mf, int16_t* dest, uint32_t len, uint32_t* written)
{
    mp3FrameError state = ID_OK;
    int err = 0;
    uint32_t consumed;
    uint32_t offset;
    uint16_t decode_len;
    uint16_t encode_len;
    MP3FrameInfo mp3_info;
    unsigned char* inbuff;

    *written = 0;

    printf("mp3FileRead start: Bytes left in working buffer: %u\n", musicFileBytesLeft(mf));

    // Fill output buffer
    do
    {
        // Decode the next valid sync word
        state = mp3FileDecodeSyncWord(mf, &mp3_info, &encode_len, &decode_len);

        if (state == ID_OK)
        {
            printf("Frame length:%u\n", encode_len);

            // Add extra data in necessary
            if (musicFileBytesLeft(mf) < encode_len)
            {
                mp3FileFillReadBuffer(mf);
            }

#ifdef NO_LOOP
            // Have we finished?
            if (musicFileBytesLeft(mf) < encode_len)
            {
                state = ID_EOF;
                break;
            }
#endif

            // Will it fit in the destination buffer? 
            // Decode counted in samples (16 bits) not bytes
            decode_len >>= 1;

            if (decode_len + *written > len)
            {
                printf("ID_FULL: %d\r\n", decode_len + *written > len);
                state = ID_FULL;
                break;
            }
            printf("NOT_FULL: %d\r\n", decode_len + *written > len);

            // Get the data
            int bytes_left = (int)musicFileBytesLeft(mf);
            unsigned char * inbuff = &mf->working[mf->work_offset];

            err = MP3Decode(mf->hMP3Decoder, &inbuff, &bytes_left, &dest[*written], 0);
            consumed = (musicFileBytesLeft(mf) - bytes_left);
            mf->work_offset += consumed;

            if (consumed != encode_len)
            {
                printf("Consumed wrong number of bytes. Predicted: %u Actual: %u\n", encode_len, consumed);
            }

            if (err) 
            {
                printf("Error: %i\n", err);

                /* error occurred */
                switch (err) 
                {
                    case ERR_MP3_INDATA_UNDERFLOW:
                        state = ID_FATAL;
                    break;

                    case ERR_MP3_MAINDATA_UNDERFLOW:
                        /* do nothing - next call to decode will provide more mainData */
                        state = ID_OK;
                        break;

                    case ERR_MP3_FREE_BITRATE_SYNC:
                        // Will have frame of silence
                        state = ID_OK;
                    break;

                    default:
                        state = ID_FATAL;
                    break;
                }
            } 
            else 
            {
                // Written counts 16 bit samples
                *written += decode_len;

                printf("MP3Decode Consumed bytes: %u Generated bytes: %u\n", consumed, decode_len << 1); 
            }
        }
    } while (state == ID_OK);

    return (state == ID_FULL || state == ID_EOF);
}


/*
 * mp3FileFillReadBuffer
 * mf           Mp3 file structure
 * 
 * Copy existing data to start of buffer, write data from file into rest of buffer
 * 
 */
static void mp3FileFillReadBuffer(music_file* mf)
{
    int nRead;
    int bytes_left = musicFileBytesLeft(mf);

    // move last, small chunk from end of working buffer to start, then fill with new data
    if (bytes_left > 0)
    {
        memmove(mf->working, &mf->working[mf->work_offset], bytes_left);
    }

    // Read the new data
    if (f_read(&mf->fil, &mf->working[bytes_left], mf->working_len - bytes_left, &nRead) != FR_OK)
    {
        printf("FillReadBuffer error\n");
    }

    // Check for wrap or need to zero pad
    if (nRead < mf->working_len - bytes_left)
    {
#ifdef NO_LOOP
        // At end of file, so pad to avoid finding false         
        memset(&mf->working[bytes_left + nRead], 0, mf->working_len - bytes_left - nRead);

        // Set the correct number of bytes in working buffer in total
        mf->working_filled = nRead + bytes_left;
#else
        // Need to read rest from start
        if (f_lseek(&mf->fil, mf->file_offset) != FR_OK)
        {
            printf("FillReadBuffer seek error\n");
        }

        if (f_read(&mf->fil, &mf->working[bytes_left + nRead], mf->working_len - bytes_left - nRead, &nRead) != FR_OK)
        {
            printf("FillReadBuffer wrap error\n");
        }
        // Buffer is always full
        mf->working_filled = mf->working_len;
#endif        
    }
    else
    {
        mf->working_filled = mf->working_len;
    }

    // Back to start of working buffer
    mf->work_offset = 0;
}


/*
 * waveFileRead
 * mf           structure containing info for the wave_file instance
 * dest         Destination buffer to fill
 * working      An allocated cache buffer, used internally to store data read from file
 * len          number of 16 bit samples to copy to buffer
 * written      number of 16 bit samples written
 * 
 * Fill the destination buffer
 *      If mono source, just fill with single samples
 *      If stereo source, fill with with left and right interleaved samples
 * 
 */
bool waveFileRead(music_file* mf, uint16_t* dest, uint32_t len, uint32_t* written)
{

    // Read data into working buffer, then reformat and copy to destination buffer
    // until destination is full
    // When calculating the size of the read to issue it should be the smallest of:
    // 1) Remaining space in destination buffer
    // 2) Size of the cache buffer
    // 3) Data in file, before wrap
    // Sample = data for each supported channel (so 4 bytes for 16 bit stereo)

    uint32_t samples_left = (mf->channels==2) ? len >> 1 : len;   // Number of samples left to write to destination buffer
                                              // Note always write two samples (for l and r channels)
    uint32_t cache_samples_size = mf->working_len / mf->sample_size; // Number of samples that fit in read cache
    uint32_t data_index = 0;                           // Index into destination buffer
    uint32_t samples_to_read;                          // Number of samples to read from file next read instance
    uint32_t samples_to_wrap;                          // Samples left to read from file before reaching EOF
    UINT read;

    // Take the smaller of the read buffer size and the destination size
    while ((samples_left) && (mf->file_pos < mf->data_size))
    {
        // Calculate the number of samples that can be read before a file wrap
        samples_to_wrap = (mf->data_size - mf->file_pos) / mf->sample_size;

        // The smaller of the number to fill the destination, or the holding buffer
        samples_to_read = (samples_left > cache_samples_size) ? cache_samples_size : samples_left;
        samples_to_read = (samples_to_read > samples_to_wrap) ? samples_to_wrap : samples_to_read;

        FRESULT fr = f_read(&mf->fil, mf->working, samples_to_read * mf->sample_size, &read);
        if (FR_OK != fr || read != (mf->sample_size * samples_to_read))
        {
            printf("Read: %i Expected: %i\n", read, mf->sample_size * samples_to_read);
            printf("Error in f_read of sample %i \n", read);
            return false;
        }
        // Write the samples - store as 16 bit signed values
        switch (mf->bits_per_sample)
        {
            case 8:
                if (mf->channels == 2)
                {
                    for (int i = 0; i < samples_to_read; ++i)
                    {
                        dest[data_index++] = ((int16_t)(mf->working[i<<1])-0x80) << 8;
                        dest[data_index++] = ((int16_t)(mf->working[(i<<1)+1]-0x80)) << 8;
                    }
                }
                else
                {
                    // Mono, so write 1 sample
                    for (int i = 0; i < samples_to_read; ++i)
                    {
                        dest[data_index++] = ((uint16_t)(mf->working[i])-0x80) << 4;
                    }
                }
            break;

            case 16:
                if (mf->channels == 2)
                {
                    memcpy(&dest[data_index], mf->working, samples_to_read * sizeof(int16_t) * 2);
                    data_index += 2 * samples_to_read;
                }
                else
                {
                    // Mono, so only write one sample
                    memcpy(&dest[data_index], mf->working, samples_to_read * sizeof(int16_t));
                    data_index += samples_to_read;
                }
            break;

            case 32:
            {
                int32_t* cache_buffer_32 = (int32_t*)mf->working;
                uint32_t temp_l;
                uint32_t temp_r;
                if (mf->channels == 2)
                {
                    for (int i = 0; i < samples_to_read; ++i)
                    {
                        temp_l = cache_buffer_32[i<<1] >> 16;
                        temp_r = cache_buffer_32[(i<<1)+1] >> 16;
                        dest[data_index++] = (uint16_t)temp_l;
                        dest[data_index++] = (uint16_t)temp_r;
                    }
                }
                else
                {
                    // Mono, so only write one sample
                    for (int i = 0; i < samples_to_read; ++i)
                    {
                        temp_l = cache_buffer_32[i] >> 16;
                        dest[data_index++] = (uint16_t)temp_l;
                    }
                }
            }
            break;
        }
        // Update the current position in the file, and wrap if needed
        mf->file_pos += read;
        samples_left -= samples_to_read;

        if (mf->file_pos == mf->data_size)
        {
#ifdef NO_LOOP
            STATUS(("EOF\n"));
#else            
            STATUS(("file wrap\n"));

            // seek back to start of data in file
            if (f_lseek(&mf->fil, mf->file_offset) != FR_OK)
            {
                printf("waveFileRead seek error\n");
                return false;
            }
            mf->file_pos = 0;
#endif            
        }
    }
    // Either buffer is filled, or EOF reached
    *written = len - samples_left * mf->channels;

    return true;
}

/*
 * waveFileCheck
 * mf           Music file structure
 *
 * Reads the wav file header, validates it, and stores info into wave file structure
 */
static bool waveFileCheck(music_file* mf)
{
    uint32_t val32;
    uint16_t val16;
    int read;
    
    // http://soundfile.sapp.org/doc/WaveFormat/

    if (&mf->fil == NULL)
    {
        printf("Invalid file handle\n");
        return false;
    }

    // ChunkID
    if (!file_read(&mf->fil, &val32, 4, "ChunkID")) return false;
    STATUS(("(0-3)   Chunk ID: %c%c%c%c\n", ((char *)&val32)[0], ((char *)&val32)[1], ((char *)&val32)[2], ((char *)&val32)[3]));
    
    // Check for "RIFF"
    if (val32 != 0x46464952)
    {
        printf("Not RIFF file: %c%c%c%c\n", ((char *)&val32)[0], ((char *)&val32)[1], ((char *)&val32)[2], ((char *)&val32)[3]);
        return false;
    }

    // ChunkSize
    if (!file_read(&mf->fil, &val32, sizeof(val32), "ChunkSize")) return false;
    STATUS(("(4-7)   ChunkSize: bytes: %u, Kb: %u\n", val32, val32/1024));

    // Format
    if (!file_read(&mf->fil, &val32, 4, "Format")) return false;    
    STATUS(("(8-11)  Format: %c%c%c%c\n", ((char *)&val32)[0], ((char *)&val32)[1], ((char *)&val32)[2], ((char *)&val32)[3]));
    
    // Check for "WAVE"
    if (val32 != 0x45564157)
    {
        printf("Not WAV file: %c%c%c%c\n", ((char *)&val32)[0], ((char *)&val32)[1], ((char *)&val32)[2], ((char *)&val32)[3]);
        return false;
    }

    // Subchunk1ID
    if (!file_read(&mf->fil, &val32, 4, "Subchunk1ID")) return false;    
    STATUS(("(12-15) Fmt marker: %c%c%c%c\n", ((char *)&val32)[0], ((char *)&val32)[1], ((char *)&val32)[2], ((char *)&val32)[3]));
    
    // Check for "fmt "
    if (val32 != 0x20746d66)
    {
        printf("Unknown Subchunk1 format: %c%c%c%c\n", ((char *)&val32)[0], ((char *)&val32)[1], ((char *)&val32)[2], ((char *)&val32)[3]);
        return false;
    }

    // Subchunk1Size
    if (!file_read(&mf->fil, &val32, sizeof(val32), "Subchunk1Size")) return false;    
    STATUS(("(16-19) Subchunk1Size: %u\n", val32));
    
    if (val32 != 16)
    {
        printf("Unexpected Subchunk1Size: %u\n", val32);
        return false;
    }

    // AudioFormat
    if (!file_read(&mf->fil, &val16, sizeof(val16),"AudioFormat")) return false;    

#ifdef DEBUG_STATUS   
    char format_name[10] = "";

    if (val16 == 1)
        strcpy(format_name,"PCM"); 
    else if (val16 == 6)
        strcpy(format_name, "A-law");
    else if (val16 == 7)
        strcpy(format_name, "Mu-law");

    printf("(20-21) Format type: %u %s\n", val16, format_name);
#endif    
    if (val16 != 1)
    {
        printf("Unsupported audio format: %u\n", val16);
        return false;
    }

    // NumChannels
    if (!file_read(&mf->fil, &mf->channels, sizeof(mf->channels), "NumChannels")) return false;    
    STATUS(("(22-23) Channels: %u\n", mf->channels));
    
    if (mf->channels > 2 || mf->channels < 1)
    {
        printf("Unsupported number of channels: %u\n", mf->channels);
        return false;
    }

    // SampleRate
    if (!file_read(&mf->fil, &mf->sample_rate, sizeof(mf->sample_rate), "SampleRate")) return false;    
    STATUS(("(24-27) Sample rate: %u\n", mf->sample_rate));
    
    if (mf->sample_rate < 8000 || mf->sample_rate > 48000)
    {
        printf("Unsupported sample rate: %u\n", mf->sample_rate);
        return false;
    }

    // Byte rate
    if (!file_read(&mf->fil, &val32, sizeof(val32), "Byte rate")) return false;    
    STATUS(("(28-31) Byte Rate: %u\n", val32));

    // Block align
    if (!file_read(&mf->fil, &val16, sizeof(val16), "Block align")) return false;    
    STATUS(("(32-33) Block Alignment: %u\n", val16));

    // Bits per sample
    if (!file_read(&mf->fil, &mf->bits_per_sample, sizeof(mf->bits_per_sample), "Bits per sample")) return false;    
    STATUS(("(34-35) Bits per sample: %u\n", mf->bits_per_sample));

    if (mf->bits_per_sample != 8 && mf->bits_per_sample != 16 && mf->bits_per_sample != 32)
    {
        printf("unsupported bits per sample: %u\n", mf->bits_per_sample);
        return false;
    }

    // Subchunk2ID
    bool found_data = false;
    uint32_t file_offset = 36;
    while (!found_data)
    {
        file_read(&mf->fil, &val32, 4, "Subchunk2ID");    
        STATUS(("(%u-%u) Data marker: %c%c%c%c\n", file_offset, file_offset+3, ((char *)&val32)[0], ((char *)&val32)[1], ((char *)&val32)[2], ((char *)&val32)[3]));
        file_offset += 4;

        // Check for "data"
        if (val32 != 0x61746164)
        {
            printf("Ignoring unknown subchunk2 format: %c%c%c%c\n", ((char *)&val32)[0], ((char *)&val32)[1], ((char *)&val32)[2], ((char *)&val32)[3]);

            // Read the sub chunk size
            if (!file_read(&mf->fil, &mf->data_size, sizeof(mf->data_size), "Subchunk2Size")) return false;    
            STATUS(("(%u-%u) Subchunk2Size: %u\n", file_offset, file_offset+3, mf->data_size));
            file_offset += (4 + mf->data_size);
            
            // skip to next chunk
            if (f_lseek(&mf->fil, file_offset) != FR_OK)
            {
                printf("seek error in waveFileCheck\n");
                return false;
            }
        }
        else
        {
            found_data = true;
        }
    }

    // Subchunk2Size
    if (!file_read(&mf->fil, &mf->data_size, sizeof(mf->data_size), "Subchunk2Size")) return false;    
    STATUS(("(%u-%u) Subchunk2Size: %u\n", file_offset, file_offset+3, mf->data_size));
    file_offset += 4;
    
    mf->file_offset = file_offset;
    mf->file_pos = 0;
    mf->sample_size = mf->channels * mf->bits_per_sample / 8;

#ifdef DEBUG_STATUS    
    // calculate no.of samples
    uint num_samples = mf->data_size / (mf->channels * mf->bits_per_sample / 8);
    printf("Number of samples: %u\n", num_samples);

    // calculate duration of file
    float duration_in_seconds = (float) mf->data_size / (mf->channels * mf->sample_rate * mf->bits_per_sample / 8);
    printf("Duration in seconds = %f\n", duration_in_seconds);
#endif    

    return true;
}

/*
 * file_read
 * fil      File structure     
 * buffer   Buffer to read data into from file
 * size     Number of bytes to read
 * msg      Message to write if read fails
 * 
 * Read specified number of bytes into the supplied buffer.
 * Print an error message if read fails
 */
static bool file_read(FIL* fil, void* buffer, uint size, const char* msg)
{
    uint read;
    bool ret = true;

    FRESULT fr = f_read(fil, buffer, size, &read);
    if (FR_OK != fr || read != size)
    {
        printf("Error in f_read %s \n", msg);
        ret = false;
    }
    return ret;
}
/* 
 * mp3FileSkipId3v2
 * mf           Mp3 file structure
 * 
 * Skip idv2 tags - which are at start of file, so offset should be 0, but allow for any value
 * 
 */
static uint32_t mp3FileSkipId3v2(music_file* mf) 
{

    uint32_t skip = 0;
    if ((mf->working_filled - mf->work_offset) >= 10) 
    {
        
        if ((mf->working[mf->work_offset] == 'I' &&
            mf->working[mf->work_offset + 1] == 'D' &&
            mf->working[mf->work_offset + 2] == '3' &&
            mf->working[mf->work_offset + 3] != 0xff &&
            mf->working[mf->work_offset + 4] != 0xff &&
            (mf->working[mf->work_offset + 5] & 0x1f) == 0 &&
            (mf->working[mf->work_offset + 6] & 0x80) == 0 &&
            (mf->working[mf->work_offset + 7] & 0x80) == 0 &&
            (mf->working[mf->work_offset + 8] & 0x80) == 0 &&
            (mf->working[mf->work_offset + 9] & 0x80) == 0)) 
        {
            skip = (mf->working[mf->work_offset + 6] << 21) | (mf->working[mf->work_offset + 7] << 14) | (mf->working[mf->work_offset + 8] << 7) | (mf->working[mf->work_offset + 9]);
            skip += 10; // size excludes the "header" (but not the "extended header")
            STATUS(("id V2 found. Skip: %u\n", skip));
        }
    }
    return skip;
}

/* 
 * mp3FileMoveToSyncWord 
 * mf           Pointer to the mp3 header
 * 
 * Finds the next valid mp3 file header and moves the pointers to it
 *      Returns ID_OK on success
 *              ID_EOF if EOF reached before valid frame found
 *              ID_NO_SYNC if no valid frame found in MPG_FILE_MAX_EMPTY_FRAMES frames
 * 
 */
static mp3FrameError mp3FileMoveToSyncWord(music_file* mf)
{
    int offset;
    int repeat = 0;

    do 
    {
        offset = MP3FindSyncWord(&mf->working[mf->work_offset], musicFileBytesLeft(mf));

        if (offset < 0)
        {
            // Buffer is exhausted
            mf->work_offset = mf->working_filled;

            // Refill
            mp3FileFillReadBuffer(mf);

#ifdef NO_LOOP
            // Check for EOF
            if (!musicFileBytesLeft(mf))
            {
                return ID_EOF;
            }
#endif
            // Avoid an infinite loop when wrapping for a file with no frames
            if (repeat > MPG_FILE_MAX_EMPTY_FRAMES)
            {
                return ID_NO_SYNC;
            }
            repeat++;
        }
    }
    while (offset < 0);

    mf->work_offset += offset;

    return ID_OK;
}

/* 
 * mp3FileDecodeSyncWord 
 * mf           Pointer to the mp3 header
 * mp3_info     Pointer to mp3 info block to be filled
 * encode_len   Returns the length of the encoded mp3 frame in bytes
 * decode_len   Returns the length of the decoded mp3 frame in bytes
 * 
 * Finds the next valid mp3 file header
 *      Returns info on frame, size in bytes, 0 if invalid frame
 * 
 */
static mp3FrameError mp3FileDecodeSyncWord(music_file* mf, MP3FrameInfo* mp3_info, uint16_t* encode_len, uint16_t* decode_len)
{
    mp3FrameError state;
    int err;
    int repeat = 0;

    do
    {
        // Find sync word
        state = mp3FileMoveToSyncWord(mf);

        if (state != ID_OK)
        {
            return state;
        }

        // Process the sync word
        err = MP3GetNextFrameInfo(mf->hMP3Decoder, mp3_info, &mf->working[mf->work_offset]);

        if (err == ERR_MP3_INVALID_FRAMEHEADER)
        {
            // Try again 
            mf->work_offset++;
        }
        else if (err == ERR_MP3_UNSUPPORTED)
        {
            // Unsupported file type, so give up immediately
            return ID_INVALID_FILE_TYPE;
        }
        else if (err == ERR_MP3_NONE)
        {
            *encode_len = mp3_info->size;
        }
        
    } while (err != ERR_MP3_NONE);

    *decode_len = ((mp3_info->bitsPerSample >> 3) * mp3_info->outputSamps);
    return ID_OK;
}
