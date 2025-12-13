#include <stdlib.h>
#include <string.h>
#include <hfs/hfsplus.h>

static inline void flipExtentDescriptor(HFSPlusExtentDescriptor* extentDescriptor) {
  FLIPENDIAN(extentDescriptor->startBlock);
  FLIPENDIAN(extentDescriptor->blockCount);
}

void flipExtentRecord(HFSPlusExtentRecord* extentRecord) {
  HFSPlusExtentDescriptor *extentDescriptor;
  extentDescriptor = (HFSPlusExtentDescriptor*)extentRecord;
  
  flipExtentDescriptor(&extentDescriptor[0]);
  flipExtentDescriptor(&extentDescriptor[1]);
  flipExtentDescriptor(&extentDescriptor[2]);
  flipExtentDescriptor(&extentDescriptor[3]);
  flipExtentDescriptor(&extentDescriptor[4]);
  flipExtentDescriptor(&extentDescriptor[5]);
  flipExtentDescriptor(&extentDescriptor[6]);
  flipExtentDescriptor(&extentDescriptor[7]);
}

static int extentCompare(BTKey* vLeft, BTKey* vRight) {
  HFSPlusExtentKey* left;
  HFSPlusExtentKey* right;
  
  left = (HFSPlusExtentKey*) vLeft;
  right =(HFSPlusExtentKey*) vRight;
  
  if(left->forkType < right->forkType) {
    return -1;
  } else if(left->forkType > right->forkType) {
    return 1;
  } else {
    if(left->fileID < right->fileID) {
      return -1;
    } else if(left->fileID > right->fileID) {
      return 1;
    } else {
      if(left->startBlock < right->startBlock) {
        return -1;
      } else if(left->startBlock > right->startBlock) {
        return 1;
      } else {
        /* do a safety check on key length. Otherwise, bad things may happen later on when we try to add or remove with this key */
        if(left->keyLength == right->keyLength) {
          return 0;
        } else if(left->keyLength < right->keyLength) {
          return -1;
        } else {
          return 1;
        }
        return 0;
      }
    }
  }
}

static BTKey* extentKeyRead(off_t offset, io_func* io) {
  HFSPlusExtentKey* key;
  
  key = (HFSPlusExtentKey*) malloc(sizeof(HFSPlusExtentKey));
  
  if(!READ(io, offset, sizeof(HFSPlusExtentKey), key))
    return NULL;
  
  FLIPENDIAN(key->keyLength);
  FLIPENDIAN(key->forkType);
  FLIPENDIAN(key->fileID);
  FLIPENDIAN(key->startBlock);
  
  return (BTKey*)key;
}

static int extentKeyWrite(off_t offset, BTKey* toWrite, io_func* io) {
  HFSPlusExtentKey* key;
  
  key = (HFSPlusExtentKey*) malloc(sizeof(HFSPlusExtentKey));
  
  memcpy(key, toWrite, sizeof(HFSPlusExtentKey));
  
  FLIPENDIAN(key->keyLength);
  FLIPENDIAN(key->forkType);
  FLIPENDIAN(key->fileID);
  FLIPENDIAN(key->startBlock);
  
  if(!WRITE(io, offset, sizeof(HFSPlusExtentKey), key))
    return FALSE;
    
  free(key);
  
  return TRUE;
}

static void extentKeyPrint(BTKey* toPrint) {
  HFSPlusExtentKey* key;
  
  key = (HFSPlusExtentKey*)toPrint;
  
  printf("extent%d:%d:%d", key->forkType, key->fileID, key->startBlock);
}

static BTKey* extentDataRead(off_t offset, io_func* io) {
  HFSPlusExtentRecord* record;
  
  record = (HFSPlusExtentRecord*) malloc(sizeof(HFSPlusExtentRecord));
  
  if(!READ(io, offset, sizeof(HFSPlusExtentRecord), record))
    return NULL;
    
  flipExtentRecord(record);
  
  return (BTKey*)record;
}

BTree* openExtentsTree(io_func* file) {
  return openBTree(file, &extentCompare, &extentKeyRead, &extentKeyWrite, &extentKeyPrint, &extentDataRead);
}
