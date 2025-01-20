/* For details on the WAV file format, see for instance
 * http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html.
 */

#include "audio/dsp/portable/read_wav_file_generic.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio/dsp/portable/logging.h"
#include "audio/dsp/portable/serialize.h"

#define kBitsPerSample 16
#define kWavFmtChunkMinSize 16
#define kWavFmtExtensionCode 0xFFFE
#define kWavPcmCode 1
#define kWavIeeeFloatingPointCode 3
#define kWavMulawCode 7
#define kWavPcmGuid "\x00\x00\x00\x00\x10\x00\x80\x00\x00\xAA\x00\x38\x9B\x71"
#define kWavFactChunkSize 4

/* We assume IEEE 754 floats. Statically assert that `sizeof(float) == 4`. */
typedef char kReadWaveFileStaticAssert_SIZEOF_FLOAT_MUST_EQUAL_4
    [(sizeof(float) == 4) ? 1 : -1];

static void ReadWithErrorCheck(void* bytes, size_t num_bytes, WavReader* w) {
  size_t read_bytes = w->read_fun(bytes, num_bytes, w->io_ptr);
  if (num_bytes != read_bytes) {
    w->has_error = 1;
    if (w->eof_fun != NULL && w->eof_fun(w->io_ptr)) {
      LOG_ERROR("Error: WAV file ended unexpectedly.\n");
    }
  }
}

static void SeekWithErrorCheck(size_t num_bytes, WavReader* w) {
  int failure = 0;
  if (w->seek_fun != NULL) {
    failure = w->seek_fun(num_bytes, w->io_ptr);
  } else {
    /* Allocate small buffer even for large seeks. */
    char dummy[256];
    for (; num_bytes > sizeof(dummy); num_bytes -= sizeof(dummy)) {
      if (w->read_fun(dummy, sizeof(dummy), w->io_ptr) != sizeof(dummy)) {
        w->has_error = 1;
        return;
      }
    }
    failure = w->read_fun(dummy, num_bytes, w->io_ptr) != num_bytes;
  }
  if (failure) {
    w->has_error = 1;
    if (w->eof_fun != NULL && w->eof_fun(w->io_ptr)) {
      LOG_ERROR("Error: WAV file ended unexpectedly.\n");
    }
  }
}

static uint8_t ReadUint8(WavReader* w) {
  uint8_t byte;
  ReadWithErrorCheck(&byte, 1, w);
  return byte;
}

static int16_t ReadMulaw(WavReader* w) {
  static const int16_t mulaw_table[0x100] = {
  -32124, -31100, -30076, -29052, -28028, -27004, -25980, -24956,
  -23932, -22908, -21884, -20860, -19836, -18812, -17788, -16764,
  -15996, -15484, -14972, -14460, -13948, -13436, -12924, -12412,
  -11900, -11388, -10876, -10364,  -9852,  -9340,  -8828,  -8316,
   -7932,  -7676,  -7420,  -7164,  -6908,  -6652,  -6396,  -6140,
   -5884,  -5628,  -5372,  -5116,  -4860,  -4604,  -4348,  -4092,
   -3900,  -3772,  -3644,  -3516,  -3388,  -3260,  -3132,  -3004,
   -2876,  -2748,  -2620,  -2492,  -2364,  -2236,  -2108,  -1980,
   -1884,  -1820,  -1756,  -1692,  -1628,  -1564,  -1500,  -1436,
   -1372,  -1308,  -1244,  -1180,  -1116,  -1052,   -988,   -924,
    -876,   -844,   -812,   -780,   -748,   -716,   -684,   -652,
    -620,   -588,   -556,   -524,   -492,   -460,   -428,   -396,
    -372,   -356,   -340,   -324,   -308,   -292,   -276,   -260,
    -244,   -228,   -212,   -196,   -180,   -164,   -148,   -132,
    -120,   -112,   -104,    -96,    -88,    -80,    -72,    -64,
     -56,    -48,    -40,    -32,    -24,    -16,     -8,      0,
   32124,  31100,  30076,  29052,  28028,  27004,  25980,  24956,
   23932,  22908,  21884,  20860,  19836,  18812,  17788,  16764,
   15996,  15484,  14972,  14460,  13948,  13436,  12924,  12412,
   11900,  11388,  10876,  10364,   9852,   9340,   8828,   8316,
    7932,   7676,   7420,   7164,   6908,   6652,   6396,   6140,
    5884,   5628,   5372,   5116,   4860,   4604,   4348,   4092,
    3900,   3772,   3644,   3516,   3388,   3260,   3132,   3004,
    2876,   2748,   2620,   2492,   2364,   2236,   2108,   1980,
    1884,   1820,   1756,   1692,   1628,   1564,   1500,   1436,
    1372,   1308,   1244,   1180,   1116,   1052,    988,    924,
     876,    844,    812,    780,    748,    716,    684,    652,
     620,    588,    556,    524,    492,    460,    428,    396,
     372,    356,    340,    324,    308,    292,    276,    260,
     244,    228,    212,    196,    180,    164,    148,    132,
     120,    112,    104,     96,     88,     80,     72,     64,
      56,     48,     40,     32,     24,     16,      8,      0,
  };
  return mulaw_table[ReadUint8(w)];
}

static uint16_t ReadUint16(WavReader* w) {
  uint8_t bytes[2];
  ReadWithErrorCheck(bytes, 2, w);
  return LittleEndianReadU16(bytes);
}

/* Reads 24 bits of data into the MSBs of a 32-bit int. */
static uint32_t Read3BytesIntoUint32(WavReader* w) {
  uint8_t bytes[3];
  ReadWithErrorCheck(bytes, 3, w);
  return (((uint32_t) bytes[0]) << 8)
      | (((uint32_t) bytes[1]) << 16)
      | (((uint32_t) bytes[2]) << 24);
}

static uint32_t ReadUint32(WavReader* w) {
  uint8_t bytes[4];
  ReadWithErrorCheck(bytes, 4, w);
  return LittleEndianReadU32(bytes);
}

static float ReadFloat32(WavReader* w) {
  uint8_t bytes[4];
  ReadWithErrorCheck(bytes, 4, w);
  return LittleEndianReadF32(bytes);
}

static float ReadFloat64IntoFloat32(WavReader* w) {
  uint8_t bytes[8];
  ReadWithErrorCheck(bytes, 8, w);
  return (float)LittleEndianReadF64(bytes);
}

static int ReadWavFmtChunk(WavReader* w, ReadWavInfo* info,
                           uint32_t chunk_size) {
  if (chunk_size < kWavFmtChunkMinSize) {
    LOG_ERROR("Error: WAV has invalid format chunk.\n");
    return 0;
  }
  uint16_t format_code = ReadUint16(w);
  const uint16_t num_channels = ReadUint16(w);
  const uint32_t sample_rate_hz = ReadUint32(w);
  SeekWithErrorCheck(4, w);  /* Skip average bytes per second field. */
  const uint16_t block_align = ReadUint16(w);
  const uint16_t significant_bits_per_sample = ReadUint16(w);

  if (format_code == kWavFmtExtensionCode && chunk_size >= 26) {
    SeekWithErrorCheck(8, w);  /* Skip to the format code. */
    format_code =  ReadUint16(w);
    SeekWithErrorCheck(chunk_size - 26, w);
  } else {
    SeekWithErrorCheck(chunk_size - 16, w);
  }

  if (w->has_error) { return 0; }

  if (num_channels == 0) {
    LOG_ERROR("Error: Invalid WAV. Channels not specified.\n");
    return 0;
  }
  if (block_align != (significant_bits_per_sample / 8) * num_channels) {
    /* The block alignment actually isn't used, so this doesn't guarantee a
     * problem with the data. It could just be a header problem.
     */
    LOG_ERROR("Error: Block alignment is incorrectly specified.\n");
  }

  switch (format_code) {
    case kWavPcmCode:
      if (significant_bits_per_sample == 16) {
        info->encoding = kPcm16Encoding;
        info->destination_alignment_bytes = 2 /* 16-bit int */;
        info->sample_format = kInt16;
      } else if (significant_bits_per_sample == 24) {
        info->encoding = kPcm24Encoding;
        info->destination_alignment_bytes = 4 /* 32-bit int */;
        info->sample_format = kInt32;
      } else {
        LOG_ERROR("Error: Only 16 and 24 bit PCM data is supported.\n");
        return 0;
      }
      break;
    case kWavIeeeFloatingPointCode:
      if (significant_bits_per_sample == 32) {
        info->encoding = kIeeeFloat32Encoding;
        info->destination_alignment_bytes = 4 /* 32-bit float */;
        info->sample_format = kFloat;
      } else if (significant_bits_per_sample == 64) {
        info->encoding = kIeeeFloat64Encoding;
        info->destination_alignment_bytes = 4 /* We will write to 32-bits. */;
        info->sample_format = kFloat;
      } else {
        LOG_ERROR("Error: Only 32-bit or 64-bit floating point data is "
                  "supported.\n");
        return 0;
      }
      break;
    case kWavMulawCode:
      info->encoding = kMuLawEncoding;
      info->destination_alignment_bytes = 2 /* 16-bit int after decoding */;
      info->sample_format = kInt16;
      if (significant_bits_per_sample != 8) {
        LOG_ERROR("Error: Mulaw data must be 8 bits per sample.\n");
        return 0;
      }
      break;
    default:
      LOG_ERROR("Error: Only PCM and mu-law formats are currently "
                "supported.\n");
      return 0;
  }
  info->num_channels = num_channels;
  info->sample_rate_hz = sample_rate_hz;
  info->bit_depth = significant_bits_per_sample;
  return 1;
}

static int ReadWavFactChunk(WavReader* w, ReadWavInfo* info,
                            uint32_t chunk_size) {
  /* fact chunk contains only the number of samples per channel for a WAV stored
   * in floating point format. Overwrite the value that may already be
   * written.
   */
  if (chunk_size != kWavFactChunkSize ||
      /* Prevent division by zero. */
      info->num_channels == 0) {
    LOG_ERROR("Error: WAV has invalid fact chunk.\n");
    return 0;
  }
  uint32_t num_frames = ReadUint32(w);

  /* Prevent overflow. */
  if (num_frames > SIZE_MAX / info->num_channels) {
    LOG_ERROR("Error: Number of WAV samples exceeds %zu.\n", SIZE_MAX);
    return 0;
  }

  info->remaining_samples = info->num_channels * num_frames;
  return 1;
}

int ReadWavHeaderGeneric(WavReader* w, ReadWavInfo* info) {
  /* The Resource Interchange File Format (RIFF) is a file structure that has
   * tagged chunks of data that allows for future-proofing. Chunks with
   * unrecognized headers can be skipped without throwing an error. In the case
   * of WAV, we are looking for chunks labeled "fmt" and "data".
   * RIFF is not specific to WAV, more information can be found here:
   * http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/Docs/riffmci.pdf
   */
  char id[4];
  if (w == NULL || w->io_ptr == NULL || info == NULL) {
    goto fail;
  }
  w->has_error = 0;  /* Clear the error flag. */

  /* WAV file should begin with "RIFF". */
  ReadWithErrorCheck(id, 4, w);
  if (memcmp(id, "RIFF", 4) != 0) {
    if (memcmp(id, "RIFX", 4) == 0) {
      LOG_ERROR("Error: Big endian WAV is unsupported.\n");
    } else {
      LOG_ERROR("Error: Expected a WAV file.\n");
    }
    goto fail;
  }
  SeekWithErrorCheck(4, w);  /* Skip RIFF size, since it is unreliable. */
  ReadWithErrorCheck(id, 4, w);
  if (memcmp(id, "WAVE", 4) != 0) {
    LOG_ERROR("Error: WAV file has invalid RIFF type.\n");
    goto fail;
  }

  if (w->has_error) { goto fail; }

  info->num_channels = 0;
  uint8_t read_fact_chunk = 0;
  /* Loop until data chunk is found. Each iteration reads one chunk. */
  while (1) {
    uint32_t chunk_size;
    ReadWithErrorCheck(id, 4, w);
    chunk_size = ReadUint32(w);

    if (memcmp(id, "fmt ", 4) == 0) {  /* Read format chunk. */
      if (!ReadWavFmtChunk(w, info, chunk_size)) {
        goto fail;
      }
    } else if (memcmp(id, "fact", 4) == 0) {  /* Read fact chunk. */
      read_fact_chunk = 1;
      if (!ReadWavFactChunk(w, info, chunk_size)) {
        goto fail;
      }
    } else if (memcmp(id, "data", 4) == 0) {  /* Found data chunk. */
      if (info->num_channels == 0) {  /* fmt chunk hasn't been read yet. */
        LOG_ERROR("Error: WAV has unsupported chunk order.\n");
        goto fail;
      }

      const uint32_t num_samples = chunk_size / (info->bit_depth / 8);
      if (UINT32_MAX > SIZE_MAX && num_samples > SIZE_MAX) {
        LOG_ERROR("Error: Number of WAV samples exceeds %zu.\n", SIZE_MAX);
        goto fail;
      }
      size_t remaining_samples = (size_t)num_samples;

      if (read_fact_chunk && remaining_samples != info->remaining_samples) {
        LOG_ERROR("Error: WAV fact and data chunks indicate different data "
                  "size. Using size from data chunk.\n");
      }
      info->remaining_samples = remaining_samples;

      return 1;
    } else {  /* Handle unknown chunk. */
      if (w->custom_chunk_fun != NULL) {
        uint8_t* extra_chunk_bytes = malloc(chunk_size * sizeof(uint8_t));
        if (extra_chunk_bytes == NULL) {
          LOG_ERROR("Error: Failed to allocate memory\n");
          goto fail;
        }
        ReadWithErrorCheck(extra_chunk_bytes, chunk_size, w);
        w->custom_chunk_fun(&id, extra_chunk_bytes, chunk_size, w->io_ptr);
        free(extra_chunk_bytes);
      } else {
        SeekWithErrorCheck(chunk_size, w);
      }
    }
    if (w->has_error) { goto fail; }
  }

fail:
  if (info != NULL) {
    info->num_channels = 0;
    info->sample_rate_hz = 0;
    info->remaining_samples = 0;
  }
  w->has_error = 1;
  return 0;
}

static size_t ReadBytesAsSamples(WavReader* w, ReadWavInfo* info,
                                 char* dst_samples, size_t num_samples) {
  size_t samples_to_read;
  size_t current_sample;
  if (info->destination_alignment_bytes != 2 &&
      info->destination_alignment_bytes != 4) {
    LOG_ERROR("Error: Destination alignment must be 2 or 4 bytes.\n");
    return 0;
  }
  if (((size_t) dst_samples) % info->destination_alignment_bytes != 0) {
    /* The `samples` pointer passed to the WAV reader must have alignment strict
     * enough for the sample type, unaligned pointers can cause undefined
     * behavior. */
    LOG_ERROR("Error: Data pointer must be aligned to the element size.\n");
    return 0;
  }

  if (w == NULL || w->io_ptr == NULL || info == NULL ||
      info->remaining_samples < (size_t)info->num_channels ||
      dst_samples == NULL || num_samples <= 0) {
    return 0;
  }
  w->has_error = 0;  /* Clear the error flag. */

  size_t src_alignment_bytes = info->bit_depth / 8;

  samples_to_read = info->remaining_samples;
  if (num_samples < samples_to_read) {
    samples_to_read = num_samples;
  }
  samples_to_read -= samples_to_read % info->num_channels;

  /* Prevent overflow. */
  if (samples_to_read > SIZE_MAX / info->destination_alignment_bytes) {
    LOG_ERROR("Error: WAV samples data exceeds %zu bytes.\n", SIZE_MAX);
    return 0;
  }

  for (current_sample = 0; current_sample < samples_to_read; ++current_sample) {
    switch (info->destination_alignment_bytes) {
      case 2:
        switch (src_alignment_bytes) {
          case 1:
            ((int16_t*)dst_samples)[current_sample] = ReadMulaw(w);
            break;
          case 2:
            /* Read 16-bit ints into a 16-bit container. */
            ((int16_t*)dst_samples)[current_sample] = (int16_t)ReadUint16(w);
            break;
        }
        break;
      case 4:
        switch (src_alignment_bytes) {
          case 1:
            ((int32_t*)dst_samples)[current_sample] =
                ((int32_t)ReadMulaw(w)) << 16;
            break;
          case 2:
            /* Read 16-bit ints into a 32-bit container. */
            ((int32_t*)dst_samples)[current_sample] =
                ((int32_t)ReadUint16(w)) << 16;
            break;
          case 3:
            /* Read 24-bit ints into a 32-bit container. */
            ((int32_t*)dst_samples)[current_sample] =
                Read3BytesIntoUint32(w);
            break;
          case 4:
            /* Read 32-bits into a float container. */
            ((float*)dst_samples)[current_sample] = ReadFloat32(w);
            break;
          case 8:
            /* Read 64-bits into a float container. */
            ((float*)dst_samples)[current_sample] = ReadFloat64IntoFloat32(w);
            break;
        }
        break;
    }
    if (w->has_error) {
      /* Tolerate a truncated data chunk, just return what was read. */
      current_sample -= current_sample % info->num_channels;
      info->remaining_samples = 0;
      LOG_ERROR("Error: File error while reading WAV.\n");
      return current_sample;
    }
  }

  info->remaining_samples -= samples_to_read;
  return samples_to_read;
}

size_t Read16BitWavSamplesGeneric(WavReader* w, ReadWavInfo* info,
                                  int16_t* samples, size_t num_samples) {
  ABSL_CHECK(info->bit_depth == 8 || info->bit_depth == 16);
  ABSL_CHECK(info->destination_alignment_bytes == 2);
  ABSL_CHECK(info->sample_format == kInt16);
  return ReadBytesAsSamples(w, info, (char*)samples, num_samples);
}

size_t ReadWavSamplesGeneric(WavReader* w, ReadWavInfo* info,
                             void* samples, size_t num_samples) {
  return ReadBytesAsSamples(w, info, (char*)samples, num_samples);
}

