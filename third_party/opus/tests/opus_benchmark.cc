// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <limits>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/opus/src/include/opus.h"

// Set to 1 to generate golden values instead of checking against them.
// Used to ease updating when new references are better or acceptable.
#define REGENERATE_REFERENCES 0

namespace {

constexpr int RETRIES = 3;
constexpr int MAX_FRAME_SAMP = 5760;  // 120ms * 48kHz

base::FilePath GetDataPath(const std::string& filename) {
  base::FilePath path;
  DCHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &path));
  return path.AppendASCII("third_party")
      .AppendASCII("opus")
      .AppendASCII("tests")
      .AppendASCII("resources")
      .AppendASCII(filename);
}

void opus_run(int frame_size_ms,
              int sampling_khz,
              int channels,
              int bit_rate,
              int complexity,
              size_t audio_duration_sec,
              float* encoding_rtf,
              float* decoding_rtf) {
  const int frame_size = frame_size_ms * sampling_khz;
  const int application =
      channels == 1 ? OPUS_APPLICATION_VOIP : OPUS_APPLICATION_AUDIO;

  /* Read PCM */
  auto in_filename = GetDataPath("speech_mono_32_48kHz.pcm").value();
  FILE* fp = fopen(in_filename.c_str(), "rb");

  // TODO(b/1002973): Review error handling. ASSERT_* just return from function,
  //                  meaning calling code is executed.
  // We might want to factorize encoder creation anyway (read pcm once instead
  //                            of re-opening it for each bitrate / complexity).
  ASSERT_NE(fp, nullptr) << "Could not open: " << in_filename;

  fseek(fp, 0, SEEK_END);
  const size_t total_samp = ftell(fp) / sizeof(int16_t);
  rewind(fp);

  int16_t* in_data =
      (int16_t*)malloc((total_samp + frame_size * channels) * sizeof(*in_data));
  const size_t check_total_samp =
      fread(&in_data[0], sizeof(int16_t), total_samp, fp);
  ASSERT_EQ(check_total_samp, total_samp) << "Error reading input pcm file.";
  fclose(fp);

  const opus_int32 max_bytes = frame_size * channels * sizeof(int16_t);
  int16_t* out_data =
      (int16_t*)malloc((frame_size * channels) * sizeof(int16_t));
  uint8_t* bit_stream = (uint8_t*)malloc(max_bytes * sizeof(*bit_stream));

  /* Create an encoder */
  opus_int32 res;
  OpusEncoder* enc =
      opus_encoder_create(sampling_khz * 1000, channels, application, &res);
  ASSERT_TRUE(res == OPUS_OK && enc != nullptr)
      << "Could not instantiate an Opus encoder. Error code: " << res;

  opus_encoder_ctl(enc, OPUS_SET_BITRATE(bit_rate));
  opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(complexity));

  /* Create a decoder */
  OpusDecoder* dec = opus_decoder_create(sampling_khz * 1000, channels, &res);
  ASSERT_TRUE(res == OPUS_OK && dec != nullptr)
      << "Could not instantiate an Opus decoder. Error code: " << res;

  /* Transcode up to |audio_duration_sec| only */
  size_t data_pointer = 0;
  size_t decoded_ms = 0;
  float encoding_sec = 0;
  float decoding_sec = 0;
  while (decoded_ms < audio_duration_sec * 1000) {
    /* Encode */
    clock_t clocks = clock();
    res = opus_encode(enc, &in_data[data_pointer], frame_size, &bit_stream[0],
                      max_bytes);
    clocks = clock() - clocks;
    EXPECT_GT(res, 0) << "No bytes were encoded successfully.";

    encoding_sec += 1. * clocks / CLOCKS_PER_SEC;

    /* Decode */
    clocks = clock();
    res =
        opus_decode(dec, &bit_stream[0], res, &out_data[0], MAX_FRAME_SAMP, 0);
    clocks = clock() - clocks;
    EXPECT_EQ(res, frame_size) << "Wrong number of samples returned by decoder";

    decoding_sec += 1. * clocks / CLOCKS_PER_SEC;

    /* Update data pointer and time tracker */
    data_pointer += frame_size * channels;
    decoded_ms += frame_size_ms;
  }

  *encoding_rtf = encoding_sec / audio_duration_sec;
  *decoding_rtf = decoding_sec / audio_duration_sec;

  /* Clean up */
  opus_encoder_destroy(enc);
  opus_decoder_destroy(dec);
  free(in_data);
  free(out_data);
  free(bit_stream);
}
}  // namespace

TEST(OpusBenchmark, SpeechMono48kHzNexus5) {
  const int sampling_khz = 48;
  const int channels = 1;
  size_t audio_duration_sec = 240;
  const int frame_size_ms = 20;
  constexpr int nr_bit_rates = 8;
  int32_t bit_rates[nr_bit_rates] = {64000, 32000, 24000, 16000,
                                     12000, 10000, 8000,  6000};
  constexpr int nr_complexities = 11;
  int32_t complexities[nr_complexities] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

#if REGENERATE_REFERENCES
  fprintf(stderr, "bitrate,complexity,encoding_rtf,decoding_rtf\n");
#else
  // Real-time factors, scaled *1000 for cosmetic purpose.
  // Lower is best, any value <= 1000 is real-time.

  // This reference numbers were generated on a NEXUS 5.
  // This means they are irrelevant if you run the test on a linux workstation.
  // TODO(b/1002973): Have a set of references by architecture?
  //              Or at least, disable the tests if not run on expected target.
  float encoding_reference[nr_bit_rates][nr_complexities] = {
      // NB: RTF is dropping for complexities 9 and 10 at bit_rate 64000.
      // On linux x64, it drops for complexities 7, 8, 9, 10.
      // That seems ok, sanitizers don't report anything.
      // It looks to be operating more often in CELT-only mode compared to
      // Hybrid mode for this particular audio file (possibly because of
      // the presence of the background music).
      {18.0, 20.2, 22.8, 24.1, 29.3, 29.2, 31.5, 31.5, 35.6, 15.5, 21.6},
      {16.9, 19.2, 21.7, 23.0, 28.1, 28.1, 30.1, 30.1, 34.2, 34.2, 32.6},
      {16.6, 18.8, 21.3, 22.6, 27.8, 27.8, 29.7, 29.7, 33.7, 33.7, 39.9},
      {12.7, 13.6, 20.9, 22.2, 27.4, 27.3, 29.2, 29.2, 33.1, 33.0, 39.1},
      {8.4, 9.1, 16.0, 17.3, 22.4, 22.4, 24.2, 24.3, 27.7, 27.7, 33.8},
      {8.4, 9.1, 10.3, 11.2, 13.8, 13.8, 14.7, 14.7, 16.6, 16.6, 26.6},
      {8.4, 9.0, 10.3, 11.1, 13.7, 13.7, 14.6, 14.6, 16.5, 16.5, 22.6},
      {8.3, 9.0, 10.2, 11.1, 13.7, 13.7, 14.6, 14.6, 16.4, 16.4, 22.5}};

  float decoding_reference[nr_bit_rates][nr_complexities] = {
      {7.1, 7.5, 7.5, 7.5, 7.5, 7.5, 7.5, 7.5, 7.5, 5.8, 5.8},
      {6.5, 6.8, 6.8, 6.8, 6.9, 6.9, 6.9, 6.9, 6.9, 6.9, 6.2},
      {6.3, 6.6, 6.6, 6.6, 6.7, 6.7, 6.7, 6.7, 6.7, 6.7, 6.7},
      {3.2, 3.2, 6.4, 6.4, 6.5, 6.4, 6.4, 6.5, 6.5, 6.4, 6.5},
      {2.2, 2.2, 3.1, 3.1, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2},
      {2.1, 2.1, 2.1, 2.1, 2.2, 2.2, 2.2, 2.2, 2.2, 1.9, 2.5},
      {2.1, 2.1, 2.1, 2.1, 2.1, 2.1, 2.1, 2.1, 2.1, 2.1, 2.1},
      {2.1, 2.1, 2.1, 2.1, 2.1, 2.1, 2.1, 2.1, 2.1, 2.1, 2.1}};
#endif

  for (int i = 0; i < nr_bit_rates; i++) {
    for (int j = 0; j < nr_complexities; j++) {
      // To reduce brittleness, each test is retried independently.
#if REGENERATE_REFERENCES
      float encoding_best = std::numeric_limits<float>::infinity();
      float decoding_best = std::numeric_limits<float>::infinity();
#endif
      for (int k = 0; k < RETRIES; k++) {
        float encoding_rtf;
        float decoding_rtf;
        opus_run(frame_size_ms, sampling_khz, channels, bit_rates[i],
                 complexities[j], audio_duration_sec, &encoding_rtf,
                 &decoding_rtf);
        // Scale to match reference unit.
        encoding_rtf *= 1000;
        decoding_rtf *= 1000;

#if REGENERATE_REFERENCES
        encoding_best = std::min(encoding_best, encoding_rtf);
        decoding_best = std::min(decoding_best, decoding_rtf);
#else
        // Allow 5% relative error.
        auto encoding_ref = encoding_reference[i][j];
        auto decoding_ref = decoding_reference[i][j];
        const float encoding_abs_error = .05 * encoding_ref;
        const float decoding_abs_error = .05 * decoding_ref;
        if (std::abs(encoding_rtf - encoding_ref) < encoding_abs_error) {
          SUCCEED();
          break;  // No need for retry.
        } else if (k == RETRIES - 1) {
          // Nb: If returned value is consistently lower (better),
          //     one just have to update the reference.
          EXPECT_NEAR(encoding_rtf, encoding_ref, encoding_abs_error)
              << "Mismatch for bitrate " << bit_rates[i] << " and complexity "
              << complexities[j];
          EXPECT_NEAR(decoding_rtf, decoding_ref, decoding_abs_error)
              << "Mismatch for bitrate " << bit_rates[i] << " and complexity "
              << complexities[j];
        }
#endif
      }
#if REGENERATE_REFERENCES
      fprintf(stderr, "%d, %d, %.2f, %.2f\n", bit_rates[i], complexities[j],
              encoding_best, decoding_best);
#endif
    }
  }
}
