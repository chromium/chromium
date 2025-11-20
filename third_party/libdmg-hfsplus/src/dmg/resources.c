#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <dmg/dmg.h>

static char plstData[1032] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const char* plistHeader = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n<plist version=\"1.0\">\n<dict>\n";
const char* plistFooter = "</dict>\n</plist>\n";

static void flipSizeResource(unsigned char* data, char out) {
  SizeResource* size;
  
  size = (SizeResource*) data;
  
  FLIPENDIANLE(size->version);
  FLIPENDIANLE(size->isHFS);
  FLIPENDIANLE(size->unknown2);
  FLIPENDIANLE(size->unknown3);
  FLIPENDIANLE(size->volumeModified);
  FLIPENDIANLE(size->unknown4);
  FLIPENDIANLE(size->volumeSignature);
  FLIPENDIANLE(size->sizePresent);
}

static void flipCSumResource(unsigned char* data, char out) {
  CSumResource* cSum;
  cSum = (CSumResource*) data;
  
  FLIPENDIANLE(cSum->version);
  FLIPENDIANLE(cSum->type);
  FLIPENDIANLE(cSum->checksum);
}

static void flipAttributionResource(unsigned char* data, char out) {
  AttributionResource* attribution;
  attribution = (AttributionResource*) data;

  FLIPENDIANLE(attribution->signature);
  FLIPENDIANLE(attribution->version);
  FLIPENDIANLE(attribution->rawPos);
  FLIPENDIANLE(attribution->rawLength);
  FLIPENDIANLE(attribution->rawChecksum);
  FLIPENDIANLE(attribution->beforeCompressedChecksum);
  FLIPENDIANLE(attribution->beforeCompressedLength);
  FLIPENDIANLE(attribution->beforeUncompressedChecksum);
  FLIPENDIANLE(attribution->beforeUncompressedLength);
  FLIPENDIANLE(attribution->afterCompressedChecksum);
  FLIPENDIANLE(attribution->afterCompressedLength);
  FLIPENDIANLE(attribution->afterUncompressedChecksum);
  FLIPENDIANLE(attribution->afterUncompressedLength);
}

static void flipBLKXRun(BLKXRun* data) {
  BLKXRun* run;
  
  run = (BLKXRun*) data;
  
  FLIPENDIAN(run->type);
  FLIPENDIAN(run->reserved);
  FLIPENDIAN(run->sectorStart);
  FLIPENDIAN(run->sectorCount);
  FLIPENDIAN(run->compOffset);
  FLIPENDIAN(run->compLength);
}

static void flipBLKX(unsigned char* data, char out) {
  BLKXTable* blkx;
  uint32_t i;
  
  blkx = (BLKXTable*) data;
  
  FLIPENDIAN(blkx->fUDIFBlocksSignature);
  FLIPENDIAN(blkx->infoVersion);
  FLIPENDIAN(blkx->firstSectorNumber);
  FLIPENDIAN(blkx->sectorCount);
  
  FLIPENDIAN(blkx->dataStart);
  FLIPENDIAN(blkx->decompressBufferRequested);
  FLIPENDIAN(blkx->blocksDescriptor);
  
  FLIPENDIAN(blkx->reserved1);
  FLIPENDIAN(blkx->reserved2);
  FLIPENDIAN(blkx->reserved3);
  FLIPENDIAN(blkx->reserved4);
  FLIPENDIAN(blkx->reserved5);
  FLIPENDIAN(blkx->reserved6);
  
  flipUDIFChecksum(&(blkx->checksum), out);

  if(out) {
    for(i = 0; i < blkx->blocksRunCount; i++) {
      flipBLKXRun(&(blkx->runs[i]));     
    }
    FLIPENDIAN(blkx->blocksRunCount);
  } else {
    FLIPENDIAN(blkx->blocksRunCount);
    for(i = 0; i < blkx->blocksRunCount; i++) {
      flipBLKXRun(&(blkx->runs[i]));
    } 
  }
/*
    printf("fUDIFBlocksSignature: 0x%x\n", blkx->fUDIFBlocksSignature);
    printf("infoVersion: 0x%x\n", blkx->infoVersion);
    printf("firstSectorNumber: 0x%llx\n", blkx->firstSectorNumber);
    printf("sectorCount: 0x%llx\n", blkx->sectorCount);
    printf("dataStart: 0x%llx\n", blkx->dataStart);
    printf("decompressBufferRequested: 0x%x\n", blkx->decompressBufferRequested);
    printf("blocksDescriptor: 0x%x\n", blkx->blocksDescriptor);
    printf("blocksRunCount: 0x%x\n", blkx->blocksRunCount);

    for(i = 0; i < 0x20; i++)
    {
	    printf("checksum[%d]: %x\n", i, blkx->checksum.data[i]);
    }*/
}

static char* getXMLString(char** location) {
  char* curLoc;
  char* tagEnd;
  char* toReturn;
  size_t strLen;
  
  curLoc = *location;
  
  curLoc = strstr(curLoc, "<string>");
  if(!curLoc)
    return NULL;
  curLoc += sizeof("<string>") - 1;
  
  tagEnd = strstr(curLoc, "</string>");
  
  strLen = (size_t)(tagEnd - curLoc);
  toReturn = (char*) malloc(strLen + 1);
  memcpy(toReturn, curLoc, strLen);
  toReturn[strLen] = '\0';
  
  curLoc = tagEnd + sizeof("</string>") - 1;
  
  *location = curLoc;
  
  return toReturn;
}

static uint32_t getXMLInteger(char** location) {
  char* curLoc;
  char* tagEnd;
  char* buffer;
  uint32_t toReturn;
  size_t strLen;
  
  curLoc = *location;
  
  curLoc = strstr(curLoc, "<integer>");
  if(!curLoc)
    return 0;
  curLoc += sizeof("<integer>") - 1;
  
  tagEnd = strstr(curLoc, "</integer>");
  
  strLen = (size_t)(tagEnd - curLoc);
  buffer = (char*) malloc(strLen + 1);
  memcpy(buffer, curLoc, strLen);
  buffer[strLen] = '\0';
  
  curLoc = tagEnd + sizeof("</integer>") - 1;
  
  sscanf(buffer, "%d", (int32_t*)(&toReturn));
  
  free(buffer);
  
  *location = curLoc;
  
  return toReturn;
}

static unsigned char* getXMLData(char** location, size_t *dataLength, char** encodedStart, size_t* encodedLength) {
  char* curLoc;
  char* tagEnd;
  char* encodedData;
  unsigned char* toReturn;
  size_t strLen;
  
  curLoc = *location;
  
  curLoc = strstr(curLoc, "<data>");
  if(!curLoc)
    return NULL;
  curLoc += sizeof("<data>") - 1;
    
    if (encodedStart)
        *encodedStart = curLoc;
  
  tagEnd = strstr(curLoc, "</data>");
  
  
  strLen = (size_t)(tagEnd - curLoc);
    
    if (encodedLength)
        *encodedLength = strLen;
  
  encodedData = (char*) malloc(strLen + 1);
  memcpy(encodedData, curLoc, strLen);
  encodedData[strLen] = '\0';
  
  curLoc = tagEnd + sizeof("</data>") - 1;
  
  *location = curLoc;
  
  toReturn = decodeBase64(encodedData, dataLength);
  
  free(encodedData);
  
  return toReturn;
}

static unsigned char* getXMLPlstName(char** location, size_t *dataLength, char** encodedStart, size_t* encodedLength) {
  char* curLoc;
  char* tagEnd;
  char* encodedData;
  unsigned char* toReturn;
  size_t strLen;

  curLoc = *location;

  curLoc = strstr(curLoc, "<string>");
  if(!curLoc)
    return NULL;
  curLoc += sizeof("<string>") - 1;

    if (encodedStart)
        *encodedStart = curLoc;

  tagEnd = strstr(curLoc, "</string>");


  strLen = (size_t)(tagEnd - curLoc);

    if (encodedLength)
        *encodedLength = strLen;

  encodedData = (char*) malloc(strLen + 1);
  memcpy(encodedData, curLoc, strLen);
  encodedData[strLen] = '\0';

  curLoc = tagEnd + sizeof("</string>") - 1;

  *location = curLoc;

  toReturn = decodeBase64(encodedData, dataLength);

  free(encodedData);

  return toReturn;
}

static void readResourceData(ResourceData* data, char** location, char* xmlStart, FlipDataFunc flipData, const unsigned char* key, bool plstNameIsAttribution) {
  char* curLoc;
  char* tagBegin;
  char* tagEnd;
  char* dictEnd;
  size_t strLen;
  char* buffer;
  char* encodedStart;
  
  curLoc = *location;
  
  data->name = NULL;
  data->attributes = 0;
  data->id = 0;
  data->data = NULL;
  
  curLoc = strstr(curLoc, "<dict>");
  dictEnd = strstr(curLoc, "</dict>"); /* hope there's not a dict type in this resource data! */
  while(curLoc != NULL && curLoc < dictEnd) {
    curLoc = strstr(curLoc, "<key>");
    if(!curLoc)
      break;
    curLoc += sizeof("<key>") - 1;
    
    tagEnd = strstr(curLoc, "</key>");
    
    strLen = (size_t)(tagEnd - curLoc);
    tagBegin = curLoc;
    curLoc = tagEnd + sizeof("</key>") - 1;
    
    if(strncmp(tagBegin, "Attributes", strLen) == 0) {
      buffer = getXMLString(&curLoc);
      sscanf(buffer, "0x%x", &(data->attributes));
      free(buffer);
    } else if(strncmp(tagBegin, "Data", strLen) == 0) {
        encodedStart = 0;
      data->data = getXMLData(&curLoc, &(data->dataLength), &encodedStart, &data->dataXmlSize);
      data->dataXmlOffset = encodedStart - xmlStart;
      if(flipData) {
        (*flipData)(data->data, 0);
      }
    } else if(strncmp(tagBegin, "ID", strLen) == 0) {
      buffer = getXMLString(&curLoc);
      sscanf(buffer, "%d", &(data->id));
      free(buffer);
    } else if(strncmp(tagBegin, "Name", strLen) == 0) {
      if (strcmp((char*) key, "plst") == 0 && plstNameIsAttribution) {
	char *nameEncodedStart;
	size_t nameXmlSize;
	size_t nameLength;
	unsigned char* attributionFromName = getXMLPlstName(&curLoc, &nameLength, &nameEncodedStart, &nameXmlSize);
	data->name = attributionFromName;
	flipAttributionResource(data->name, 0);
      } else {
	data->name = getXMLString(&curLoc);
      }
    }
  }
  
  curLoc = dictEnd + sizeof("</dict>") - 1;
  
  *location = curLoc;
}

static void readNSizResource(NSizResource* data, char** location) {
  char* curLoc;
  char* tagBegin;
  char* tagEnd;
  char* dictEnd;
  size_t strLen;
  size_t dummy;
  
  curLoc = *location;
  
  data->isVolume = FALSE;
  data->sha1Digest = NULL;
  data->blockChecksum2 = 0;
  data->bytes = 0;
  data->modifyDate = 0;
  data->partitionNumber = 0;
  data->version = 0;
  data->volumeSignature = 0;
  
  curLoc = strstr(curLoc, "<dict>");
  dictEnd = strstr(curLoc, "</dict>"); /* hope there's not a dict type in this resource data! */
  while(curLoc != NULL && curLoc < dictEnd) {
    curLoc = strstr(curLoc, "<key>");
    if(!curLoc)
      break;
    curLoc += sizeof("<key>") - 1;
    
    tagEnd = strstr(curLoc, "</key>");
    
    strLen = (size_t)(tagEnd - curLoc);
    tagBegin = curLoc;
    curLoc = tagEnd + sizeof("</key>") - 1;
    
    if(strncmp(tagBegin, "SHA-1-digest", strLen) == 0) {
      data->sha1Digest = getXMLData(&curLoc, &dummy, 0, 0);
      /*flipEndian(data->sha1Digest, 4);*/
    } else if(strncmp(tagBegin, "block-checksum-2", strLen) == 0) {
      data->blockChecksum2 = getXMLInteger(&curLoc);
    } else if(strncmp(tagBegin, "bytes", strLen) == 0) {
      data->bytes = getXMLInteger(&curLoc);
    } else if(strncmp(tagBegin, "date", strLen) == 0) {
      data->modifyDate = getXMLInteger(&curLoc);
    } else if(strncmp(tagBegin, "part-num", strLen) == 0) {
      data->partitionNumber = getXMLInteger(&curLoc);
    } else if(strncmp(tagBegin, "version", strLen) == 0) {
      data->version = getXMLInteger(&curLoc);
    } else if(strncmp(tagBegin, "volume-signature", strLen) == 0) {
      data->volumeSignature = getXMLInteger(&curLoc);
      data->isVolume = TRUE;
    }
  }
  
  curLoc = dictEnd + sizeof("</dict>") - 1;
  
  *location = curLoc;
}

static void writeNSizResource(NSizResource* data, char* buffer) {
  char itemBuffer[1024];
  char* sha1Buffer;
  
  (*buffer) = '\0';
  itemBuffer[0] = '\0';
  
  strcat(buffer, plistHeader);
  if(data->sha1Digest != NULL) {
    sha1Buffer = convertBase64(data->sha1Digest, 20, 1, 42);
    sprintf(itemBuffer, "\t<key>SHA-1-digest</key>\n\t<data>\n%s\t</data>\n", sha1Buffer);
    free(sha1Buffer);
    strcat(buffer, itemBuffer);
  }
  sprintf(itemBuffer, "\t<key>block-checksum-2</key>\n\t<integer>%d</integer>\n", (int32_t)(data->blockChecksum2));
  strcat(buffer, itemBuffer);
  if(data->isVolume) {
    sprintf(itemBuffer, "\t<key>bytes</key>\n\t<integer>%d</integer>\n", (int32_t)(data->bytes));
    strcat(buffer, itemBuffer);
    sprintf(itemBuffer, "\t<key>date</key>\n\t<integer>%d</integer>\n", (int32_t)(data->modifyDate));
    strcat(buffer, itemBuffer);
  }
  sprintf(itemBuffer, "\t<key>part-num</key>\n\t<integer>%d</integer>\n", (int32_t)(data->partitionNumber));
  strcat(buffer, itemBuffer);
  sprintf(itemBuffer, "\t<key>version</key>\n\t<integer>%d</integer>\n", (int32_t)(data->version));
  strcat(buffer, itemBuffer);
  if(data->isVolume) {
    sprintf(itemBuffer, "\t<key>volume-signature</key>\n\t<integer>%d</integer>\n", (int32_t)(data->volumeSignature));
    strcat(buffer, itemBuffer);
  }
  strcat(buffer, plistFooter);
}


NSizResource* readNSiz(ResourceKey* resources) {
  ResourceData* curData;
  NSizResource* toReturn;
  NSizResource* curNSiz;
  char* curLoc;
  uint32_t modifyDate;
  
  curData = getResourceByKey(resources, "nsiz")->data;
  toReturn = NULL;
   
  while(curData != NULL) {
    curLoc = (char*) curData->data;
    
    if(toReturn == NULL) {
      toReturn = (NSizResource*) malloc(sizeof(NSizResource));
      curNSiz = toReturn;
    } else {
      curNSiz->next = (NSizResource*) malloc(sizeof(NSizResource));
      curNSiz = curNSiz->next;
    }
    
    curNSiz->next = NULL;
    
    readNSizResource(curNSiz, &curLoc);
    
    
    printf("block-checksum-2:\t0x%x\n", curNSiz->blockChecksum2);
    printf("part-num:\t\t0x%x\n", curNSiz->partitionNumber);
    printf("version:\t\t0x%x\n", curNSiz->version);
    
    if(curNSiz->isVolume) {
      printf("has SHA1:\t\t%d\n", curNSiz->sha1Digest != NULL);
      printf("bytes:\t\t\t0x%x\n", curNSiz->bytes);
      modifyDate = APPLE_TO_UNIX_TIME(curNSiz->modifyDate);
      printf("date:\t\t\t%s", ctime((time_t*)(&modifyDate)));
      printf("volume-signature:\t0x%x\n", curNSiz->volumeSignature);
    }
    
    printf("\n");
    
    curData = curData->next;
  }
  
  return toReturn;
}

ResourceKey* writeNSiz(NSizResource* nSiz) {
  NSizResource* curNSiz;
  ResourceKey* key;
  ResourceData* curData;
  char buffer[1024];
  
  curNSiz = nSiz;
  
  key = (ResourceKey*) malloc(sizeof(ResourceKey));
  key->key = (unsigned char*) malloc(sizeof("nsiz") + 1);
  strcpy((char*) key->key, "nsiz");
  key->next = NULL;
  key->flipData = NULL;
  key->data = NULL;
  
  while(curNSiz != NULL) {
    writeNSizResource(curNSiz, buffer);
    if(key->data == NULL) {
      key->data = (ResourceData*) malloc(sizeof(ResourceData));
      curData = key->data;
    } else {
      curData->next = (ResourceData*) malloc(sizeof(ResourceData));
      curData = curData->next;
    }
    
    curData->attributes = 0;
    curData->id = curNSiz->partitionNumber;
    curData->name = (char*) malloc(sizeof(char));
    curData->name[0] = '\0';
    curData->next = NULL;
    curData->dataLength = sizeof(char) * strlen(buffer);
    curData->data = (unsigned char*) malloc(curData->dataLength);
    memcpy(curData->data, buffer, curData->dataLength);
    
    curNSiz = curNSiz->next;
  }
  
  return key;
}

void releaseNSiz(NSizResource* nSiz) {
  NSizResource* curNSiz;
  NSizResource* toRemove;
  
  curNSiz = nSiz;

  while(curNSiz != NULL) {
    if(curNSiz->sha1Digest != NULL)
      free(curNSiz->sha1Digest);
    
    toRemove = curNSiz;
    curNSiz = curNSiz->next;
    free(toRemove);
  }
}

void outResources(AbstractFile* file, AbstractFile* out)
{
	char* xml;
	UDIFResourceFile resourceFile;
	off_t fileLength;

	fileLength = file->getLength(file);
	file->seek(file, fileLength - sizeof(UDIFResourceFile));
	readUDIFResourceFile(file, &resourceFile);
	xml = (char*) malloc((size_t)resourceFile.fUDIFXMLLength);
	file->seek(file, (off_t)(resourceFile.fUDIFXMLOffset));
	ASSERT(file->read(file, xml, (size_t)resourceFile.fUDIFXMLLength) == (size_t)resourceFile.fUDIFXMLLength, "fread");
	ASSERT(out->write(out, xml, (size_t)resourceFile.fUDIFXMLLength) == (size_t)resourceFile.fUDIFXMLLength, "fwrite");

	file->close(file);
	out->close(out);
}

ResourceKey* readResources(char* xml, size_t length, bool plstNameIsAttribution) {
  char* curLoc;
  char* tagEnd;
  size_t strLen;
  
  ResourceKey* toReturn;
  ResourceKey* curResource;
  ResourceData* curData;
  
  if(!xml)
    return NULL;

  toReturn = NULL;
  curResource = NULL;
  curData = NULL;
  
  curLoc = strstr(xml, "<key>resource-fork</key>");
  if(!curLoc)
    return NULL;
  curLoc += sizeof("<key>resource-fork</key>") - 1;
  
  curLoc = strstr(curLoc, "<dict>");
  if(!curLoc)
    return NULL;
  curLoc += sizeof("<dict>") - 1;
  
  while(TRUE) {
    curLoc = strstr(curLoc, "<key>");
    if(!curLoc)
      break;
    curLoc += sizeof("<key>") - 1;
    
    tagEnd = strstr(curLoc, "</key>");
    if(!tagEnd)
      break;
    
    if(toReturn == NULL) {    
      toReturn = (ResourceKey*) malloc(sizeof(ResourceKey));
      curResource = toReturn;
    } else {
      curResource->next = (ResourceKey*) malloc(sizeof(ResourceKey));
      curResource = curResource->next;
    }
    
    curResource->data = NULL;
    curResource->next = NULL;
    curResource->flipData = NULL;
    
    strLen = (size_t)(tagEnd - curLoc);
    curResource->key = (unsigned char*) malloc(strLen + 1);
    memcpy(curResource->key, curLoc, strLen);
    curResource->key[strLen] = '\0';
    
    curLoc = tagEnd + sizeof("</key>") - 1;
    
    curLoc = strstr(curLoc, "<array>");
    if(!curLoc)
      return NULL;
    curLoc += sizeof("<array>") - 1;
    
    tagEnd = strstr(curLoc, "</array>");
    if(!tagEnd)
      break;
      
    if(strcmp((char*) curResource->key, "blkx") == 0) {
      curResource->flipData = &flipBLKX;
    } else if(strcmp((char*) curResource->key, "size") == 0) {
      curResource->flipData = &flipSizeResource;
    } else if(strcmp((char*) curResource->key, "cSum") == 0) {
      curResource->flipData = &flipCSumResource;
    }
    
    curLoc = strstr(curLoc, "<dict>");
    while(curLoc != NULL && curLoc < tagEnd) {
      if(curResource->data == NULL) {
        curResource->data = (ResourceData*) malloc(sizeof(ResourceData));
        curData = curResource->data;
      } else {
        curData->next = (ResourceData*) malloc(sizeof(ResourceData));
        curData = curData->next;
      }
      
      curData->next = NULL;
      
      readResourceData(curData, &curLoc, xml, curResource->flipData, curResource->key, plstNameIsAttribution);
      curLoc = strstr(curLoc, "<dict>");
    }
       
    curLoc = tagEnd + sizeof("</array>") - 1;
  }
  
  return toReturn;
}

static void writeResourceData(AbstractFile* file, ResourceData* data, ResourceKey* curResource, FlipDataFunc flipData, int tabLength, bool plstNameIsAttribution) {
  unsigned char* dataBuf;
  char* tabs;
  int i;
  
  tabs = (char*) malloc(sizeof(char) * (tabLength + 1));
  for(i = 0; i < tabLength; i++) {
    tabs[i] = '\t';
  }
  tabs[tabLength] = '\0';
  
  abstractFilePrint(file, "%s<dict>\n", tabs);
  abstractFilePrint(file, "%s\t<key>Attributes</key>\n%s\t<string>0x%04x</string>\n", tabs, tabs, data->attributes);

  if(strcmp((char*) curResource->key, "blkx") == 0)
    abstractFilePrint(file, "%s\t<key>CFName</key>\n%s\t<string>%s</string>\n", tabs, tabs, data->name);

  abstractFilePrint(file, "%s\t<key>Data</key>\n%s\t<data>\n", tabs, tabs);
  
  if(flipData) {
    dataBuf = (unsigned char*) malloc(data->dataLength);
    memcpy(dataBuf, data->data, data->dataLength);
    (*flipData)(dataBuf, 1);
    writeBase64(file, dataBuf, data->dataLength, tabLength + 1, 43);
    free(dataBuf);
  } else {
    writeBase64(file, data->data, data->dataLength, tabLength + 1, 43);
  }
  
  abstractFilePrint(file, "%s\t</data>\n", tabs);
  abstractFilePrint(file, "%s\t<key>ID</key>\n%s\t<string>%d</string>\n", tabs, tabs, data->id);
  if (strcmp((char*) curResource->key, "plst") == 0 && plstNameIsAttribution) {
    unsigned char* nameBuf = (unsigned char*) malloc(sizeof(AttributionResource));
    memcpy(nameBuf, data->name, sizeof(AttributionResource));
      flipAttributionResource(nameBuf, 1);
    abstractFilePrint(file, "%s\t<key>Name</key>\n%s\t<string>\n", tabs, tabs);
    writeBase64(file, nameBuf, sizeof(AttributionResource), tabLength + 1, 43);
    abstractFilePrint(file, "%s\t</string>\n", tabs);
  }
  else {
    abstractFilePrint(file, "%s\t<key>Name</key>\n%s\t<string>%s</string>\n", tabs, tabs, data->name);
  }
  abstractFilePrint(file, "%s</dict>\n", tabs);
  
  free(tabs);
}

void writeResources(AbstractFile* file, ResourceKey* resources, bool plstNameIsAttribution) {
  ResourceKey* curResource;
  ResourceData* curData;
  
  abstractFilePrint(file, plistHeader);
  abstractFilePrint(file, "\t<key>resource-fork</key>\n\t<dict>\n");
  
  curResource = resources;
  while(curResource != NULL) {
    abstractFilePrint(file, "\t\t<key>%s</key>\n\t\t<array>\n", curResource->key);
    curData = curResource->data;
    while(curData != NULL) {
	    writeResourceData(file, curData, curResource, curResource->flipData, 3, plstNameIsAttribution);
	    curData = curData->next;
    }
    abstractFilePrint(file, "\t\t</array>\n", curResource->key);
    curResource = curResource->next;
  }  

  abstractFilePrint(file, "\t</dict>\n");
  abstractFilePrint(file, plistFooter);
   
}

static void releaseResourceData(ResourceData* data) {
  ResourceData* curData;
  ResourceData* nextData;
  
  nextData = data;
  while(nextData != NULL) {
    curData = nextData;
    
    if(curData->name)
      free(curData->name);
    
    if(curData->data)
      free(curData->data);
      
    nextData = nextData->next;
    free(curData);
  }
}

void releaseResources(ResourceKey* resources) {
  ResourceKey* curResource;
  ResourceKey* nextResource;
  
  nextResource = resources;
  while(nextResource != NULL) {
    curResource = nextResource;
    free(curResource->key);
    releaseResourceData(curResource->data);
    nextResource = nextResource->next;
    free(curResource);
  }
}

ResourceKey* getResourceByKey(ResourceKey* resources, const char* key) {
  ResourceKey* curResource;
  
  curResource = resources;
  while(curResource != NULL) {
    if(strcmp((char*) curResource->key, key) == 0) {
      return curResource;
    }
    curResource = curResource->next;
  }
  
  return NULL;
}

ResourceData* getDataByID(ResourceKey* resource, int id) {
  ResourceData* curData;
  
  curData = resource->data;
  
  while(curData != NULL) {
    if(curData->id == id) {
      return curData;
    }
    curData = curData->next;
  }
  
  return NULL;
}

ResourceKey* insertData(ResourceKey* resources, const char* key, int id, const char* name, size_t nameLength, bool nameAsData, const char* data, size_t dataLength, uint32_t attributes) {
  ResourceKey* curResource;
  ResourceKey* lastResource;
  ResourceData* curData;

  lastResource = resources;
  curResource = resources;
  while(curResource != NULL) {
    if(strcmp((char*) curResource->key, key) == 0) {
      break;
    }
    lastResource = curResource;
    curResource = curResource->next;
  }
  
  if(curResource == NULL) {
    if(lastResource == NULL) {
      curResource = (ResourceKey*) malloc(sizeof(ResourceKey));
    } else {
      lastResource->next = (ResourceKey*) malloc(sizeof(ResourceKey));
      curResource = lastResource->next;
    }
    
    curResource->key = (unsigned char*) malloc(strlen(key) + 1);
    strcpy((char*) curResource->key, key);
    curResource->next = NULL;
    
    if(strcmp((char*) curResource->key, "blkx") == 0) {
      curResource->flipData = &flipBLKX;
    } else if(strcmp((char*) curResource->key, "size") == 0) {
	  printf("we know to flip this size resource\n");
      curResource->flipData = &flipSizeResource;
    } else if(strcmp((char*) curResource->key, "cSum") == 0) {
      curResource->flipData = &flipCSumResource;
    } else {
      curResource->flipData = NULL;
    }
    
    curResource->data = NULL;
  }
  
  if(curResource->data == NULL) {
    curData = (ResourceData*) malloc(sizeof(ResourceData));
    curResource->data = curData;
    curData->next = NULL;
  } else {
    curData = curResource->data;
    while(curData->next != NULL) {
      if(curData->id == id) {
        break;
      }
      curData = curData->next;
    }
    
    if(curData->id != id) {
      curData->next = (ResourceData*) malloc(sizeof(ResourceData));
      curData = curData->next;
      curData->next = NULL;
    } else {
      free(curData->data);
      free(curData->name);
    }
  }
  
  curData->attributes = attributes;
  curData->dataLength = dataLength;
  curData->id = id;
  if (nameAsData) {
    curData->name = (unsigned char*) malloc(nameLength);
    memcpy(curData->name, name, nameLength);
  }
  else {
    curData->name = (char*) malloc(strlen(name) + 1);
    strcpy((char*) curData->name, name);
  }
  curData->data = (unsigned char*) malloc(dataLength);
  memcpy(curData->data, data, dataLength);
  
  int i = 0;
  if(resources) {
    curResource = resources;
    while(curResource) {
      curResource = curResource->next;
      i++;
    }
    return resources;
  } else {
    return curResource;
  }
}

ResourceKey* makePlst(const char* name, size_t nameLength, bool nameAsData) {
  return insertData(NULL, "plst", 0, name, nameLength, nameAsData, plstData, sizeof(plstData), ATTRIBUTE_HDIUTIL);
}

ResourceKey* makeSize(HFSPlusVolumeHeader* volumeHeader) {
  SizeResource size;
  memset(&size, 0, sizeof(SizeResource));
  size.version = 5;
  size.isHFS = 1;
  size.unknown1 = 0;
  size.unknown2 = 0;
  size.unknown3 = 0;
  size.volumeModified = volumeHeader->modifyDate;
  size.unknown4 = 0;
  size.volumeSignature = volumeHeader->signature;
  size.sizePresent = 1;

  printf("making size data\n");  
  return insertData(NULL, "size", 2, "", 0, false, (const char*)(&size), sizeof(SizeResource), 0);
}

