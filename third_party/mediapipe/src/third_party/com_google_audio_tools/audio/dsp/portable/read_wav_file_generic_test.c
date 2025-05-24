#include "audio/dsp/portable/read_wav_file_generic.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio/dsp/portable/logging.h"

/* A 48kHz mono WAV file with int16_t samples {7, -2, INT16_MAX, INT16_MIN}.
 * See read_wav_file_test for more details about how this was generated.  */
static const uint8_t kTest16BitMonoWavFile[52] = {
/*  R    I    F    F                      W    A    V   E   f    m    t    _ */
    82,  73,  70,  70,  44,  0,  0,   0,  87,  65,  86, 69, 102, 109, 116, 32,
    16,  0,   0,   0,   1,   0,  1,   0,  128, 187, 0,  0,  0,   119, 1,   0,
/*                      d    a   t    a                                      */
    2,   0,   16,  0,   100, 97, 116, 97, 8,   0,   0,  0,  7,   0,   254, 255,
    255, 127, 0,   128};

/* A WAV file encoded with a nonstandard chunk denoted by tag 'cust'. */
static const uint8_t kNonStandardWavFile[56] = {
/*  R    I   F   F                        W    A    V   E   f    m    t    _ */
    82,  73, 70, 70,  48,  0,   0,   0,   87,  65,  86, 69, 102, 109, 116, 32,
    16,  0,  0,  0,   1,   0,   1,   0,   128, 187, 0,  0,  0,   119, 1,   0,
/*                    c    u    s    t                      a    d    p    r */
    2,   0,  16, 0,   99,  117, 115, 116, 4,   0,   0,  0,  97,  100, 112, 114,
/*  d    a    t    a                                     */
    100, 97,  116, 97,  0,   0,   0,   0};

struct TestData {
  uint8_t /* bool */ found_custom_chunk;
  char custom_chunk_contents[4];  /* We know the chunk is of size 4. */
  int chunk_size;
  FILE* f;
};

typedef struct TestData TestData;

TestData MakeTestData(FILE* file) {
  TestData data;
  data.found_custom_chunk = 0;
  memset(data.custom_chunk_contents, 0, 4);
  data.chunk_size = 0;
  data.f = file;
  return data;
}

/* An implementation of standard I/O callbacks. */
static int Seek(size_t num_bytes, void* io_ptr) {
  return fseek(((TestData*)(io_ptr))->f, num_bytes, SEEK_CUR);
}

static int EndOfFile(void* io_ptr) {
  return feof(((TestData*)(io_ptr))->f);
}

static size_t ReadBytes(void* bytes, size_t num_bytes, void* io_ptr) {
  return fread(bytes, 1, num_bytes, ((TestData*)(io_ptr))->f);
}

static void HandleCustomChunk(
    char (*id)[4], const void * data, size_t num_bytes, void* io_ptr) {
  TestData* result = (TestData*)(io_ptr);
  result->found_custom_chunk = 1;
  if (memcmp(*id, "cust", 4) == 0) {
    if (num_bytes <= 4) {
      memcpy(result->custom_chunk_contents, data, num_bytes);
    }
    result->chunk_size = num_bytes;
  }
}

static WavReader CustomChunkWavReader(TestData* data) {
  WavReader w;
  w.read_fun = ReadBytes;
  w.seek_fun = Seek;
  w.eof_fun = EndOfFile;
  w.custom_chunk_fun = HandleCustomChunk;
  w.io_ptr = data;
  return w;
}

static void WriteBytesAsFile(const char* file_name,
                             const uint8_t* bytes, size_t num_bytes) {
  FILE* f = ABSL_CHECK_NOTNULL(fopen(file_name, "wb"));
  ABSL_CHECK(fwrite(bytes, 1, num_bytes, f) == num_bytes);
  fclose(f);
}

static void TestReadMonoWav(void) {
  puts("TestReadMonoWav");
  const char* wav_file_name = NULL;
  FILE* f;
  ReadWavInfo info;

  wav_file_name = ABSL_CHECK_NOTNULL(tmpnam(NULL));
  WriteBytesAsFile(wav_file_name, kTest16BitMonoWavFile, 52);

  f = ABSL_CHECK_NOTNULL(fopen(wav_file_name, "rb"));

  TestData data = MakeTestData(f);
  WavReader reader = CustomChunkWavReader(&data);
  ABSL_CHECK(ReadWavHeaderGeneric(&reader, &info));
  ABSL_CHECK(info.num_channels == 1);
  ABSL_CHECK(info.sample_rate_hz == 48000);
  ABSL_CHECK(info.remaining_samples == 4);

  ABSL_CHECK(!data.found_custom_chunk);

  fclose(f);
}

static void TestNonstandardWavFile(void) {
  puts("TestNonstandardWavFile");
  const char* wav_file_name = NULL;
  FILE* f;
  ReadWavInfo info;

  wav_file_name = ABSL_CHECK_NOTNULL(tmpnam(NULL));
  WriteBytesAsFile(wav_file_name, kNonStandardWavFile, 56);

  f = ABSL_CHECK_NOTNULL(fopen(wav_file_name, "rb"));

  TestData data = MakeTestData(f);
  WavReader reader = CustomChunkWavReader(&data);
  ABSL_CHECK(ReadWavHeaderGeneric(&reader, &info));
  ABSL_CHECK(info.num_channels == 1);
  ABSL_CHECK(info.sample_rate_hz == 48000);
  ABSL_CHECK(info.remaining_samples == 0);
  ABSL_CHECK(data.found_custom_chunk);
  ABSL_CHECK(memcmp(data.custom_chunk_contents, "adpr", 4) == 0);
  ABSL_CHECK(data.chunk_size == 4);

  fclose(f);
}

int main(int argc, char** argv) {
  srand(0);
  TestReadMonoWav();
  TestNonstandardWavFile();

  puts("PASS");
  return EXIT_SUCCESS;
}
