// WAV file header reader and audio player for I2S mode
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "command_struct.h"
#include "fatfs/ff.h"
#include "pirate/storage.h"
#include "lib/bp_args/bp_cmd.h"
#include "ui/ui_help.h"
#include "system_config.h"
#include "hardware/pio.h"
#include "pio_config.h"
#include "mode/i2s.h"

static const char* const usage[] = { "wav -f <file>",
                                     "Read WAV file header and play audio:%s wav -f test.wav" };

static const bp_command_opt_t wav_opts[] = {
    { "file", 'f', BP_ARG_REQUIRED, NULL, T_HELP_DUMMY_COMMANDS },
    { 0 }
};

const bp_command_def_t wav_def = {
    .name         = "wav",
    .description  = T_HELP_DUMMY_COMMANDS,
    .opts         = wav_opts,
    .positionals      = NULL,
    .positional_count = 0,
    .usage        = usage,
    .usage_count  = count_of(usage),
};

// WAV file header structure
typedef struct {
    char chunk_id[4];        // "RIFF"
    uint32_t chunk_size;     // Size of the entire file minus 8 bytes
    char format[4];          // "WAVE"
    char subchunk1_id[4];    // "fmt "
    uint32_t subchunk1_size; // Size of the fmt chunk (16 for PCM)
    uint16_t audio_format;   // Audio format (1 for PCM)
    uint16_t num_channels;   // Number of channels (1 = mono, 2 = stereo)
    uint32_t sample_rate;    // Sampling rate (e.g., 44100 Hz)
    uint32_t byte_rate;      // Byte rate = SampleRate * NumChannels * BitsPerSample / 8
    uint16_t block_align;    // Block align = NumChannels * BitsPerSample / 8
    uint16_t bits_per_sample;// Bits per sample (e.g., 16 bits)
    char subchunk2_id[4];    // "data"
    uint32_t subchunk2_size; // Size of the data chunk (number of bytes of audio data)
} wav_header_t;

void wav_handler(struct command_result* res) {
    char file[13];

    if (bp_cmd_help_check(&wav_def, res->help_flag)) {
        return;
    }

    FIL file_handle;
    FRESULT fresult;
    wav_header_t header;
    bool f_flag = bp_cmd_get_string(&wav_def, 'f', file, sizeof(file));

    if (!f_flag) {
        printf("Set a file name with -f flag to read a WAV file header.\r\n");
        printf("Example: wav -f test.wav\r\n");
        return;
    }

    fresult = f_open(&file_handle, file, FA_READ);
    if (fresult != FR_OK) {
        printf("Error opening file %s for reading\r\n", file);
        system_config.error = true;
        return;
    }
    printf("File %s opened for reading\r\n", file);

    UINT bytes_read;
    fresult = f_read(&file_handle, &header, sizeof(header), &bytes_read);
    if (fresult == FR_OK) {
        printf("Read %d bytes from file %s\r\n", bytes_read, file);
    } else {
        printf("Error reading file %s\r\n", file);
        return;
    }

    printf("Chunk ID: %.4s\r\n", header.chunk_id);
    printf("Chunk Size: %u\r\n", header.chunk_size);
    printf("Format: %.4s\r\n", header.format);
    printf("Subchunk1 ID: %.4s\r\n", header.subchunk1_id);
    printf("Subchunk1 Size: %u\r\n", header.subchunk1_size);
    printf("Audio Format: %u\r\n", header.audio_format);
    printf("Number of Channels: %u\r\n", header.num_channels);
    printf("Sample Rate: %u\r\n", header.sample_rate);
    printf("Byte Rate: %u\r\n", header.byte_rate);
    printf("Block Align: %u\r\n", header.block_align);
    printf("Bits Per Sample: %u\r\n", header.bits_per_sample);
    printf("Subchunk2 ID: %.4s\r\n", header.subchunk2_id);
    printf("Subchunk2 Size: %u\r\n", header.subchunk2_size);

    int16_t audio_data[1000];
    fresult = f_read(&file_handle, audio_data, sizeof(audio_data), &bytes_read);
    if (fresult == FR_OK) {
        printf("Read %d bytes of audio data from file %s\r\n", bytes_read, file);
    } else {
        printf("Error reading audio data from file %s\r\n", file);
        return;
    }

    uint32_t samples = 1000;

    for (uint32_t cnt = 0; cnt < 5; cnt++) {
        printf("Pushing %d samples to PIO\r\n", samples);
        for (uint32_t i = 0; i < samples; i++) {
            pio_sm_put_blocking(i2s_pio_config_out.pio, i2s_pio_config_out.sm, (audio_data[i]));
        }
    }

    fresult = f_close(&file_handle);
    if (fresult != FR_OK) {
        printf("Error closing file %s\r\n", file);
        system_config.error = true;
        return;
    }
    printf("File %s closed\r\n", file);
}
