#include <stdlib.h>
#include <string.h>
#include <hfs/hfsplus.h>

int writeExtents(RawFile* rawFile);

int isBlockUsed(Volume* volume, uint32_t block)
{
	unsigned char byte;

	READ(volume->allocationFile, block / 8, 1, &byte);
	return (byte & (1 << (7 - (block % 8)))) != 0;
}

int setBlockUsed(Volume* volume, uint32_t block, int used) {
	unsigned char byte;

	READ(volume->allocationFile, block / 8, 1, &byte);
	if(used) {
		byte |= (1 << (7 - (block % 8)));
	} else {
		byte &= ~(1 << (7 - (block % 8)));
	}
	ASSERT(WRITE(volume->allocationFile, block / 8, 1, &byte), "WRITE");

	return TRUE;
}

int allocate(RawFile* rawFile, off_t size) {
	unsigned char* zeros;
	Volume* volume;
	HFSPlusForkData* forkData;
	uint32_t blocksNeeded;
	uint32_t blocksToAllocate;
	Extent* extent;
	Extent* lastExtent;

	uint32_t curBlock;

	volume = rawFile->volume;
	forkData = rawFile->forkData;
	extent = rawFile->extents;

	blocksNeeded = ((uint64_t)size / (uint64_t)volume->volumeHeader->blockSize) + (((size % volume->volumeHeader->blockSize) == 0) ? 0 : 1);

	if(blocksNeeded > forkData->totalBlocks) {
		zeros = (unsigned char*) malloc(volume->volumeHeader->blockSize);
		memset(zeros, 0, volume->volumeHeader->blockSize);

		blocksToAllocate = blocksNeeded - forkData->totalBlocks;

		if(blocksToAllocate > volume->volumeHeader->freeBlocks) {
			return FALSE;
		}

		lastExtent = NULL;
		while(extent != NULL) {
			lastExtent = extent;
			extent = extent->next;
		}

		if(lastExtent == NULL) {
			rawFile->extents = (Extent*) malloc(sizeof(Extent));
			lastExtent = rawFile->extents;
			lastExtent->blockCount = 0;
			lastExtent->next = NULL;
			curBlock = volume->volumeHeader->nextAllocation;
		} else {
			curBlock = lastExtent->startBlock + lastExtent->blockCount;
		}

		while(blocksToAllocate > 0) {
			if(isBlockUsed(volume, curBlock)) {
				if(lastExtent->blockCount > 0) {
					lastExtent->next = (Extent*) malloc(sizeof(Extent));
					lastExtent = lastExtent->next;
					lastExtent->blockCount = 0;
					lastExtent->next = NULL;
				}
				curBlock = volume->volumeHeader->nextAllocation;
				volume->volumeHeader->nextAllocation++;
				if(volume->volumeHeader->nextAllocation >= volume->volumeHeader->totalBlocks) {
					volume->volumeHeader->nextAllocation = 0;
				}
			} else {
				if(lastExtent->blockCount == 0) {
					lastExtent->startBlock = curBlock;
				}

				/* zero out allocated block */
				ASSERT(WRITE(volume->image, curBlock * volume->volumeHeader->blockSize, volume->volumeHeader->blockSize, zeros), "WRITE");

				setBlockUsed(volume, curBlock, TRUE);
				volume->volumeHeader->freeBlocks--;
				blocksToAllocate--;
				curBlock++;
				lastExtent->blockCount++;

				if(curBlock >= volume->volumeHeader->totalBlocks) {
					curBlock = volume->volumeHeader->nextAllocation;
				}
			}
		}

		free(zeros);
	} else if(blocksNeeded < forkData->totalBlocks) {
		blocksToAllocate = blocksNeeded;

		lastExtent = NULL;

		while(blocksToAllocate > 0) {
			if(blocksToAllocate > extent->blockCount) {
				blocksToAllocate -= extent->blockCount;
				lastExtent = extent;
				extent = extent->next;
			} else {
				break;
			}
		}


		if(blocksToAllocate == 0 && lastExtent != NULL) {
			// snip the extent list here, since we don't need the rest
			lastExtent->next = NULL;
		} else if(blocksNeeded == 0) {
			rawFile->extents = NULL;
		}

		do {
			for(curBlock = (extent->startBlock + blocksToAllocate); curBlock < (extent->startBlock + extent->blockCount); curBlock++) {
				setBlockUsed(volume, curBlock, FALSE);
				volume->volumeHeader->freeBlocks++;
			}
			lastExtent = extent;
			extent = extent->next;

			if(blocksToAllocate == 0)
			{ 
				free(lastExtent);
			} else {
				lastExtent->next = NULL;
				lastExtent->blockCount = blocksToAllocate;
			}

			blocksToAllocate = 0;
		} while(extent != NULL);
	}

	writeExtents(rawFile);

	forkData->logicalSize = size;
	forkData->totalBlocks = blocksNeeded;

	updateVolume(rawFile->volume);

	if(rawFile->catalogRecord != NULL) {
		updateCatalog(rawFile->volume, rawFile->catalogRecord);
	}

	return TRUE;
}

static int rawFileRead(io_func* io,off_t location, size_t size, void *buffer) {
	RawFile* rawFile;
	Volume* volume;
	Extent* extent;

	size_t blockSize;
	off_t fileLoc;
	off_t locationInBlock;
	size_t possible;

	rawFile = (RawFile*) io->data;
	volume = rawFile->volume;
	blockSize = volume->volumeHeader->blockSize;

	if(!rawFile->extents)
		return FALSE;

	extent = rawFile->extents;
	fileLoc = 0;

	locationInBlock = location;
	while(TRUE) {
		fileLoc += extent->blockCount * blockSize;
		if(fileLoc <= location) {
			locationInBlock -= extent->blockCount * blockSize;
			extent = extent->next;
			if(extent == NULL)
				break;
		} else {
			break;
		}
	}

	while(size > 0) {
		if(extent == NULL)
			return FALSE;

		possible = extent->blockCount * blockSize - locationInBlock;

		if(size > possible) {
			ASSERT(READ(volume->image, extent->startBlock * blockSize + locationInBlock, possible, buffer), "READ");
			size -= possible;
			buffer = (void*)(((size_t)buffer) + possible);
			extent = extent->next;
		} else {
			ASSERT(READ(volume->image, extent->startBlock * blockSize + locationInBlock, size, buffer), "READ");
			break;
		}

		locationInBlock = 0;
	}

	return TRUE;
}

static int rawFileWrite(io_func* io,off_t location, size_t size, void *buffer) {
	RawFile* rawFile;
	Volume* volume;
	Extent* extent;

	size_t blockSize;
	off_t fileLoc;
	off_t locationInBlock;
	size_t possible;

	rawFile = (RawFile*) io->data;
	volume = rawFile->volume;
	blockSize = volume->volumeHeader->blockSize;

	if(rawFile->forkData->logicalSize < (location + size)) {
		ASSERT(allocate(rawFile, location + size), "allocate");
	}

	extent = rawFile->extents;
	fileLoc = 0;

	locationInBlock = location;
	while(TRUE) {
		fileLoc += extent->blockCount * blockSize;
		if(fileLoc <= location) {
			locationInBlock -= extent->blockCount * blockSize;
			extent = extent->next;
			if(extent == NULL)
				break;
		} else {
			break;
		}
	}

	while(size > 0) {
		if(extent == NULL)
			return FALSE;

		possible = extent->blockCount * blockSize - locationInBlock;

		if(size > possible) {
			ASSERT(WRITE(volume->image, extent->startBlock * blockSize + locationInBlock, possible, buffer), "WRITE");
			size -= possible;
			buffer = (void*)(((size_t)buffer) + possible);
			extent = extent->next;
		} else {
			ASSERT(WRITE(volume->image, extent->startBlock * blockSize + locationInBlock, size, buffer), "WRITE");
			break;
		}

		locationInBlock = 0;
	}

	return TRUE;
}

static void closeRawFile(io_func* io) {
	RawFile* rawFile;
	Extent* extent;
	Extent* toRemove;

	rawFile = (RawFile*) io->data;
	extent = rawFile->extents;

	while(extent != NULL) {
		toRemove = extent;
		extent = extent->next;
		free(toRemove);
	}

	free(rawFile);
	free(io);
}

int removeExtents(RawFile* rawFile) {
	uint32_t blocksLeft;
	HFSPlusForkData* forkData;
	uint32_t currentBlock;

	uint32_t startBlock;
	uint32_t blockCount;

	HFSPlusExtentDescriptor* descriptor;
	int currentExtent;
	HFSPlusExtentKey extentKey;
	int exact;

	extentKey.keyLength = sizeof(HFSPlusExtentKey) - sizeof(extentKey.keyLength);
	extentKey.forkType = 0;
	extentKey.fileID = rawFile->id;

	forkData = rawFile->forkData;
	blocksLeft = forkData->totalBlocks;
	currentExtent = 0;
	currentBlock = 0;
	descriptor = (HFSPlusExtentDescriptor*) forkData->extents;

	while(blocksLeft > 0) {       
		if(currentExtent == 8) {
			if(rawFile->volume->extentsTree == NULL) {
				hfs_panic("no extents overflow file loaded yet!");
				return FALSE;
			}

			if(descriptor != ((HFSPlusExtentDescriptor*) forkData->extents)) {
				free(descriptor);
			}

			extentKey.startBlock = currentBlock;
			descriptor = (HFSPlusExtentDescriptor*) search(rawFile->volume->extentsTree, (BTKey*)(&extentKey), &exact, NULL, NULL);
			if(descriptor == NULL || exact == FALSE) {
				hfs_panic("inconsistent extents information!");
				return FALSE;
			} else {
				removeFromBTree(rawFile->volume->extentsTree, (BTKey*)(&extentKey));
				currentExtent = 0;
				continue;
			}
		}

		startBlock = descriptor[currentExtent].startBlock;
		blockCount = descriptor[currentExtent].blockCount;

		currentBlock += blockCount;
		blocksLeft -= blockCount;
		currentExtent++;
	}

	if(descriptor != ((HFSPlusExtentDescriptor*) forkData->extents)) {
		free(descriptor);
	}

	return TRUE;
}

int writeExtents(RawFile* rawFile) {
	Extent* extent;
	int currentExtent;
	HFSPlusExtentKey extentKey;
	HFSPlusExtentDescriptor descriptor[8];
	HFSPlusForkData* forkData;

	removeExtents(rawFile);

	forkData = rawFile->forkData;
	currentExtent = 0;
	extent = rawFile->extents;

	memset(forkData->extents, 0, sizeof(HFSPlusExtentRecord));
	while(extent != NULL && currentExtent < 8) {
		((HFSPlusExtentDescriptor*)forkData->extents)[currentExtent].startBlock = extent->startBlock;
		((HFSPlusExtentDescriptor*)forkData->extents)[currentExtent].blockCount = extent->blockCount;
		extent = extent->next;
		currentExtent++;
	}

	if(extent != NULL) {
		extentKey.keyLength = sizeof(HFSPlusExtentKey) - sizeof(extentKey.keyLength);
		extentKey.forkType = 0;
		extentKey.fileID = rawFile->id;

		currentExtent = 0;

		while(extent != NULL) {
			if(currentExtent == 0) {
				memset(descriptor, 0, sizeof(HFSPlusExtentRecord));
			}

			if(currentExtent == 8) {
				extentKey.startBlock = descriptor[0].startBlock;
				addToBTree(rawFile->volume->extentsTree, (BTKey*)(&extentKey), sizeof(HFSPlusExtentRecord), (unsigned char *)(&(descriptor[0])));
				currentExtent = 0;
			}

			descriptor[currentExtent].startBlock = extent->startBlock;
			descriptor[currentExtent].blockCount = extent->blockCount;

			currentExtent++;
			extent = extent->next;
		}

		extentKey.startBlock = descriptor[0].startBlock;
		addToBTree(rawFile->volume->extentsTree, (BTKey*)(&extentKey), sizeof(HFSPlusExtentRecord), (unsigned char *)(&(descriptor[0])));
	}

	return TRUE;
}

int readExtents(RawFile* rawFile) {
	uint32_t blocksLeft;
	HFSPlusForkData* forkData;
	uint32_t currentBlock;

	Extent* extent;
	Extent* lastExtent;

	HFSPlusExtentDescriptor* descriptor;
	int currentExtent;
	HFSPlusExtentKey extentKey;
	int exact;

	extentKey.keyLength = sizeof(HFSPlusExtentKey) - sizeof(extentKey.keyLength);
	extentKey.forkType = 0;
	extentKey.fileID = rawFile->id;

	forkData = rawFile->forkData;
	blocksLeft = forkData->totalBlocks;
	currentExtent = 0;
	currentBlock = 0;
	descriptor = (HFSPlusExtentDescriptor*) forkData->extents;

	lastExtent = NULL;

	while(blocksLeft > 0) {
		extent = (Extent*) malloc(sizeof(Extent));

		if(currentExtent == 8) {
			if(rawFile->volume->extentsTree == NULL) {
				hfs_panic("no extents overflow file loaded yet!");
				return FALSE;
			}

			if(descriptor != ((HFSPlusExtentDescriptor*) forkData->extents)) {
				free(descriptor);
			}

			extentKey.startBlock = currentBlock;
			descriptor = (HFSPlusExtentDescriptor*) search(rawFile->volume->extentsTree, (BTKey*)(&extentKey), &exact, NULL, NULL);
			if(descriptor == NULL || exact == FALSE) {
				hfs_panic("inconsistent extents information!");
				return FALSE;
			} else {
				currentExtent = 0;
				continue;
			}
		}

		extent->startBlock = descriptor[currentExtent].startBlock;
		extent->blockCount = descriptor[currentExtent].blockCount;
		extent->next = NULL;

		currentBlock += extent->blockCount;
		blocksLeft -= extent->blockCount;
		currentExtent++;

		if(lastExtent == NULL) {
			rawFile->extents = extent;
		} else {
			lastExtent->next = extent;
		}

		lastExtent = extent;
	}

	if(descriptor != ((HFSPlusExtentDescriptor*) forkData->extents)) {
		free(descriptor);
	}

	return TRUE;
}

io_func* openRawFile(HFSCatalogNodeID id, HFSPlusForkData* forkData, HFSPlusCatalogRecord* catalogRecord, Volume* volume) {
	io_func* io;
	RawFile* rawFile;

	io = (io_func*) malloc(sizeof(io_func));
	rawFile = (RawFile*) malloc(sizeof(RawFile));

	rawFile->id = id;
	rawFile->volume = volume;
	rawFile->forkData = forkData;
	rawFile->catalogRecord = catalogRecord;
	rawFile->extents = NULL;

	io->data = rawFile;
	io->read = &rawFileRead;
	io->write = &rawFileWrite;
	io->close = &closeRawFile;

	if(!readExtents(rawFile)) {
		return NULL;
	}

	return io;
}
