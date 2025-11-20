#include <stdlib.h>
#include <string.h>
#include <hfs/hfsplus.h>

void flipForkData(HFSPlusForkData* forkData) {
  FLIPENDIAN(forkData->logicalSize);
  FLIPENDIAN(forkData->clumpSize);
  FLIPENDIAN(forkData->totalBlocks);
  
  flipExtentRecord(&forkData->extents);
}

static HFSPlusVolumeHeader* readVolumeHeader(io_func* io, off_t offset) {
  HFSPlusVolumeHeader* volumeHeader;
  
  volumeHeader = (HFSPlusVolumeHeader*) malloc(sizeof(HFSPlusVolumeHeader));
  
  if(!(READ(io, offset, sizeof(HFSPlusVolumeHeader), volumeHeader)))
    return NULL;
    
  FLIPENDIAN(volumeHeader->signature);
  FLIPENDIAN(volumeHeader->version);
  FLIPENDIAN(volumeHeader->attributes);
  FLIPENDIAN(volumeHeader->lastMountedVersion);
  FLIPENDIAN(volumeHeader->journalInfoBlock);
  FLIPENDIAN(volumeHeader->createDate);
  FLIPENDIAN(volumeHeader->modifyDate);
  FLIPENDIAN(volumeHeader->backupDate);
  FLIPENDIAN(volumeHeader->checkedDate);
  FLIPENDIAN(volumeHeader->fileCount);
  FLIPENDIAN(volumeHeader->folderCount);
  FLIPENDIAN(volumeHeader->blockSize);
  FLIPENDIAN(volumeHeader->totalBlocks);
  FLIPENDIAN(volumeHeader->freeBlocks);
  FLIPENDIAN(volumeHeader->nextAllocation);
  FLIPENDIAN(volumeHeader->rsrcClumpSize);
  FLIPENDIAN(volumeHeader->dataClumpSize);
  FLIPENDIAN(volumeHeader->nextCatalogID);
  FLIPENDIAN(volumeHeader->writeCount);
  FLIPENDIAN(volumeHeader->encodingsBitmap);
  
  
  flipForkData(&volumeHeader->allocationFile);
  flipForkData(&volumeHeader->extentsFile);
  flipForkData(&volumeHeader->catalogFile);
  flipForkData(&volumeHeader->attributesFile);
  flipForkData(&volumeHeader->startupFile);
 
  return volumeHeader;
}

static int writeVolumeHeader(io_func* io, HFSPlusVolumeHeader* volumeHeaderToWrite, off_t offset) {
  HFSPlusVolumeHeader* volumeHeader;
  
  volumeHeader = (HFSPlusVolumeHeader*) malloc(sizeof(HFSPlusVolumeHeader));
  memcpy(volumeHeader, volumeHeaderToWrite, sizeof(HFSPlusVolumeHeader));
    
  FLIPENDIAN(volumeHeader->signature);
  FLIPENDIAN(volumeHeader->version);
  FLIPENDIAN(volumeHeader->attributes);
  FLIPENDIAN(volumeHeader->lastMountedVersion);
  FLIPENDIAN(volumeHeader->journalInfoBlock);
  FLIPENDIAN(volumeHeader->createDate);
  FLIPENDIAN(volumeHeader->modifyDate);
  FLIPENDIAN(volumeHeader->backupDate);
  FLIPENDIAN(volumeHeader->checkedDate);
  FLIPENDIAN(volumeHeader->fileCount);
  FLIPENDIAN(volumeHeader->folderCount);
  FLIPENDIAN(volumeHeader->blockSize);
  FLIPENDIAN(volumeHeader->totalBlocks);
  FLIPENDIAN(volumeHeader->freeBlocks);
  FLIPENDIAN(volumeHeader->nextAllocation);
  FLIPENDIAN(volumeHeader->rsrcClumpSize);
  FLIPENDIAN(volumeHeader->dataClumpSize);
  FLIPENDIAN(volumeHeader->nextCatalogID);
  FLIPENDIAN(volumeHeader->writeCount);
  FLIPENDIAN(volumeHeader->encodingsBitmap);
  
  
  flipForkData(&volumeHeader->allocationFile);
  flipForkData(&volumeHeader->extentsFile);
  flipForkData(&volumeHeader->catalogFile);
  flipForkData(&volumeHeader->attributesFile);
  flipForkData(&volumeHeader->startupFile);
  
  if(!(WRITE(io, offset, sizeof(HFSPlusVolumeHeader), volumeHeader)))
    return FALSE;
  
  free(volumeHeader);
 
  return TRUE;
}

int updateVolume(Volume* volume) {
  ASSERT(writeVolumeHeader(volume->image, volume->volumeHeader,
    ((off_t)volume->volumeHeader->totalBlocks * (off_t)volume->volumeHeader->blockSize) - 1024), "writeVolumeHeader");
  return writeVolumeHeader(volume->image, volume->volumeHeader, 1024);
}

Volume* openVolume(io_func* io) {
	Volume* volume;
	io_func* file;

	volume = (Volume*) malloc(sizeof(Volume));
	volume->image = io;
	volume->extentsTree = NULL;

	volume->volumeHeader = readVolumeHeader(io, 1024);
	if(volume->volumeHeader == NULL) {
		free(volume);
		return NULL;
	}

	file = openRawFile(kHFSExtentsFileID, &volume->volumeHeader->extentsFile, NULL, volume);
	if(file == NULL) {
		free(volume->volumeHeader);
		free(volume);
		return NULL;
	}

	volume->extentsTree = openExtentsTree(file);
	if(volume->extentsTree == NULL) {
		free(volume->volumeHeader);
		free(volume);
		return NULL;
	}

	file = openRawFile(kHFSCatalogFileID, &volume->volumeHeader->catalogFile, NULL, volume);
	if(file == NULL) {
		closeBTree(volume->extentsTree);
		free(volume->volumeHeader);
		free(volume);
		return NULL;
	}

	volume->catalogTree = openCatalogTree(file);
	if(volume->catalogTree == NULL) {
		closeBTree(volume->extentsTree);
		free(volume->volumeHeader);
		free(volume);
		return NULL;
	}

	volume->allocationFile = openRawFile(kHFSAllocationFileID, &volume->volumeHeader->allocationFile, NULL, volume);
	if(volume->allocationFile == NULL) {
		closeBTree(volume->catalogTree);
		closeBTree(volume->extentsTree);
		free(volume->volumeHeader);
		free(volume);
		return NULL;
	}

	volume->attrTree = NULL;
	file = openRawFile(kHFSAttributesFileID, &volume->volumeHeader->attributesFile, NULL, volume);
	if(file != NULL) {
		volume->attrTree = openAttributesTree(file);
		if(!volume->attrTree) {
			CLOSE(file);
		}
	}

	volume->metadataDir = getMetadataDirectoryID(volume);

	return volume;
}

void closeVolume(Volume *volume) {
  if(volume->attrTree)
    closeBTree(volume->attrTree);

  CLOSE(volume->allocationFile);
  closeBTree(volume->catalogTree);
  closeBTree(volume->extentsTree);
  free(volume->volumeHeader);
  free(volume);
}
