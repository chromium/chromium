#ifndef FILEVAULT_H
#define FILEVAULT_H

#include <stdint.h>
#include "dmg.h"

#ifdef HAVE_CRYPT

#include <openssl/hmac.h>
#include <openssl/aes.h>

#define FILEVAULT_CIPHER_KEY_LENGTH	16
#define FILEVAULT_CIPHER_BLOCKSIZE	16
#define FILEVAULT_CHUNK_SIZE		4096
#define FILEVAULT_PBKDF2_ITER_COUNT	1000
#define FILEVAULT_MSGDGST_LENGTH	20

/*
 * Information about the FileVault format was yoinked from vfdecrypt, which was written by Ralf-Philipp Weinmann <ralf@coderpunks.org>,
 * Jacob Appelbaum <jacob@appelbaum.net>, and Christian Fromme <kaner@strace.org>
 */

#define FILEVAULT_V2_SIGNATURE 0x656e637263647361ULL

typedef struct FileVaultV1Header {
	uint8_t     padding1[48];
	uint32_t    kdfIterationCount;
	uint32_t    kdfSaltLen;
	uint8_t     kdfSalt[48];
	uint8_t     unwrapIV[0x20];
	uint32_t    wrappedAESKeyLen;
	uint8_t     wrappedAESKey[296];
	uint32_t    wrappedHMACSHA1KeyLen;
	uint8_t     wrappedHMACSHA1Key[300];
	uint32_t    integrityKeyLen;
	uint8_t     integrityKey[48];
	uint8_t     padding2[484];
} __attribute__((__packed__)) FileVaultV1Header;

typedef struct FileVaultV2Header {
	uint64_t    signature;
	uint32_t    version;
	uint32_t    encIVSize;
	uint32_t    unk1;
	uint32_t    unk2;
	uint32_t    unk3;
	uint32_t    unk4;
	uint32_t    unk5;
	UDIFID      uuid;
	uint32_t    blockSize;
	uint64_t    dataSize;
	uint64_t    dataOffset;
	uint8_t     padding[0x260];
	uint32_t    kdfAlgorithm;
	uint32_t    kdfPRNGAlgorithm;
	uint32_t    kdfIterationCount;
	uint32_t    kdfSaltLen;
	uint8_t     kdfSalt[0x20];
	uint32_t    blobEncIVSize;
	uint8_t     blobEncIV[0x20];
	uint32_t    blobEncKeyBits;
	uint32_t    blobEncAlgorithm;
	uint32_t    blobEncPadding;
	uint32_t    blobEncMode;
	uint32_t    encryptedKeyblobSize;
	uint8_t     encryptedKeyblob[0x30];
} __attribute__((__packed__)) FileVaultV2Header;

typedef struct FileVaultInfo {
	union {
		FileVaultV1Header v1;
		FileVaultV2Header v2;
	}             header;

	uint8_t       version;
	uint64_t      dataOffset;
	uint64_t      dataSize;
	uint32_t      blockSize;

	AbstractFile* file;

	HMAC_CTX*	hmacCTX;
	AES_KEY		aesKey;
	AES_KEY		aesEncKey;

	off_t         offset;

	uint32_t      curChunk;
	unsigned char chunk[FILEVAULT_CHUNK_SIZE];

	char          dirty;
	char          headerDirty;
} FileVaultInfo;
#endif

AbstractFile* createAbstractFileFromFileVault(AbstractFile* file, const char* key);

#endif
