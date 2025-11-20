#ifndef DMG_H
#define DMG_H

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <hfs/hfsplus.h>
#include "abstractfile.h"
#include "attribution.h"
#include "common.h"
#include "compress.h"

// Okay, this value sucks. You shouldn't touch it because it affects how many ignore sections get added to the blkx list
// If the blkx list gets too fragmented with ignore sections, then the copy list in certain versions of the iPhone's
// asr becomes too big. Due to Apple's BUGGY CODE, this causes asr to segfault! This is because the copy list becomes
// too large for the initial buffer allocated, and realloc is called by asr. Unfortunately, after the realloc, the initial
// pointer is still used by asr for a little while! Frakking noob mistake.

// The only reason why it works at all is their really idiotic algorithm to determine where to put ignore blocks. It's
// certainly nothing reasonable like "put in an ignore block if you encounter more than X blank sectors" (like mine)
// There's always a large-ish one at the end, and a tiny 2 sector one at the end too, to take care of the space after
// the backup volume header. No frakking clue how they go about determining how to do that.

#define DEFAULT_SECTORS_AT_A_TIME 0x200

#define CHECKSUM_UDIF_CRC32 0x00000002
#define CHECKSUM_MD5 0x00000004
#define CHECKSUM_MKBLOCK 0x0002
#define CHECKSUM_NONE 0x0000

#define BLOCK_ZEROES 0x00000000
#define BLOCK_RAW 0x00000001
#define BLOCK_IGNORE 0x00000002
#define BLOCK_ADC 0x80000004
#define BLOCK_ZLIB 0x80000005
#define BLOCK_BZIP2 0x80000006
#define BLOCK_LZFSE 0x80000007
#define BLOCK_LZMA 0x80000008
#define BLOCK_COMMENT 0x7FFFFFFE
#define BLOCK_TERMINATOR 0xFFFFFFFF

#define SECTOR_SIZE 512

#define DRIVER_DESCRIPTOR_SIGNATURE 0x4552
#define APPLE_PARTITION_MAP_SIGNATURE 0x504D
#define UDIF_BLOCK_SIGNATURE 0x6D697368
#define KOLY_SIGNATURE 0x6B6F6C79
#define HFSX_SIGNATURE 0x4858

#define ATTRIBUTE_HDIUTIL 0x0050

#define HFSX_VOLUME_TYPE "Apple_HFSX"

#define DDM_SIZE 0x1
#define PARTITION_SIZE 0x3f
#define ATAPI_SIZE 0x8
#define FREE_SIZE 0xa
#define EXTRA_SIZE (ATAPI_OFFSET + ATAPI_SIZE + FREE_SIZE)

#define DDM_OFFSET 0x0
#define PARTITION_OFFSET (DDM_SIZE)
#define ATAPI_OFFSET 64
#define USER_OFFSET (ATAPI_OFFSET + ATAPI_SIZE)

#define BOOTCODE_DMMY 0x444D4D59
#define BOOTCODE_GOON 0x676F6F6E

#define bool short
#define true 1
#define false 0

enum {
	kUDIFFlagsFlattened = 1
};

enum {
	kUDIFDeviceImageType = 1,
	kUDIFPartitionImageType = 2
};

typedef struct {
	uint32_t type;
	uint32_t bitness;
	uint32_t data[0x20];
} __attribute__((__packed__)) UDIFChecksum;

typedef struct {
	uint32_t data1; /* smallest */
	uint32_t data2;
	uint32_t data3;
	uint32_t data4; /* largest */
} __attribute__((__packed__)) UDIFID;

typedef struct {
	uint32_t fUDIFSignature;
	uint32_t fUDIFVersion;
	uint32_t fUDIFHeaderSize;
	uint32_t fUDIFFlags;
	
	uint64_t fUDIFRunningDataForkOffset;
	uint64_t fUDIFDataForkOffset;
	uint64_t fUDIFDataForkLength;
	uint64_t fUDIFRsrcForkOffset;
	uint64_t fUDIFRsrcForkLength;
	
	uint32_t fUDIFSegmentNumber;
	uint32_t fUDIFSegmentCount;
	UDIFID fUDIFSegmentID;  /* a 128-bit number like a GUID, but does not seem to be a OSF GUID, since it doesn't have the proper versioning byte */
	
	UDIFChecksum fUDIFDataForkChecksum;
	
	uint64_t fUDIFXMLOffset;
	uint64_t fUDIFXMLLength;
	
	uint8_t reserved1[0x78]; /* this is actually the perfect amount of space to store every thing in this struct until the checksum */
	
	UDIFChecksum fUDIFMasterChecksum;
	
	uint32_t fUDIFImageVariant;
	uint64_t fUDIFSectorCount;
	
	uint32_t reserved2;
	uint32_t reserved3;
	uint32_t reserved4;
	
} __attribute__((__packed__)) UDIFResourceFile;

typedef struct {
	uint32_t type;
	uint32_t reserved;
	uint64_t sectorStart;
	uint64_t sectorCount;
	uint64_t compOffset;
	uint64_t compLength;
} __attribute__((__packed__)) BLKXRun;

typedef struct {
	uint16_t version; /* set to 5 */
	uint32_t isHFS; /* first dword of v53(ImageInfoRec): Set to 1 if it's a HFS or HFS+ partition -- duh. */
	uint32_t unknown1; /* second dword of v53: seems to be garbage if it's HFS+, stuff related to HFS embedded if it's that*/
	uint8_t dataLen; /* length of data that proceeds, comes right before the data in ImageInfoRec. Always set to 0 for HFS, HFS+ */
	uint8_t data[255]; /* other data from v53, dataLen + 1 bytes, the rest NULL filled... a string? Not set for HFS, HFS+ */
	uint32_t unknown2; /* 8 bytes before volumeModified in v53, seems to be always set to 0 for HFS, HFS+  */
	uint32_t unknown3; /* 4 bytes before volumeModified in v53, seems to be always set to 0 for HFS, HFS+ */
	uint32_t volumeModified; /* offset 272 in v53 */
	uint32_t unknown4; /* always seems to be 0 for UDIF */
	uint16_t volumeSignature; /* HX in our case */
	uint16_t sizePresent; /* always set to 1 */
} __attribute__((__packed__)) SizeResource;

typedef struct {
	uint16_t version; /* set to 1 */
	uint32_t type; /* set to 0x2 for MKBlockChecksum */
	uint32_t checksum;
} __attribute__((__packed__)) CSumResource;

typedef struct NSizResource {
	char isVolume;
	unsigned char* sha1Digest;
	uint32_t blockChecksum2;
	uint32_t bytes;
	uint32_t modifyDate;
	uint32_t partitionNumber;
	uint32_t version;
	uint32_t volumeSignature;
	struct NSizResource* next;
} NSizResource;

#define DDM_DESCRIPTOR 0xFFFFFFFF
#define ENTIRE_DEVICE_DESCRIPTOR 0xFFFFFFFE

typedef struct {
	uint32_t fUDIFBlocksSignature;
	uint32_t infoVersion;
	uint64_t firstSectorNumber;
	uint64_t sectorCount;
	
	uint64_t dataStart;
	uint32_t decompressBufferRequested;
	uint32_t blocksDescriptor;
	
	uint32_t reserved1;
	uint32_t reserved2;
	uint32_t reserved3;
	uint32_t reserved4;
	uint32_t reserved5;
	uint32_t reserved6;
	
	UDIFChecksum checksum;
	
	uint32_t blocksRunCount;
	BLKXRun runs[0];
} __attribute__((__packed__)) BLKXTable;

typedef struct {
	uint32_t ddBlock;
	uint16_t ddSize;
	uint16_t ddType;
} __attribute__((__packed__)) DriverDescriptor;

typedef struct {
	uint16_t pmSig;
	uint16_t pmSigPad;
	uint32_t pmMapBlkCnt;
	uint32_t pmPyPartStart;
	uint32_t pmPartBlkCnt;
	unsigned char pmPartName[32];
	unsigned char pmParType[32];
	uint32_t pmLgDataStart;
	uint32_t pmDataCnt;
	uint32_t pmPartStatus;
	uint32_t pmLgBootStart;
	uint32_t pmBootSize;
	uint32_t pmBootAddr;
	uint32_t pmBootAddr2;
	uint32_t pmBootEntry;
	uint32_t pmBootEntry2;
	uint32_t pmBootCksum;
	unsigned char pmProcessor[16];
	uint32_t bootCode;
	uint16_t pmPad[186];
} __attribute__((__packed__)) Partition;

typedef struct {
	uint16_t sbSig;
	uint16_t sbBlkSize;
	uint32_t sbBlkCount;
	uint16_t sbDevType;
	uint16_t sbDevId;
	uint32_t sbData;
	uint16_t sbDrvrCount;
	uint32_t ddBlock;
	uint16_t ddSize;
	uint16_t ddType;
	DriverDescriptor ddPad[0];
} __attribute__((__packed__)) DriverDescriptorRecord;

typedef struct ResourceData {
	uint32_t attributes;
	unsigned char* data;
	size_t dataLength;
	int id;
	char* name;
	size_t dataXmlOffset;
	size_t dataXmlSize;
	struct ResourceData* next;
} ResourceData;

typedef void (*FlipDataFunc)(unsigned char* data, char out);
typedef void (*ChecksumFunc)(void* ckSum, const unsigned char* data, size_t len);

typedef struct ResourceKey {
	unsigned char* key;
	ResourceData* data;
	struct ResourceKey* next;
	FlipDataFunc flipData;
} ResourceKey;

static inline uint32_t readUInt32(AbstractFile* file) {
	uint32_t data;
	
	ASSERT(file->read(file, &data, sizeof(data)) == sizeof(data), "fread");
	FLIPENDIAN(data);
	
	return data;
}

static inline void writeUInt32(AbstractFile* file, uint32_t data) {
	FLIPENDIAN(data);
	ASSERT(file->write(file, &data, sizeof(data)) == sizeof(data), "fwrite");
}

static inline uint64_t readUInt64(AbstractFile* file) {
	uint64_t data;
	
	ASSERT(file->read(file, &data, sizeof(data)) == sizeof(data), "fread");
	FLIPENDIAN(data);
	
	return data;
}

static inline void writeUInt64(AbstractFile* file, uint64_t data) {
	FLIPENDIAN(data);
	ASSERT(file->write(file, &data, sizeof(data)) == sizeof(data), "fwrite");
}

#ifdef __cplusplus
extern "C" {
#endif
	void outResources(AbstractFile* file, AbstractFile* out);

	uint32_t checksumBitness(uint32_t type);

	uint32_t CRC32Checksum(uint32_t* crc, const unsigned char *buf, size_t len);
	uint32_t MKBlockChecksum(uint32_t* ckSum, const unsigned char* data, size_t len);

	void BlockSHA1CRC(void* token, const unsigned char* data, size_t len);
	void BlockCRC(void* token, const unsigned char* data, size_t len);
	void CRCProxy(void* token, const unsigned char* data, size_t len);

	void SHA1Init(SHA1_CTX* context);
	void SHA1Update(SHA1_CTX* context, const uint8_t* data, const size_t len);
	void SHA1Final(uint8_t digest[SHA1_DIGEST_SIZE], SHA1_CTX* context);

	void flipUDIFChecksum(UDIFChecksum* o, char out);
	void readUDIFChecksum(AbstractFile* file, UDIFChecksum* o);
	void writeUDIFChecksum(AbstractFile* file, UDIFChecksum* o);
	void readUDIFID(AbstractFile* file, UDIFID* o);
	void writeUDIFID(AbstractFile* file, UDIFID* o);
	void readUDIFResourceFile(AbstractFile* file, UDIFResourceFile* o);
	void writeUDIFResourceFile(AbstractFile* file, UDIFResourceFile* o);

	ResourceKey* readResources(char* xml, size_t length, bool plstNameIsAttribution);
	void writeResources(AbstractFile* file, ResourceKey* resources, bool plstNameIsAttribution);
	void releaseResources(ResourceKey* resources);

	NSizResource* readNSiz(ResourceKey* resources);
	ResourceKey* writeNSiz(NSizResource* nSiz);
	void releaseNSiz(NSizResource* nSiz);

	extern const char* plistHeader;
	extern const char* plistFooter;

	ResourceKey* getResourceByKey(ResourceKey* resources, const char* key);
	ResourceData* getDataByID(ResourceKey* resource, int id);
	ResourceKey* insertData(ResourceKey* resources, const char* key, int id, const char* name, size_t nameLength, bool nameAsData, const char* data, size_t dataLength, uint32_t attributes);
	ResourceKey* makePlst(const char* name, size_t nameLength, bool nameAsData);
	ResourceKey* makeSize(HFSPlusVolumeHeader* volumeHeader);

	void flipDriverDescriptorRecord(DriverDescriptorRecord* record, char out);
	void flipPartition(Partition* partition, char out, unsigned int BlockSize);
	void flipPartitionMultiple(Partition* partition, char multiple, char out, unsigned int BlockSize);

	void readDriverDescriptorMap(AbstractFile* file, ResourceKey* resources);
	DriverDescriptorRecord* createDriverDescriptorMap(uint32_t numSectors, unsigned int BlockSize);
	int writeDriverDescriptorMap(int pNum, AbstractFile* file, DriverDescriptorRecord* DDM, unsigned int BlockSize, ChecksumFunc dataForkChecksum, void* dataForkToken, ResourceKey **resources,
		Compressor *comp, size_t runSectors);
	void readApplePartitionMap(AbstractFile* file, ResourceKey* resources, unsigned int BlockSize);
	Partition* createApplePartitionMap(uint32_t numSectors, const char* volumeType, unsigned int BlockSize);
	int writeApplePartitionMap(int pNum, AbstractFile* file, Partition* partitions, unsigned int BlockSize, ChecksumFunc dataForkChecksum, void* dataForkToken, ResourceKey **resources, NSizResource** nsizIn,
		Compressor *comp, size_t runSectors);
	int writeATAPI(int pNum, AbstractFile* file, unsigned int BlockSize, ChecksumFunc dataForkChecksum, void* dataForkToken, ResourceKey **resources, NSizResource** nsizIn,
		Compressor *comp, size_t runSectors);
	int writeFreePartition(int pNum, AbstractFile* outFile, uint32_t offset, uint32_t numSectors, ResourceKey** resources);

	void extractBLKX(AbstractFile* in, AbstractFile* out, BLKXTable* blkx);
	BLKXTable* insertBLKX(AbstractFile* out, AbstractFile* in, uint32_t firstSectorNumber, uint32_t numSectors, uint32_t blocksDescriptor,
	            uint32_t checksumType, ChecksumFunc uncompressedChk, void* uncompressedChkToken, ChecksumFunc compressedChk,
				void* compressedChkToken, Volume* volume, AbstractAttribution* attribution, Compressor* comp, size_t runSectors);


	int extractDmg(AbstractFile* abstractIn, AbstractFile* abstractOut, int partNum);
	int buildDmg(AbstractFile* abstractIn, AbstractFile* abstractOut, unsigned int BlockSize, const char* sentinel, Compressor *comp, size_t runSectors);
	int convertToISO(AbstractFile* abstractIn, AbstractFile* abstractOut);
	int convertToDMG(AbstractFile* abstractIn, AbstractFile* abstractOut, Compressor* comp, size_t runSectors);
#ifdef __cplusplus
}
#endif

#endif
