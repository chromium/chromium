#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include <dmg/dmg.h>
#include <dmg/compress.h>
#include <dmg/attribution.h>
#include <inttypes.h>

typedef struct block {
	size_t bufferSize;

	uint32_t idx;
	BLKXRun run;
	int keepRaw;

	unsigned char* inbuf;
	size_t insize;

	unsigned char* outbuf;
	size_t outsize;
	
	struct block* next;
} block;

typedef struct {
	size_t runSectors;
	size_t bufferSize;
	AbstractAttribution* attribution;

	// Read
	pthread_mutex_t inMut;
	AbstractFile* in;
	uint32_t numSectors;
	uint32_t curRun;
	uint64_t curSector;
	uint64_t startOff;
	unsigned char *nextInBuffer;
	size_t nextInSize;	
	enum ShouldKeepRaw keepRaw;

	// Write
	pthread_mutex_t outMut;
	AbstractFile* out;
	BLKXTable *blkx;
	uint32_t roomForRuns;
	ChecksumFunc uncompressedChk;
	void* uncompressedChkToken;
	ChecksumFunc compressedChk;
	void* compressedChkToken;
	Compressor *compressor;
	size_t nextPending;
	block* pending;
} threadData;

static block* blockAlloc(size_t bufferSize, size_t idx) {
	block* b;
	ASSERT(b = (block*)malloc(sizeof(block)), "malloc");

	b->idx = idx;
	b->bufferSize = bufferSize;
	b->keepRaw = 0;
	b->run.reserved = 0;

	ASSERT(b->inbuf = (unsigned char*)malloc(bufferSize), "malloc");
	ASSERT(b->outbuf = (unsigned char*)malloc(bufferSize), "malloc");
	return b;
}

static void blockFree(block* b) {
	free(b->inbuf);
	free(b->outbuf);
	free(b);
}

// Return NULL when no more blocks
static block* blockRead(threadData* d) {
	ASSERT(pthread_mutex_lock(&d->inMut) == 0, "pthread_mutex_lock");

	if (d->numSectors == 0) {
		ASSERT(pthread_mutex_unlock(&d->inMut) == 0, "pthread_mutex_unlock");
		return NULL;
	}
	
	block* b = blockAlloc(d->bufferSize, d->curRun);
		
	b->run.sectorStart = d->curSector;
	b->run.sectorCount = (d->numSectors > d->runSectors) ? d->runSectors : d->numSectors;
	size_t readSize = b->run.sectorCount * SECTOR_SIZE;

	if (b->idx == 0) {
		ASSERT((b->insize = d->in->read(d->in, b->inbuf, readSize)) == readSize, "mRead");	
	} else {
		// Steal from the next block
		memcpy(b->inbuf, d->nextInBuffer, d->nextInSize);
		b->insize = d->nextInSize;
	}

	if (d->numSectors - b->run.sectorCount > 0) {
		d->nextInSize = d->in->read(d->in, d->nextInBuffer, readSize);
	}

	// printf("run %d: sectors=%" PRId64 ", left=%d\n", b->idx, b->run.sectorCount, d->numSectors);

	if (d->attribution) {
		// We either haven't found the sentinel value yet, or are already past it.
		// Either way, keep searching.
		if (d->keepRaw == KeepNoneRaw) {
			d->keepRaw = d->attribution->shouldKeepRaw(d->attribution, b->inbuf, b->insize, d->nextInBuffer, d->nextInSize);
		}
		// KeepCurrentAndNextRaw means that the *previous* time through the loop `shouldKeepRaw`
		// found the sentinel string, and that it crosses two runs. The previous
		// loop already kept its run raw, and so must we. We don't want the _next_ run
		// to also be raw though, so we adjust this appropriately.
		// Note that KeepCurrentRaw will switch to KeepNoneRaw further down, when we've
		// set the run raw.
		else if (d->keepRaw == KeepCurrentAndNextRaw) {
			d->keepRaw = KeepCurrentRaw;
		}
		else if (d->keepRaw == KeepCurrentRaw) {
			d->keepRaw = KeepRemainingRaw;
		}
		// printf("keepRaw = %d (%p, %ld)\n", d->keepRaw, b->inbuf, b->insize);
		b->keepRaw = (d->keepRaw == KeepCurrentRaw || d->keepRaw == KeepCurrentAndNextRaw);
	}
	
	d->curSector += b->run.sectorCount;
	d->numSectors -= b->run.sectorCount;
	d->curRun++;

	ASSERT(pthread_mutex_unlock(&d->inMut) == 0, "pthread_mutex_unlock");

	return b;
}

static void blockCompress(block* b, Compressor* comp) {
	if (!b->keepRaw) {
		ASSERT(comp->compress(b->inbuf, b->insize, b->outbuf, b->bufferSize, &b->outsize, comp->level) == 0, "compress");
	}
	
	if(b->keepRaw || ((b->outsize / SECTOR_SIZE) >= (b->run.sectorCount - 15))) {
		// printf("Setting type = BLOCK_RAW\n");
		b->run.type = BLOCK_RAW;
		memcpy(b->outbuf, b->inbuf, b->insize);
		b->outsize = b->insize;
	} else {
		b->run.type = comp->block_type;
	}
	b->run.compLength = b->outsize;
}

static void blockWrite(threadData* d, block* b) {
	if(d->uncompressedChk)
		(*d->uncompressedChk)(d->uncompressedChkToken, b->inbuf, b->run.sectorCount * SECTOR_SIZE);
	if(d->compressedChk)
		(*d->compressedChk)(d->compressedChkToken, b->outbuf, b->outsize);
	if (d->attribution)
		d->attribution->observeBuffers(d->attribution, b->keepRaw, b->inbuf, b->insize, b->outbuf, b->outsize);		

	b->run.compOffset = d->out->tell(d->out) - d->blkx->dataStart;
	ASSERT(d->out->write(d->out, b->outbuf, b->outsize) == b->outsize, "fwrite");

	if(b->idx >= d->roomForRuns) {
		d->roomForRuns <<= 1;
		d->blkx = (BLKXTable*) realloc(d->blkx, sizeof(BLKXTable) + (d->roomForRuns * sizeof(BLKXRun)));
	}
	d->blkx->runs[b->idx] = b->run;

	blockFree(b);
}

static void blockQueue(threadData* d, block* b) {
	// Add to correct slot in ordered pending list
	block** bp;
	for (bp = &d->pending; *bp && (*bp)->idx < b->idx; bp = &(*bp)->next)
		; // pass
	b->next = *bp;
	*bp = b;
}

static void blockWriteAll(threadData* d) {
	while (d->pending && d->pending->idx == d->nextPending) {
		block* next = d->pending->next;
		blockWrite(d, d->pending);
		d->nextPending++;
		d->pending = next;
	}
}

static void blockQueueAndWrite(threadData* d, block* b) {
	ASSERT(pthread_mutex_lock(&d->outMut) == 0, "pthread_mutex_lock");
	blockQueue(d, b);
	blockWriteAll(d);
	ASSERT(pthread_mutex_unlock(&d->outMut) == 0, "pthread_mutex_unlock");
}

static void *threadWorker(void* arg) {
	threadData* d = (threadData*)arg;
	block *b;
	
	while(true) {
		if (!(b = blockRead(d)))
			break;

		blockCompress(b, d->compressor);
		blockQueueAndWrite(d, b);
	}

	return NULL;
}

BLKXTable* insertBLKX(AbstractFile* out_, AbstractFile* in_, uint32_t firstSectorNumber, uint32_t numSectors_, uint32_t blocksDescriptor,
			uint32_t checksumType, ChecksumFunc uncompressedChk_, void* uncompressedChkToken_, ChecksumFunc compressedChk_,
			void* compressedChkToken_, Volume* volume, AbstractAttribution* attribution_, Compressor* comp, size_t runSectors) {
	threadData td = {
		.out = out_,
		.in = in_,
		.runSectors = runSectors,
		.numSectors = numSectors_,
		.uncompressedChk = uncompressedChk_,
		.uncompressedChkToken = uncompressedChkToken_,
		.compressedChk = compressedChk_,
		.compressedChkToken = compressedChkToken_,
		.attribution = attribution_,
		.nextPending = 0,
		.pending = NULL,
	};
	pthread_mutex_init(&td.inMut, NULL);
	pthread_mutex_init(&td.outMut, NULL);

	td.compressor = comp;

	td.blkx = (BLKXTable*) malloc(sizeof(BLKXTable) + (2 * sizeof(BLKXRun)));
	td.roomForRuns = 2;
	memset(td.blkx, 0, sizeof(BLKXTable) + (td.roomForRuns * sizeof(BLKXRun)));

	td.blkx->fUDIFBlocksSignature = UDIF_BLOCK_SIGNATURE;
	td.blkx->infoVersion = 1;
	td.blkx->firstSectorNumber = firstSectorNumber;
	td.blkx->sectorCount = td.numSectors;
	td.blkx->dataStart = 0;

	td.blkx->decompressBufferRequested = comp->decompressBuffer(runSectors);
	if (MIN_DECOMPRESS_BUFFER_SECTORS > td.blkx->decompressBufferRequested) {
		td.blkx->decompressBufferRequested = MIN_DECOMPRESS_BUFFER_SECTORS;
	}

	td.blkx->blocksDescriptor = blocksDescriptor;
	td.blkx->reserved1 = 0;
	td.blkx->reserved2 = 0;
	td.blkx->reserved3 = 0;
	td.blkx->reserved4 = 0;
	td.blkx->reserved5 = 0;
	td.blkx->reserved6 = 0;
	memset(&(td.blkx->checksum), 0, sizeof(td.blkx->checksum));
	td.blkx->checksum.type = checksumType;
	td.blkx->checksum.bitness = checksumBitness(checksumType);
	td.blkx->blocksRunCount = 0;

	td.bufferSize = SECTOR_SIZE * td.blkx->decompressBufferRequested;
	ASSERT(td.nextInBuffer = (unsigned char*) malloc(td.bufferSize), "malloc");

	td.curRun = 0;
	td.curSector = 0;

	td.startOff = td.in->tell(td.in);
	td.keepRaw = KeepNoneRaw;

	size_t nthreads = sysconf(_SC_NPROCESSORS_ONLN) + 2; // input + output
	pthread_t* threads;
	ASSERT(threads = (pthread_t*) malloc(nthreads * sizeof(pthread_t)), "malloc");
	size_t i;
	for (i = 0; i < nthreads; i++)
		ASSERT(pthread_create(&threads[i], NULL, threadWorker, &td) == 0, "pthread_create");
	for (i = 0; i < nthreads; i++) {
		void *ret;
		ASSERT(pthread_join(threads[i], &ret) == 0, "pthread_join");
		ASSERT(ret == NULL, "thread return");
	}

	if(td.curRun >= td.roomForRuns) {
		td.roomForRuns <<= 1;
		td.blkx = (BLKXTable*) realloc(td.blkx, sizeof(BLKXTable) + (td.roomForRuns * sizeof(BLKXRun)));
	}

	td.blkx->runs[td.curRun].type = BLOCK_TERMINATOR;
	td.blkx->runs[td.curRun].reserved = 0;
	td.blkx->runs[td.curRun].sectorStart = td.curSector;
	td.blkx->runs[td.curRun].sectorCount = 0;
	td.blkx->runs[td.curRun].compOffset = td.out->tell(td.out) - td.blkx->dataStart;
	td.blkx->runs[td.curRun].compLength = 0;
	td.blkx->blocksRunCount = td.curRun + 1;

	free(td.nextInBuffer);

	return td.blkx;
}

#define DEFAULT_BUFFER_SIZE (1 * 1024 * 1024)

void extractBLKX(AbstractFile* in, AbstractFile* out, BLKXTable* blkx) {
	unsigned char* inBuffer;
	unsigned char* outBuffer;
	unsigned char zero;
	size_t bufferSize;
	size_t have;
	size_t expectedSize;
	off_t initialOffset;
	int i;
	int ret;
	uint32_t type;

	bufferSize = SECTOR_SIZE * blkx->decompressBufferRequested;
	ASSERT(inBuffer = (unsigned char*) malloc(bufferSize), "malloc");

	initialOffset =	out->tell(out);
	ASSERT(initialOffset != -1, "ftello");

	zero = 0;

	for(i = 0; i < blkx->blocksRunCount; i++) {
		ASSERT(in->seek(in, blkx->dataStart + blkx->runs[i].compOffset) == 0, "fseeko");
		ASSERT(out->seek(out, initialOffset + (blkx->runs[i].sectorStart * SECTOR_SIZE)) == 0, "mSeek");

		if(blkx->runs[i].sectorCount > 0) {
			ASSERT(out->seek(out, initialOffset + (blkx->runs[i].sectorStart + blkx->runs[i].sectorCount) * SECTOR_SIZE - 1) == 0, "mSeek");
			ASSERT(out->write(out, &zero, 1) == 1, "mWrite");
			ASSERT(out->seek(out, initialOffset + (blkx->runs[i].sectorStart * SECTOR_SIZE)) == 0, "mSeek");
		}

		if(blkx->runs[i].type == BLOCK_TERMINATOR) {
			break;
		}

		if( blkx->runs[i].compLength == 0) {
			continue;
		}

		printf("run %d: start=%" PRId64 " sectors=%" PRId64 ", length=%" PRId64 ", fileOffset=0x%" PRIx64 "\n", i, initialOffset + (blkx->runs[i].sectorStart * SECTOR_SIZE), blkx->runs[i].sectorCount, blkx->runs[i].compLength, blkx->runs[i].compOffset);



		switch(blkx->runs[i].type) {
			case BLOCK_RAW:
				if(blkx->runs[i].compLength > bufferSize) {
					uint64_t left = blkx->runs[i].compLength;
					void* pageBuffer = malloc(DEFAULT_BUFFER_SIZE);
					while(left > 0) {
						size_t thisRead;
						if(left > DEFAULT_BUFFER_SIZE) {
							thisRead = DEFAULT_BUFFER_SIZE;
						} else {
							thisRead = left;
						}
						ASSERT((have = in->read(in, pageBuffer, thisRead)) == thisRead, "fread");
						ASSERT(out->write(out, pageBuffer, have) == have, "mWrite");
						left -= have;
					}
					free(pageBuffer);
				} else {
					ASSERT((have = in->read(in, inBuffer, blkx->runs[i].compLength)) == blkx->runs[i].compLength, "fread");
					ASSERT(out->write(out, inBuffer, have) == have, "mWrite");
				}
				break;
			case BLOCK_IGNORE:
				break;
			case BLOCK_COMMENT:
				break;
			case BLOCK_TERMINATOR:
				break;
			default:
				type = blkx->runs[i].type;
				if (compressionBlockTypeSupported(type) != 0) {
					fprintf(stderr, "Unsupported block type %#08x\n", type);
					exit(1);
				}

				expectedSize = blkx->runs[i].sectorCount * SECTOR_SIZE;
				ASSERT(outBuffer = (unsigned char*)malloc(expectedSize), "malloc");
				ASSERT(in->read(in, inBuffer, blkx->runs[i].compLength) == blkx->runs[i].compLength, "fread");
				ASSERT(decompressRun(type, inBuffer, blkx->runs[i].compLength, outBuffer, expectedSize) == 0,
					"decompression failed");
				ASSERT(out->write(out, outBuffer, expectedSize) == expectedSize, "mWrite");
				free(outBuffer);
		}
	}

	free(inBuffer);
}
