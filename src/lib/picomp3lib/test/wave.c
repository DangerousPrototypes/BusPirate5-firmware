#include "wave.h"

bool writeWavHeader(FIL* fo, uint32_t sample_rate, uint16_t num_channels)
{
    uint32_t val32;
    uint16_t val16;
    UINT written;

    // 0 RIFF
    val32 = 0x46464952;
    if ((f_write(fo, &val32, sizeof(uint32_t), &written) != FR_OK) &&
        (written != sizeof(uint32_t)))
    {
        return false;
    }

    // 4 cksize - to be written at end
    val32 = 0;
    if ((f_write(fo, &val32, sizeof(uint32_t), &written) != FR_OK) &&
        (written != sizeof(uint32_t)))
    {
        return false;
    }

    // 8 WAVE
    val32 = 0x45564157;
    if ((f_write(fo, &val32, sizeof(uint32_t), &written) != FR_OK) &&
        (written != sizeof(uint32_t)))
    {
        return false;
    }

    // 12 fmt
    val32 = 0x20746d66;
    if ((f_write(fo, &val32, sizeof(uint32_t), &written) != FR_OK) &&
        (written != sizeof(uint32_t)))
    {
        return false;
    }

    // 16 Subchunk1Size
    val32 = 16;
    if ((f_write(fo, &val32, sizeof(uint32_t), &written) != FR_OK) &&
        (written != sizeof(uint32_t)))
    {
        return false;
    }

    // 20 Audio format - PCM
    val16 = 1;
    if ((f_write(fo, &val16, sizeof(uint16_t), &written) != FR_OK) &&
        (written != sizeof(uint16_t)))
    {
        return false;
    }

    // 22 Number of channels
    if ((f_write(fo, &num_channels, sizeof(uint16_t), &written) != FR_OK) &&
        (written != sizeof(uint16_t)))
    {
        return false;
    }

    // 24 Sample rate
    if ((f_write(fo, &sample_rate, sizeof(uint32_t), &written) != FR_OK) &&
        (written != sizeof(uint32_t)))
    {
        return false;
    }

    // 28 Byte rate per sec = num_channel * sample_rate * sample_size_in_bytes
    val32 = sample_rate * num_channels * 2;
    if ((f_write(fo, &val32, sizeof(uint32_t), &written) != FR_OK) &&
        (written != sizeof(uint32_t)))
    {
        return false;
    }

    // 32 Block alignment
    val16 = (uint16_t)(num_channels * 2);
    if ((f_write(fo, &val16, sizeof(uint16_t), &written) != FR_OK) &&
        (written != sizeof(uint16_t)))
    {
        return false;
    }

    // 34 Bits per sample
    val16 = 16;
    if ((f_write(fo, &val16, sizeof(uint16_t), &written) != FR_OK) &&
        (written != sizeof(uint16_t)))
    {
        return false;
    }

    // 36 data
    val32 = 0x61746164;
    if ((f_write(fo, &val32, sizeof(uint32_t), &written) != FR_OK) &&
        (written != sizeof(uint32_t)))
    {
        return false;
    }

    // 40 Size of data - write 0, then update at end
    val32 = 0;
    if ((f_write(fo, &val32, sizeof(uint32_t), &written) != FR_OK) &&
        (written != sizeof(uint32_t)))
    {
        return false;
    }

    return true;
}

bool updateWavHeader(FIL* fo, uint32_t num_samples, uint16_t num_channels)
{
    uint32_t val32;
    UINT written;

    if (f_lseek(fo, 4) != FR_OK)
        return false;

    val32 = 36 + num_samples * num_channels * sizeof(int16_t);
    if ((f_write(fo, &val32, sizeof(uint32_t), &written) != FR_OK) &&
        (written != sizeof(uint32_t)))
    {
        return false;
    }

    if (f_lseek(fo, 40) != FR_OK)
        return false;

    val32 = num_samples * num_channels * sizeof(int16_t);
    if ((f_write(fo, &val32, sizeof(uint32_t), &written) != FR_OK) &&
        (written != sizeof(uint32_t)))
    {
        return false;
    }

    return true;
}

