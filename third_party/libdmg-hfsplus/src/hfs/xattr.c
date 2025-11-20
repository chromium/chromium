#include <stdlib.h>
#include <string.h>
#include <hfs/hfsplus.h>

static inline void flipAttrData(HFSPlusAttrData* data) {
  FLIPENDIAN(data->recordType);
  FLIPENDIAN(data->size);
}

static inline void flipAttrForkData(HFSPlusAttrForkData* data) {
  FLIPENDIAN(data->recordType);
  flipForkData(&data->theFork);
}

static inline void flipAttrExtents(HFSPlusAttrExtents* data) {
  FLIPENDIAN(data->recordType);
  flipExtentRecord(&data->extents);
}

static int attrCompare(BTKey* vLeft, BTKey* vRight) {
	HFSPlusAttrKey* left;
	HFSPlusAttrKey* right;
	uint16_t i;

	uint16_t cLeft;
	uint16_t cRight;

	left = (HFSPlusAttrKey*) vLeft;
	right =(HFSPlusAttrKey*) vRight;

	if(left->fileID < right->fileID) {
		return -1;
	} else if(left->fileID > right->fileID) {
		return 1;
	} else {
		for(i = 0; i < left->name.length; i++) {
			if(i >= right->name.length) {
				return 1;
			} else {
				cLeft = left->name.unicode[i];
				cRight = right->name.unicode[i];

				if(cLeft < cRight)
					return -1;
				else if(cLeft > cRight)
					return 1;
			}
		}

		if(i < right->name.length) {
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

#define UNICODE_START (sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint16_t))

static BTKey* attrKeyRead(off_t offset, io_func* io) {
	int i;
	HFSPlusAttrKey* key;

	key = (HFSPlusAttrKey*) malloc(sizeof(HFSPlusAttrKey));

	if(!READ(io, offset, UNICODE_START, key))
		return NULL;

	FLIPENDIAN(key->keyLength);
	// uint16_t pad;
	FLIPENDIAN(key->fileID);
	FLIPENDIAN(key->startBlock);
	FLIPENDIAN(key->name.length);

	if(!READ(io, offset + UNICODE_START, key->name.length * sizeof(uint16_t), ((unsigned char *)key) + UNICODE_START))
		return NULL;

	for(i = 0; i < key->name.length; i++) {
		FLIPENDIAN(key->name.unicode[i]);
	}

	return (BTKey*)key;
}

static int attrKeyWrite(off_t offset, BTKey* toWrite, io_func* io) {
	HFSPlusAttrKey* key;
	uint16_t keyLength;
	uint16_t nodeNameLength;
	int i;

	keyLength = toWrite->keyLength + sizeof(uint16_t);
	key = (HFSPlusAttrKey*) malloc(keyLength);
	memcpy(key, toWrite, keyLength);

	nodeNameLength = key->name.length;

	FLIPENDIAN(key->keyLength);
	FLIPENDIAN(key->fileID);
	FLIPENDIAN(key->startBlock);
	FLIPENDIAN(key->name.length);

	for(i = 0; i < nodeNameLength; i++) {
		FLIPENDIAN(key->name.unicode[i]);
	}

	if(!WRITE(io, offset, keyLength, key))
		return FALSE;

	free(key);

	return TRUE;
}

static void attrKeyPrint(BTKey* toPrint) {
	HFSPlusAttrKey* key;

	key = (HFSPlusAttrKey*)toPrint;

	printf("attribute%d:%d:", key->fileID, key->startBlock);
	printUnicode(&key->name);
}

static BTKey* attrDataRead(off_t offset, io_func* io) {
	HFSPlusAttrRecord* record;

	record = (HFSPlusAttrRecord*) malloc(sizeof(HFSPlusAttrRecord));

	if(!READ(io, offset, sizeof(uint32_t), record))
		return NULL;

	FLIPENDIAN(record->recordType);
	switch(record->recordType)
	{
		case kHFSPlusAttrInlineData:
			if(!READ(io, offset, sizeof(HFSPlusAttrData), record))
				return NULL;

			flipAttrData((HFSPlusAttrData*) record);

			record = realloc(record, sizeof(HFSPlusAttrData) + ((HFSPlusAttrData*) record)->size);
			if(!READ(io, offset + sizeof(HFSPlusAttrData), ((HFSPlusAttrData*) record)->size, ((HFSPlusAttrData*) record)->data))
				return NULL;

			break;

		case kHFSPlusAttrForkData:
			if(!READ(io, offset, sizeof(HFSPlusAttrForkData), record))
				return NULL;

			flipAttrForkData((HFSPlusAttrForkData*) record);

			break;

		case kHFSPlusAttrExtents:
			if(!READ(io, offset, sizeof(HFSPlusAttrExtents), record))
				return NULL;

			flipAttrExtents((HFSPlusAttrExtents*) record);

			break;
	}

	return (BTKey*)record;
}

static int updateAttributes(Volume* volume, HFSPlusAttrKey* skey, HFSPlusAttrRecord* srecord) {
	HFSPlusAttrKey key;
	HFSPlusAttrRecord* record;
	int ret, len;
	int exact;

	// Must copy the leading `keyLength` field itself.
	memcpy(&key, skey, skey->keyLength + sizeof(uint16_t));

	record = (HFSPlusAttrRecord*) search(volume->attrTree, (BTKey*)(&key), &exact, NULL, NULL);
	if(exact && record) {
		free(record);
		record = NULL;
		removeFromBTree(volume->attrTree, (BTKey*)(&key));
	}

	switch(srecord->recordType) {
		case kHFSPlusAttrInlineData:
			len = srecord->attrData.size + sizeof(HFSPlusAttrData);
			record = (HFSPlusAttrRecord*) malloc(len);
      			memcpy(record, srecord, len);
			flipAttrData((HFSPlusAttrData*) record);
			ret = addToBTree(volume->attrTree, (BTKey*)(&key), len, (unsigned char *)record);
			free(record);
			break;
		case kHFSPlusAttrForkData:
			record = (HFSPlusAttrRecord*) malloc(sizeof(HFSPlusAttrForkData));
      			memcpy(record, srecord, sizeof(HFSPlusAttrForkData));
			flipAttrForkData((HFSPlusAttrForkData*) record);
			ret = addToBTree(volume->attrTree, (BTKey*)(&key), sizeof(HFSPlusAttrForkData), (unsigned char *)record);
			free(record);
			break;
		case kHFSPlusAttrExtents:
			record = (HFSPlusAttrRecord*) malloc(sizeof(HFSPlusAttrExtents));
      			memcpy(record, srecord, sizeof(HFSPlusAttrExtents));
			flipAttrExtents((HFSPlusAttrExtents*) record);
			ret = addToBTree(volume->attrTree, (BTKey*)(&key), sizeof(HFSPlusAttrExtents), (unsigned char *)record);
			free(record);
			break;
	}

	return ret;
}

size_t getAttribute(Volume* volume, uint32_t fileID, const char* name, uint8_t** data) {
	HFSPlusAttrKey key;
	HFSPlusAttrRecord* record;
	size_t size;
	int exact;

	if(!volume->attrTree)
		return FALSE;

	memset(&key, 0 , sizeof(HFSPlusAttrKey));
	key.fileID = fileID;
	key.startBlock = 0;
	ASCIIToUnicode(name, &key.name);
	key.keyLength = sizeof(HFSPlusAttrKey) - sizeof(uint16_t) - sizeof(HFSUniStr255) + sizeof(key.name.length) + (sizeof(uint16_t) * key.name.length);

	*data = NULL;

	record = (HFSPlusAttrRecord*) search(volume->attrTree, (BTKey*)(&key), &exact, NULL, NULL);

	if(exact == FALSE) {
		if(record)
			free(record);

		return 0;
	}

	switch(record->recordType)
	{
		case kHFSPlusAttrInlineData:
			size = record->attrData.size;
			*data = (uint8_t*) malloc(size);
			memcpy(*data, record->attrData.data, size);
			free(record);
			return size;
		default:
			fprintf(stderr, "unsupported attribute node format\n");
			return 0;
	}
}

int setAttribute(Volume* volume, uint32_t fileID, const char* name, uint8_t* data, size_t size) {
	HFSPlusAttrKey key;
	HFSPlusAttrData* record;
	int ret, exact;

	if(!volume->attrTree)
		return FALSE;

	memset(&key, 0 , sizeof(HFSPlusAttrKey));
	key.fileID = fileID;
	key.startBlock = 0;
	ASCIIToUnicode(name, &key.name);
	key.keyLength = sizeof(HFSPlusAttrKey) - sizeof(uint16_t) - sizeof(HFSUniStr255) + sizeof(key.name.length) + (sizeof(uint16_t) * key.name.length);

	record = (HFSPlusAttrData*) malloc(sizeof(HFSPlusAttrData) + size);
	memset(record, 0, sizeof(HFSPlusAttrData));

	record->recordType = kHFSPlusAttrInlineData;
	record->size = size;
	memcpy(record->data, data, size);

	ret = updateAttributes(volume, &key, (HFSPlusAttrRecord*) record);

	free(record);
	return ret;
}

int unsetAttribute(Volume* volume, uint32_t fileID, const char* name) {
	HFSPlusAttrKey key;

	if(!volume->attrTree)
		return FALSE;

	memset(&key, 0 , sizeof(HFSPlusAttrKey));
	key.fileID = fileID;
	key.startBlock = 0;
	ASCIIToUnicode(name, &key.name);
	key.keyLength = sizeof(HFSPlusAttrKey) - sizeof(uint16_t) - sizeof(HFSUniStr255) + sizeof(key.name.length) + (sizeof(uint16_t) * key.name.length);
	return removeFromBTree(volume->attrTree, (BTKey*)(&key));
}

XAttrList* getAllExtendedAttributes(HFSCatalogNodeID CNID, Volume* volume) {
	BTree* tree;
	HFSPlusAttrKey key;
	HFSPlusAttrRecord* record;
	uint32_t nodeNumber;
	int recordNumber;
	BTNodeDescriptor* descriptor;
	HFSPlusAttrKey* currentKey;
	off_t recordOffset;
	XAttrList* list = NULL;
	XAttrList* lastItem = NULL;
	XAttrList* item = NULL;

	if(!volume->attrTree)
		return NULL;

	memset(&key, 0 , sizeof(HFSPlusAttrKey));
	key.fileID = CNID;
	key.startBlock = 0;
	key.name.length = 0;
	key.keyLength = sizeof(HFSPlusAttrKey) - sizeof(uint16_t) - sizeof(HFSUniStr255) + sizeof(key.name.length) + (sizeof(uint16_t) * key.name.length);

	tree = volume->attrTree;
	record = (HFSPlusAttrRecord*) search(tree, (BTKey*)(&key), NULL, &nodeNumber, &recordNumber);
	if(record == NULL)
		return NULL;

	free(record);
	
	while(nodeNumber != 0) {    
		descriptor = readBTNodeDescriptor(nodeNumber, tree);

		while(recordNumber < descriptor->numRecords) {
			recordOffset = getRecordOffset(recordNumber, nodeNumber, tree);
			currentKey = (HFSPlusAttrKey*) READ_KEY(tree, recordOffset, tree->io);

			if(currentKey->fileID == CNID) {
				item = (XAttrList*) malloc(sizeof(XAttrList));
				item->name = (char*) malloc(currentKey->name.length + 1);
				int i;
				for(i = 0; i < currentKey->name.length; i++) {
					item->name[i] = currentKey->name.unicode[i];
				}
				item->name[currentKey->name.length] = '\0';
				item->next = NULL;

				if(lastItem != NULL) {
					lastItem->next = item;
				} else {
					list = item;
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

BTree* openAttributesTree(io_func* file) {
	return openBTree(file, &attrCompare, &attrKeyRead, &attrKeyWrite, &attrKeyPrint, &attrDataRead);
}

