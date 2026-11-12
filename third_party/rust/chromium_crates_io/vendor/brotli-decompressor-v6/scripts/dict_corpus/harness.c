/* Fixture-generation harness for the dictionary differential corpus.
 *
 * Compresses or decompresses one file with a raw or serialized shared
 * dictionary attached, using the reference C implementation. Build with
 * BROTLI_EXPERIMENTAL so that serialized dictionaries are supported:
 *
 *   gcc -O1 -DBROTLI_EXPERIMENTAL=1 -I $BROTLI/c/include harness.c \
 *       $BROTLI/c/common/*.c $BROTLI/c/dec/*.c $BROTLI/c/enc/*.c \
 *       -o harness -lm
 *
 * usage: harness enc|dec raw|serialized dict in out [quality] [lgwin]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <brotli/encode.h>
#include <brotli/decode.h>
#include <brotli/shared_dictionary.h>

static unsigned char* read_file(const char* path, size_t* size) {
  FILE* f = fopen(path, "rb");
  if (!f) { fprintf(stderr, "open %s failed\n", path); exit(2); }
  fseek(f, 0, SEEK_END); *size = (size_t)ftell(f); fseek(f, 0, SEEK_SET);
  unsigned char* buf = malloc(*size ? *size : 1);
  if (fread(buf, 1, *size, f) != *size) { exit(2); }
  fclose(f);
  return buf;
}

static void write_file(const char* path, const unsigned char* data, size_t size) {
  FILE* f = fopen(path, "wb");
  fwrite(data, 1, size, f);
  fclose(f);
}

int main(int argc, char** argv) {
  if (argc < 6) {
    fprintf(stderr,
            "usage: %s enc|dec raw|serialized dict in out [quality] [lgwin]\n",
            argv[0]);
    return 2;
  }
  BrotliSharedDictionaryType type = !strcmp(argv[2], "raw")
      ? BROTLI_SHARED_DICTIONARY_RAW : BROTLI_SHARED_DICTIONARY_SERIALIZED;
  size_t dict_size, in_size;
  unsigned char* dict = read_file(argv[3], &dict_size);
  unsigned char* in = read_file(argv[4], &in_size);
  int quality = argc > 6 ? atoi(argv[6]) : 9;
  int lgwin = argc > 7 ? atoi(argv[7]) : 16;
  if (!strcmp(argv[1], "enc")) {
    BrotliEncoderPreparedDictionary* prepared = BrotliEncoderPrepareDictionary(
        type, dict_size, dict, quality, NULL, NULL, NULL);
    if (!prepared) { fprintf(stderr, "prepare failed\n"); return 3; }
    BrotliEncoderState* enc = BrotliEncoderCreateInstance(NULL, NULL, NULL);
    BrotliEncoderSetParameter(enc, BROTLI_PARAM_QUALITY, (uint32_t)quality);
    if (lgwin > 24) {
      BrotliEncoderSetParameter(enc, BROTLI_PARAM_LARGE_WINDOW, 1);
    }
    BrotliEncoderSetParameter(enc, BROTLI_PARAM_LGWIN, (uint32_t)lgwin);
    if (!BrotliEncoderAttachPreparedDictionary(enc, prepared)) {
      fprintf(stderr, "attach failed\n"); return 3;
    }
    size_t out_cap = in_size + (in_size >> 1) + 4096;
    unsigned char* out = malloc(out_cap);
    size_t avail_in = in_size, avail_out = out_cap;
    const unsigned char* next_in = in;
    unsigned char* next_out = out;
    while (1) {
      if (!BrotliEncoderCompressStream(enc, BROTLI_OPERATION_FINISH,
            &avail_in, &next_in, &avail_out, &next_out, NULL)) {
        fprintf(stderr, "compress failed\n"); return 3;
      }
      if (BrotliEncoderIsFinished(enc)) break;
    }
    write_file(argv[5], out, out_cap - avail_out);
  } else {
    BrotliDecoderState* dec = BrotliDecoderCreateInstance(NULL, NULL, NULL);
    BrotliDecoderSetParameter(dec, BROTLI_DECODER_PARAM_LARGE_WINDOW, 1);
    if (!BrotliDecoderAttachDictionary(dec, type, dict_size, dict)) {
      fprintf(stderr, "decoder attach failed\n"); return 3;
    }
    size_t out_cap = 1 << 26;
    unsigned char* out = malloc(out_cap);
    size_t avail_in = in_size, avail_out = out_cap;
    const unsigned char* next_in = in;
    unsigned char* next_out = out;
    BrotliDecoderResult r = BrotliDecoderDecompressStream(
        dec, &avail_in, &next_in, &avail_out, &next_out, NULL);
    if (r != BROTLI_DECODER_RESULT_SUCCESS) {
      fprintf(stderr, "decompress failed: %s\n",
              BrotliDecoderErrorString(BrotliDecoderGetErrorCode(dec)));
      return 3;
    }
    write_file(argv[5], out, out_cap - avail_out);
  }
  return 0;
}
