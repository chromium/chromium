#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <dmg/dmg.h>
#include <dmg/filevault.h>
#include <inttypes.h>

#ifdef HAVE_CRYPT

#include <openssl/hmac.h>
#include <openssl/aes.h>
#include <openssl/evp.h>

#define CHUNKNO(oft, info) ((uint32_t)((oft)/info->blockSize))
#define CHUNKOFFSET(oft, info) ((size_t)((oft) - ((off_t)(CHUNKNO(oft, info)) * (off_t)info->blockSize)))

static void flipFileVaultV2Header(FileVaultV2Header* header) {
	FLIPENDIAN(header->signature);
	FLIPENDIAN(header->version);
	FLIPENDIAN(header->encIVSize);
	FLIPENDIAN(header->unk1);
	FLIPENDIAN(header->unk2);
	FLIPENDIAN(header->unk3);
	FLIPENDIAN(header->unk4);
	FLIPENDIAN(header->unk5);
	FLIPENDIAN(header->unk5);

	FLIPENDIAN(header->blockSize);
	FLIPENDIAN(header->dataSize);
	FLIPENDIAN(header->dataOffset);
	FLIPENDIAN(header->kdfAlgorithm);
	FLIPENDIAN(header->kdfPRNGAlgorithm);
	FLIPENDIAN(header->kdfIterationCount);
	FLIPENDIAN(header->kdfSaltLen);
	FLIPENDIAN(header->blobEncIVSize);
	FLIPENDIAN(header->blobEncKeyBits);
	FLIPENDIAN(header->blobEncAlgorithm);
	FLIPENDIAN(header->blobEncPadding);
	FLIPENDIAN(header->blobEncMode);
	FLIPENDIAN(header->encryptedKeyblobSize);
}

static void writeChunk(FileVaultInfo* info) {
	unsigned char buffer[info->blockSize];
	unsigned char buffer2[info->blockSize];
	unsigned char msgDigest[FILEVAULT_MSGDGST_LENGTH];
	uint32_t msgDigestLen;
	uint32_t myChunk;

	myChunk = info->curChunk;

	FLIPENDIAN(myChunk);
    HMAC_Init_ex(info->hmacCTX, NULL, 0, NULL, NULL);
	HMAC_Update(info->hmacCTX, (unsigned char *) &myChunk, sizeof(uint32_t));
	HMAC_Final(info->hmacCTX, msgDigest, &msgDigestLen);

	AES_cbc_encrypt(info->chunk, buffer, info->blockSize, &(info->aesEncKey), msgDigest, AES_ENCRYPT);

	info->file->seek(info->file, (info->curChunk * info->blockSize) + info->dataOffset);
	info->file->read(info->file, buffer2, info->blockSize);

	info->file->seek(info->file, (info->curChunk * info->blockSize) + info->dataOffset);
	info->file->write(info->file, buffer, info->blockSize);

	info->dirty = FALSE;
}

static void cacheChunk(FileVaultInfo* info, uint32_t chunk) {
	unsigned char buffer[info->blockSize];
	unsigned char msgDigest[FILEVAULT_MSGDGST_LENGTH];
	uint32_t msgDigestLen;

	if(chunk == info->curChunk) {
		return;
	}

	if(info->dirty) {
		writeChunk(info);
	}

	info->file->seek(info->file, chunk * info->blockSize + info->dataOffset);
	info->file->read(info->file, buffer, info->blockSize);

	info->curChunk = chunk;

	FLIPENDIAN(chunk);
    HMAC_Init_ex(info->hmacCTX, NULL, 0, NULL, NULL);
	HMAC_Update(info->hmacCTX, (unsigned char *) &chunk, sizeof(uint32_t));
	HMAC_Final(info->hmacCTX, msgDigest, &msgDigestLen);

	AES_cbc_encrypt(buffer, info->chunk, info->blockSize, &(info->aesKey), msgDigest, AES_DECRYPT);
}

size_t fvRead(AbstractFile* file, void* data, size_t len) {
	FileVaultInfo* info;
	size_t toRead;

	info = (FileVaultInfo*) (file->data);

	if((CHUNKOFFSET(info->offset, info) + len) > info->blockSize) {
		toRead = info->blockSize - CHUNKOFFSET(info->offset, info);
		memcpy(data, (void *)((uint8_t*)(&(info->chunk)) + CHUNKOFFSET(info->offset, info)), toRead);
		info->offset += toRead;
		cacheChunk(info, CHUNKNO(info->offset, info));
		return toRead + fvRead(file, (void *)((uint8_t*)data + toRead), len - toRead);
	} else {
		toRead = len;
		memcpy(data, (void *)((uint8_t*)(&(info->chunk)) + CHUNKOFFSET(info->offset, info)), toRead);
		info->offset += toRead;
		cacheChunk(info, CHUNKNO(info->offset, info));
		return toRead;
	}
}

size_t fvWrite(AbstractFile* file, const void* data, size_t len) {
	FileVaultInfo* info;
	size_t toRead;
	int i;

	info = (FileVaultInfo*) (file->data);

	if(info->dataSize < (info->offset + len)) {
		if(info->version == 2) {
			info->header.v2.dataSize = info->offset + len;
		}
		info->headerDirty = TRUE;
	}

	if((CHUNKOFFSET(info->offset, info) + len) > info->blockSize) {
		toRead = info->blockSize - CHUNKOFFSET(info->offset, info);
		for(i = 0; i < toRead; i++) {
			ASSERT(*((char*)((uint8_t*)(&(info->chunk)) + (uint32_t)CHUNKOFFSET(info->offset, info) + i)) == ((char*)data)[i], "blah");
		}
		memcpy((void *)((uint8_t*)(&(info->chunk)) + (uint32_t)CHUNKOFFSET(info->offset, info)), data, toRead);
		info->dirty = TRUE;
		info->offset += toRead;
		cacheChunk(info, CHUNKNO(info->offset, info));
		return toRead + fvWrite(file, (void *)((uint8_t*)data + toRead), len - toRead);
	} else {
		toRead = len;
		for(i = 0; i < toRead; i++) {
			ASSERT(*((char*)((uint8_t*)(&(info->chunk)) + CHUNKOFFSET(info->offset, info) + i)) == ((char*)data)[i], "blah");
		}
		memcpy((void *)((uint8_t*)(&(info->chunk)) + CHUNKOFFSET(info->offset, info)), data, toRead);
		info->dirty = TRUE;
		info->offset += toRead;
		cacheChunk(info, CHUNKNO(info->offset, info));
		return toRead;
	}
}

int fvSeek(AbstractFile* file, off_t offset) {
	FileVaultInfo* info = (FileVaultInfo*) (file->data);
	info->offset = offset;
	cacheChunk(info, CHUNKNO(offset, info));
	return 0;
}

off_t fvTell(AbstractFile* file) {
	FileVaultInfo* info = (FileVaultInfo*) (file->data);
	return info->offset;
}

off_t fvGetLength(AbstractFile* file) {
	FileVaultInfo* info = (FileVaultInfo*) (file->data);
	return info->dataSize;
}

void fvClose(AbstractFile* file) {
	FileVaultInfo* info = (FileVaultInfo*) (file->data);

	/* force a flush */
	if(info->curChunk == 0) {
		cacheChunk(info, 1);
	} else {
		cacheChunk(info, 0);
	}

	HMAC_CTX_free(info->hmacCTX);

	if(info->headerDirty) {
		if(info->version == 2) {
			file->seek(file, 0);
			flipFileVaultV2Header(&(info->header.v2));
			file->write(file, &(info->header.v2), sizeof(FileVaultV2Header));
		}
	}

	info->file->close(info->file);
	free(info);
	free(file);
}

AbstractFile* createAbstractFileFromFileVault(AbstractFile* file, const char* key) {
	FileVaultInfo* info;
	AbstractFile* toReturn;
	uint64_t signature;
	uint8_t aesKey[16];	
	uint8_t hmacKey[20];
	
	int i;

	if(file == NULL)
		return NULL;
	
	file->seek(file, 0);
	file->read(file, &signature, sizeof(uint64_t));
	FLIPENDIAN(signature);
	if(signature != FILEVAULT_V2_SIGNATURE) {
		printf("Unknown signature: %" PRId64 "\n", signature);
		/* no FileVault v1 handling yet */
		return NULL;
	}

	toReturn = (AbstractFile*) malloc(sizeof(AbstractFile));	
	info = (FileVaultInfo*) malloc(sizeof(FileVaultInfo));

	info->version = 2;

	file->seek(file, 0);
	file->read(file, &(info->header.v2), sizeof(FileVaultV2Header));
	flipFileVaultV2Header(&(info->header.v2));
	
	for(i = 0; i < 16; i++) {
		unsigned int curByte;
		sscanf(&(key[i * 2]), "%02x", &curByte);
		aesKey[i] = curByte;
	}

	for(i = 0; i < 20; i++) {
		unsigned int curByte;
		sscanf(&(key[(16 * 2) + i * 2]), "%02x", &curByte);
		hmacKey[i] = curByte;
	}

    info->hmacCTX = HMAC_CTX_new();
	HMAC_Init_ex(info->hmacCTX, hmacKey, sizeof(hmacKey), EVP_sha1(), NULL);
	AES_set_decrypt_key(aesKey, FILEVAULT_CIPHER_KEY_LENGTH * 8, &(info->aesKey));
	AES_set_encrypt_key(aesKey, FILEVAULT_CIPHER_KEY_LENGTH * 8, &(info->aesEncKey));

	info->dataOffset = info->header.v2.dataOffset;
	info->dataSize = info->header.v2.dataSize;
	info->blockSize = info->header.v2.blockSize;
	info->offset = 0;
	info->file = file;

	info->headerDirty = FALSE;
	info->dirty = FALSE;
	info->curChunk = 1; /* just to set it to a value not 0 */
	cacheChunk(info, 0);

	toReturn->data = info;
	toReturn->read = fvRead;
	toReturn->write = fvWrite;
	toReturn->seek = fvSeek;
	toReturn->tell = fvTell;
	toReturn->getLength = fvGetLength;
	toReturn->close = fvClose;
	return toReturn;
}

#else

AbstractFile* createAbstractFileFromFileVault(AbstractFile* file, const char* key) {
	return NULL;
}

#endif
