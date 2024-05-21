#ifndef _RAR_DEFS_
#define _RAR_DEFS_

#define  Min(x,y) (((x)<(y)) ? (x):(y))
#define  Max(x,y) (((x)>(y)) ? (x):(y))

// Universal replacement of abs function.
#define  Abs(x) (((x)<0) ? -(x):(x))

#define  ASIZE(x) (sizeof(x)/sizeof(x[0]))

// MAXPASSWORD and MAXPASSWORD_RAR are expected to be multiple of
// CRYPTPROTECTMEMORY_BLOCK_SIZE (16) for CryptProtectMemory in SecPassword.
// We allow a larger MAXPASSWORD to unpack archives with lengthy passwords
// in non-RAR formats in GUI versions. For RAR format we set MAXPASSWORD_RAR
// to 128 for compatibility and because it is enough for AES-256.
#define  MAXPASSWORD       512
#define  MAXPASSWORD_RAR   128

// Set some arbitrary sensible limit to maximum path length to prevent
// the excessive memory allocation for dynamically allocated strings.
#define  MAXPATHSIZE       0x10000

#define  MAXSFXSIZE        0x200000

#define  MAXCMTSIZE        0x40000

#ifdef _WIN_32
#define  DefSFXName        L"default32.sfx"
#else
#define  DefSFXName        L"default.sfx"
#endif
#define  DefSortListName   L"rarfiles.lst"

// Maximum dictionary allowed by compression. Can be less than
// maximum dictionary supported by decompression.
#define PACK_MAX_DICT      0x1000000000ULL // 64 GB.

// Maximum dictionary allowed by decompression.
#define UNPACK_MAX_DICT    0x1000000000ULL // 64 GB.


#ifndef SFX_MODULE
#define USE_QOPEN
#endif

// Produce the value, which is equal or larger than 'v' and aligned to 'a'.
#define ALIGN_VALUE(v,a) (size_t(v) + ( (~size_t(v) + 1) & (a - 1) ) )

#endif
