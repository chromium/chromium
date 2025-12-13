#ifndef COMMON_H
#define COMMON_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#ifdef WIN32
#include <unistd.h>
#define fseeko fseeko64
#define ftello ftello64
#define off_t off64_t
#define mkdir(x, y) mkdir(x)
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR "/"
#endif

#define TRUE 1
#define FALSE 0

#define FLIPENDIAN(x) flipEndian((unsigned char *)(&(x)), sizeof(x))
#define FLIPENDIANLE(x) flipEndianLE((unsigned char *)(&(x)), sizeof(x))

#define IS_BIG_ENDIAN      0
#define IS_LITTLE_ENDIAN   1

#define TIME_OFFSET_FROM_UNIX 2082844800L
#define APPLE_TO_UNIX_TIME(x) ((x) - TIME_OFFSET_FROM_UNIX)
#define UNIX_TO_APPLE_TIME(x) ((x) + TIME_OFFSET_FROM_UNIX)

#define ASSERT(x, m) if(!(x)) { assertPrint(m); }

static inline void assertPrint(const char *msg) {
	int errsave = errno;
	fflush(stdout);
	fprintf(stderr, "error: %s\n", msg);
	if (errsave != 0)
		fprintf(stderr, "system error: %s\n", strerror(errsave));
	fflush(stderr);
	exit(1);
}

extern char endianness;

static inline void flipEndian(unsigned char* x, int length) {
	int i;
	unsigned char tmp;

	if(endianness == IS_BIG_ENDIAN) {
		return;
	} else {
		for(i = 0; i < (length / 2); i++) {
			tmp = x[i];
			x[i] = x[length - i - 1];
			x[length - i - 1] = tmp;
		}
	}
}

static inline void flipEndianLE(unsigned char* x, int length) {
	int i;
	unsigned char tmp;

	if(endianness == IS_LITTLE_ENDIAN) {
		return;
	} else {
		for(i = 0; i < (length / 2); i++) {
			tmp = x[i];
			x[i] = x[length - i - 1];
			x[length - i - 1] = tmp;
		}
	}
}

static inline void hexToBytes(const char* hex, uint8_t** buffer, size_t* bytes) {
	*bytes = strlen(hex) / 2;
	*buffer = (uint8_t*) malloc(*bytes);
	size_t i;
	for(i = 0; i < *bytes; i++) {
		uint32_t byte;
		sscanf(hex, "%2x", &byte);
		(*buffer)[i] = byte;
		hex += 2;
	}
}

static inline void hexToInts(const char* hex, unsigned int** buffer, size_t* bytes) {
	*bytes = strlen(hex) / 2;
	*buffer = (unsigned int*) malloc((*bytes) * sizeof(int));
	size_t i;
	for(i = 0; i < *bytes; i++) {
		sscanf(hex, "%2x", &((*buffer)[i]));
		hex += 2;
	}
}

struct io_func_struct;

typedef int (*readFunc)(struct io_func_struct* io, off_t location, size_t size, void *buffer);
typedef int (*writeFunc)(struct io_func_struct* io, off_t location, size_t size, void *buffer);
typedef void (*closeFunc)(struct io_func_struct* io);

typedef struct io_func_struct {
	void* data;
	readFunc read;
	writeFunc write;
	closeFunc close;
} io_func;

struct AbstractFile;

unsigned char* decodeBase64(char* toDecode, size_t* dataLength);
void writeBase64(struct AbstractFile* file, unsigned char* data, size_t dataLength, int tabLength, int width);
char* convertBase64(unsigned char* data, size_t dataLength, int tabLength, int width);

#define SHA1_DIGEST_SIZE 20

typedef struct {
	uint32_t state[5];
	uint32_t count[2];
	uint8_t  buffer[64];
} SHA1_CTX;

typedef struct {
	uint32_t block;
	uint32_t crc;
	SHA1_CTX sha1;
} ChecksumToken;

#endif
