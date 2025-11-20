#include "dmg/dmg.h"
#include "dmg/compress.h"

#include <zlib.h>
#include <bzlib.h>
#include "dmg/adc.h"

#ifdef HAVE_LIBLZMA
  #include <lzma.h>

  // LZMA_FAIL_FAST was introduced in 5.3.3, attempting to use it in earlier versions
  // yields an error.
  #ifdef LZMA_FAIL_FAST
    #define LZMA_DECODE_OPTS LZMA_FAIL_FAST
  #else
    #define LZMA_DECODE_OPTS 0
  #endif

  static int lzmaDecompress(unsigned char* inBuffer, size_t inSize, unsigned char* outBuffer, size_t outBufSize, size_t *decompSize)
  {
    lzma_ret lret;
    /* More memory than lzma ever needs */
    uint64_t error_if_memory_usage_exceeds = 65 * 1024 * 1024;
    size_t inPos = 0;
    *decompSize = 0;

    lret = lzma_stream_buffer_decode(&error_if_memory_usage_exceeds, LZMA_DECODE_OPTS, NULL,
      inBuffer, &inPos, inSize, outBuffer, decompSize, outBufSize);
    return lret != LZMA_OK;
  }

  static int lzmaCompress(unsigned char *inBuffer, size_t inSize,
                          unsigned char *outBuffer, size_t outBufSize, size_t *compSize, int level)
  {
    lzma_ret lret;

    *compSize = 0;
    if (level == COMPRESSION_LEVEL_DEFAULT)
      level = LZMA_PRESET_DEFAULT;
    lret = lzma_easy_buffer_encode(level, LZMA_CHECK_NONE, NULL, inBuffer, inSize, outBuffer,
      compSize, outBufSize);
    return lret != LZMA_OK;
  }
#endif

#ifdef HAVE_LZFSE
  #include <lzfse.h>

  static int lzfseDecompress(unsigned char* inBuffer, size_t inSize, unsigned char* outBuffer, size_t outBufSize, size_t *decompSize)
  {
    *decompSize = lzfse_decode_buffer(outBuffer, outBufSize, inBuffer, inSize, NULL);
    return !*decompSize;
  }

  static int lzfseCompress(unsigned char *inBuffer, size_t inSize,
                          unsigned char *outBuffer, size_t outBufSize, size_t *compSize, int level)
  {
    *compSize = lzfse_encode_buffer(outBuffer, outBufSize, inBuffer, inSize, NULL);
    return !*compSize;
  }
#endif

static int bz2Compress(unsigned char *inBuffer, size_t inSize,
                       unsigned char *outBuffer, size_t outBufSize, size_t *compSize, int level)
{
  unsigned int bz2CompSize = outBufSize;
  if (level == COMPRESSION_LEVEL_DEFAULT)
    level = 9;
  int ret = (BZ2_bzBuffToBuffCompress((char*)outBuffer, &bz2CompSize, (char*)inBuffer, inSize, level, 0, 0) != BZ_OK);
  *compSize = bz2CompSize;
  return ret;
}

static int zlibCompress(unsigned char *inBuffer, size_t inSize,
                        unsigned char *outBuffer, size_t outBufSize, size_t *compSize, int level)
{
  *compSize = outBufSize;
  if (level == COMPRESSION_LEVEL_DEFAULT)
    level = Z_DEFAULT_COMPRESSION;
  return (compress2(outBuffer, compSize, inBuffer, inSize, level) != Z_OK);
}

size_t oldDecompressBuffer(size_t runSectors)
{
  // A reasonable heuristic
  // Bzip2 at level 1 usually needs the largest extra space, compared to other compressors.
  // Happens to equal 0x208 for 0x200 sectors, same as before.
  return runSectors + 4 + (runSectors >> 7);
}

size_t modernDecompressBuffer(size_t runSectors)
{
  // Modern algorithms need more space for some reason, about double the size of a run.
  // Sometimes it's a bit more, so add a generous amount of padding.
  // It's unclear why so much is needed, lzma/lzfse shouldn't need this much in normal usage!
  return runSectors * 2 + 64;
}

void initDefaultCompressor(Compressor* comp)
{
  comp->level = COMPRESSION_LEVEL_DEFAULT;
  getCompressor(comp, COMPRESSOR_DEFAULT);
}

int getCompressor(Compressor* comp, char *name)
{
  if (strcasecmp(name, "bzip2") == 0) {
    comp->block_type = BLOCK_BZIP2;
    comp->compress = bz2Compress;
    comp->decompressBuffer = oldDecompressBuffer;
    return 0;
  }
  if (strcasecmp(name, "zlib") == 0) {
    comp->block_type = BLOCK_ZLIB;
    comp->compress = zlibCompress;
    comp->decompressBuffer = oldDecompressBuffer;
    return 0;
  }
#ifdef HAVE_LIBLZMA
  if (strcasecmp(name, "lzma") == 0) {
    comp->block_type = BLOCK_LZMA;
    comp->compress = lzmaCompress;
    comp->decompressBuffer = modernDecompressBuffer;
    return 0;
  }
#endif
#ifdef HAVE_LZFSE
  if (strcasecmp(name, "lzfse") == 0) {
    comp->block_type = BLOCK_LZFSE;
    comp->compress = lzfseCompress;
    comp->decompressBuffer = modernDecompressBuffer;
    return 0;
  }
#endif

  return 1;
}

const char *compressionNames()
{
  return "bzip2, zlib"
#ifdef HAVE_LIBLZMA
    ", lzma"
#endif
#ifdef HAVE_LZFSE
    ", lzfse"
#endif
  ;
}

int compressionBlockTypeSupported(uint32_t type)
{
  switch (type) {
    case BLOCK_ADC:
    case BLOCK_BZIP2:
    case BLOCK_ZLIB:
#ifdef HAVE_LIBLZMA
    case BLOCK_LZMA:
#endif
#ifdef HAVE_LZFSE
    case BLOCK_LZFSE:
#endif
      return 0;
  }
  return 1;
}

int decompressRun(uint32_t type,
                  unsigned char* inBuffer, size_t inSize,
                  unsigned char* outBuffer, size_t expectedSize)
{
  size_t decompSize;
  int ret;

  if (type == BLOCK_ADC) {
    ret = (adc_decompress(inSize, inBuffer, expectedSize, outBuffer, &decompSize) != inSize);
  } else if (type == BLOCK_ZLIB) {
    decompSize = expectedSize;
    ret = (uncompress(outBuffer, &decompSize, inBuffer, inSize) != Z_OK);
  } else if (type == BLOCK_BZIP2) {
    unsigned int bz2DecompSize = expectedSize;
    ret = (BZ2_bzBuffToBuffDecompress((char*)outBuffer, &bz2DecompSize, (char*)inBuffer, inSize, 0, 0) != BZ_OK);
    decompSize = bz2DecompSize;
#ifdef HAVE_LIBLZMA
  } else if (type == BLOCK_LZMA) {
    ret = lzmaDecompress(inBuffer, inSize, outBuffer, expectedSize, &decompSize);
#endif
#ifdef HAVE_LZFSE
  } else if (type == BLOCK_LZFSE) {
    ret = lzfseDecompress(inBuffer, inSize, outBuffer, expectedSize, &decompSize);
#endif
  } else {
    fprintf(stderr, "Unsupported block type: %#08x\n", type);
    return 1;
  }

  if (ret == 0) {
    ASSERT(decompSize == expectedSize, "Decompressed size mismatch");
  }
  return ret;
}

