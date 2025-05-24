#include "audio/dsp/portable/write_wav_file.h"

#include <string.h>

#include "audio/dsp/portable/logging.h"

/* A 48kHz mono WAV file with int16_t samples {7, -2, INT16_MAX, INT16_MIN}. */
static const uint8_t kTestMonoWavFile[52] = {
    82,  73,  70, 70,  44, 0, 0, 0,   87,  65,  86,  69,  102,
    109, 116, 32, 16,  0,  0, 0, 1,   0,   1,   0,   128, 187,
    0,   0,   0,  119, 1,  0, 2, 0,   16,  0,   100, 97,  116,
    97,  8,   0,  0,   0,  7, 0, 254, 255, 255, 127, 0,   128};
/* A 16kHz 3-channel WAV file with int16_t samples {{0, 1, 2}, {3, 4, 5}}. */
static const uint8_t kTest3ChannelWavFile[92] = {
    82, 73, 70, 70, 84,  0,   0,  0,   87,  65, 86,  69,  102, 109, 116, 32,
    40, 0,  0,  0,  254, 255, 3,  0,   128, 62, 0,   0,   0,   119, 1,   0,
    6,  0,  16, 0,  22,  0,   16, 0,   0,   0,  0,   0,   1,   0,   0,   0,
    0,  0,  16, 0,  128, 0,   0,  170, 0,   56, 155, 113, 102, 97,  99,  116,
    4,  0,  0,  0,  2,   0,   0,  0,   100, 97, 116, 97,  12,  0,   0,   0,
    0,  0,  1,  0,  2,   0,   3,  0,   4,   0,  5,   0};

static void CheckFileBytes(const char* file_name, const uint8_t* expected_bytes,
                           size_t num_bytes) {
  uint8_t* bytes = ABSL_CHECK_NOTNULL((uint8_t *) malloc(num_bytes + 1));
  FILE* f = ABSL_CHECK_NOTNULL(fopen(file_name, "rb"));
  ABSL_CHECK(fread(bytes, 1, num_bytes + 1, f) == num_bytes);
  fclose(f);
  ABSL_CHECK(memcmp(bytes, expected_bytes, num_bytes) == 0);
  free(bytes);
}

void TestWriteMonoWav() {
  static const int16_t kSamples[4] = {7, -2, INT16_MAX, INT16_MIN};
  const char* wav_file_name = NULL;

  puts("Running TestWriteMonoWav");
  wav_file_name = ABSL_CHECK_NOTNULL(tmpnam(NULL));
  ABSL_CHECK(WriteWavFile(wav_file_name, kSamples, 4, 48000, 1));

  CheckFileBytes(wav_file_name, kTestMonoWavFile, 52);
  remove(wav_file_name);
}

void TestWriteMonoWavStreaming() {
  static const int16_t kSamples[4] = {7, -2, INT16_MAX, INT16_MIN};
  const char* wav_file_name = NULL;

  puts("Running TestWriteMonoWavStreaming");
  wav_file_name = ABSL_CHECK_NOTNULL(tmpnam(NULL));
  FILE* f = NULL;
  ABSL_CHECK(f = fopen(wav_file_name, "wb"));
  ABSL_CHECK(WriteWavHeader(f, 0, 48000, 1)); /* Write a dummy header. */
  ABSL_CHECK(WriteWavSamples(f, kSamples + 0, 2));
  ABSL_CHECK(WriteWavSamples(f, kSamples + 2, 2));
  fseek(f, 0, SEEK_SET);
  ABSL_CHECK(WriteWavHeader(f, 4, 48000, 1));
  fclose(f);

  CheckFileBytes(wav_file_name, kTestMonoWavFile, 52);
  remove(wav_file_name);
}

void TestWrite3ChannelWav() {
  static const int16_t kSamples[6] = {0, 1, 2, 3, 4, 5};
  const char* wav_file_name = NULL;

  puts("Running TestWrite3ChannelWav");
  wav_file_name = ABSL_CHECK_NOTNULL(tmpnam(NULL));
  ABSL_CHECK(WriteWavFile(wav_file_name, kSamples, 6, 16000, 3));
  CheckFileBytes(wav_file_name, kTest3ChannelWavFile, 92);
  remove(wav_file_name);
}

int main(int argc, char** argv) {
  TestWriteMonoWav();
  TestWriteMonoWavStreaming();
  TestWrite3ChannelWav();

  puts("PASS");
  return EXIT_SUCCESS;
}
