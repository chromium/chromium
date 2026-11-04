/* Copyright 2019, 2022 Google LLC
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

#include "audio/dsp/portable/write_wav_file.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "audio/dsp/portable/logging.h"
#include "audio/dsp/portable/write_wav_file_generic.h"

static size_t WriteBytes(const void* bytes, size_t num_bytes, void* io_ptr) {
  return fwrite(bytes, 1, num_bytes, (FILE*)io_ptr);
}

static WavWriter WavWriterLocal(FILE* f) {
  WavWriter w;
  w.write_fun = WriteBytes;
  w.io_ptr = f;
  return w;
}

int WriteWavHeader(FILE* f, size_t num_samples, int sample_rate_hz,
                   int num_channels) {
  WavWriter w = WavWriterLocal(f);
  return WriteWavHeaderGeneric(&w, num_samples, sample_rate_hz, num_channels);
}

int WriteWavHeader24Bit(FILE* f, size_t num_samples, int sample_rate_hz,
                        int num_channels) {
  WavWriter w = WavWriterLocal(f);
  return WriteWavHeaderGeneric24Bit(&w, num_samples, sample_rate_hz,
                                    num_channels);
}

int WriteWavHeader32Bit(FILE* f, size_t num_samples, int sample_rate_hz,
                        int num_channels) {
  WavWriter w = WavWriterLocal(f);
  return WriteWavHeaderGeneric32Bit(&w, num_samples, sample_rate_hz,
                                    num_channels);
}

int WriteWavSamples(FILE* f, const int16_t* samples, size_t num_samples) {
  WavWriter w = WavWriterLocal(f);
  return WriteWavSamplesGeneric(&w, samples, num_samples);
}

int WriteWavSamples24Bit(FILE* f, const int32_t* samples, size_t num_samples) {
  WavWriter w = WavWriterLocal(f);
  return WriteWavSamplesGeneric24Bit(&w, samples, num_samples);
}

int WriteWavSamples32Bit(FILE* f, const int32_t* samples, size_t num_samples) {
  WavWriter w = WavWriterLocal(f);
  return WriteWavSamplesGeneric32Bit(&w, samples, num_samples);
}

int WriteWavFileInternal(const char* file_name, const void* samples,
                         size_t num_samples, int sample_rate_hz,
                         int num_channels, int bit_depth) {
  if (file_name == NULL || sample_rate_hz <= 0 || num_channels <= 0 ||
      num_samples % num_channels != 0 || num_samples > (UINT32_MAX - 60) / 2) {
    goto fail; /* Invalid input arguments. */
  }
  FILE* f = fopen(file_name, "wb");
  WavWriter w = WavWriterLocal(f);
  w.has_error = 0; /* Clear the error flag. */
  if (!f) {
    LOG_ERROR("Error: Failed to open \"%s\" for writing: %s\n", file_name,
              strerror(errno));
    goto fail; /* Failed to open file_name for writing. */
  }

  if (bit_depth == 32) {
    WriteWavHeaderGeneric32Bit(&w, num_samples, sample_rate_hz, num_channels);
  } else if (bit_depth == 24) {
    WriteWavHeaderGeneric24Bit(&w, num_samples, sample_rate_hz, num_channels);
  } else {
    WriteWavHeaderGeneric(&w, num_samples, sample_rate_hz, num_channels);
  }

  if (w.has_error) {
    LOG_ERROR("Error while writing \"%s\".\n", file_name);
    goto fail;
  }

  if (bit_depth == 32) {
    WriteWavSamplesGeneric32Bit(&w, (const int32_t*)samples, num_samples);
  } else if (bit_depth == 24) {
    WriteWavSamplesGeneric24Bit(&w, (const int32_t*)samples, num_samples);
  } else {
    WriteWavSamplesGeneric(&w, (const int16_t*)samples, num_samples);
  }

  if (w.has_error) {
    LOG_ERROR("Error while writing \"%s\".\n", file_name);
    goto fail;
  }

  if (fclose(f)) {
    LOG_ERROR("Error while closing \"%s\".\n", file_name);
    goto fail; /* I/O error while closing file. */
  }
  return 1;

fail:
  w.has_error = 1;
  return 0;
}

int WriteWavFile(const char* file_name, const int16_t* samples,
                 size_t num_samples, int sample_rate_hz, int num_channels) {
  return WriteWavFileInternal(file_name, samples, num_samples, sample_rate_hz,
                              num_channels, 16);
}

int WriteWavFile24Bit(const char* file_name, const int32_t* samples,
                      size_t num_samples, int sample_rate_hz,
                      int num_channels) {
  return WriteWavFileInternal(file_name, samples, num_samples, sample_rate_hz,
                              num_channels, 24);
}

int WriteWavFile32Bit(const char* file_name, const int32_t* samples,
                      size_t num_samples, int sample_rate_hz,
                      int num_channels) {
  return WriteWavFileInternal(file_name, samples, num_samples, sample_rate_hz,
                              num_channels, 32);
}
