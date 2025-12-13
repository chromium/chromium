/*
 *  dmgfile.h
 *  libdmg-hfsplus
 *
 */

#include <dmg/dmg.h>

io_func* openDmgFile(AbstractFile* dmg);
io_func* seekDmgPartition(io_func*, int partition);

// combines openDmgFile+seekDmgPartition
io_func* openDmgFilePartition(AbstractFile* dmg, int partition);

typedef struct DMG {
	UDIFResourceFile resourceFile;
	char* resourceXML;
	AbstractFile* dmg;
	ResourceKey* resources;
	uint32_t numBLKX;
	BLKXTable** blkx;
	void* runData;
	uint32_t runType;
	uint64_t runStart;
	uint64_t runEnd;
	uint64_t offset;
} DMG;
