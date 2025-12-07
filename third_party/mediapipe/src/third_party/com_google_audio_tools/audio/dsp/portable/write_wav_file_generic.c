/* For details on the WAV file format, see for instance
 * http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html.
 */

#include "audio/dsp/portable/write_wav_file_generic.h"

#include "audio/dsp/portable/serialize.h"

#define kBitsPerSample 16
#define kWavFmtExtensionCode 0xFFFE
#define kWavPcmCode 1
#define kWavPcmGuid "\x00\x00\x00\x00\x10\x00\x80\x00\x00\xAA\x00\x38\x9B\x71"

static void WriteWithErrorCheck(const void* bytes, size_t num_bytes,
                                WavWriter* w) {
  size_t written_bytes = w->write_fun(bytes, num_bytes, w->io_ptr);
  w->has_error |= num_bytes != written_bytes;
}

/* Mimics putc. */
static void WriteUnsignedChar(int value, WavWriter* w) {
  char value_char = value & 0xFF;
  WriteWithErrorCheck(&value_char, 1, w);
}

/* Mimics fputs. */
static void WriteString(const char* str, WavWriter* w) {
  int length = 0;
  const char* str2 = str;
  /* Compute the length of the string in bytes. */
  while (*(str2++)) { ++length; }
  WriteWithErrorCheck(str, length, w);
}

/* Write uint16_t in little endian order. */
static void WriteUint16(uint16_t value, WavWriter* w) {
  uint8_t bytes[2];
  LittleEndianWriteU16(value, bytes);
  WriteWithErrorCheck(bytes, 2, w);
}

/* Write uint32_t in little endian order. */
static void WriteUint32(uint32_t value, WavWriter* w) {
  uint8_t bytes[4];
  LittleEndianWriteU32(value, bytes);
  WriteWithErrorCheck(bytes, 4, w);
}

int WriteWavHeaderGeneric(WavWriter* w, size_t num_samples, int sample_rate_hz,
                          int num_channels) {
  /* The fmt chunk extension should be used when num_channels is more than 2. */
  const int extended = num_channels > 2;
  const uint32_t fmt_chunk_size = extended ? 40 : 16;
  const uint32_t data_chunk_size = (kBitsPerSample / 8) * num_samples;
  const int block_align = num_channels * (kBitsPerSample / 8);

  if (w == NULL || w->io_ptr == NULL) {
    return 0;
  }
  w->has_error = 0;  /* Clear the error flag. */
  WriteString("RIFF", w);
  /* Write the size of the file minus the 8 bytes for the "RIFF" ID and RIFF
   * chunk size. The constant 20 counts the "WAVE" ID, "fmt " ID, fmt chunk
   * size, "data" ID, and data chunk size.
   */
  WriteUint32(20 + fmt_chunk_size + (extended ? 12 : 0) + data_chunk_size, w);
  WriteString("WAVE", w);
  if (w->has_error) { return 0; }

  /* Write fmt chunk. */
  WriteString("fmt ", w);
  WriteUint32(fmt_chunk_size, w);
  /* Either set linear PCM format or indicate presence of fmt extension. */
  WriteUint16(extended ? kWavFmtExtensionCode : kWavPcmCode, w);
  WriteUint16(num_channels, w);
  WriteUint32(sample_rate_hz, w);
  WriteUint32(block_align * sample_rate_hz, w); /* Average bytes/s. */
  WriteUint16(block_align, w); /* Number of bytes for one block. */
  WriteUint16(kBitsPerSample, w);
  if (w->has_error) { return 0; }

  if (extended) {
    WriteUint16(22, w);             /* Size of the fmt extension. */
    WriteUint16(kBitsPerSample, w); /* Valid bits per sample. */
    WriteUint32(0, w);              /* Channel mask. */
    WriteUint16(kWavPcmCode, w);    /* Set linear PCM sample format. */
    WriteWithErrorCheck(kWavPcmGuid, 14, w);

    /* Also write a fact chunk when using fmt extension. */
    WriteString("fact", w);
    WriteUint32(4, w);
    WriteUint32(num_samples / num_channels, w);
  }
  if (w->has_error) { return 0; }

  /* Write data chunk. */
  WriteString("data", w);
  WriteUint32(data_chunk_size, w);
  if (w->has_error) { return 0; }

  return 1;
}

int WriteWavSamplesGeneric(WavWriter* w, const int16_t* samples,
                           size_t num_samples) {
  int i;
  if (w == NULL || w->io_ptr == NULL || samples == NULL) {
    return 0;
  }
  w->has_error = 0;  /* Clear the error flag. */
  for (i = 0; i < num_samples; ++i) {
    WriteUint16(samples[i], w);
  }
  if (w->has_error) { return 0; }

  return 1;
}
