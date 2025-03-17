#include "audio/dsp/portable/write_wav_file.h"

#include <stdio.h>

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

int WriteWavSamples(FILE* f, const int16_t* samples, size_t num_samples) {
  WavWriter w = WavWriterLocal(f);
  return WriteWavSamplesGeneric(&w, samples, num_samples);
}

int WriteWavFile(const char* file_name, const int16_t* samples,
                 size_t num_samples, int sample_rate_hz, int num_channels) {
  if (file_name == NULL || sample_rate_hz <= 0 || num_channels <= 0 ||
      num_samples % num_channels != 0 || num_samples > (UINT32_MAX - 60) / 2) {
    goto fail; /* Invalid input arguments. */
  }
  FILE* f = fopen(file_name, "wb");
  WavWriter w = WavWriterLocal(f);
  w.has_error = 0;  /* Clear the error flag. */
  if (!f) {
    goto fail; /* Failed to open file_name for writing. */
  }

  WriteWavHeaderGeneric(&w, num_samples, sample_rate_hz, num_channels);
  if (w.has_error) { goto fail; }
  WriteWavSamplesGeneric(&w, samples, num_samples);

  if (fclose(f)) {
    goto fail; /* I/O error while closing file. */
  }
  return 1;

fail:
  w.has_error = 1;
  return 0;
}
