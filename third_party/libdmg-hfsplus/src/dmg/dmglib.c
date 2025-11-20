#include <string.h>
#include "common.h"
#include "abstractfile.h"
#include <dmg/attribution.h>
#include <dmg/dmg.h>
#include <dmg/dmgfile.h>

uint32_t calculateMasterChecksum(ResourceKey* resources);

int extractDmg(AbstractFile* abstractIn, AbstractFile* abstractOut, int partNum) 
{
    io_func* dmgfile = openDmgFile(abstractIn);
    
    if (!dmgfile) {
        fprintf(stderr, "cannot open dmg file\n");
        return FALSE;
    }
    
    DMG* dmg = (DMG*)dmgfile->data;
	ResourceKey* resources = dmg->resources;
	ResourceData* blkxData;
		
	printf("Writing out data..\n"); fflush(stdout);
	
	/* reasonable assumption that 2 is the main partition, given that that's usually the case in SPUD layouts */
	if(partNum < 0) {
		blkxData = getResourceByKey(resources, "blkx")->data;
		while(blkxData != NULL) {
			if(strstr(blkxData->name, "Apple_HFS") != NULL) {
				break;
			}
			blkxData = blkxData->next;
		}
	} else {
		blkxData = getDataByID(getResourceByKey(resources, "blkx"), partNum);
	}
	
	if(blkxData) {
		extractBLKX(abstractIn, abstractOut, (BLKXTable*)(blkxData->data));
	} else {
		printf("BLKX not found!\n"); fflush(stdout);
	}
	abstractOut->close(abstractOut);
	
    dmgfile->close(dmgfile); // will also close abstractIn
	
	return TRUE;
}

uint32_t calculateMasterChecksum(ResourceKey* resources) {
	ResourceKey* blkxKeys;
	ResourceData* data;
	BLKXTable* blkx;
	unsigned char* buffer;
	int blkxNum;
	uint32_t result;
	
	blkxKeys = getResourceByKey(resources, "blkx");
	
	data = blkxKeys->data;
	blkxNum = 0;
	while(data != NULL) {
		blkx = (BLKXTable*) data->data;
		if(blkx->checksum.type == CHECKSUM_UDIF_CRC32) {
			blkxNum++;
		}
		data = data->next;
	}
	
	buffer = (unsigned char*) malloc(4 * blkxNum) ;
	data = blkxKeys->data;
	blkxNum = 0;
	while(data != NULL) {
		blkx = (BLKXTable*) data->data;
		if(blkx->checksum.type == CHECKSUM_UDIF_CRC32) {
			buffer[(blkxNum * 4) + 0] = (blkx->checksum.data[0] >> 24) & 0xff;
			buffer[(blkxNum * 4) + 1] = (blkx->checksum.data[0] >> 16) & 0xff;
			buffer[(blkxNum * 4) + 2] = (blkx->checksum.data[0] >> 8) & 0xff;
			buffer[(blkxNum * 4) + 3] = (blkx->checksum.data[0] >> 0) & 0xff;
			blkxNum++;
		}
		data = data->next;
	}
	
	result = 0;
	CRC32Checksum(&result, (const unsigned char*) buffer, 4 * blkxNum);
	free(buffer);
	return result;  
}

int buildDmg(AbstractFile* abstractIn, AbstractFile* abstractOut, unsigned int BlockSize, const char* sentinel, Compressor *comp, size_t runSectors) {
	io_func* io;
	Volume* volume;  
	
	HFSPlusVolumeHeader* volumeHeader;
	DriverDescriptorRecord* DDM;
	Partition* partitions;
	
	ResourceKey* resources;
	ResourceKey* curResource;
	
	NSizResource* nsiz;
	NSizResource* myNSiz;
	CSumResource csum;
	
	BLKXTable* blkx;
	ChecksumToken uncompressedToken;
	
	ChecksumToken dataForkToken;
	
	UDIFResourceFile koly = {0};

	off_t plistOffset;
	uint32_t plistSize;
	uint32_t dataForkChecksum;
	
	io = IOFuncFromAbstractFile(abstractIn);
	ASSERT(volume = openVolume(io), "parse HFS volume");
	volumeHeader = volume->volumeHeader;
	

	if(volumeHeader->signature != HFSX_SIGNATURE) {
		printf("Warning: ASR data only reverse engineered for case-sensitive HFS+ volumes\n");fflush(stdout);
	}
    
	resources = NULL;
	nsiz = NULL;
    
	memset(&dataForkToken, 0, sizeof(ChecksumToken));
	memset(koly.fUDIFMasterChecksum.data, 0, sizeof(koly.fUDIFMasterChecksum.data));
	memset(koly.fUDIFDataForkChecksum.data, 0, sizeof(koly.fUDIFDataForkChecksum.data));
	
	printf("Creating and writing DDM and partition map...\n"); fflush(stdout);
	
	DDM = createDriverDescriptorMap((volumeHeader->totalBlocks * volumeHeader->blockSize)/SECTOR_SIZE, BlockSize);
	
	partitions = createApplePartitionMap((volumeHeader->totalBlocks * volumeHeader->blockSize)/SECTOR_SIZE, HFSX_VOLUME_TYPE, BlockSize);
	
	int pNum = writeDriverDescriptorMap(-1, abstractOut, DDM, BlockSize, &CRCProxy, (void*) (&dataForkToken), &resources, comp, runSectors);
	free(DDM);
	pNum = writeApplePartitionMap(pNum, abstractOut, partitions, BlockSize, &CRCProxy, (void*) (&dataForkToken), &resources, &nsiz, comp, runSectors);
	free(partitions);
	pNum = writeATAPI(pNum, abstractOut, BlockSize, &CRCProxy, (void*) (&dataForkToken), &resources, &nsiz, comp, runSectors);
	
	memset(&uncompressedToken, 0, sizeof(uncompressedToken));
	SHA1Init(&(uncompressedToken.sha1));
	
	printf("Writing main data blkx...\n"); fflush(stdout);
	
	abstractIn->seek(abstractIn, 0);

	AbstractAttribution* attribution = NULL;
	if (sentinel) {
		attribution = createAbstractAttributionPreservingSentinel(sentinel);
	}

	if (attribution) {
		attribution->beforeMainBlkx(attribution, abstractOut, &dataForkToken);
	}

	blkx = insertBLKX(abstractOut, abstractIn, USER_OFFSET, (volumeHeader->totalBlocks * volumeHeader->blockSize)/SECTOR_SIZE,
				pNum, CHECKSUM_UDIF_CRC32, &BlockSHA1CRC, &uncompressedToken, &CRCProxy, &dataForkToken, volume, attribution, comp, runSectors);

	AttributionResource attributionResource;
	memset(&attributionResource, 0, sizeof(AttributionResource));

	if (attribution) {
		attribution->afterMainBlkx(attribution, abstractOut, &dataForkToken, &attributionResource);
	}
	
	blkx->checksum.data[0] = uncompressedToken.crc;
	printf("Inserting main blkx...\n"); fflush(stdout);

	char pName[100];
	sprintf(pName, "Mac_OS_X (Apple_HFSX : %d)", pNum + 1);	
	resources = insertData(resources, "blkx", pNum, pName, 0, false, (const char*) blkx, sizeof(BLKXTable) + (blkx->blocksRunCount * sizeof(BLKXRun)), ATTRIBUTE_HDIUTIL);
	free(blkx);

	printf("Inserting cSum data...\n"); fflush(stdout);
	
	csum.version = 1;
	csum.type = CHECKSUM_MKBLOCK;
	csum.checksum = uncompressedToken.block;
	
	resources = insertData(resources, "cSum", 2, "", 0, false, (const char*) (&csum), sizeof(csum), 0);
	
	printf("Inserting nsiz data\n"); fflush(stdout);
	
	myNSiz = (NSizResource*) malloc(sizeof(NSizResource));
	memset(myNSiz, 0, sizeof(NSizResource));
	myNSiz->isVolume = TRUE;
	myNSiz->blockChecksum2 = uncompressedToken.block;
	myNSiz->partitionNumber = 2;
	myNSiz->version = 6;
	myNSiz->bytes = (volumeHeader->totalBlocks - volumeHeader->freeBlocks) * volumeHeader->blockSize;
	myNSiz->modifyDate = volumeHeader->modifyDate;
	myNSiz->volumeSignature = volumeHeader->signature;
	myNSiz->sha1Digest = (unsigned char *)malloc(20);
	SHA1Final(myNSiz->sha1Digest, &(uncompressedToken.sha1));
	myNSiz->next = NULL;
	if(nsiz == NULL) {
		nsiz = myNSiz;
	} else {
		NSizResource* curNsiz = nsiz;
		while(curNsiz->next != NULL)
		{
			curNsiz = curNsiz->next;
		}
		curNsiz->next = myNSiz;
	}
	
	pNum++;

	printf("Writing free partition...\n"); fflush(stdout);
	
	pNum = writeFreePartition(pNum, abstractOut, USER_OFFSET + (volumeHeader->totalBlocks * volumeHeader->blockSize)/SECTOR_SIZE, 
			(FREE_SIZE + (BlockSize / SECTOR_SIZE / 2)) / (BlockSize / SECTOR_SIZE) * (BlockSize / SECTOR_SIZE), &resources);
	
	dataForkChecksum = dataForkToken.crc;
	
	printf("Writing XML data...\n"); fflush(stdout);
	curResource = resources;
	while(curResource->next != NULL)
		curResource = curResource->next;

	curResource->next = writeNSiz(nsiz);
	curResource = curResource->next;
	releaseNSiz(nsiz);
	
	curResource->next = makePlst((const char*) (&attributionResource), sizeof(attributionResource), true);
	curResource = curResource->next;
	
	curResource->next = makeSize(volumeHeader);
	curResource = curResource->next;
	
	plistOffset = abstractOut->tell(abstractOut);
	writeResources(abstractOut, resources, true);
	plistSize = abstractOut->tell(abstractOut) - plistOffset;
	
	printf("Generating UDIF metadata...\n"); fflush(stdout);
	
	koly.fUDIFSignature = KOLY_SIGNATURE;
	koly.fUDIFVersion = 4;
	koly.fUDIFHeaderSize = sizeof(koly);
	koly.fUDIFFlags = kUDIFFlagsFlattened;
	koly.fUDIFRunningDataForkOffset = 0;
	koly.fUDIFDataForkOffset = 0;
	koly.fUDIFDataForkLength = plistOffset;
	koly.fUDIFRsrcForkOffset = 0;
	koly.fUDIFRsrcForkLength = 0;
	
	koly.fUDIFSegmentNumber = 1;
	koly.fUDIFSegmentCount = 1;
	koly.fUDIFSegmentID.data1 = rand();
	koly.fUDIFSegmentID.data2 = rand();
	koly.fUDIFSegmentID.data3 = rand();
	koly.fUDIFSegmentID.data4 = rand();
	koly.fUDIFDataForkChecksum.type = CHECKSUM_UDIF_CRC32;
	koly.fUDIFDataForkChecksum.bitness = checksumBitness(CHECKSUM_UDIF_CRC32);
	koly.fUDIFDataForkChecksum.data[0] = dataForkChecksum;
	koly.fUDIFXMLOffset = plistOffset;
	koly.fUDIFXMLLength = plistSize;
	memset(&(koly.reserved1), 0, 0x78);
	
	koly.fUDIFMasterChecksum.type = CHECKSUM_UDIF_CRC32;
	koly.fUDIFMasterChecksum.bitness = checksumBitness(CHECKSUM_UDIF_CRC32);
	koly.fUDIFMasterChecksum.data[0] = calculateMasterChecksum(resources);
	printf("Master checksum: %x\n", koly.fUDIFMasterChecksum.data[0]); fflush(stdout); 
	
	koly.fUDIFImageVariant = kUDIFDeviceImageType;
	koly.fUDIFSectorCount = (volumeHeader->totalBlocks * volumeHeader->blockSize)/SECTOR_SIZE
		+ ((EXTRA_SIZE + (BlockSize / SECTOR_SIZE / 2)) / (BlockSize / SECTOR_SIZE) * (BlockSize / SECTOR_SIZE));
	koly.reserved2 = 0;
	koly.reserved3 = 0;
	koly.reserved4 = 0;
	
	printf("Writing out UDIF resource file...\n"); fflush(stdout); 
	
	writeUDIFResourceFile(abstractOut, &koly);
	
	printf("Cleaning up...\n"); fflush(stdout);
	
	releaseResources(resources);
	
	abstractOut->close(abstractOut);
	closeVolume(volume);
	CLOSE(io);
	
	printf("Done.\n"); fflush(stdout);
	
	return TRUE;
}

int convertToDMG(AbstractFile* abstractIn, AbstractFile* abstractOut, Compressor *comp, size_t runSectors) {
	Partition* partitions;
	DriverDescriptorRecord* DDM;
	int i;
	
	BLKXTable* blkx;
	ResourceKey* resources;
	ResourceKey* curResource;
	
	ChecksumToken dataForkToken;
	ChecksumToken uncompressedToken;
	
	NSizResource* nsiz;
	NSizResource* myNSiz;
	CSumResource csum;
	
	off_t plistOffset;
	uint32_t plistSize;
	uint32_t dataForkChecksum;
	uint64_t numSectors;
	
	UDIFResourceFile koly = {0};

	char partitionName[512];
	
	off_t fileLength;
	size_t partitionTableSize;
	
	unsigned int BlockSize;
	
	numSectors = 0;
	
	resources = NULL;
	nsiz = NULL;
	myNSiz = NULL;
	memset(&dataForkToken, 0, sizeof(ChecksumToken));
	memset(koly.fUDIFMasterChecksum.data, 0, sizeof(koly.fUDIFMasterChecksum.data));
	memset(koly.fUDIFDataForkChecksum.data, 0, sizeof(koly.fUDIFDataForkChecksum.data));
	
	partitions = (Partition*) malloc(SECTOR_SIZE);
	
	printf("Processing DDM...\n"); fflush(stdout);
	DDM = (DriverDescriptorRecord*) malloc(SECTOR_SIZE);
	abstractIn->seek(abstractIn, 0);
	ASSERT(abstractIn->read(abstractIn, DDM, SECTOR_SIZE) == SECTOR_SIZE, "fread");
	flipDriverDescriptorRecord(DDM, FALSE);
	
	if(DDM->sbSig == DRIVER_DESCRIPTOR_SIGNATURE) {
		BlockSize = DDM->sbBlkSize;
		int pNum = writeDriverDescriptorMap(-1, abstractOut, DDM, BlockSize, &CRCProxy, (void*) (&dataForkToken), &resources, comp, runSectors);
		free(DDM);
		
		printf("Processing partition map...\n"); fflush(stdout);
		
		abstractIn->seek(abstractIn, BlockSize);
		ASSERT(abstractIn->read(abstractIn, partitions, BlockSize) == BlockSize, "fread");
		flipPartitionMultiple(partitions, FALSE, FALSE, BlockSize);
		
		partitionTableSize = BlockSize * partitions->pmMapBlkCnt;
		partitions = (Partition*) realloc(partitions, partitionTableSize);
		
		abstractIn->seek(abstractIn, BlockSize);
		ASSERT(abstractIn->read(abstractIn, partitions, partitionTableSize) == partitionTableSize, "fread");
		flipPartition(partitions, FALSE, BlockSize);
		
		printf("Writing blkx (%d)...\n", partitions->pmMapBlkCnt); fflush(stdout);
		
		for(i = 0; i < partitions->pmMapBlkCnt; i++) {
			if(partitions[i].pmSig != APPLE_PARTITION_MAP_SIGNATURE) {
				break;
			}
			
			printf("Processing blkx %d, total %d...\n", i, partitions->pmMapBlkCnt); fflush(stdout);
			
			sprintf(partitionName, "%s (%s : %d)", partitions[i].pmPartName, partitions[i].pmParType, i + 1);
			
			memset(&uncompressedToken, 0, sizeof(uncompressedToken));
			
			abstractIn->seek(abstractIn, partitions[i].pmPyPartStart * BlockSize);
			blkx = insertBLKX(abstractOut, abstractIn, partitions[i].pmPyPartStart, partitions[i].pmPartBlkCnt, i, CHECKSUM_UDIF_CRC32,
						&BlockCRC, &uncompressedToken, &CRCProxy, &dataForkToken, NULL, NULL, comp, runSectors);
			
			blkx->checksum.data[0] = uncompressedToken.crc;	
			resources = insertData(resources, "blkx", i, partitionName, 0, false, (const char*) blkx, sizeof(BLKXTable) + (blkx->blocksRunCount * sizeof(BLKXRun)), ATTRIBUTE_HDIUTIL);
			free(blkx);
			
			memset(&csum, 0, sizeof(CSumResource));
			csum.version = 1;
			csum.type = CHECKSUM_MKBLOCK;
			csum.checksum = uncompressedToken.block;
			resources = insertData(resources, "cSum", i, "", 0, false, (const char*) (&csum), sizeof(csum), 0);
			
			if(nsiz == NULL) {
				nsiz = myNSiz = (NSizResource*) malloc(sizeof(NSizResource));
			} else {
				myNSiz->next = (NSizResource*) malloc(sizeof(NSizResource));
				myNSiz = myNSiz->next;
			}
			
			memset(myNSiz, 0, sizeof(NSizResource));
			myNSiz->isVolume = FALSE;
			myNSiz->blockChecksum2 = uncompressedToken.block;
			myNSiz->partitionNumber = i;
			myNSiz->version = 6;
			myNSiz->next = NULL;
			
			if((partitions[i].pmPyPartStart + partitions[i].pmPartBlkCnt) > numSectors) {
				numSectors = partitions[i].pmPyPartStart + partitions[i].pmPartBlkCnt;
			}
		}
		
		koly.fUDIFImageVariant = kUDIFDeviceImageType;
	} else {
		printf("No DDM! Just doing one huge blkx then...\n"); fflush(stdout);
		
		fileLength = abstractIn->getLength(abstractIn);
		
		memset(&uncompressedToken, 0, sizeof(uncompressedToken));
		
		abstractIn->seek(abstractIn, 0);
		blkx = insertBLKX(abstractOut, abstractIn, 0, fileLength/SECTOR_SIZE, ENTIRE_DEVICE_DESCRIPTOR, CHECKSUM_UDIF_CRC32,
					&BlockCRC, &uncompressedToken, &CRCProxy, &dataForkToken, NULL, NULL, comp, runSectors);
		blkx->checksum.data[0] = uncompressedToken.crc;
		resources = insertData(resources, "blkx", 0, "whole disk (unknown partition : 0)", 0, false, (const char*) blkx, sizeof(BLKXTable) + (blkx->blocksRunCount * sizeof(BLKXRun)), ATTRIBUTE_HDIUTIL);
		free(blkx);
		
		memset(&csum, 0, sizeof(CSumResource));
		csum.version = 1;
		csum.type = CHECKSUM_MKBLOCK;
		csum.checksum = uncompressedToken.block;
		resources = insertData(resources, "cSum", 0, "", 0, false, (const char*) (&csum), sizeof(csum), 0);
		
		if(nsiz == NULL) {
			nsiz = myNSiz = (NSizResource*) malloc(sizeof(NSizResource));
		} else {
			myNSiz->next = (NSizResource*) malloc(sizeof(NSizResource));
			myNSiz = myNSiz->next;
		}
		
		memset(myNSiz, 0, sizeof(NSizResource));
		myNSiz->isVolume = FALSE;
		myNSiz->blockChecksum2 = uncompressedToken.block;
		myNSiz->partitionNumber = 0;
		myNSiz->version = 6;
		myNSiz->next = NULL;
		
		koly.fUDIFImageVariant = kUDIFPartitionImageType;
	}
	
	dataForkChecksum = dataForkToken.crc;
	
	printf("Writing XML data...\n"); fflush(stdout);
	curResource = resources;
	while(curResource->next != NULL)
		curResource = curResource->next;
    
	curResource->next = writeNSiz(nsiz);
	curResource = curResource->next;
	releaseNSiz(nsiz);
	
	curResource->next = makePlst("", 0, false);
	curResource = curResource->next;
	
	plistOffset = abstractOut->tell(abstractOut);
	// Note: Passing false here means that attribution data is not preserved through
	// a `dmg convert` operation.
	writeResources(abstractOut, resources, false);
	plistSize = abstractOut->tell(abstractOut) - plistOffset;
	
	printf("Generating UDIF metadata...\n"); fflush(stdout);
	
	koly.fUDIFSignature = KOLY_SIGNATURE;
	koly.fUDIFVersion = 4;
	koly.fUDIFHeaderSize = sizeof(koly);
	koly.fUDIFFlags = kUDIFFlagsFlattened;
	koly.fUDIFRunningDataForkOffset = 0;
	koly.fUDIFDataForkOffset = 0;
	koly.fUDIFDataForkLength = plistOffset;
	koly.fUDIFRsrcForkOffset = 0;
	koly.fUDIFRsrcForkLength = 0;
	
	koly.fUDIFSegmentNumber = 1;
	koly.fUDIFSegmentCount = 1;
	koly.fUDIFSegmentID.data1 = rand();
	koly.fUDIFSegmentID.data2 = rand();
	koly.fUDIFSegmentID.data3 = rand();
	koly.fUDIFSegmentID.data4 = rand();
	koly.fUDIFDataForkChecksum.type = CHECKSUM_UDIF_CRC32;
	koly.fUDIFDataForkChecksum.bitness = checksumBitness(CHECKSUM_UDIF_CRC32);
	koly.fUDIFDataForkChecksum.data[0] = dataForkChecksum;
	koly.fUDIFXMLOffset = plistOffset;
	koly.fUDIFXMLLength = plistSize;
	memset(&(koly.reserved1), 0, 0x78);
	
	koly.fUDIFMasterChecksum.type = CHECKSUM_UDIF_CRC32;
	koly.fUDIFMasterChecksum.bitness = checksumBitness(CHECKSUM_UDIF_CRC32);
	koly.fUDIFMasterChecksum.data[0] = calculateMasterChecksum(resources);
	printf("Master checksum: %x\n", koly.fUDIFMasterChecksum.data[0]); fflush(stdout); 
	
	koly.fUDIFSectorCount = numSectors;
	koly.reserved2 = 0;
	koly.reserved3 = 0;
	koly.reserved4 = 0;
	
	printf("Writing out UDIF resource file...\n"); fflush(stdout); 
	
	writeUDIFResourceFile(abstractOut, &koly);
	
	printf("Cleaning up...\n"); fflush(stdout);
	
	releaseResources(resources);
	
	abstractIn->close(abstractIn);
	free(partitions);
	
	printf("Done\n"); fflush(stdout);

	abstractOut->close(abstractOut);
	
	return TRUE;
}

int convertToISO(AbstractFile* abstractIn, AbstractFile* abstractOut) 
{
    io_func* dmgfile = openDmgFile(abstractIn);
    
    if (!dmgfile) {
        fprintf(stderr, "cannot open dmg file\n");
        return FALSE;
    }
    
    DMG* dmg = (DMG*)dmgfile->data;
	ResourceKey* resources = dmg->resources;
	ResourceData* blkx;
	BLKXTable* blkxTable;

	blkx = (getResourceByKey(resources, "blkx"))->data;
	
	printf("Writing out data..\n"); fflush(stdout);
	
	while(blkx != NULL) {
		blkxTable = (BLKXTable*)(blkx->data);
		abstractOut->seek(abstractOut, blkxTable->firstSectorNumber * 512);
		extractBLKX(abstractIn, abstractOut, blkxTable);
		blkx = blkx->next;
	}
	
	abstractOut->close(abstractOut);
	dmgfile->close(dmgfile); // will also close abstractIn
	
	return TRUE;
	
}
