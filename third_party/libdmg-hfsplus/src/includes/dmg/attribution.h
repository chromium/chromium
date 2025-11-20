#ifndef ATTRIBUTION_H
#define ATTRIBUTION_H

#include "abstractfile.h"
#include "common.h"
#include <stdint.h>

#define ATTR_SIGNATURE 0x61747472 // 'attr'

/**
 * An "attributable" DMG or a DMG "structured for attribution" has:
 * - either one or two BZ_RAW raw blocks
 * - a serialized form of the `AttributionResource` data structure
 *   tucked into the `Name` in the `plst` section of the XML plist
 *   which describes the raw block
 *
 * An "attributable" DMG can be "attributed" inexpensively: bytes in the raw
 * block of an "attributable" DMG can be changed and the DMG's internal
 * checksums updated without parsing and/or decompressing the entire DMG file.
 */

  // We need:
  // - the BZ_RAW block offset and size
  // - the CRCs before and after the BZ_RAW block
  // - the BZIP checksum offset
  // - the two DMG 'koly' block checksum offsets
  // fUDIFDataForkChecksum: 0x430e7
  // fUDIFMasterChecksum: 0x431f7
  // There's a UDIF checksum (34 bytes?) in each <blkx> dict, which is
  // part of a Base64 encoded struct.  We can Base64 decode to bytes,
  // swizzle the 4 bytes of the checksum, and then Base64 encode back
  // to the same number of bytes.
	/* <dict> */
	/* 	<key>blkx</key> */
	/* 	<array> */
	/* 		<dict> */
	/* 			<key>Attributes</key> */
	/* 			<string>0x0050</string> */
	/* 			<key>CFName</key> */
	/* 			<string>Driver Descriptor Map (DDM : 0)</string> */
	/* 			<key>Data</key> */
	/* 			<data> */
	/* 			bWlzaAAAAAEAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAA */
	/* 			AAII/////wAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA */
	/* 			AAIAAAAgXDMYCQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA */
	/* 			AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA */
	/* 			AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA */
	/* 			AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA */
	/* 			AAAAAAACgAAABgAAAAAAAAAAAAAAAAAAAAAAAAABAAAA */
	/* 			AAAAAAAAAAAAAAAANf////8AAAAAAAAAAAAAAAEAAAAA */
	/* 			AAAAAAAAAAAAAAA1AAAAAAAAAAA= */
	/* 			</data> */
	/* 			<key>ID</key> */
	/* 			<string>-1</string> */
	/* 			<key>Name</key> */
	/* 			<string>Driver Descriptor Map (DDM : 0)</string> */
	/* 		</dict> */

/**
 * Binary representation of data needed to quickly "attribute" a DMG that is
 * structured for attribution.
 */
typedef struct AttributionResource {
  uint32_t signature; /* Set to 'attr'. */
  uint32_t version; /* Set to 1. */
  uint32_t beforeCompressedChecksum; /* CRC of compressed bytes before raw block. */
  uint64_t beforeCompressedLength; /* Number of compressed bytes before raw block. */
  uint32_t beforeUncompressedChecksum; /* CRC of uncompressed bytes before raw block. */
  uint64_t beforeUncompressedLength; /* Number of uncompressed bytes before raw block. */
  uint64_t rawPos; /* Position, in bytes, from start of file to raw block. */
  uint64_t rawLength; /* Length, in bytes, of raw block. */
  uint32_t rawChecksum; /* CRC of bytes in raw block. */
  uint32_t afterCompressedChecksum; /* CRC of compressed bytes after raw block. */
  uint64_t afterCompressedLength; /* Number of compressed bytes after raw block. */
  uint32_t afterUncompressedChecksum; /* CRC of uncompressed bytes after raw block. */
  uint64_t afterUncompressedLength; /* Number of uncompressed bytes after raw block. */
} __attribute__ ((packed)) AttributionResource;

typedef struct AbstractAttribution AbstractAttribution;

enum ShouldKeepRaw {
  KeepNoneRaw,
  KeepCurrentRaw,
  KeepCurrentAndNextRaw,
  KeepRemainingRaw,
};

typedef void (*BeforeMainBlkxFunc)(AbstractAttribution* attribution, AbstractFile* abstractOut, ChecksumToken* dataForkToken);
typedef enum ShouldKeepRaw (*ShouldKeepRawFunc)(AbstractAttribution* attribution, const void* data, size_t len, const void* nextData, size_t nextLen);
typedef void (*ObserveBuffersFunc)(AbstractAttribution* attribution, int didKeepRaw, const void* uncompressedData, size_t uncompressedLen, const void* compressedData, size_t compressedLen);
typedef void (*AfterMainBlkxFunc)(AbstractAttribution* attribution, AbstractFile* abstractOut, ChecksumToken* dataForkToken, AttributionResource* attributionResource);

struct AbstractAttribution {
  // Use this to persist state during operation.
  void* data;

  // Return non-zero if the given buffer should be a raw block (type `BLOCK_RAW`).
  ShouldKeepRawFunc shouldKeepRaw;
  // Invoked for each BLKX run with the uncompressed and compressed data.
  ObserveBuffersFunc observeBuffers;
  // Invoked once immediately before the main BLKX is inserted.
  BeforeMainBlkxFunc beforeMainBlkx;
  // Invoked once immediately after the main BLKX is inserted.
  AfterMainBlkxFunc afterMainBlkx;
};

#ifdef __cplusplus
extern "C" {
#endif
  // Return an `AbstractAttribution` structure (instance) that will preserve the
  // *unique* instance of the given string sentinel.  If no instance, or more
  // than one instance, of the given sentinel is found, attribution will fail.
  AbstractAttribution* createAbstractAttributionPreservingSentinel(const char* sentinel);

  // Copy an "attributable" DMG -- one structured for attribution -- to the
  // given output, write `len` of the given `bytes` over the `sentinel`.
  int updateAttribution(AbstractFile* abstractIn, AbstractFile* abstractOut, const char* sentinel, const char* bytes, size_t len);
#ifdef __cplusplus
}
#endif

#endif
