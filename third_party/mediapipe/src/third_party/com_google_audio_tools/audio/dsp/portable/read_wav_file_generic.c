/* Copyright 2019, 2021-2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "audio/dsp/portable/read_wav_file_generic.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio/dsp/portable/logging.h"
#include "audio/dsp/portable/read_wav_info.h"
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

static const int16_t kMuLawTable[256] = {
    -32124, -31100, -30076, -29052, -28028, -27004, -25980, -24956, -23932,
    -22908, -21884, -20860, -19836, -18812, -17788, -16764, -15996, -15484,
    -14972, -14460, -13948, -13436, -12924, -12412, -11900, -11388, -10876,
    -10364, -9852,  -9340,  -8828,  -8316,  -7932,  -7676,  -7420,  -7164,
    -6908,  -6652,  -6396,  -6140,  -5884,  -5628,  -5372,  -5116,  -4860,
    -4604,  -4348,  -4092,  -3900,  -3772,  -3644,  -3516,  -3388,  -3260,
    -3132,  -3004,  -2876,  -2748,  -2620,  -2492,  -2364,  -2236,  -2108,
    -1980,  -1884,  -1820,  -1756,  -1692,  -1628,  -1564,  -1500,  -1436,
    -1372,  -1308,  -1244,  -1180,  -1116,  -1052,  -988,   -924,   -876,
    -844,   -812,   -780,   -748,   -716,   -684,   -652,   -620,   -588,
    -556,   -524,   -492,   -460,   -428,   -396,   -372,   -356,   -340,
    -324,   -308,   -292,   -276,   -260,   -244,   -228,   -212,   -196,
    -180,   -164,   -148,   -132,   -120,   -112,   -104,   -96,    -88,
    -80,    -72,    -64,    -56,    -48,    -40,    -32,    -24,    -16,
    -8,     0,      32124,  31100,  30076,  29052,  28028,  27004,  25980,
    24956,  23932,  22908,  21884,  20860,  19836,  18812,  17788,  16764,
    15996,  15484,  14972,  14460,  13948,  13436,  12924,  12412,  11900,
    11388,  10876,  10364,  9852,   9340,   8828,   8316,   7932,   7676,
    7420,   7164,   6908,   6652,   6396,   6140,   5884,   5628,   5372,
    5116,   4860,   4604,   4348,   4092,   3900,   3772,   3644,   3516,
    3388,   3260,   3132,   3004,   2876,   2748,   2620,   2492,   2364,
    2236,   2108,   1980,   1884,   1820,   1756,   1692,   1628,   1564,
    1500,   1436,   1372,   1308,   1244,   1180,   1116,   1052,   988,
    924,    876,    844,    812,    780,    748,    716,    684,    652,
    620,    588,    556,    524,    492,    460,    428,    396,    372,
    356,    340,    324,    308,    292,    276,    260,    244,    228,
    212,    196,    180,    164,    148,    132,    120,    112,    104,
    96,     88,     80,     72,     64,     56,     48,     40,     32,
    24,     16,     8,      0,
};

static size_t ReadWithErrorCheck(void* bytes, size_t num_bytes, WavReader* w) {
  size_t read_bytes = w->read_fun(bytes, num_bytes, w->io_ptr);
  if (num_bytes != read_bytes) {
    if (read_bytes < num_bytes) {
      memset((char*)bytes + read_bytes, 0, num_bytes - read_bytes);
    }
    w->has_error = 1;
    if (w->eof_fun != NULL && w->eof_fun(w->io_ptr)) {
      LOG_ERROR("Error: WAV file ended unexpectedly.\n");
    }
  }
  return read_bytes;
}

static void SeekWithErrorCheck(size_t num_bytes, WavReader* w) {
  int failure = 0;
  if (w->seek_fun != NULL) {
    failure = w->seek_fun(num_bytes, w->io_ptr);
  } else {
    /* Allocate small buffer even for large seeks. */
    char buffer[256];
    for (; num_bytes > sizeof(buffer); num_bytes -= sizeof(buffer)) {
      if (w->read_fun(buffer, sizeof(buffer), w->io_ptr) != sizeof(buffer)) {
        w->has_error = 1;
        return;
      }
    }
    failure = w->read_fun(buffer, num_bytes, w->io_ptr) != num_bytes;
  }
  if (failure) {
    w->has_error = 1;
    if (w->eof_fun != NULL && w->eof_fun(w->io_ptr)) {
      LOG_ERROR("Error: WAV file ended unexpectedly.\n");
    }
  }
}

static uint16_t ReadUint16(WavReader* w) {
  uint8_t bytes[2];
  ReadWithErrorCheck(bytes, 2, w);
  return LittleEndianReadU16(bytes);
}

static uint32_t ReadUint32(WavReader* w) {
  uint8_t bytes[4];
  ReadWithErrorCheck(bytes, 4, w);
  return LittleEndianReadU32(bytes);
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
      } else if (significant_bits_per_sample == 32) {
        info->encoding = kPcm32Encoding;
        info->destination_alignment_bytes = 4 /* 32-bit int */;
        info->sample_format = kInt32;
      } else {
        LOG_ERROR("Error: Only 16, 24, and 32 bit PCM data is supported.\n");
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
  if (((size_t)dst_samples) % info->destination_alignment_bytes != 0) {
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
  w->has_error = 0; /* Clear the error flag. */

  /* Convert bit depth to bytes. */
  const size_t src_alignment_bytes = info->bit_depth / 8;

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

  /* Create an internal buffer for reading samples. This reduces overhead of
   * calling read_fun() calls and IO operations for a significant speed up. A
   * larger buffer helps more but with diminishing returns. From benchmarking, a
   * 1 KB buffer gets +90% of the benefit and is small enough to reasonably
   * allocate on the stack.
   */
  uint8_t buffer[1024];
  const int max_count = sizeof(buffer) / src_alignment_bytes;

  for (current_sample = 0; current_sample < samples_to_read;) {
    int count = max_count;
    const size_t remaining = samples_to_read - current_sample;
    if ((size_t)count > remaining) { count = (int)remaining; }
    const size_t num_bytes = count * src_alignment_bytes;

    /* Call the reading callback for ~1 KB at a time. */
    const size_t bytes_read = ReadWithErrorCheck(buffer, num_bytes, w);
    if (bytes_read < num_bytes) {
      count = bytes_read / src_alignment_bytes;
    } else if (bytes_read > num_bytes) {
      count = 0;
    }

    int i = 0;

    switch (info->destination_alignment_bytes) {
      case 2: /* The destination is 16-bit. */
        switch (src_alignment_bytes) {
          case 1: /* Read 8-bit mu-law samples into a 16-bit container. */
            for (i = 0; i < count; ++i) {
              ((int16_t*)dst_samples)[i] = kMuLawTable[buffer[i]];
            }
            break;
          case 2: /* Read 16-bit ints into a 16-bit container. */
            for (i = 0; i < count; ++i) {
              const uint16_t value_u16 = LittleEndianReadU16(buffer + 2 * i);
              memcpy((int16_t*)dst_samples + i, &value_u16, sizeof(int16_t));
            }
            break;
        }
        break;
      case 4: /* The destination is 32-bit. */
        switch (src_alignment_bytes) {
          case 1: /* Read 8-bit mu-law samples into a 32-bit container. */
            for (i = 0; i < count; ++i) {
              const uint32_t value_u32 = (uint32_t)kMuLawTable[buffer[i]] << 16;
              memcpy((int32_t*)dst_samples + i, &value_u32, sizeof(int32_t));
            }
            break;
          case 2: /* Read 16-bit ints into a 32-bit container. */
            for (i = 0; i < count; ++i) {
              const uint32_t value_u32 =
                  (uint32_t)LittleEndianReadU16(buffer + 2 * i) << 16;
              memcpy((int32_t*)dst_samples + i, &value_u32, sizeof(int32_t));
            }
            break;
          case 3: { /* Read 24-bit ints into a 32-bit container. */
            int offset;
            for (i = 0, offset = 0; i < count; ++i, offset += 3) {
              const uint32_t value_u32 =
                  (((uint32_t)buffer[offset + 0]) << 8) |
                  (((uint32_t)buffer[offset + 1]) << 16) |
                  (((uint32_t)buffer[offset + 2]) << 24);
              memcpy((int32_t*)dst_samples + i, &value_u32, sizeof(int32_t));
            }
          } break;
          case 4: /* Read 32-bits into a float container. */
            for (i = 0; i < count; ++i) {
              ((float*)dst_samples)[i] = LittleEndianReadF32(buffer + 4 * i);
            }
            break;
          case 8: /* Read 64-bits into a float container. */
            for (i = 0; i < count; ++i) {
              ((float*)dst_samples)[i] =
                  (float)LittleEndianReadF64(buffer + 8 * i);
            }
            break;
        }
        break;
    }

    dst_samples += count * info->destination_alignment_bytes;
    current_sample += count;

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
