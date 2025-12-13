#include <zlib.h>
#include "common.h"
#include <hfs/hfsplus.h>
#include <hfs/hfscompress.h>

void flipHFSPlusDecmpfs(HFSPlusDecmpfs* compressData) {
	FLIPENDIANLE(compressData->magic);
	FLIPENDIANLE(compressData->flags);
	FLIPENDIANLE(compressData->size);
}

void flipRsrcHead(HFSPlusCmpfRsrcHead* data) {
	FLIPENDIAN(data->headerSize);
	FLIPENDIAN(data->totalSize);
	FLIPENDIAN(data->dataSize);
	FLIPENDIAN(data->flags);
}

void flipRsrcBlockHead(HFSPlusCmpfRsrcBlockHead* data) {
	FLIPENDIAN(data->dataSize);
	FLIPENDIANLE(data->numBlocks);
}

void flipRsrcBlock(HFSPlusCmpfRsrcBlock* data) {
	FLIPENDIANLE(data->offset);
	FLIPENDIANLE(data->size);
}

void flipHFSPlusCmpfEnd(HFSPlusCmpfEnd* data) {
	FLIPENDIAN(data->unk1);
	FLIPENDIAN(data->unk2);
	FLIPENDIAN(data->unk3);
	FLIPENDIAN(data->magic);
	FLIPENDIAN(data->flags);
	FLIPENDIANLE(data->size);
	FLIPENDIANLE(data->unk4);
}

static int compressedRead(io_func* io, off_t location, size_t size, void *buffer) {
	HFSPlusCompressed* data = (HFSPlusCompressed*) io->data;
	size_t toRead;

	while(size > 0) {
		if(data->cached && location >= data->cachedStart && location < data->cachedEnd) {
			if((data->cachedEnd - location) < size)
				toRead = data->cachedEnd - location;
			else
				toRead = size;

			memcpy(buffer, data->cached + (location - data->cachedStart), toRead);

			size -= toRead;
			location += toRead;
			buffer = ((uint8_t*) buffer) + toRead;
		}

		if(size == 0)
			break;

		// Try to cache
		uLongf actualSize;
		uint32_t block = location / 0x10000;
		uint8_t* compressed = (uint8_t*) malloc(data->blocks->blocks[block].size);
		if(!READ(data->io, data->rsrcHead.headerSize + sizeof(uint32_t) + data->blocks->blocks[block].offset, data->blocks->blocks[block].size, compressed)) {
			hfs_panic("error reading");
		}

		if(data->cached)
			free(data->cached);

		data->cached = (uint8_t*) malloc(0x10000);
		actualSize = 0x10000;
		if(compressed[0] == 0xff) {
			actualSize = data->blocks->blocks[block].size - 1;
			memcpy(data->cached, compressed + 1, actualSize);
		} else {
			uncompress(data->cached, &actualSize, compressed, data->blocks->blocks[block].size);
		}
		data->cachedStart = block * 0x10000;
		data->cachedEnd = data->cachedStart + actualSize;
		free(compressed);
	}

	return TRUE;
}

static int compressedWrite(io_func* io, off_t location, size_t size, void *buffer) {
	HFSPlusCompressed* data = (HFSPlusCompressed*) io->data;

	if(data->cachedStart != 0 || data->cachedEnd != data->decmpfs->size) {
		// Cache entire file
		uint8_t* newCache = (uint8_t*) malloc(data->decmpfs->size);
		compressedRead(io, 0, data->decmpfs->size, newCache);
		if(data->cached)
			free(data->cached);

		data->cached = newCache;
		data->cachedStart = 0;
		data->cachedEnd = data->decmpfs->size;
	}

	if((location + size) > data->decmpfs->size) {
		data->decmpfs->size = location + size;
		data->cached = (uint8_t*) realloc(data->cached, data->decmpfs->size);
		data->cachedEnd = data->decmpfs->size;
	}

	memcpy(data->cached + location, buffer, size);

	data->dirty = TRUE;
	return TRUE;
}

static void closeHFSPlusCompressed(io_func* io) {
	HFSPlusCompressed* data = (HFSPlusCompressed*) io->data;

	if(data->io)
		CLOSE(data->io);

	if(data->dirty) {
		if(data->blocks)
			free(data->blocks);

		data->decmpfs->magic = CMPFS_MAGIC; 
		data->decmpfs->flags = 0x4;
		data->decmpfsSize = sizeof(HFSPlusDecmpfs);

		uint32_t numBlocks = (data->decmpfs->size + 0xFFFF) / 0x10000;
		uint32_t blocksSize = sizeof(HFSPlusCmpfRsrcBlockHead) + (numBlocks * sizeof(HFSPlusCmpfRsrcBlock));
		data->blocks = (HFSPlusCmpfRsrcBlockHead*) malloc(sizeof(HFSPlusCmpfRsrcBlockHead) + (numBlocks * sizeof(HFSPlusCmpfRsrcBlock)));
		data->blocks->numBlocks = numBlocks;
		data->blocks->dataSize = blocksSize - sizeof(uint32_t); // without the front dataSize in BlockHead.

		data->rsrcHead.headerSize = 0x100;
		data->rsrcHead.dataSize = blocksSize;
		data->rsrcHead.totalSize = data->rsrcHead.headerSize + data->rsrcHead.dataSize;
		data->rsrcHead.flags = 0x32;

		uint8_t* buffer = (uint8_t*) malloc((0x10000 * 1.1) + 12);
		uint32_t curFileOffset = data->blocks->dataSize;
		uint32_t i;
		for(i = 0; i < numBlocks; i++) {
			data->blocks->blocks[i].offset = curFileOffset;
			uLongf actualSize = (0x10000 * 1.1) + 12;
			compress(buffer, &actualSize, data->cached + (0x10000 * i),
					(data->decmpfs->size - (0x10000 * i)) > 0x10000 ? 0x10000 : (data->decmpfs->size - (0x10000 * i))); 
			data->blocks->blocks[i].size = actualSize;

			// check if we can fit the whole thing into an inline extended attribute
			// a little fudge factor here since sizeof(HFSPlusAttrKey) is bigger than it ought to be, since only 127 characters are strictly allowed
			if(numBlocks <= 1 && (actualSize + sizeof(HFSPlusDecmpfs) + sizeof(HFSPlusAttrKey)) <= 0x1000) {
				data->decmpfs->flags = 0x3;
				memcpy(data->decmpfs->data, buffer, actualSize);
				data->decmpfsSize = sizeof(HFSPlusDecmpfs) + actualSize;
				printf("inline data\n");
				break;
			} else {
				if(i == 0) {
					data->io = openRawFile(data->file->fileID, &data->file->resourceFork, (HFSPlusCatalogRecord*)data->file, data->volume);
					if(!data->io) {
						hfs_panic("error opening resource fork");
					}
				}

				WRITE(data->io, data->rsrcHead.headerSize + sizeof(uint32_t) + data->blocks->blocks[i].offset, data->blocks->blocks[i].size, buffer);

				curFileOffset += data->blocks->blocks[i].size;
				data->blocks->dataSize += data->blocks->blocks[i].size;
				data->rsrcHead.dataSize += data->blocks->blocks[i].size;
				data->rsrcHead.totalSize += data->blocks->blocks[i].size;
			}
		}

		free(buffer);
		
		if(data->decmpfs->flags == 0x4) {
			flipRsrcHead(&data->rsrcHead);
			WRITE(data->io, 0, sizeof(HFSPlusCmpfRsrcHead), &data->rsrcHead);
			flipRsrcHead(&data->rsrcHead);

			for(i = 0; i < data->blocks->numBlocks; i++) {
				flipRsrcBlock(&data->blocks->blocks[i]);
			}
			flipRsrcBlockHead(data->blocks);
			WRITE(data->io, data->rsrcHead.headerSize, blocksSize, data->blocks);
			flipRsrcBlockHead(data->blocks);
			for(i = 0; i < data->blocks->numBlocks; i++) {
				flipRsrcBlock(&data->blocks->blocks[i]);
			}

			HFSPlusCmpfEnd end;
			memset(&end, 0, sizeof(HFSPlusCmpfEnd));
			end.unk1 = 0x1C;
			end.unk2 = 0x32;
			end.unk3 = 0x0;
			end.magic = CMPFS_MAGIC;
			end.flags = 0xA;
			end.size = 0xFFFF01;
			end.unk4 = 0x0;

			flipHFSPlusCmpfEnd(&end);
			WRITE(data->io, data->rsrcHead.totalSize, sizeof(HFSPlusCmpfEnd), &end);
			flipHFSPlusCmpfEnd(&end);

			CLOSE(data->io);
		}

		flipHFSPlusDecmpfs(data->decmpfs);
		setAttribute(data->volume, data->file->fileID, "com.apple.decmpfs", (uint8_t*)(data->decmpfs), data->decmpfsSize);
		flipHFSPlusDecmpfs(data->decmpfs);
	}

	if(data->cached)
		free(data->cached);

	if(data->blocks)
		free(data->blocks);

	free(data->decmpfs);
	free(data);
	free(io);
}

io_func* openHFSPlusCompressed(Volume* volume, HFSPlusCatalogFile* file) {
	io_func* io;
	HFSPlusCompressed* data;
	uLongf actualSize;

	io = (io_func*) malloc(sizeof(io_func));
	data = (HFSPlusCompressed*) malloc(sizeof(HFSPlusCompressed));

	data->volume = volume;
	data->file = file;

	io->data = data;
	io->read = &compressedRead;
	io->write = &compressedWrite;
	io->close = &closeHFSPlusCompressed;

	data->cached = NULL;
	data->cachedStart = 0;
	data->cachedEnd = 0;
	data->io = NULL;
	data->blocks = NULL;
	data->dirty = FALSE;

	data->decmpfsSize = getAttribute(volume, file->fileID, "com.apple.decmpfs", (uint8_t**)(&data->decmpfs));
	if(data->decmpfsSize == 0) {
		data->decmpfs = (HFSPlusDecmpfs*) malloc(0x1000);
		data->decmpfs->size = 0;
		return io;		// previously not compressed file
	}

	flipHFSPlusDecmpfs(data->decmpfs);

	if(data->decmpfs->flags == 0x3) {
		data->cached = (uint8_t*) malloc(data->decmpfs->size);
		actualSize = data->decmpfs->size;
		if(data->decmpfs->data[0] == 0xff) {
			memcpy(data->cached, data->decmpfs->data + 1, actualSize);
		} else {
			uncompress(data->cached, &actualSize, data->decmpfs->data, data->decmpfsSize - sizeof(HFSPlusDecmpfs));
			if(actualSize != data->decmpfs->size) {
				fprintf(stderr, "decmpfs: size mismatch\n");
			}
		}
		data->cachedStart = 0;
		data->cachedEnd = actualSize;
	} else {
		data->io = openRawFile(file->fileID, &file->resourceFork, (HFSPlusCatalogRecord*)file, volume);
		if(!data->io) {
			hfs_panic("error opening resource fork");
		}

		if(!READ(data->io, 0, sizeof(HFSPlusCmpfRsrcHead), &data->rsrcHead)) {
			hfs_panic("error reading");
		}

		flipRsrcHead(&data->rsrcHead);

		data->blocks = (HFSPlusCmpfRsrcBlockHead*) malloc(sizeof(HFSPlusCmpfRsrcBlockHead));
		if(!READ(data->io, data->rsrcHead.headerSize, sizeof(HFSPlusCmpfRsrcBlockHead), data->blocks)) {
			hfs_panic("error reading");
		}

		flipRsrcBlockHead(data->blocks);

		data->blocks = (HFSPlusCmpfRsrcBlockHead*) realloc(data->blocks, sizeof(HFSPlusCmpfRsrcBlockHead) + (sizeof(HFSPlusCmpfRsrcBlock) * data->blocks->numBlocks));
		if(!READ(data->io, data->rsrcHead.headerSize + sizeof(HFSPlusCmpfRsrcBlockHead), sizeof(HFSPlusCmpfRsrcBlock) * data->blocks->numBlocks, data->blocks->blocks)) {
			hfs_panic("error reading");
		}

		int i;
		for(i = 0; i < data->blocks->numBlocks; i++) {
			flipRsrcBlock(&data->blocks->blocks[i]);
		}
	}

	return io;
}

