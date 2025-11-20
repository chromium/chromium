#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <hfs/hfsplus.h>

static inline void flipBSDInfo(HFSPlusBSDInfo* info) {
  FLIPENDIAN(info->ownerID);
  FLIPENDIAN(info->groupID);
  FLIPENDIAN(info->fileMode);
  FLIPENDIAN(info->special);
}

static inline void flipPoint(Point* point) {
  FLIPENDIAN(point->v);
  FLIPENDIAN(point->h);
}

static inline void flipRect(Rect* rect) {
  FLIPENDIAN(rect->top);
  FLIPENDIAN(rect->left);
  FLIPENDIAN(rect->bottom);
  FLIPENDIAN(rect->right);
}

static inline void flipFolderInfo(FolderInfo* info) {
  flipRect(&info->windowBounds);
  FLIPENDIAN(info->finderFlags);
  flipPoint(&info->location);
}

static inline void flipExtendedFolderInfo(ExtendedFolderInfo* info) {
  flipPoint(&info->scrollPosition);
  FLIPENDIAN(info->extendedFinderFlags);
  FLIPENDIAN(info->putAwayFolderID);
}

static inline void flipFileInfo(FileInfo* info) {
  FLIPENDIAN(info->fileType);
  FLIPENDIAN(info->fileCreator);
  FLIPENDIAN(info->finderFlags);
  flipPoint(&info->location);
}

static inline void flipExtendedFileInfo(ExtendedFileInfo* info) {
  FLIPENDIAN(info->extendedFinderFlags);
  FLIPENDIAN(info->putAwayFolderID);
}

void flipCatalogFolder(HFSPlusCatalogFolder* record) {
  FLIPENDIAN(record->recordType);
  FLIPENDIAN(record->flags);
  FLIPENDIAN(record->valence);
  FLIPENDIAN(record->folderID);
  FLIPENDIAN(record->createDate);
  FLIPENDIAN(record->contentModDate);
  FLIPENDIAN(record->attributeModDate);
  FLIPENDIAN(record->accessDate);
  FLIPENDIAN(record->backupDate);
  
  flipBSDInfo(&record->permissions);
  flipFolderInfo(&record->userInfo);
  flipExtendedFolderInfo(&record->finderInfo);
  
  FLIPENDIAN(record->textEncoding);
  FLIPENDIAN(record->folderCount);
}

void flipCatalogFile(HFSPlusCatalogFile* record) {
  FLIPENDIAN(record->recordType);
  FLIPENDIAN(record->flags);
  FLIPENDIAN(record->fileID);
  FLIPENDIAN(record->createDate);
  FLIPENDIAN(record->contentModDate);
  FLIPENDIAN(record->attributeModDate);
  FLIPENDIAN(record->accessDate);
  FLIPENDIAN(record->backupDate);
  
  flipBSDInfo(&record->permissions);
  flipFileInfo(&record->userInfo);
  flipExtendedFileInfo(&record->finderInfo);
  
  FLIPENDIAN(record->textEncoding);
  
  flipForkData(&record->dataFork);
  flipForkData(&record->resourceFork);
}

void flipCatalogThread(HFSPlusCatalogThread* record, int out) {
  int i;
  int nameLength;
  
  FLIPENDIAN(record->recordType);
  FLIPENDIAN(record->parentID);
  if(out) {
    nameLength = record->nodeName.length;
    FLIPENDIAN(record->nodeName.length);
  } else {
    FLIPENDIAN(record->nodeName.length);
    nameLength = record->nodeName.length;
  }
  
  for(i = 0; i < nameLength; i++) {
    if(out) {
	  if(record->nodeName.unicode[i] == ':') {
	    record->nodeName.unicode[i] = '/';
	  }
	  FLIPENDIAN(record->nodeName.unicode[i]);
	} else {	
	  FLIPENDIAN(record->nodeName.unicode[i]);
	  if(record->nodeName.unicode[i] == '/') {
	    record->nodeName.unicode[i] = ':';
	  }
	}
  }
}

#define UNICODE_START (sizeof(uint16_t) + sizeof(HFSCatalogNodeID) + sizeof(uint16_t))

static void catalogKeyPrint(BTKey* toPrint) {
  HFSPlusCatalogKey* key;
  
  key = (HFSPlusCatalogKey*) toPrint;
  
  printf("%d:", key->parentID);
  printUnicode(&key->nodeName);
}

static int catalogCompare(BTKey* vLeft, BTKey* vRight) {
  HFSPlusCatalogKey* left;
  HFSPlusCatalogKey* right;
  uint16_t i;

  uint16_t cLeft;
  uint16_t cRight;
  
  left = (HFSPlusCatalogKey*) vLeft;
  right =(HFSPlusCatalogKey*) vRight;
   
  if(left->parentID < right->parentID) {
    return -1;
  } else if(left->parentID > right->parentID) {
    return 1;
  } else {
    for(i = 0; i < left->nodeName.length; i++) {
      if(i >= right->nodeName.length) {
        return 1;
      } else {
		/* ugly hack to support weird : to / conversion on iPhone */
	    if(left->nodeName.unicode[i] == ':') {
			cLeft = '/';
		} else {
			cLeft = left->nodeName.unicode[i] ;
		}
		
		if(right->nodeName.unicode[i] == ':') {
			cRight = '/';
		} else {
			cRight = right->nodeName.unicode[i];
		}
		
        if(cLeft < cRight)
          return -1;
        else if(cLeft > cRight)
          return 1;
      }
    }
    
    if(i < right->nodeName.length) {
      return -1;
    } else {
      /* do a safety check on key length. Otherwise, bad things may happen later on when we try to add or remove with this key */
      /*if(left->keyLength == right->keyLength) {
        return 0;
      } else if(left->keyLength < right->keyLength) {
        return -1;
      } else {
        return 1;
      }*/
      return 0;
    }
  }
}

static int catalogCompareCS(BTKey* vLeft, BTKey* vRight) {
  HFSPlusCatalogKey* left;
  HFSPlusCatalogKey* right;
  
  left = (HFSPlusCatalogKey*) vLeft;
  right =(HFSPlusCatalogKey*) vRight;
   
  if(left->parentID < right->parentID) {
    return -1;
  } else if(left->parentID > right->parentID) {
    return 1;
  } else {
    return FastUnicodeCompare(left->nodeName.unicode, left->nodeName.length, right->nodeName.unicode, right->nodeName.length);
  }
}

static BTKey* catalogKeyRead(off_t offset, io_func* io) {
  HFSPlusCatalogKey* key;
  uint16_t i;
  
  key = (HFSPlusCatalogKey*) malloc(sizeof(HFSPlusCatalogKey));
  
  if(!READ(io, offset, UNICODE_START, key))
    return NULL;
  
  FLIPENDIAN(key->keyLength);
  FLIPENDIAN(key->parentID);
  FLIPENDIAN(key->nodeName.length);
   
  if(!READ(io, offset + UNICODE_START, key->nodeName.length * sizeof(uint16_t), ((unsigned char *)key) + UNICODE_START))
    return NULL;
    
  for(i = 0; i < key->nodeName.length; i++) {
    FLIPENDIAN(key->nodeName.unicode[i]);
	if(key->nodeName.unicode[i] == '/') /* ugly hack that iPhone seems to do */
		key->nodeName.unicode[i] = ':';
  }
  
  return (BTKey*)key;
}

static int catalogKeyWrite(off_t offset, BTKey* toWrite, io_func* io) {
  HFSPlusCatalogKey* key;
  uint16_t i;
  uint16_t keyLength;
  uint16_t nodeNameLength;
  
  keyLength = toWrite->keyLength + sizeof(uint16_t);
  key = (HFSPlusCatalogKey*) malloc(keyLength);
  memcpy(key, toWrite, keyLength);
  
  nodeNameLength = key->nodeName.length;
  
  FLIPENDIAN(key->keyLength);
  FLIPENDIAN(key->parentID);
  FLIPENDIAN(key->nodeName.length);
  
  for(i = 0; i < nodeNameLength; i++) {
	if(key->nodeName.unicode[i] == ':') /* ugly hack that iPhone seems to do */
		key->nodeName.unicode[i] = '/';

    FLIPENDIAN(key->nodeName.unicode[i]);
  }
 
  if(!WRITE(io, offset, keyLength, key))
    return FALSE;
  
  free(key);
    
  return TRUE;
}

static BTKey* catalogDataRead(off_t offset, io_func* io) {
  int16_t recordType;
  HFSPlusCatalogRecord* record;
  uint16_t nameLength;
  
  if(!READ(io, offset, sizeof(int16_t), &recordType))
    return NULL;
    
  FLIPENDIAN(recordType); fflush(stdout);

  switch(recordType) {
    case kHFSPlusFolderRecord:
      record = (HFSPlusCatalogRecord*) malloc(sizeof(HFSPlusCatalogFolder));
      if(!READ(io, offset, sizeof(HFSPlusCatalogFolder), record))
        return NULL;
      flipCatalogFolder((HFSPlusCatalogFolder*)record);
      break;
      
    case kHFSPlusFileRecord:
      record = (HFSPlusCatalogRecord*) malloc(sizeof(HFSPlusCatalogFile));
      if(!READ(io, offset, sizeof(HFSPlusCatalogFile), record))
        return NULL;
      flipCatalogFile((HFSPlusCatalogFile*)record);
      break;
      
    case kHFSPlusFolderThreadRecord:
    case kHFSPlusFileThreadRecord:
      record = (HFSPlusCatalogRecord*) malloc(sizeof(HFSPlusCatalogThread));
      
      if(!READ(io, offset + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint32_t), sizeof(uint16_t), &nameLength))
        return NULL;

      FLIPENDIAN(nameLength);
      
      if(!READ(io, offset, sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint16_t) + (sizeof(uint16_t) * nameLength), record))
        return NULL;
      
      flipCatalogThread((HFSPlusCatalogThread*)record, FALSE);
      break;
  }

  return (BTKey*)record;
}

void ASCIIToUnicode(const char* ascii, HFSUniStr255* unistr) {
  int count;
  
  count = 0;
  while(ascii[count] != '\0') {
    unistr->unicode[count] = ascii[count];
    count++;
  }
  
  unistr->length = count;
}

HFSPlusCatalogRecord* getRecordByCNID(HFSCatalogNodeID CNID, Volume* volume) {
	HFSPlusCatalogKey key;
	HFSPlusCatalogThread* thread;
	HFSPlusCatalogRecord* record;
	int exact;
	
	key.keyLength = sizeof(key.parentID) + sizeof(key.nodeName.length);
	key.parentID = CNID;
	key.nodeName.length = 0;
	
	thread = (HFSPlusCatalogThread*) search(volume->catalogTree, (BTKey*)(&key), &exact, NULL, NULL);
	
	if(thread == NULL) {
		return NULL;
	}
	
	if(exact == FALSE) {
		free(thread);
		return NULL;
	}
	
	key.parentID = thread->parentID;
    key.nodeName = thread->nodeName;
    
	free(thread);
	
    record = (HFSPlusCatalogRecord*) search(volume->catalogTree, (BTKey*)(&key), &exact, NULL, NULL);
	
	if(record == NULL || exact == FALSE)
		return NULL;
	else
		return record;
}

CatalogRecordList* getFolderContents(HFSCatalogNodeID CNID, Volume* volume) {
	BTree* tree;
	HFSPlusCatalogThread* record; 
	HFSPlusCatalogKey key;
	uint32_t nodeNumber;
	int recordNumber;

	BTNodeDescriptor* descriptor;
	off_t recordOffset;
	off_t recordDataOffset;
	HFSPlusCatalogKey* currentKey;

	CatalogRecordList* list;
	CatalogRecordList* lastItem;
	CatalogRecordList* item;

	char pathBuffer[1024];
	HFSPlusCatalogRecord* toReturn;
	HFSPlusCatalogKey nkey;
	int exact;

	tree = volume->catalogTree;

	key.keyLength = sizeof(key.parentID) + sizeof(key.nodeName.length);
	key.parentID = CNID;
	key.nodeName.length = 0;

	list = NULL;

	record = (HFSPlusCatalogThread*) search(tree, (BTKey*)(&key), NULL, &nodeNumber, &recordNumber);

	if(record == NULL)
		return NULL;

	free(record);

	++recordNumber;

	while(nodeNumber != 0) {    
		descriptor = readBTNodeDescriptor(nodeNumber, tree);

		while(recordNumber < descriptor->numRecords) {
			recordOffset = getRecordOffset(recordNumber, nodeNumber, tree);
			currentKey = (HFSPlusCatalogKey*) READ_KEY(tree, recordOffset, tree->io);
			recordDataOffset = recordOffset + currentKey->keyLength + sizeof(currentKey->keyLength);

			if(currentKey->parentID == CNID) {
				item = (CatalogRecordList*) malloc(sizeof(CatalogRecordList));
				item->name = currentKey->nodeName;
				item->record = (HFSPlusCatalogRecord*) READ_DATA(tree, recordDataOffset, tree->io);

				if(item->record->recordType == kHFSPlusFileRecord && (((HFSPlusCatalogFile*)item->record)->userInfo.fileType) == kHardLinkFileType) {
					sprintf(pathBuffer, "iNode%d", ((HFSPlusCatalogFile*)item->record)->permissions.special.iNodeNum);
					nkey.parentID = volume->metadataDir;
					ASCIIToUnicode(pathBuffer, &nkey.nodeName); 
					nkey.keyLength = sizeof(nkey.parentID) + sizeof(nkey.nodeName.length) + (sizeof(uint16_t) * nkey.nodeName.length);

					toReturn = (HFSPlusCatalogRecord*) search(volume->catalogTree, (BTKey*)(&nkey), &exact, NULL, NULL);

					free(item->record);
					item->record = toReturn;
				}
				item->next = NULL;

				if(list == NULL) {
					list = item;
				} else {
					lastItem->next = item;
				}

				lastItem = item;
				free(currentKey);
			} else {
				free(currentKey);
				free(descriptor);
				return list;
			}

			recordNumber++;
		}

		nodeNumber = descriptor->fLink;
		recordNumber = 0;

		free(descriptor);
	}

	return list;
}

void releaseCatalogRecordList(CatalogRecordList* list) {
	CatalogRecordList* next;
	while(list) {
		next = list->next;
		free(list->record);
		free(list);
		list = next;
	}
}

HFSPlusCatalogRecord* getLinkTarget(HFSPlusCatalogRecord* record, HFSCatalogNodeID parentID, HFSPlusCatalogKey *key, Volume* volume) {
	io_func* io;
	char pathBuffer[1024];
	HFSPlusCatalogRecord* toReturn;
	HFSPlusCatalogKey nkey;
	int exact;

	if(record->recordType == kHFSPlusFileRecord && (((HFSPlusCatalogFile*)record)->permissions.fileMode & S_IFLNK) == S_IFLNK) {
		io = openRawFile(((HFSPlusCatalogFile*)record)->fileID, &(((HFSPlusCatalogFile*)record)->dataFork), record, volume);
		READ(io, 0, (((HFSPlusCatalogFile*)record)->dataFork).logicalSize, pathBuffer);
		CLOSE(io);
		pathBuffer[(((HFSPlusCatalogFile*)record)->dataFork).logicalSize] = '\0';
		toReturn = getRecordFromPath3(pathBuffer, volume, NULL, key, TRUE, TRUE, parentID);
		free(record);
		return toReturn;
	} else if(record->recordType == kHFSPlusFileRecord && (((HFSPlusCatalogFile*)record)->userInfo.fileType) == kHardLinkFileType) {
		sprintf(pathBuffer, "iNode%d", ((HFSPlusCatalogFile*)record)->permissions.special.iNodeNum);
		nkey.parentID = volume->metadataDir;
    		ASCIIToUnicode(pathBuffer, &nkey.nodeName); 
		nkey.keyLength = sizeof(nkey.parentID) + sizeof(nkey.nodeName.length) + (sizeof(uint16_t) * nkey.nodeName.length);

		toReturn = (HFSPlusCatalogRecord*) search(volume->catalogTree, (BTKey*)(&nkey), &exact, NULL, NULL);

		free(record);

		return toReturn;
	} else {
		return record;
	}
}

static const uint16_t METADATA_DIR[] = {0, 0, 0, 0, 'H', 'F', 'S', '+', ' ', 'P', 'r', 'i', 'v', 'a', 't', 'e', ' ', 'D', 'a', 't', 'a'};

HFSCatalogNodeID getMetadataDirectoryID(Volume* volume) {
	HFSPlusCatalogKey key;
	HFSPlusCatalogFolder* record;
	int exact;
	HFSCatalogNodeID id;

	key.nodeName.length = sizeof(METADATA_DIR) / sizeof(uint16_t);
	key.keyLength = sizeof(key.parentID) + sizeof(key.nodeName.length) + sizeof(METADATA_DIR);
	key.parentID = kHFSRootFolderID;
	memcpy(key.nodeName.unicode, METADATA_DIR, sizeof(METADATA_DIR));

	record = (HFSPlusCatalogFolder*) search(volume->catalogTree, (BTKey*)(&key), &exact, NULL, NULL);
	id = record->folderID;

	free(record);

	return id;
}

HFSPlusCatalogRecord* getRecordFromPath(const char* path, Volume* volume, char **name, HFSPlusCatalogKey* retKey) {
	return getRecordFromPath2(path, volume, name, retKey, TRUE);
}

HFSPlusCatalogRecord* getRecordFromPath2(const char* path, Volume* volume, char **name, HFSPlusCatalogKey* retKey, char traverse) {
	return getRecordFromPath3(path, volume, name, retKey, TRUE, TRUE, kHFSRootFolderID);
}

HFSPlusCatalogRecord* getRecordFromPath3(const char* path, Volume* volume, char **name, HFSPlusCatalogKey* retKey, char traverse, char returnLink, HFSCatalogNodeID parentID) {
  HFSPlusCatalogKey key;
  HFSPlusCatalogRecord* record;
  
  char* origPath;
  char* myPath;
  char* word;
  char* pathLimit;
  
  uint32_t realParent;
 
  int exact;
  
  if(path[0] == '\0' || (path[0] == '/' && path[1] == '\0')) {
    if(name != NULL)
      *name = (char*)path;
    
    key.keyLength = sizeof(key.parentID) + sizeof(key.nodeName.length);
    key.parentID = kHFSRootFolderID;
    key.nodeName.length = 0;
    
    record = (HFSPlusCatalogRecord*) search(volume->catalogTree, (BTKey*)(&key), &exact, NULL, NULL);
    key.parentID = ((HFSPlusCatalogThread*)record)->parentID;
    key.nodeName = ((HFSPlusCatalogThread*)record)->nodeName;
    
    free(record);
	
    record = (HFSPlusCatalogRecord*) search(volume->catalogTree, (BTKey*)(&key), &exact, NULL, NULL);
    return record;
  }
  
  myPath = strdup(path);
  origPath = myPath;
  
  record = NULL;

  if(path[0] == '/') {
    key.parentID = kHFSRootFolderID;
  } else {
    key.parentID = parentID;
  }

  pathLimit = myPath + strlen(myPath);
  
  for(word = (char*)strtok(myPath, "/"); word && (word < pathLimit);
      word = ((word + strlen(word) + 1) < pathLimit) ? (char*)strtok(word + strlen(word) + 1, "/") : NULL) {

    if(name != NULL)
      *name = (char*)(path + (word - origPath));
    
    if(record != NULL) {
      free(record);
      record = NULL;
    }
      
    if(word[0] == '\0') {
      continue;
    }

    if(strcmp(word, "..") == 0) {
      key.nodeName.length = 0;
    } else {
      ASCIIToUnicode(word, &key.nodeName);
    }
    key.keyLength = sizeof(key.parentID) + sizeof(key.nodeName.length) + (sizeof(uint16_t) * key.nodeName.length);
    record = (HFSPlusCatalogRecord*) search(volume->catalogTree, (BTKey*)(&key), &exact, NULL, NULL);

    if(record == NULL || exact == FALSE) {
      free(origPath);
      if(record != NULL) {
	      free(record);
      }
      return NULL;
    }

    if(traverse) {
      if(((word + strlen(word) + 1) < pathLimit) || returnLink) {
        record = getLinkTarget(record, key.parentID, &key, volume);
        if(record == NULL || exact == FALSE) {
          free(origPath);
          return NULL;
        }
      }
    }

    if(record->recordType == kHFSPlusFileRecord) {
      if((word + strlen(word) + 1) >= pathLimit) {
        free(origPath);

        if(retKey != NULL) {
          memcpy(retKey, &key, sizeof(HFSPlusCatalogKey));
        }

        return record;
      } else {
        free(origPath);
        free(record);
        return NULL;
      }
    }

    if(record->recordType == kHFSPlusFolderThreadRecord) {
      key.parentID = ((HFSPlusCatalogThread*)record)->parentID;
      continue;
    }

    if(record->recordType != kHFSPlusFolderRecord)
      hfs_panic("inconsistent catalog tree!");
    
    realParent = key.parentID;
    key.parentID = ((HFSPlusCatalogFolder*)record)->folderID;
  }

  if(record->recordType == kHFSPlusFolderThreadRecord) {
    free(record);
    record = getRecordByCNID(key.parentID, volume);
  }
  
  if(retKey != NULL) {
    memcpy(retKey, &key, sizeof(HFSPlusCatalogKey));
    retKey->parentID = realParent;
  }
  
  free(origPath);
  return record;
}

int updateCatalog(Volume* volume, HFSPlusCatalogRecord* catalogRecord) {
  HFSPlusCatalogKey key;
  HFSPlusCatalogRecord* record;
  HFSPlusCatalogFile file;
  HFSPlusCatalogFolder folder;
  int exact;
  
  key.keyLength = sizeof(key.parentID) + sizeof(key.nodeName.length);
  if(catalogRecord->recordType == kHFSPlusFolderRecord) {
    key.parentID = ((HFSPlusCatalogFolder*)catalogRecord)->folderID;
  } else if(catalogRecord->recordType == kHFSPlusFileRecord) {
    key.parentID = ((HFSPlusCatalogFile*)catalogRecord)->fileID;
  } else {
    /* unexpected */
    return FALSE;
  }
  key.nodeName.length = 0;
  
  record = (HFSPlusCatalogRecord*) search(volume->catalogTree, (BTKey*)(&key), &exact, NULL, NULL);
  
  key.parentID = ((HFSPlusCatalogThread*)record)->parentID;
  key.nodeName = ((HFSPlusCatalogThread*)record)->nodeName;
  key.keyLength = sizeof(key.parentID) + sizeof(key.nodeName.length) + (sizeof(uint16_t) * key.nodeName.length);

  free(record);

  record = (HFSPlusCatalogRecord*) search(volume->catalogTree, (BTKey*)(&key), &exact, NULL, NULL);
  
  removeFromBTree(volume->catalogTree, (BTKey*)(&key));
  
  switch(record->recordType) {
    case kHFSPlusFolderRecord:
      memcpy(&folder, catalogRecord, sizeof(HFSPlusCatalogFolder));
	  flipCatalogFolder(&folder);
	  free(record);
      return addToBTree(volume->catalogTree, (BTKey*)(&key), sizeof(HFSPlusCatalogFolder), (unsigned char *)(&folder));
      break;
      
    case kHFSPlusFileRecord:
      memcpy(&file, catalogRecord, sizeof(HFSPlusCatalogFile));
      flipCatalogFile(&file);
	  free(record);
      return addToBTree(volume->catalogTree, (BTKey*)(&key), sizeof(HFSPlusCatalogFile), (unsigned char *)(&file));
      break;
  }
  
  return TRUE;
}

int move(const char* source, const char* dest, Volume* volume) {
  HFSPlusCatalogRecord* srcRec;
  HFSPlusCatalogFolder* srcFolderRec;
  HFSPlusCatalogFolder* destRec;
  char* destPath;
  char* destName;
  char* curChar;
  char* lastSeparator;
  
  int i;
  int threadLength;

  HFSPlusCatalogKey srcKey;
  HFSPlusCatalogKey destKey;
  HFSPlusCatalogThread* thread;
  
  srcRec = getRecordFromPath3(source, volume, NULL, &srcKey, TRUE, FALSE, kHFSRootFolderID);
  if(srcRec == NULL) {
    free(srcRec);
    return FALSE;
  }
  
  srcFolderRec = (HFSPlusCatalogFolder*) getRecordByCNID(srcKey.parentID, volume);
    
  if(srcFolderRec == NULL || srcFolderRec->recordType != kHFSPlusFolderRecord) {
    free(srcRec);
    free(srcFolderRec);
    return FALSE;
  }
    
  destPath = strdup(dest);
  
  curChar = destPath;
  lastSeparator = NULL;
  
  while((*curChar) != '\0') {
    if((*curChar) == '/')
      lastSeparator = curChar;
    curChar++;
  }
  
  if(lastSeparator == NULL) {
    destRec = (HFSPlusCatalogFolder*) getRecordFromPath("/", volume, NULL, NULL);
    destName = destPath;
  } else {
    destName = lastSeparator + 1;
    *lastSeparator = '\0';
    destRec = (HFSPlusCatalogFolder*) getRecordFromPath(destPath, volume, NULL, NULL);
    
    if(destRec == NULL || destRec->recordType != kHFSPlusFolderRecord) {
      free(destPath);
      free(srcRec);
      free(destRec);
      free(srcFolderRec);
      return FALSE;
    }
  }
  
  removeFromBTree(volume->catalogTree, (BTKey*)(&srcKey));
  
  srcKey.nodeName.length = 0;
  if(srcRec->recordType == kHFSPlusFolderRecord) {
    srcKey.parentID = ((HFSPlusCatalogFolder*)srcRec)->folderID;
  } else if(srcRec->recordType == kHFSPlusFileRecord) {
    srcKey.parentID = ((HFSPlusCatalogFile*)srcRec)->fileID;
  } else {
    /* unexpected */
    return FALSE;
  }
  srcKey.keyLength = sizeof(srcKey.parentID) + sizeof(srcKey.nodeName.length);
  
  removeFromBTree(volume->catalogTree, (BTKey*)(&srcKey));
  

  destKey.nodeName.length = strlen(destName);

  threadLength = sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint16_t) + (sizeof(uint16_t) * destKey.nodeName.length);
  thread = (HFSPlusCatalogThread*) malloc(threadLength);
  thread->reserved = 0;
  destKey.parentID = destRec->folderID;
  thread->parentID = destKey.parentID;  
  thread->nodeName.length = destKey.nodeName.length;
  for(i = 0; i < destKey.nodeName.length; i++) {
    destKey.nodeName.unicode[i] = destName[i];
    thread->nodeName.unicode[i] = destName[i];
  }
  
  destKey.keyLength = sizeof(uint32_t) + sizeof(uint16_t) + (sizeof(uint16_t) * destKey.nodeName.length);
  
  switch(srcRec->recordType) {
    case kHFSPlusFolderRecord:
      thread->recordType = kHFSPlusFolderThreadRecord;
      flipCatalogFolder((HFSPlusCatalogFolder*)srcRec);
      addToBTree(volume->catalogTree, (BTKey*)(&destKey), sizeof(HFSPlusCatalogFolder), (unsigned char *)(srcRec));
      break;
      
    case kHFSPlusFileRecord:
      thread->recordType = kHFSPlusFileThreadRecord;
      flipCatalogFile((HFSPlusCatalogFile*)srcRec);
      addToBTree(volume->catalogTree, (BTKey*)(&destKey), sizeof(HFSPlusCatalogFile), (unsigned char *)(srcRec));
      break;
  }
  
  destKey.nodeName.length = 0;
  destKey.parentID = srcKey.parentID;
  destKey.keyLength = sizeof(destKey.parentID) + sizeof(destKey.nodeName.length);
  
  flipCatalogThread(thread, TRUE);
  addToBTree(volume->catalogTree, (BTKey*)(&destKey), threadLength, (unsigned char *)(thread));
    
  /* adjust valence */
  srcFolderRec->valence--;
  updateCatalog(volume, (HFSPlusCatalogRecord*) srcFolderRec);
  destRec->valence++;
  updateCatalog(volume, (HFSPlusCatalogRecord*) destRec);
  
  free(thread);
  free(destPath);
  free(srcRec);
  free(destRec);
  free(srcFolderRec);
      
  return TRUE;
}

int removeFile(const char* fileName, Volume* volume) {
	HFSPlusCatalogRecord* record;
	HFSPlusCatalogKey key;
	io_func* io;
  HFSPlusCatalogFolder* parentFolder = 0;

	record = getRecordFromPath3(fileName, volume, NULL, &key, TRUE, FALSE, kHFSRootFolderID);
	if(record != NULL) {
		parentFolder = (HFSPlusCatalogFolder*) getRecordByCNID(key.parentID, volume);
		if(parentFolder != NULL) {
			if(parentFolder->recordType != kHFSPlusFolderRecord) {
				ASSERT(FALSE, "parent not folder");
				free(parentFolder);
				return FALSE;
			}
		} else {
			ASSERT(FALSE, "can't find parent");
			return FALSE;
		}

		if(record->recordType == kHFSPlusFileRecord) {
			io = openRawFile(((HFSPlusCatalogFile*)record)->fileID, &((HFSPlusCatalogFile*)record)->dataFork, record, volume);
			allocate((RawFile*)io->data, 0);
			CLOSE(io);

			removeFromBTree(volume->catalogTree, (BTKey*)(&key));
			XAttrList* next;
			XAttrList* attrs = getAllExtendedAttributes(((HFSPlusCatalogFile*)record)->fileID, volume);
			if(attrs != NULL) {
				while(attrs != NULL) {
					next = attrs->next;
					unsetAttribute(volume, ((HFSPlusCatalogFile*)record)->fileID, attrs->name);
					free(attrs->name);
					free(attrs);
					attrs = next;
				}	
			}	


			key.nodeName.length = 0;
			key.parentID = ((HFSPlusCatalogFile*)record)->fileID;
			key.keyLength = sizeof(key.parentID) + sizeof(key.nodeName.length);
			removeFromBTree(volume->catalogTree, (BTKey*)(&key));

			volume->volumeHeader->fileCount--;
		} else {
			if(((HFSPlusCatalogFolder*)record)->valence > 0) {
				free(record);
				free(parentFolder);
				ASSERT(FALSE, "folder not empty");
				return FALSE;
			} else {
				removeFromBTree(volume->catalogTree, (BTKey*)(&key));
				XAttrList* next;
				XAttrList* attrs = getAllExtendedAttributes(((HFSPlusCatalogFolder*)record)->folderID, volume);
				if(attrs != NULL) {
					while(attrs != NULL) {
						next = attrs->next;
						unsetAttribute(volume, ((HFSPlusCatalogFolder*)record)->folderID, attrs->name);
						free(attrs->name);
						free(attrs);
						attrs = next;
					}	
				}	

				key.nodeName.length = 0;
				key.parentID = ((HFSPlusCatalogFolder*)record)->folderID;
				key.keyLength = sizeof(key.parentID) + sizeof(key.nodeName.length);
				removeFromBTree(volume->catalogTree, (BTKey*)(&key));
			}

			parentFolder->folderCount--;
			volume->volumeHeader->folderCount--;
		}
		parentFolder->valence--;
		updateCatalog(volume, (HFSPlusCatalogRecord*) parentFolder);
		updateVolume(volume);

		free(record);
		free(parentFolder);

		return TRUE;
	} else {
		free(parentFolder);
		ASSERT(FALSE, "cannot find record");
		return FALSE;
	}
}

int makeSymlink(const char* pathName, const char* target, Volume* volume) {
	io_func* io;
	HFSPlusCatalogFile* record;

	record = (HFSPlusCatalogFile*) getRecordFromPath3(pathName, volume, NULL, NULL, TRUE, FALSE, kHFSRootFolderID);

	if(!record) {
		newFile(pathName, volume);
		record = (HFSPlusCatalogFile*) getRecordFromPath(pathName, volume, NULL, NULL);
		if(!record) {
			return FALSE;
		}
		record->permissions.fileMode |= S_IFLNK;
		record->userInfo.fileType = kSymLinkFileType;
		record->userInfo.fileCreator = kSymLinkCreator;
		updateCatalog(volume, (HFSPlusCatalogRecord*) record);
	} else {
		if(record->recordType != kHFSPlusFileRecord || (((HFSPlusCatalogFile*)record)->permissions.fileMode & S_IFLNK) != S_IFLNK) {
			free(record);
			return FALSE;
		}
	}

	io = openRawFile(record->fileID, &record->dataFork, (HFSPlusCatalogRecord*) record, volume);
	WRITE(io, 0, strlen(target), (void*) target);
	CLOSE(io);
	free(record);

	return TRUE;
}

HFSCatalogNodeID newFolder(const char* pathName, Volume* volume) {
  HFSPlusCatalogFolder* parentFolder;
  HFSPlusCatalogFolder folder;
  HFSPlusCatalogKey key;
  HFSPlusCatalogThread thread;
  
  uint32_t newFolderID;
  
  int threadLength;
  
  char* path;
  char* name;
  char* curChar;
  char* lastSeparator;
  
  path = strdup(pathName);
  
  curChar = path;
  lastSeparator = NULL;
  
  while((*curChar) != '\0') {
    if((*curChar) == '/')
      lastSeparator = curChar;
    curChar++;
  }
  
  if(lastSeparator == NULL) {
    parentFolder = (HFSPlusCatalogFolder*) getRecordFromPath("/", volume, NULL, NULL);
    name = path;
  } else {
    name = lastSeparator + 1;
    *lastSeparator = '\0';
    parentFolder = (HFSPlusCatalogFolder*) getRecordFromPath(path, volume, NULL, NULL);  
  }

  if(parentFolder == NULL || parentFolder->recordType != kHFSPlusFolderRecord) {
    free(path);
    free(parentFolder);
    return FALSE;
  }
  
  newFolderID = volume->volumeHeader->nextCatalogID++;
  volume->volumeHeader->folderCount++;
  
  folder.recordType = kHFSPlusFolderRecord;
  folder.flags = kHFSHasFolderCountMask;
  folder.valence = 0;
  folder.folderID = newFolderID;
  folder.createDate = UNIX_TO_APPLE_TIME(time(NULL));
  folder.contentModDate = folder.createDate;
  folder.attributeModDate = folder.createDate;
  folder.accessDate = folder.createDate;
  folder.backupDate = folder.createDate;
  folder.permissions.ownerID = parentFolder->permissions.ownerID;
  folder.permissions.groupID = parentFolder->permissions.groupID;
  folder.permissions.adminFlags = 0;
  folder.permissions.ownerFlags = 0;
  folder.permissions.fileMode = S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
  folder.permissions.special.iNodeNum = 0;
  memset(&folder.userInfo, 0, sizeof(folder.userInfo));
  memset(&folder.finderInfo, 0, sizeof(folder.finderInfo));
  folder.textEncoding = 0;
  folder.folderCount = 0;
  
  key.parentID = parentFolder->folderID;
  ASCIIToUnicode(name, &key.nodeName);
  key.keyLength = sizeof(key.parentID) + STR_SIZE(key.nodeName);
  
  thread.recordType = kHFSPlusFolderThreadRecord;
  thread.reserved = 0;
  thread.parentID = parentFolder->folderID;
  ASCIIToUnicode(name, &thread.nodeName);
  threadLength = sizeof(thread.recordType) + sizeof(thread.reserved) + sizeof(thread.parentID) + STR_SIZE(thread.nodeName);
  flipCatalogThread(&thread, TRUE);
  flipCatalogFolder(&folder);
  
  ASSERT(addToBTree(volume->catalogTree, (BTKey*)(&key), sizeof(HFSPlusCatalogFolder), (unsigned char *)(&folder)), "addToBTree");
  key.nodeName.length = 0;
  key.parentID = newFolderID;
  key.keyLength = sizeof(key.parentID) + sizeof(key.nodeName.length);
  ASSERT(addToBTree(volume->catalogTree, (BTKey*)(&key), threadLength, (unsigned char *)(&thread)), "addToBTree");
  
  parentFolder->folderCount++;
  parentFolder->valence++;
  updateCatalog(volume, (HFSPlusCatalogRecord*) parentFolder);
  
  updateVolume(volume);
  
  free(parentFolder);
  free(path);
  
  return newFolderID;
}

HFSCatalogNodeID newFile(const char* pathName, Volume* volume) {
  HFSPlusCatalogFolder* parentFolder;
  HFSPlusCatalogFile file;
  HFSPlusCatalogKey key;
  HFSPlusCatalogThread thread;
  
  uint32_t newFileID;
  
  int threadLength;
  
  char* path;
  char* name;
  char* curChar;
  char* lastSeparator;
  
  path = strdup(pathName);
  
  curChar = path;
  lastSeparator = NULL;
  
  while((*curChar) != '\0') {
    if((*curChar) == '/')
      lastSeparator = curChar;
    curChar++;
  }
  
  if(lastSeparator == NULL) {
    parentFolder = (HFSPlusCatalogFolder*) getRecordFromPath("/", volume, NULL, NULL);
    name = path;
  } else {
    name = lastSeparator + 1;
    *lastSeparator = '\0';
    parentFolder = (HFSPlusCatalogFolder*) getRecordFromPath(path, volume, NULL, NULL);
  }

  if(parentFolder == NULL || parentFolder->recordType != kHFSPlusFolderRecord) {
    free(path);
    free(parentFolder);
    return FALSE;
  }
  
  newFileID = volume->volumeHeader->nextCatalogID++;
  volume->volumeHeader->fileCount++;
  
  file.recordType = kHFSPlusFileRecord;
  file.flags = kHFSThreadExistsMask;
  file.reserved1 = 0;
  file.fileID = newFileID;
  file.createDate = UNIX_TO_APPLE_TIME(time(NULL));
  file.contentModDate = file.createDate;
  file.attributeModDate = file.createDate;
  file.accessDate = file.createDate;
  file.backupDate = file.createDate;
  file.permissions.ownerID = parentFolder->permissions.ownerID;
  file.permissions.groupID = parentFolder->permissions.groupID;
  file.permissions.adminFlags = 0;
  file.permissions.ownerFlags = 0;
  file.permissions.fileMode = S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  file.permissions.special.iNodeNum = 0;
  memset(&file.userInfo, 0, sizeof(file.userInfo));
  memset(&file.finderInfo, 0, sizeof(file.finderInfo));
  file.textEncoding = 0;
  file.reserved2 = 0;
  memset(&file.dataFork, 0, sizeof(file.dataFork));
  memset(&file.resourceFork, 0, sizeof(file.resourceFork));
  
  key.parentID = parentFolder->folderID;
  ASCIIToUnicode(name, &key.nodeName);
  key.keyLength = sizeof(key.parentID) + STR_SIZE(key.nodeName);
  
  thread.recordType = kHFSPlusFileThreadRecord;
  thread.reserved = 0;
  thread.parentID = parentFolder->folderID;
  ASCIIToUnicode(name, &thread.nodeName);
  threadLength = sizeof(thread.recordType) + sizeof(thread.reserved) + sizeof(thread.parentID) + STR_SIZE(thread.nodeName);
  flipCatalogThread(&thread, TRUE);
  flipCatalogFile(&file);
  
  ASSERT(addToBTree(volume->catalogTree, (BTKey*)(&key), sizeof(HFSPlusCatalogFile), (unsigned char *)(&file)), "addToBTree");
  key.nodeName.length = 0;
  key.parentID = newFileID;
  key.keyLength = sizeof(key.parentID) + sizeof(key.nodeName.length);
  ASSERT(addToBTree(volume->catalogTree, (BTKey*)(&key), threadLength, (unsigned char *)(&thread)), "addToBTree");
  
  parentFolder->valence++;
  updateCatalog(volume, (HFSPlusCatalogRecord*) parentFolder);
  
  updateVolume(volume);
  
  free(parentFolder);
  free(path);
  
  return newFileID;
}

int chmodFile(const char* pathName, int mode, Volume* volume) {
  HFSPlusCatalogRecord* record;
   
  record = getRecordFromPath(pathName, volume, NULL, NULL);
  
  if(record == NULL) {
    return FALSE;
  }
  
  if(record->recordType == kHFSPlusFolderRecord) {
    ((HFSPlusCatalogFolder*)record)->permissions.fileMode = (((HFSPlusCatalogFolder*)record)->permissions.fileMode & 0770000) | mode;
  } else if(record->recordType == kHFSPlusFileRecord) {
    ((HFSPlusCatalogFile*)record)->permissions.fileMode = (((HFSPlusCatalogFolder*)record)->permissions.fileMode & 0770000) | mode;
  } else {
    return FALSE;
  }
  
  updateCatalog(volume, record);
  
  free(record);
  
  return TRUE;
}

int attrFile(const char* pathName, const char* flags, Volume* volume) {
  HFSPlusCatalogRecord* record;
  uint16_t flag = 0;
  uint16_t mask = 0;
  uint16_t file_mask = kIsOnDesk|kColor|kIsShared|kHasNoINITs|kHasBeenInited|kHasCustomIcon|kIsStationery|kNameLocked|kHasBundle|kIsInvisible|kIsAlias;
  uint16_t folder_mask = kIsOnDesk|kColor|kHasCustomIcon|kNameLocked|kIsInvisible;

  while (*flags != 0) {
    switch(*flags++) {
    // custom icon
    case 'C':
      flag |= kHasCustomIcon;
    case 'c':
      mask |= kHasCustomIcon;
      break;

    // invisible
    case 'V':
      flag |= kIsInvisible;
    case 'v':
      mask |= kIsInvisible;
      break;

    // inited
    case 'I':
      flag |= kHasBeenInited;
    case 'i':
      mask |= kHasBeenInited;
      break;

    // no INIT resource
    case 'N':
      flag |= kHasNoINITs;
    case 'n':
      mask |= kHasNoINITs;
      break;

    // located on the desktop
    case 'D':
      flag |= kIsOnDesk;
    case 'd':
      mask |= kIsOnDesk;
      break;

    // name locked
    case 'S':
      flag |= kNameLocked;
    case 's':
      mask |= kNameLocked;
      break;

    // stationery pad file
    case 'T':
      flag |= kIsStationery;
    case 't':
      mask |= kIsStationery;
      break;

    // shared
    case 'M':
      flag |= kIsShared;
    case 'm':
      mask |= kIsShared;
      break;

    // alias file
    case 'A':
      flag |= kIsAlias;
    case 'a':
      mask |= kIsAlias;
      break;

    // has bundle
    case 'B':
      flag |= kHasBundle;
    case 'b':
      mask |= kHasBundle;
      break;
    }
  }

  record = getRecordFromPath(pathName, volume, NULL, NULL);

  if(record == NULL) {
    printf("Path '%s' not found.\n", pathName);
    return FALSE;
  }

  if(record->recordType == kHFSPlusFolderRecord) {
    flag &= folder_mask;
    mask &= folder_mask;
    ((HFSPlusCatalogFolder*)record)->userInfo.finderFlags = (((HFSPlusCatalogFolder*)record)->userInfo.finderFlags & (~mask)) | flag;
    printf("%x\n", ((HFSPlusCatalogFolder*)record)->userInfo.finderFlags);
  } else if(record->recordType == kHFSPlusFileRecord) {
    flag &= file_mask;
    mask &= file_mask;
    ((HFSPlusCatalogFile*)record)->userInfo.finderFlags = (((HFSPlusCatalogFile*)record)->userInfo.finderFlags & (~mask)) | flag;
    printf("%x\n", ((HFSPlusCatalogFile*)record)->userInfo.finderFlags);
  } else {
    printf("unknown record type %x\n", record->recordType);
    return FALSE;
  }

  updateCatalog(volume, record);

  free(record);

  return TRUE;
}

int chownFile(const char* pathName, uint32_t owner, uint32_t group, Volume* volume) {
  HFSPlusCatalogRecord* record;
   
  record = getRecordFromPath(pathName, volume, NULL, NULL);
  
  if(record == NULL) {
    return FALSE;
  }
  
  if(record->recordType == kHFSPlusFolderRecord) {
    ((HFSPlusCatalogFolder*)record)->permissions.ownerID = owner;
    ((HFSPlusCatalogFolder*)record)->permissions.groupID = group;
  } else if(record->recordType == kHFSPlusFileRecord) {
    ((HFSPlusCatalogFile*)record)->permissions.ownerID = owner;
    ((HFSPlusCatalogFile*)record)->permissions.groupID = group;
  } else {
    return FALSE;
  }
  
  updateCatalog(volume, record);
  
  free(record);
  
  return TRUE;
}


BTree* openCatalogTree(io_func* file) {
  BTree* btree;

  btree = openBTree(file, &catalogCompare, &catalogKeyRead, &catalogKeyWrite, &catalogKeyPrint, &catalogDataRead);

  if(btree->headerRec->keyCompareType == kHFSCaseFolding) {
    btree->compare = &catalogCompareCS;
  }

  return btree;
}

