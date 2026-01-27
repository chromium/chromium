#include "common.h"
#include "dmg/adc.h"
#include "dmg/dmg.h"
#include "dmg/compress.h"

#include <bzlib.h>
#include <pthread.h>
#include <zlib.h>

#include "third_party/lzma_sdk/C/Alloc.h"
#include "third_party/lzma_sdk/C/7zCrc.h"
#include "third_party/lzma_sdk/C/7zTypes.h"
#include "third_party/lzma_sdk/C/Lzma2Enc.h"
#include "third_party/lzma_sdk/C/Xz.h"
#include "third_party/lzma_sdk/C/XzCrc64.h"
#include "third_party/lzma_sdk/C/XzEnc.h"
#include "third_party/lzma_sdk/google/seven_zip_c_buf_stream.h"

// Calls once-only initialization functions for `lzma_sdk`. Not threadsafe.
static void reallyInitLzmaCrc()
{
  CrcGenerateTable();
  Crc64GenerateTable();
}

// Calls once-only initialization functions for `lzma_sdk` once per process.
// Threadsafe. If `pthread_once` fails, the application crashes.
static void mustInitLzmaCrcIfNeeded()
{
  static pthread_once_t once = PTHREAD_ONCE_INIT;
  ASSERT(!pthread_once(&once, reallyInitLzmaCrc), "can't initialize LZMA CRC tables");
}

// Decodes an XZ block. Returns 0 on success.
static int lzmaDecompress(unsigned char* inBuffer, size_t inSize, unsigned char* outBuffer, size_t outBufSize, size_t* decompSize)
{
  mustInitLzmaCrcIfNeeded();

  CXzUnpacker unpacker;
  ECoderStatus status = CODER_STATUS_NOT_SPECIFIED;
  XzUnpacker_Construct(&unpacker, &g_Alloc);

  SRes result = XzUnpacker_CodeFull(&unpacker, outBuffer, &outBufSize,
    inBuffer, &inSize, CODER_FINISH_ANY, &status);

  // `lzma_sdk` reports final size by writing back into its destLen arg; report
  // that via `decompSize`, matching the existing API designed around `xz`.
  *decompSize = outBufSize;

  int ret = (result != SZ_OK) || (!XzUnpacker_IsStreamWasFinished(&unpacker));

  XzUnpacker_Free(&unpacker);
  return ret;
}

static int lzmaCompress(unsigned char* inBuffer, size_t inSize,
                        unsigned char* outBuffer, size_t outBufSize, size_t* compSize, int level)
{
  mustInitLzmaCrcIfNeeded();

  Z7CBufSeqInStream inWrapper = {
    .buf = inBuffer,
    .bufSz = inSize,
    .offset = 0
  };
  Z7CBufSeqInStream_CreateVTable(&inWrapper);

  Z7CBufSeqOutStream outWrapper = {
    .buf = outBuffer,
    .bufSz = outBufSize,
    .offset = 0
  };
  Z7CBufSeqOutStream_CreateVTable(&outWrapper);

  CXzProps props;
  XzProps_Init(&props);
  // `lzma_sdk` recognizes any negative value for `CLzmaEncProps.level` as
  // "use default". `libdmg-hfsplus`'s "default" sentinel is negative, so
  // this code does not check for it, instead letting `Xz_Encode` eventually
  // handle it.
  props.lzma2Props.lzmaProps.level = level;
  props.lzma2Props.lzmaProps.reduceSize = inSize;
  props.lzma2Props.blockSize = LZMA2_ENC_PROPS_BLOCK_SIZE_SOLID;
  props.reduceSize = inSize;
  props.blockSize = XZ_PROPS_BLOCK_SIZE_SOLID;
  props.checkId = XZ_CHECK_NO;

  SRes codeResult = Xz_Encode(&outWrapper.vt, &inWrapper.vt, &props, NULL);
  *compSize = outWrapper.offset;
  return codeResult != SZ_OK;
}

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
  if (level == COMPRESSION_LEVEL_DEFAULT) {
    level = 9;
  }
  int ret = (BZ2_bzBuffToBuffCompress((char*)outBuffer, &bz2CompSize, (char*)inBuffer, inSize, level, 0, 0) != BZ_OK);
  *compSize = bz2CompSize;
  return ret;
}

static int zlibCompress(unsigned char *inBuffer, size_t inSize,
                        unsigned char *outBuffer, size_t outBufSize, size_t *compSize, int level)
{
  *compSize = outBufSize;
  if (level == COMPRESSION_LEVEL_DEFAULT) {
    level = Z_DEFAULT_COMPRESSION;
  }
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
    comp->compressFn = bz2Compress;
    comp->decompressBuffer = oldDecompressBuffer;
    return 0;
  }
  if (strcasecmp(name, "zlib") == 0) {
    comp->block_type = BLOCK_ZLIB;
    comp->compressFn = zlibCompress;
    comp->decompressBuffer = oldDecompressBuffer;
    return 0;
  }
  if (strcasecmp(name, "lzma") == 0) {
    comp->block_type = BLOCK_LZMA;
    comp->compressFn = lzmaCompress;
    comp->decompressBuffer = modernDecompressBuffer;
    return 0;
  }
#ifdef HAVE_LZFSE
  if (strcasecmp(name, "lzfse") == 0) {
    comp->block_type = BLOCK_LZFSE;
    comp->compressFn = lzfseCompress;
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
    case BLOCK_LZMA:
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
  } else if (type == BLOCK_LZMA) {
    ret = lzmaDecompress(inBuffer, inSize, outBuffer, expectedSize, &decompSize);
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

