#include <stdlib.h>
#include <hfs/hfsplus.h>

BTNodeDescriptor* readBTNodeDescriptor(uint32_t num, BTree* tree) {
  BTNodeDescriptor* descriptor;
  
  descriptor = (BTNodeDescriptor*) malloc(sizeof(BTNodeDescriptor));
  
  if(!READ(tree->io, num * tree->headerRec->nodeSize, sizeof(BTNodeDescriptor), descriptor))
    return NULL;
    
  FLIPENDIAN(descriptor->fLink);
  FLIPENDIAN(descriptor->bLink);
  FLIPENDIAN(descriptor->numRecords);
  
  return descriptor;
}

static int writeBTNodeDescriptor(BTNodeDescriptor* descriptor, uint32_t num, BTree* tree) {
  BTNodeDescriptor myDescriptor;
  
  myDescriptor = *descriptor;

  FLIPENDIAN(myDescriptor.fLink);
  FLIPENDIAN(myDescriptor.bLink);
  FLIPENDIAN(myDescriptor.numRecords);
  
  if(!WRITE(tree->io, num * tree->headerRec->nodeSize, sizeof(BTNodeDescriptor), &myDescriptor))
    return FALSE;
    
  return TRUE;
}

BTHeaderRec* readBTHeaderRec(io_func* io) {
  BTHeaderRec* headerRec;
 
  headerRec = (BTHeaderRec*) malloc(sizeof(BTHeaderRec));
  
  if(!READ(io, sizeof(BTNodeDescriptor), sizeof(BTHeaderRec), headerRec)) {
    free(headerRec);
    return NULL;
  }
    
  FLIPENDIAN(headerRec->treeDepth);
  FLIPENDIAN(headerRec->rootNode);
  FLIPENDIAN(headerRec->leafRecords);
  FLIPENDIAN(headerRec->firstLeafNode);
  FLIPENDIAN(headerRec->lastLeafNode);
  FLIPENDIAN(headerRec->nodeSize);
  FLIPENDIAN(headerRec->maxKeyLength);
  FLIPENDIAN(headerRec->totalNodes);
  FLIPENDIAN(headerRec->freeNodes);
  FLIPENDIAN(headerRec->clumpSize);
  FLIPENDIAN(headerRec->attributes);
  
  /*printf("treeDepth: %d\n", headerRec->treeDepth);
  printf("rootNode: %d\n", headerRec->rootNode);
  printf("leafRecords: %d\n", headerRec->leafRecords);
  printf("firstLeafNode: %d\n", headerRec->firstLeafNode);
  printf("lastLeafNode: %d\n", headerRec->lastLeafNode);
  printf("nodeSize: %d\n", headerRec->nodeSize);
  printf("maxKeyLength: %d\n", headerRec->maxKeyLength);
  printf("totalNodes: %d\n", headerRec->totalNodes);
  printf("freeNodes: %d\n", headerRec->freeNodes);
  printf("clumpSize: %d\n", headerRec->clumpSize);
  printf("bTreeType: 0x%x\n", headerRec->btreeType);
  printf("keyCompareType: 0x%x\n", headerRec->keyCompareType);
  printf("attributes: 0x%x\n", headerRec->attributes);
  fflush(stdout);*/
  
  return headerRec;
}

static int writeBTHeaderRec(BTree* tree) {
  BTHeaderRec headerRec;
  
  headerRec = *tree->headerRec;
  
  FLIPENDIAN(headerRec.treeDepth);
  FLIPENDIAN(headerRec.rootNode);
  FLIPENDIAN(headerRec.leafRecords);
  FLIPENDIAN(headerRec.firstLeafNode);
  FLIPENDIAN(headerRec.lastLeafNode);
  FLIPENDIAN(headerRec.nodeSize);
  FLIPENDIAN(headerRec.maxKeyLength);
  FLIPENDIAN(headerRec.totalNodes);
  FLIPENDIAN(headerRec.freeNodes);
  FLIPENDIAN(headerRec.clumpSize);
  FLIPENDIAN(headerRec.attributes);
  
  if(!WRITE(tree->io, sizeof(BTNodeDescriptor), sizeof(BTHeaderRec), &headerRec))
    return FALSE;
    
  return TRUE;
}


BTree* openBTree(io_func* io, compareFunc compare, dataReadFunc keyRead, keyWriteFunc keyWrite, keyPrintFunc keyPrint, dataReadFunc dataRead) {
  BTree* tree;
  
  tree = (BTree*) malloc(sizeof(BTree));
  tree->io = io;
  tree->headerRec = readBTHeaderRec(tree->io);
  
  if(tree->headerRec == NULL) {
    free(tree);
    return NULL;
  }
  
  tree->compare = compare;
  tree->keyRead = keyRead;
  tree->keyWrite = keyWrite;
  tree->keyPrint = keyPrint;
  tree->dataRead = dataRead;
   
  return tree;
}

void closeBTree(BTree* tree) {
  (*tree->io->close)(tree->io);
  free(tree->headerRec);
  free(tree);
}

off_t getRecordOffset(int num, uint32_t nodeNum, BTree* tree) {
  uint16_t offset;
  off_t nodeOffset;
  
  nodeOffset = nodeNum * tree->headerRec->nodeSize;
  
  if(!READ(tree->io, nodeOffset + tree->headerRec->nodeSize - (sizeof(uint16_t) * (num + 1)), sizeof(uint16_t), &offset)) {
    hfs_panic("cannot get record offset!");
  }
  
  FLIPENDIAN(offset);
  
  //printf("%d: %d %d\n", nodeOffset + tree->headerRec->nodeSize - (sizeof(uint16_t) * (num + 1)), nodeOffset + offset, offset);
  
  return (nodeOffset + offset);
}

static off_t getFreeSpace(uint32_t nodeNum, BTNodeDescriptor* descriptor, BTree* tree) {
  uint16_t num;
  off_t nodeOffset;
  off_t freespaceOffsetOffset;
  uint16_t offset;
  off_t freespaceOffset;
  
  num = descriptor->numRecords;
  
  nodeOffset = nodeNum * tree->headerRec->nodeSize;
  freespaceOffsetOffset = nodeOffset + tree->headerRec->nodeSize - (sizeof(uint16_t) * (num + 1));
  
  if(!READ(tree->io, freespaceOffsetOffset, sizeof(uint16_t), &offset)) {
    hfs_panic("cannot get record offset!");
  }
  
  FLIPENDIAN(offset);
  
  freespaceOffset = nodeOffset + offset;
  
  return (freespaceOffsetOffset - freespaceOffset);
}

off_t getNodeNumberFromPointerRecord(off_t offset, io_func* io) {
  uint32_t nodeNum;
  
  if(!READ(io, offset, sizeof(uint32_t), &nodeNum)) {
    hfs_panic("cannot get node number from pointer record!");
  }
  
  FLIPENDIAN(nodeNum);
  
  return nodeNum;
}

static void* searchNode(BTree* tree, uint32_t root, BTKey* searchKey, int *exact, uint32_t *nodeNumber, int *recordNumber) {
  BTNodeDescriptor* descriptor;
  BTKey* key;
  off_t recordOffset;
  off_t recordDataOffset;
  off_t lastRecordDataOffset;
  
  int res;
  int i;
  
  descriptor = readBTNodeDescriptor(root, tree);
   
  if(descriptor == NULL)
    return NULL;
    
  lastRecordDataOffset = 0;
  
  for(i = 0; i < descriptor->numRecords; i++) {
    recordOffset = getRecordOffset(i, root, tree);
    key = READ_KEY(tree, recordOffset, tree->io);
    recordDataOffset = recordOffset + key->keyLength + sizeof(key->keyLength);
    
    res = COMPARE(tree, key, searchKey);
	free(key);
    if(res == 0) {
      if(descriptor->kind == kBTLeafNode) {
        if(nodeNumber != NULL)
          *nodeNumber = root;
          
        if(recordNumber != NULL)
          *recordNumber = i;
        
        if(exact != NULL)
          *exact = TRUE;
                 
        free(descriptor);

        return READ_DATA(tree, recordDataOffset, tree->io);
      } else {
      
        free(descriptor);
        return searchNode(tree, getNodeNumberFromPointerRecord(recordDataOffset, tree->io), searchKey, exact, nodeNumber, recordNumber);
      }
    } else if(res > 0) {
      break;
    }

    lastRecordDataOffset = recordDataOffset;
  }

  if(lastRecordDataOffset == 0) {
    hfs_panic("BTree inconsistent!");
    return NULL;
  }
  
  if(descriptor->kind == kBTLeafNode) {        
    if(nodeNumber != NULL)
      *nodeNumber = root;
      
    if(recordNumber != NULL)
      *recordNumber = i;
    
    if(exact != NULL)
      *exact = FALSE;
      
    free(descriptor);
    return READ_DATA(tree, lastRecordDataOffset, tree->io);
  } else if(descriptor->kind == kBTIndexNode) {
  
    free(descriptor);
    return searchNode(tree, getNodeNumberFromPointerRecord(lastRecordDataOffset, tree->io), searchKey, exact, nodeNumber, recordNumber);
  } else {
    if(nodeNumber != NULL)
      *nodeNumber = root;
      
    if(recordNumber != NULL)
      *recordNumber = i;
    
    if(exact != NULL)
      *exact = FALSE;

    free(descriptor);
    return NULL;
  }
}

void* search(BTree* tree, BTKey* searchKey, int *exact, uint32_t *nodeNumber, int *recordNumber) {
  return searchNode(tree, tree->headerRec->rootNode, searchKey, exact, nodeNumber, recordNumber);
}

static uint32_t linearCheck(uint32_t* heightTable, unsigned char* map, BTree* tree, uint32_t *errCount) {
  uint8_t i;
  uint8_t j;
  uint32_t node;
  
  uint32_t count;
  uint32_t leafRecords;
  
  BTNodeDescriptor* descriptor;
  
  uint32_t prevNode;
  
  off_t recordOffset;
  BTKey* key;
  BTKey* previousKey;
  
  count = 0;
  
  leafRecords = 0;
  
  for(i = 0; i <= tree->headerRec->treeDepth; i++) {
    node = heightTable[i];
    if(node != 0) {
      descriptor = readBTNodeDescriptor(node, tree);
      while(descriptor->bLink != 0) {
        node = descriptor->bLink;
        free(descriptor);
        descriptor = readBTNodeDescriptor(node, tree);
      }
      free(descriptor);
      
      prevNode = 0;
      previousKey = NULL;
      
      if(i == 1) {
        if(node != tree->headerRec->firstLeafNode) {
          printf("BTREE CONSISTENCY ERROR: First leaf node (%d) is not correct. Should be: %d\n", tree->headerRec->firstLeafNode, node);
          (*errCount)++;
        }
      }
      
      while(node != 0) {
        descriptor = readBTNodeDescriptor(node, tree);
        if(descriptor->bLink != prevNode) {
          printf("BTREE CONSISTENCY ERROR: Node %d is not properly linked with previous node %d\n", node, prevNode);
          (*errCount)++;
        }
        
        if(descriptor->height != i) {
          printf("BTREE CONSISTENCY ERROR: Node %d (%d) is not properly linked with nodes of the same height %d\n", node, descriptor->height, i);
          (*errCount)++;
        }
        
        if((map[node / 8] & (1 << (7 - (node % 8)))) == 0) {
          printf("BTREE CONSISTENCY ERROR: Node %d not marked allocated\n", node);
          (*errCount)++;
        }
        
        /*if(descriptor->kind == kBTIndexNode && descriptor->numRecords < 2) {
          printf("BTREE CONSISTENCY ERROR: Node %d does not have at least two children\n", node);
          (*errCount)++;
        }*/
        
        for(j = 0; j < descriptor->numRecords; j++) {
          recordOffset = getRecordOffset(j, node, tree);
          key = READ_KEY(tree, recordOffset, tree->io);
          if(previousKey != NULL) {
            if(COMPARE(tree, key, previousKey) < 0) {
              printf("BTREE CONSISTENCY ERROR: Ordering not preserved during linear check for record %d node %d: ", j, node);
              (*errCount)++;
              tree->keyPrint(previousKey);
              printf(" < ");
              tree->keyPrint(key);
              printf("\n");
            }
            free(previousKey);
          }
          
          if(i == 1) {
            leafRecords++;
          }
          previousKey = key;
        }
        
        count++;
        
        prevNode = node;
        node = descriptor->fLink;
        free(descriptor);
      }
      
      if(i == 1) {
        if(prevNode != tree->headerRec->lastLeafNode) {
          printf("BTREE CONSISTENCY ERROR: Last leaf node (%d) is not correct. Should be: %d\n", tree->headerRec->lastLeafNode, node);
          (*errCount)++;
        }
      }
      
      free(previousKey);
    }
  }
  
  if(leafRecords != tree->headerRec->leafRecords) {
    printf("BTREE CONSISTENCY ERROR: leafRecords (%d) is not correct. Should be: %d\n", tree->headerRec->leafRecords, leafRecords);
    (*errCount)++;
  }
  
  return count;
}

static uint32_t traverseNode(uint32_t nodeNum, BTree* tree, unsigned char* map, int parentHeight, BTKey** firstKey, BTKey** lastKey,
                                  uint32_t* heightTable, uint32_t* errCount, int displayTree) {
  BTNodeDescriptor* descriptor;
  BTKey* key;
  BTKey* previousKey;
  BTKey* retFirstKey;
  BTKey* retLastKey;
  int i, j;
  
  int res;
  
  uint32_t count;
  
  off_t recordOffset;
  off_t recordDataOffset;
  
  off_t lastrecordDataOffset;
  
  descriptor = readBTNodeDescriptor(nodeNum, tree);
  
  previousKey = NULL;
  
  count = 1;
  
  if(displayTree) {
    for(i = 0; i < descriptor->height; i++) {
      printf("  ");
    }
  }
  
  if(descriptor->kind == kBTLeafNode) {
    if(displayTree)
      printf("Leaf %d: %d", nodeNum, descriptor->numRecords);
    
    if(descriptor->height != 1) {
      printf("BTREE CONSISTENCY ERROR: Leaf node %d does not have height 1\n", nodeNum); fflush(stdout);
      (*errCount)++;
    }    
  } else if(descriptor->kind == kBTIndexNode) {
    if(displayTree)
      printf("Index %d: %d", nodeNum, descriptor->numRecords);

  } else {
    printf("BTREE CONSISTENCY ERROR: Unexpected node %d has kind %d\n", nodeNum, descriptor->kind); fflush(stdout);
    (*errCount)++;
  }
  
  if(displayTree) {
    printf("\n"); fflush(stdout);
  }
  
  if((map[nodeNum / 8] & (1 << (7 - (nodeNum % 8)))) == 0) {
    printf("BTREE CONSISTENCY ERROR: Node %d not marked allocated\n", nodeNum); fflush(stdout);
    (*errCount)++;
  }
  
  if(nodeNum == tree->headerRec->rootNode) {
    if(descriptor->height != tree->headerRec->treeDepth) {
      printf("BTREE CONSISTENCY ERROR: Root node %d (%d) does not have the proper height (%d)\n", nodeNum,
                    descriptor->height, tree->headerRec->treeDepth); fflush(stdout);
      (*errCount)++;
    }
  } else {
    if(descriptor->height != (parentHeight - 1)) {
      printf("BTREE CONSISTENCY ERROR: Node %d does not have the proper height\n", nodeNum); fflush(stdout);
      (*errCount)++;
    }
  }
  
  /*if(descriptor->kind == kBTIndexNode && descriptor->numRecords < 2) {
    printf("BTREE CONSISTENCY ERROR: Node %d does not have at least two children\n", nodeNum);
    (*errCount)++;
  }*/
  
  heightTable[descriptor->height] = nodeNum;
  lastrecordDataOffset = 0;
   
  for(i = 0; i < descriptor->numRecords; i++) {
    recordOffset = getRecordOffset(i, nodeNum, tree);
    key = READ_KEY(tree, recordOffset, tree->io);
    recordDataOffset = recordOffset + key->keyLength + sizeof(key->keyLength);
    
    if((recordDataOffset - (nodeNum * tree->headerRec->nodeSize)) > (tree->headerRec->nodeSize - (sizeof(uint16_t) * (descriptor->numRecords + 1)))) {
      printf("BTREE CONSISTENCY ERROR: Record data extends past offsets in node %d record %d\n", nodeNum, i); fflush(stdout);
      (*errCount)++;
    }

    if(i == 0) {
      *firstKey = READ_KEY(tree, recordOffset, tree->io);
    }
    
    if(i == (descriptor->numRecords - 1)) {
      *lastKey = READ_KEY(tree, recordOffset, tree->io);
    }
    
    if(previousKey != NULL) {
      res = COMPARE(tree, key, previousKey);
      if(res < 0) {
        printf("BTREE CONSISTENCY ERROR(traverse): Ordering between records within node not preserved in record %d node %d for ", i, nodeNum);
        (*errCount)++;
        tree->keyPrint(previousKey);
        printf(" < ");
        tree->keyPrint(key);
        printf("\n"); fflush(stdout);
      }
      free(previousKey);
      previousKey = NULL;
    }
    
    if(displayTree) {
      for(j = 0; j < (descriptor->height - 1); j++) {
        printf("  ");
      }
      tree->keyPrint(key);
      printf("\n");
    }
    
    if(descriptor->kind == kBTIndexNode) {
      count += traverseNode(getNodeNumberFromPointerRecord(recordDataOffset, tree->io),
                              tree, map, descriptor->height, &retFirstKey, &retLastKey, heightTable, errCount, displayTree);
                              
      if(COMPARE(tree, retFirstKey, key) != 0) {
        printf("BTREE CONSISTENCY ERROR: Index node key does not match first key in record %d node %d\n", i, nodeNum); fflush(stdout);
        (*errCount)++;
      }
      if(COMPARE(tree, retLastKey, key) < 0) {
        printf("BTREE CONSISTENCY ERROR: Last key is less than the index node key in record %d node %d\n", i, nodeNum); fflush(stdout);
        (*errCount)++;
      }
      free(retFirstKey);
      free(key);
      previousKey = retLastKey;
    } else {
      previousKey = key;
    }
    
    if(recordOffset < lastrecordDataOffset) {
      printf("BTREE CONSISTENCY ERROR: Record offsets are not in order in node %d starting at record %d\n", nodeNum, i); fflush(stdout);
      (*errCount)++;
    }
    
    lastrecordDataOffset = recordDataOffset;
  }
  
  if(previousKey != NULL) free(previousKey);

  free(descriptor);
  
  return count;
  
}

static unsigned char* mapNodes(BTree* tree, uint32_t* numMapNodes, uint32_t* errCount) {
  unsigned char *map;
  
  BTNodeDescriptor* descriptor;
  
  unsigned char byte;
  
  uint32_t totalNodes;
  uint32_t freeNodes;
  
  uint32_t byteNumber;
  uint32_t byteTracker;
  
  uint32_t mapNode;
  
  off_t mapRecordStart;
  off_t mapRecordLength;
  
  int i;

  map = (unsigned char *)malloc(tree->headerRec->totalNodes/8 + 1);

  byteTracker = 0;
  freeNodes = 0;
  totalNodes = 0;
  
  mapRecordStart = getRecordOffset(2, 0, tree);
  mapRecordLength = tree->headerRec->nodeSize - 256;
  byteNumber = 0;
  mapNode = 0;
  
  *numMapNodes = 0;
   
  while(TRUE) {
    while(byteNumber < mapRecordLength) {
      READ(tree->io, mapRecordStart + byteNumber, 1, &byte);
      map[byteTracker] = byte;
      byteTracker++;
      byteNumber++;
      for(i = 0; i < 8; i++) {
        if((byte & (1 << (7 - i))) == 0) {
          freeNodes++;
        }
        totalNodes++;
        
        if(totalNodes == tree->headerRec->totalNodes)
          goto done;
      }
    }
        
    descriptor = readBTNodeDescriptor(mapNode, tree);
    mapNode = descriptor->fLink;
    free(descriptor);
    
    (*numMapNodes)++;
    
    if(mapNode == 0) {
      printf("BTREE CONSISTENCY ERROR: Not enough map nodes allocated! Allocated for: %d, needed: %d\n", totalNodes, tree->headerRec->totalNodes);
      (*errCount)++;
      break;
    }
    
    mapRecordStart = mapNode * tree->headerRec->nodeSize + 14;
    mapRecordLength = tree->headerRec->nodeSize - 20;
    byteNumber = 0;
  }
  
  done:
     
  if(freeNodes != tree->headerRec->freeNodes) {
    printf("BTREE CONSISTENCY ERROR: Free nodes %d differ from actually allocated %d\n", tree->headerRec->freeNodes, freeNodes);
    (*errCount)++;
  }
  
  return map;
}

int debugBTree(BTree* tree, int displayTree) {
  unsigned char* map;
  uint32_t *heightTable;
  BTKey* retFirstKey;
  BTKey* retLastKey;
  
  uint32_t numMapNodes;
  uint32_t traverseCount;
  uint32_t linearCount;
  uint32_t errorCount;
  
  uint8_t i;
  
  errorCount = 0;
  
  printf("Mapping nodes...\n"); fflush(stdout);
  map = mapNodes(tree, &numMapNodes, &errorCount);
  
  printf("Initializing height table...\n"); fflush(stdout);
  heightTable = (uint32_t*) malloc(sizeof(uint32_t) * (tree->headerRec->treeDepth + 1));
  for(i = 0; i <= tree->headerRec->treeDepth; i++) {
    heightTable[i] = 0;
  }

  if(tree->headerRec->rootNode == 0) {
    if(tree->headerRec->firstLeafNode == 0 && tree->headerRec->lastLeafNode == 0) {
      traverseCount = 0;
      linearCount = 0;
    } else {
      printf("BTREE CONSISTENCY ERROR: First leaf node (%d) and last leaf node (%d) inconsistent with empty BTree\n",
                tree->headerRec->firstLeafNode, tree->headerRec->lastLeafNode);
      
      // Try to see if we can get a linear count
      if(tree->headerRec->firstLeafNode != 0)
        heightTable[1] = tree->headerRec->firstLeafNode;
      else
        heightTable[1] = tree->headerRec->lastLeafNode;
        
      linearCount = linearCheck(heightTable, map, tree, &errorCount);
    }
  } else {
    printf("Performing tree traversal...\n"); fflush(stdout);
    traverseCount = traverseNode(tree->headerRec->rootNode, tree, map, 0, &retFirstKey, &retLastKey, heightTable, &errorCount, displayTree);

    free(retFirstKey);
    free(retLastKey);

    printf("Performing linear traversal...\n"); fflush(stdout);
    linearCount = linearCheck(heightTable, map, tree, &errorCount);
  }
  
  printf("Total traverse nodes: %d\n", traverseCount); fflush(stdout);
  printf("Total linear nodes: %d\n", linearCount); fflush(stdout);
  printf("Error count: %d\n", errorCount); fflush(stdout);

  if(traverseCount != linearCount) {
    printf("BTREE CONSISTENCY ERROR: Linear count and traverse count are inconsistent\n");
  }
  
  if(traverseCount != (tree->headerRec->totalNodes - tree->headerRec->freeNodes - numMapNodes - 1)) {
    printf("BTREE CONSISTENCY ERROR: Free nodes and total nodes (%d) and traverse count are inconsistent\n",
      tree->headerRec->totalNodes - tree->headerRec->freeNodes);
  }

  free(heightTable);
  free(map); 
  
  return errorCount;
}

static uint32_t findFree(BTree* tree) {
  unsigned char byte;
  uint32_t byteNumber;
  uint32_t mapNode;
  
  BTNodeDescriptor* descriptor;
  
  off_t mapRecordStart;
  off_t mapRecordLength;
  
  int i;

  mapRecordStart = getRecordOffset(2, 0, tree);
  mapRecordLength = tree->headerRec->nodeSize - 256;
  mapNode = 0;
  byteNumber = 0;
  
  while(TRUE) {
    while(byteNumber < mapRecordLength) {
      READ(tree->io, mapRecordStart + byteNumber, 1, &byte);
      if(byte != 0xFF) {
        for(i = 0; i < 8; i++) {
          if((byte & (1 << (7 - i))) == 0) {
            byte |= (1 << (7 - i));
            tree->headerRec->freeNodes--;
            ASSERT(writeBTHeaderRec(tree), "writeBTHeaderRec");
            ASSERT(WRITE(tree->io, mapRecordStart + byteNumber, 1, &byte), "WRITE");
            return ((byteNumber * 8) + i);
          }
        }
      }
      byteNumber++;
    }
    
    descriptor = readBTNodeDescriptor(mapNode, tree);
    mapNode = descriptor->fLink;
    free(descriptor);
    
    if(mapNode == 0) {
      return 0;
    }
    
    mapRecordStart = mapNode * tree->headerRec->nodeSize + 14;
    mapRecordLength = tree->headerRec->nodeSize - 20;
    byteNumber = 0;
  }
}

static int markUsed(uint32_t node, BTree* tree) {
  BTNodeDescriptor* descriptor;
  uint32_t mapNode;
  uint32_t byteNumber;
  
  unsigned char byte;
  
  mapNode = 0;
  byteNumber = node / 8;

  if(byteNumber >= (tree->headerRec->nodeSize - 256)) {
    while(TRUE) {
      descriptor = readBTNodeDescriptor(mapNode, tree);
      mapNode = descriptor->fLink;
      free(descriptor);
      
      if(byteNumber > (tree->headerRec->nodeSize - 20)) {
        byteNumber -= tree->headerRec->nodeSize - 20;
      } else {
        break;
      }
    }
  }
  
  ASSERT(READ(tree->io, mapNode * tree->headerRec->nodeSize + 14 + byteNumber, 1, &byte), "READ");
  byte |= (1 << (7 - (node % 8)));
  ASSERT(WRITE(tree->io, mapNode * tree->headerRec->nodeSize + 14 + byteNumber, 1, &byte), "WRITE");
  
  return TRUE;
}

static int growBTree(BTree* tree) {
  int i;
  unsigned char* buffer;
  uint16_t offset;
  
  uint32_t byteNumber;
  uint32_t mapNode;
  int increasedNodes;
  
  off_t newNodeOffset;
  uint32_t newNodesStart;
  
  BTNodeDescriptor* descriptor;
  BTNodeDescriptor newDescriptor;
  
  allocate((RawFile*)(tree->io->data), ((RawFile*)(tree->io->data))->forkData->logicalSize + ((RawFile*)(tree->io->data))->forkData->clumpSize);
  increasedNodes = (((RawFile*)(tree->io->data))->forkData->logicalSize/tree->headerRec->nodeSize) - tree->headerRec->totalNodes;
  
  newNodesStart = tree->headerRec->totalNodes / tree->headerRec->nodeSize;
  
  tree->headerRec->freeNodes += increasedNodes;
  tree->headerRec->totalNodes += increasedNodes;
  
  byteNumber = tree->headerRec->totalNodes / 8;
  mapNode = 0;

  buffer = (unsigned char*) malloc(tree->headerRec->nodeSize - 20);
  for(i = 0; i < (tree->headerRec->nodeSize - 20); i++) {
    buffer[i] = 0;
  }
  
  if(byteNumber < (tree->headerRec->nodeSize - 256)) {
    ASSERT(writeBTHeaderRec(tree), "writeBTHeaderREc");
    free(buffer);
    return TRUE;
  } else {
    byteNumber -= tree->headerRec->nodeSize - 256;
    
    while(TRUE) {
      descriptor = readBTNodeDescriptor(mapNode, tree);

      if(descriptor->fLink == 0) {
        descriptor->fLink = newNodesStart;
        ASSERT(writeBTNodeDescriptor(descriptor, mapNode, tree), "writeBTNodeDescriptor");
        
        newDescriptor.fLink = 0;
        newDescriptor.bLink = 0;
        newDescriptor.kind = kBTMapNode;
        newDescriptor.height = 0;
        newDescriptor.numRecords = 1;
        newDescriptor.reserved = 0;
        ASSERT(writeBTNodeDescriptor(&newDescriptor, descriptor->fLink, tree), "writeBTNodeDescriptor");
        
        newNodeOffset = descriptor->fLink * tree->headerRec->nodeSize;
        
        ASSERT(WRITE(tree->io, newNodeOffset + 14, tree->headerRec->nodeSize - 20, buffer), "WRITE");
        offset = 14;
        FLIPENDIAN(offset);
        ASSERT(WRITE(tree->io, newNodeOffset + tree->headerRec->nodeSize - 2, sizeof(offset), &offset), "WRITE");
        offset = 14 + tree->headerRec->nodeSize - 20;
        FLIPENDIAN(offset);
        ASSERT(WRITE(tree->io, newNodeOffset + tree->headerRec->nodeSize - 4, sizeof(offset), &offset), "WRITE");
        
        // mark the map node as being used
        ASSERT(markUsed(newNodesStart, tree), "markUsed");
        tree->headerRec->freeNodes--;
        newNodesStart++;
      }
      mapNode = descriptor->fLink;
      
      if(byteNumber > (tree->headerRec->nodeSize - 20)) {
        byteNumber -= tree->headerRec->nodeSize - 20;
      } else {
        free(buffer);
        
        ASSERT(writeBTHeaderRec(tree), "writeBTHeaderRec");
        return TRUE;
      }
    }
  }
  
  return FALSE;
}

static uint32_t getNewNode(BTree* tree) {
  if(tree->headerRec->freeNodes == 0) {
    growBTree(tree);
  }
  
  return findFree(tree);
}

static uint32_t removeNode(BTree* tree, uint32_t node) {
  unsigned char byte;
  off_t mapRecordStart;
  uint32_t mapNode;
  size_t mapRecordLength;
  BTNodeDescriptor *descriptor;
  BTNodeDescriptor *oDescriptor;
  
  mapRecordStart = getRecordOffset(2, 0, tree);
  mapRecordLength = tree->headerRec->nodeSize - 256;
  mapNode = 0;
    
  while((node / 8) >= mapRecordLength) {
    descriptor = readBTNodeDescriptor(mapNode, tree);
    mapNode = descriptor->fLink;
    free(descriptor);
    
    if(mapNode == 0) {
      hfs_panic("Cannot remove node because I can't map it!");
      return 0;
    }
    
    mapRecordStart = mapNode * tree->headerRec->nodeSize + 14;
    mapRecordLength = tree->headerRec->nodeSize - 20;
    node -= mapRecordLength * 8;
  }
  
  READ(tree->io, mapRecordStart + (node / 8), 1, &byte);
  
  byte &= ~(1 << (7 - (node % 8)));
  
  tree->headerRec->freeNodes++;
  
  descriptor = readBTNodeDescriptor(node, tree);
  
  if(tree->headerRec->firstLeafNode == node) {
    tree->headerRec->firstLeafNode = descriptor->fLink;
  }
  
  if(tree->headerRec->lastLeafNode == node) {
    tree->headerRec->lastLeafNode = descriptor->bLink;
  }
  
  if(node == tree->headerRec->rootNode) {
    tree->headerRec->rootNode = 0;
  }
  
  if(descriptor->bLink != 0) {
    oDescriptor = readBTNodeDescriptor(descriptor->bLink, tree);
    oDescriptor->fLink = descriptor->fLink;
    ASSERT(writeBTNodeDescriptor(oDescriptor, descriptor->bLink, tree), "writeBTNodeDescriptor");
	free(oDescriptor);
  }
  
  if(descriptor->fLink != 0) {
    oDescriptor = readBTNodeDescriptor(descriptor->fLink, tree);
    oDescriptor->bLink = descriptor->bLink;
    ASSERT(writeBTNodeDescriptor(oDescriptor, descriptor->fLink, tree), "writeBTNodeDescriptor");
	free(oDescriptor);
  }
  
  free(descriptor);
  
  ASSERT(WRITE(tree->io, mapRecordStart + (node / 8), 1, &byte), "WRITE");
  ASSERT(writeBTHeaderRec(tree), "writeBTHeaderRec");
  
  return TRUE;
}

static uint32_t splitNode(uint32_t node, BTNodeDescriptor* descriptor, BTree* tree) {
  int nodesToMove;
  
  int i;
  off_t internalOffset;
  
  BTNodeDescriptor* fDescriptor;
  
  BTNodeDescriptor newDescriptor;
  uint32_t newNodeNum;
  off_t newNodeOffset;
  
  off_t toMove;
  size_t toMoveLength;
  unsigned char *buffer;
  
  off_t offsetsToMove;
  size_t offsetsToMoveLength;
  uint16_t *offsetsBuffer;
    
  nodesToMove = descriptor->numRecords - (descriptor->numRecords/2);
   
  toMove = getRecordOffset(descriptor->numRecords/2, node, tree);
  toMoveLength = getRecordOffset(descriptor->numRecords, node, tree) - toMove;
  buffer = (unsigned char *)malloc(toMoveLength);
  ASSERT(READ(tree->io, toMove, toMoveLength, buffer), "READ");
  
  offsetsToMove = (node * tree->headerRec->nodeSize) + tree->headerRec->nodeSize - (sizeof(uint16_t) * (descriptor->numRecords + 1));
  offsetsToMoveLength = sizeof(uint16_t) * (nodesToMove + 1);
  offsetsBuffer = (uint16_t *)malloc(offsetsToMoveLength);
  ASSERT(READ(tree->io, offsetsToMove, offsetsToMoveLength, offsetsBuffer), "READ");
  
  for(i = 0; i < (nodesToMove + 1); i++) {
    FLIPENDIAN(offsetsBuffer[i]);
  }
  
  internalOffset = offsetsBuffer[nodesToMove] - 14;
  
  for(i = 0; i < (nodesToMove + 1); i++) {
    offsetsBuffer[i] -= internalOffset;
    FLIPENDIAN(offsetsBuffer[i]);
  }
  
  newNodeNum = getNewNode(tree);
  newNodeOffset = newNodeNum * tree->headerRec->nodeSize;
  
  newDescriptor.fLink = descriptor->fLink;
  newDescriptor.bLink = node;
  newDescriptor.kind = descriptor->kind;
  newDescriptor.height = descriptor->height;
  newDescriptor.numRecords = nodesToMove;
  newDescriptor.reserved = 0;
  ASSERT(writeBTNodeDescriptor(&newDescriptor, newNodeNum, tree), "writeBTNodeDescriptor");
  
  if(newDescriptor.fLink != 0) {    
    fDescriptor = readBTNodeDescriptor(newDescriptor.fLink, tree);
    fDescriptor->bLink = newNodeNum;
    ASSERT(writeBTNodeDescriptor(fDescriptor, newDescriptor.fLink, tree), "writeBTNodeDescriptor");
    free(fDescriptor);
  }
  
  descriptor->fLink = newNodeNum;
  descriptor->numRecords = descriptor->numRecords/2;
  ASSERT(writeBTNodeDescriptor(descriptor, node, tree), "writeBTNodeDescriptor");
  
  ASSERT(WRITE(tree->io, newNodeOffset + 14, toMoveLength, buffer), "WRITE");
  ASSERT(WRITE(tree->io, newNodeOffset + tree->headerRec->nodeSize - (sizeof(uint16_t) * (nodesToMove + 1)), offsetsToMoveLength, offsetsBuffer), "WRITE");
  
  // The offset for the existing descriptor's new numRecords will happen to be where the old data was, which is now where the free space starts
  // So we don't have to manually set the free space offset
  
  free(buffer);
  free(offsetsBuffer);
  
  if(descriptor->kind == kBTLeafNode && node == tree->headerRec->lastLeafNode) {
    tree->headerRec->lastLeafNode = newNodeNum;
    ASSERT(writeBTHeaderRec(tree), "writeBTHeaderRec");
  }
  
  return newNodeNum;
}

static int moveRecordsDown(BTree* tree, BTNodeDescriptor* descriptor, int record, uint32_t node, int length, int moveOffsets) {
  off_t firstRecordStart;
  off_t lastRecordEnd;
  unsigned char* records;
  
  off_t firstOffsetStart;
  off_t lastOffsetEnd;
  uint16_t* offsets;
  
  int i; 
  
  firstRecordStart = getRecordOffset(record, node, tree);
  lastRecordEnd = getRecordOffset(descriptor->numRecords, node, tree);
  
  records = (unsigned char*)malloc(lastRecordEnd - firstRecordStart);
   
  ASSERT(READ(tree->io, firstRecordStart, lastRecordEnd - firstRecordStart, records), "READ");
  firstOffsetStart = (node * tree->headerRec->nodeSize) + tree->headerRec->nodeSize - (sizeof(uint16_t) * (descriptor->numRecords + 1));
  lastOffsetEnd = (node * tree->headerRec->nodeSize) + tree->headerRec->nodeSize - (sizeof(uint16_t) * record);
  
  offsets = (uint16_t*)malloc(lastOffsetEnd - firstOffsetStart);
  ASSERT(READ(tree->io, firstOffsetStart, lastOffsetEnd - firstOffsetStart, offsets), "READ");
  
  for(i = 0; i < (lastOffsetEnd - firstOffsetStart)/sizeof(uint16_t); i++) {
    FLIPENDIAN(offsets[i]);
    offsets[i] += length;
    FLIPENDIAN(offsets[i]);
  }
  
  ASSERT(WRITE(tree->io, firstRecordStart + length, lastRecordEnd - firstRecordStart, records), "WRITE");
  
  if(moveOffsets > 0) {
    ASSERT(WRITE(tree->io, firstOffsetStart - sizeof(uint16_t), lastOffsetEnd - firstOffsetStart, offsets), "WRITE");
  } else if(moveOffsets < 0) {
    ASSERT(WRITE(tree->io, firstOffsetStart + sizeof(uint16_t), lastOffsetEnd - firstOffsetStart, offsets), "WRITE");
  } else {
    ASSERT(WRITE(tree->io, firstOffsetStart, lastOffsetEnd - firstOffsetStart, offsets), "WRITE");
  }
  
  free(records);
  free(offsets);
  
  return TRUE;
}

static int doAddRecord(BTree* tree, uint32_t root, BTKey* searchKey, size_t length, unsigned char* content) {
  BTNodeDescriptor* descriptor;
  BTKey* key;
  off_t recordOffset;
  off_t recordDataOffset;
  off_t lastRecordDataOffset;

  uint16_t offset;

  int res;
  int i;
  
  descriptor = readBTNodeDescriptor(root, tree);
   
  if(descriptor == NULL)
    return FALSE;
    
  lastRecordDataOffset = 0;
  
  for(i = 0; i < descriptor->numRecords; i++) {
    recordOffset = getRecordOffset(i, root, tree);
    key = READ_KEY(tree, recordOffset, tree->io);
    recordDataOffset = recordOffset + key->keyLength + sizeof(key->keyLength);
    
    res = COMPARE(tree, key, searchKey);
    if(res == 0) {
      free(key);
      free(descriptor);
      
      return FALSE;
    } else if(res > 0) {
      free(key);
      
      break;
    }
    
    free(key);
    
    lastRecordDataOffset = recordDataOffset;
  }
   
  if(i != descriptor->numRecords) {
    // first, move everyone else down
    
    moveRecordsDown(tree, descriptor, i, root, sizeof(searchKey->keyLength) + searchKey->keyLength + length, 1);
        
    // then insert ourself
    ASSERT(WRITE_KEY(tree, recordOffset, searchKey, tree->io), "WRITE_KEY");
    ASSERT(WRITE(tree->io, recordOffset + sizeof(searchKey->keyLength) + searchKey->keyLength, length, content), "WRITE");
    
    offset = recordOffset - (root * tree->headerRec->nodeSize);
    FLIPENDIAN(offset);
    ASSERT(WRITE(tree->io, (root * tree->headerRec->nodeSize) + tree->headerRec->nodeSize - (sizeof(uint16_t) * (i + 1)),
                  sizeof(uint16_t), &offset), "WRITE");
  } else {
    // just insert ourself at the end
    recordOffset = getRecordOffset(i, root, tree);
    ASSERT(WRITE_KEY(tree, recordOffset, searchKey, tree->io), "WRITE_KEY");
    ASSERT(WRITE(tree->io, recordOffset + sizeof(uint16_t) + searchKey->keyLength, length, content), "WRITE");
    
    // write the new free offset
    offset = (recordOffset + sizeof(searchKey->keyLength) + searchKey->keyLength + length) - (root * tree->headerRec->nodeSize);
    FLIPENDIAN(offset);
    ASSERT(WRITE(tree->io, (root * tree->headerRec->nodeSize) + tree->headerRec->nodeSize - (sizeof(uint16_t) * (descriptor->numRecords + 2)),
                  sizeof(uint16_t), &offset), "WRITE");
  }
  
  descriptor->numRecords++;
  
  if(descriptor->height == 1) {
    tree->headerRec->leafRecords++;
  }

  ASSERT(writeBTNodeDescriptor(descriptor, root, tree), "writeBTNodeDescriptor");
  ASSERT(writeBTHeaderRec(tree), "writeBTHeaderRec");
  
  free(descriptor);
  return TRUE;
}

static int addRecord(BTree* tree, uint32_t root, BTKey* searchKey, size_t length, unsigned char* content, int* callAgain) {
  BTNodeDescriptor* descriptor;
  BTKey* key;
  off_t recordOffset;
  off_t recordDataOffset;
  off_t lastRecordDataOffset;
  
  size_t freeSpace;
  
  int res;
  int i;
  
  uint32_t newNode;
  uint32_t newNodeBigEndian;
  
  uint32_t nodeBigEndian;
  
  descriptor = readBTNodeDescriptor(root, tree);
   
  if(descriptor == NULL)
    return 0;
    
  freeSpace = getFreeSpace(root, descriptor, tree);
  
  lastRecordDataOffset = 0;
     
  for(i = 0; i < descriptor->numRecords; i++) {
    recordOffset = getRecordOffset(i, root, tree);
    key = READ_KEY(tree, recordOffset, tree->io);
    recordDataOffset = recordOffset + key->keyLength + sizeof(key->keyLength);
    
    res = COMPARE(tree, key, searchKey);
    if(res == 0) {
      free(key);
      free(descriptor);
      
      return 0;
    } else if(res > 0) {
      free(key);
      
      break;
    }
    
    free(key);
    
    lastRecordDataOffset = recordDataOffset;
  }
    
  if(descriptor->kind == kBTLeafNode) {    
    if(freeSpace < (sizeof(searchKey->keyLength) + searchKey->keyLength + length + sizeof(uint16_t))) {
      newNode = splitNode(root, descriptor, tree);
      if(i < descriptor->numRecords) {
        doAddRecord(tree, root, searchKey, length, content);
      } else {
        doAddRecord(tree, newNode, searchKey, length, content);
      }
      
      free(descriptor);
      return newNode;
    } else {
      doAddRecord(tree, root, searchKey, length, content);
      
      free(descriptor);
      return 0;
    }
  } else {  
    if(lastRecordDataOffset == 0) {
      if(descriptor->numRecords == 0) {
        hfs_panic("empty index node in btree");
        return 0;
      }
      
      key = READ_KEY(tree, (root * tree->headerRec->nodeSize) + 14, tree->io);
      
      recordDataOffset = recordOffset + key->keyLength + sizeof(key->keyLength);
      nodeBigEndian = getNodeNumberFromPointerRecord(recordDataOffset, tree->io);
      
      FLIPENDIAN(nodeBigEndian);
      
	  free(key);
      key = READ_KEY(tree, (root * tree->headerRec->nodeSize) + 14, tree->io);
      recordDataOffset = recordOffset + key->keyLength + sizeof(key->keyLength);
      
      if(searchKey->keyLength != key->keyLength) {
        if(searchKey->keyLength > key->keyLength && freeSpace < (searchKey->keyLength - key->keyLength)) {
          // very unlikely. We need to split this node before we can resize the key of this index. Do that first, and tell them to call again.
          *callAgain = TRUE;
          return splitNode(root, descriptor, tree);
        }

        moveRecordsDown(tree, descriptor, 1, root, searchKey->keyLength - key->keyLength, 0);
      }
      
      free(key);
      
      ASSERT(WRITE_KEY(tree, recordOffset, searchKey, tree->io), "WRITE_KEY");
      ASSERT(WRITE(tree->io, recordOffset + sizeof(uint16_t) + searchKey->keyLength, sizeof(uint32_t), &nodeBigEndian), "WRITE");
      
      FLIPENDIAN(nodeBigEndian);
      
      newNode = addRecord(tree, nodeBigEndian, searchKey, length, content, callAgain);
    } else {
      newNode = addRecord(tree, getNodeNumberFromPointerRecord(lastRecordDataOffset, tree->io), searchKey, length, content, callAgain);
    }
    
    if(newNode == 0) {
      free(descriptor);
      return 0;
    } else {
      newNodeBigEndian = newNode;
      key = READ_KEY(tree, newNode * tree->headerRec->nodeSize + 14, tree->io);
      FLIPENDIAN(newNodeBigEndian);
      
      if(freeSpace < (sizeof(key->keyLength) + key->keyLength + sizeof(newNodeBigEndian) + sizeof(uint16_t))) {
        newNode = splitNode(root, descriptor, tree);
        
        if(i < descriptor->numRecords) {
          doAddRecord(tree, root, key, sizeof(newNodeBigEndian), (unsigned char*)(&newNodeBigEndian));
        } else {
          doAddRecord(tree, newNode, key, sizeof(newNodeBigEndian), (unsigned char*)(&newNodeBigEndian));
        }
        
        free(key);
        free(descriptor);
        return newNode;
      } else {
        doAddRecord(tree, root, key, sizeof(newNodeBigEndian), (unsigned char*)(&newNodeBigEndian));
        
        free(key);
        free(descriptor);
        return 0;
      }
    }
  }
}

static int increaseHeight(BTree* tree, uint32_t newNode) {
  uint32_t oldRoot;
  uint32_t newRoot;
  BTNodeDescriptor newDescriptor;
  
  BTKey* oldRootKey;
  BTKey* newNodeKey;
  
  uint16_t oldRootOffset;
  uint16_t newNodeOffset;
  uint16_t freeOffset;
   
  oldRoot = tree->headerRec->rootNode;
  
  oldRootKey = READ_KEY(tree, (oldRoot * tree->headerRec->nodeSize) + 14, tree->io);
  newNodeKey = READ_KEY(tree, (newNode * tree->headerRec->nodeSize) + 14, tree->io);
  
  newRoot = getNewNode(tree);
  
  newDescriptor.fLink = 0;
  newDescriptor.bLink = 0;
  newDescriptor.kind = kBTIndexNode;
  newDescriptor.height = tree->headerRec->treeDepth + 1;
  newDescriptor.numRecords = 2;
  newDescriptor.reserved = 0;
  
  oldRootOffset = 14;
  newNodeOffset = oldRootOffset + sizeof(oldRootKey->keyLength) + oldRootKey->keyLength + sizeof(uint32_t);
  freeOffset = newNodeOffset + sizeof(newNodeKey->keyLength) + newNodeKey->keyLength + sizeof(uint32_t);
  
  tree->headerRec->rootNode = newRoot;
  tree->headerRec->treeDepth = newDescriptor.height;
  
  ASSERT(WRITE_KEY(tree, newRoot * tree->headerRec->nodeSize + oldRootOffset, oldRootKey, tree->io), "WRITE_KEY");
  FLIPENDIAN(oldRoot);
  ASSERT(WRITE(tree->io, newRoot * tree->headerRec->nodeSize + oldRootOffset + sizeof(oldRootKey->keyLength) + oldRootKey->keyLength,
                  sizeof(uint32_t), &oldRoot), "WRITE");

  ASSERT(WRITE_KEY(tree, newRoot * tree->headerRec->nodeSize + newNodeOffset, newNodeKey, tree->io), "WRITE_KEY");
  FLIPENDIAN(newNode);
  ASSERT(WRITE(tree->io, newRoot * tree->headerRec->nodeSize + newNodeOffset + sizeof(newNodeKey->keyLength) + newNodeKey->keyLength,
                  sizeof(uint32_t), &newNode), "WRITE");
  
  FLIPENDIAN(oldRootOffset);
  ASSERT(WRITE(tree->io, (newRoot * tree->headerRec->nodeSize) + tree->headerRec->nodeSize - (sizeof(uint16_t) * 1),
                  sizeof(uint16_t), &oldRootOffset), "WRITE");
  
  FLIPENDIAN(newNodeOffset);
  ASSERT(WRITE(tree->io, (newRoot * tree->headerRec->nodeSize) + tree->headerRec->nodeSize - (sizeof(uint16_t) * 2),
                  sizeof(uint16_t), &newNodeOffset), "WRITE");
                  
  FLIPENDIAN(freeOffset);
  ASSERT(WRITE(tree->io, (newRoot * tree->headerRec->nodeSize) + tree->headerRec->nodeSize - (sizeof(uint16_t) * 3),
                  sizeof(uint16_t), &freeOffset), "WRITE");

  ASSERT(writeBTNodeDescriptor(&newDescriptor, tree->headerRec->rootNode, tree), "writeBTNodeDescriptor");
  ASSERT(writeBTHeaderRec(tree), "writeBTHeaderRec");

  free(oldRootKey);
  free(newNodeKey); 
  return TRUE;
}

int addToBTree(BTree* tree, BTKey* searchKey, size_t length, unsigned char* content) {
  int callAgain;
  BTNodeDescriptor newDescriptor;
  uint16_t offset;
  uint16_t freeOffset;
  uint32_t newNode;
  
  if(tree->headerRec->rootNode != 0) {
    do {
      callAgain = FALSE;
      newNode = addRecord(tree, tree->headerRec->rootNode, searchKey, length, content, &callAgain);
      if(newNode != 0) {
        increaseHeight(tree, newNode);
      }
    } while(callAgain);
  } else {
    // add the first leaf node
    tree->headerRec->rootNode = getNewNode(tree);
    tree->headerRec->firstLeafNode = tree->headerRec->rootNode;
    tree->headerRec->lastLeafNode = tree->headerRec->rootNode;
    tree->headerRec->leafRecords = 1;
    tree->headerRec->treeDepth = 1;
    
    newDescriptor.fLink = 0;
    newDescriptor.bLink = 0;
    newDescriptor.kind = kBTLeafNode;
    newDescriptor.height = 1;
    newDescriptor.numRecords = 1;
    newDescriptor.reserved = 0;
    
    offset = 14;
    freeOffset = offset + sizeof(searchKey->keyLength) + searchKey->keyLength + length;
    
    ASSERT(WRITE_KEY(tree, tree->headerRec->rootNode * tree->headerRec->nodeSize + offset, searchKey, tree->io), "WRITE_KEY");    
    ASSERT(WRITE(tree->io, tree->headerRec->rootNode * tree->headerRec->nodeSize + offset + sizeof(searchKey->keyLength) + searchKey->keyLength,
                  length, content), "WRITE");
    
    FLIPENDIAN(offset);
    ASSERT(WRITE(tree->io, (tree->headerRec->rootNode * tree->headerRec->nodeSize) + tree->headerRec->nodeSize - (sizeof(uint16_t) * 1),
                    sizeof(uint16_t), &offset), "WRITE");
    
    FLIPENDIAN(freeOffset);
    ASSERT(WRITE(tree->io, (tree->headerRec->rootNode * tree->headerRec->nodeSize) + tree->headerRec->nodeSize - (sizeof(uint16_t) * 2),
                    sizeof(uint16_t), &freeOffset), "WRITE");

    ASSERT(writeBTNodeDescriptor(&newDescriptor, tree->headerRec->rootNode, tree), "writeBTNodeDescriptor");
    ASSERT(writeBTHeaderRec(tree), "writeBTHeaderRec");
  }
  
  return TRUE;
}

static uint32_t removeRecord(BTree* tree, uint32_t root, BTKey* searchKey, int* callAgain, int* gone) {
  BTNodeDescriptor* descriptor;
  int length;
  int i;
  int res;
  
  uint32_t newNode;
  uint32_t nodeToTraverse;
  uint32_t newNodeBigEndian;
  
  BTKey* key;
  off_t recordOffset;
  off_t recordDataOffset;
  off_t lastRecordDataOffset;
  
  int childGone;
  int checkForChangedKey  ;
  
  size_t freeSpace;
  
  descriptor = readBTNodeDescriptor(root, tree);
  
  freeSpace = getFreeSpace(root, descriptor, tree);
  
  nodeToTraverse = 0;
  lastRecordDataOffset = 0;
  newNode = 0;
  
  (*gone) = FALSE;
  checkForChangedKey = FALSE;
    
  for(i = 0; i < descriptor->numRecords; i++) {
    recordOffset = getRecordOffset(i, root, tree);
    key = READ_KEY(tree, recordOffset, tree->io);
    recordDataOffset = recordOffset + key->keyLength + sizeof(key->keyLength);
    
    res = COMPARE(tree, key, searchKey);
    if(res == 0) {
      free(key);
      
      if(descriptor->kind == kBTLeafNode) {
        if(i != (descriptor->numRecords - 1)) {
          length = getRecordOffset(i + 1, root, tree) - recordOffset;
          moveRecordsDown(tree, descriptor, i + 1, root, -length, -1);
        } // don't need to do that if we're the last record, because our old offset pointer will be the new free space pointer
               
        descriptor->numRecords--;
        tree->headerRec->leafRecords--;
        ASSERT(writeBTNodeDescriptor(descriptor, root, tree), "writeBTNodeDescriptor");
        ASSERT(writeBTHeaderRec(tree), "writeBTHeaderRec");
        
        if(descriptor->numRecords >= 1) {
          free(descriptor);
          return 0;
        } else {
          free(descriptor);
          removeNode(tree, root);
          (*gone) = TRUE;
          return 0;
        }
      } else {
        nodeToTraverse = getNodeNumberFromPointerRecord(recordDataOffset, tree->io);
        checkForChangedKey = TRUE;
        break;
      }
    } else if(res > 0) {
      free(key);
      
      if(lastRecordDataOffset == 0 || descriptor->kind == kBTLeafNode) {
        // not found;
        free(descriptor);
        return 0;
      } else {
        nodeToTraverse = getNodeNumberFromPointerRecord(lastRecordDataOffset, tree->io);
        break;
      }
    }
    
    lastRecordDataOffset = recordDataOffset;
    
    free(key);
  }
  
  if(nodeToTraverse == 0) {
    nodeToTraverse = getNodeNumberFromPointerRecord(lastRecordDataOffset, tree->io);
  }
  
  if(i == descriptor->numRecords) {
    i = descriptor->numRecords - 1;
  }
  
  newNode = removeRecord(tree, nodeToTraverse, searchKey, callAgain, &childGone);
  
  if(childGone) {
    if(i != (descriptor->numRecords - 1)) {
      length = getRecordOffset(i + 1, root, tree) - recordOffset;
      moveRecordsDown(tree, descriptor, i + 1, root, -length, -1);
    } // don't need to do that if we're the last record, because our old offset pointer will be the new free space pointer
           
    descriptor->numRecords--;
    ASSERT(writeBTNodeDescriptor(descriptor, root, tree), "writeBTNodeDescriptor");
  } else {
    if(checkForChangedKey) {
      // we will remove the first item in the child node, so our index has to change
      
      key = READ_KEY(tree, getRecordOffset(0, nodeToTraverse, tree), tree->io);
      
      if(searchKey->keyLength != key->keyLength) {
        if(key->keyLength > searchKey->keyLength && freeSpace < (key->keyLength - searchKey->keyLength)) {
          // very unlikely. We need to split this node before we can resize the key of this index. Do that first, and tell them to call again.
          *callAgain = TRUE;
          return splitNode(root, descriptor, tree);
        }
        
        moveRecordsDown(tree, descriptor, i + 1, root, key->keyLength - searchKey->keyLength, 0);
      }
      
      ASSERT(WRITE_KEY(tree, recordOffset, key, tree->io), "WRITE_KEY");
      FLIPENDIAN(nodeToTraverse);
      ASSERT(WRITE(tree->io, recordOffset + sizeof(uint16_t) + key->keyLength, sizeof(uint32_t), &nodeToTraverse), "WRITE");
      FLIPENDIAN(nodeToTraverse);
      
      free(key);
    }
  }
  
  if(newNode == 0) {
    if(descriptor->numRecords == 0) {
      removeNode(tree, root);
      (*gone) = TRUE;
    }
    
    free(descriptor);
    return 0;
  } else {
    newNodeBigEndian = newNode;
    key = READ_KEY(tree, newNode * tree->headerRec->nodeSize + 14, tree->io);
    FLIPENDIAN(newNodeBigEndian);
    
    if(freeSpace < (sizeof(key->keyLength) + key->keyLength + sizeof(newNodeBigEndian) + sizeof(uint16_t))) {
      newNode = splitNode(root, descriptor, tree);
      
      if(i < descriptor->numRecords) {
        doAddRecord(tree, root, key, sizeof(newNodeBigEndian), (unsigned char*)(&newNodeBigEndian));
      } else {
        doAddRecord(tree, newNode, key, sizeof(newNodeBigEndian), (unsigned char*)(&newNodeBigEndian));
      }
      
      if(descriptor->numRecords == 0) {
        removeNode(tree, root);
        (*gone) = TRUE;
      }
      
      free(key);
      free(descriptor);
      return newNode;
    } else {
      doAddRecord(tree, root, key, sizeof(newNodeBigEndian), (unsigned char*)(&newNodeBigEndian));
           
      free(key);
      free(descriptor);
      return 0;
    }
  }
  
  return FALSE;
}

int removeFromBTree(BTree* tree, BTKey* searchKey) {
  int callAgain;
  int gone;
  uint32_t newNode;
  
  do {
    callAgain = FALSE;
    newNode = removeRecord(tree, tree->headerRec->rootNode, searchKey, &callAgain, &gone);
    if(newNode != 0) {
      increaseHeight(tree, newNode);
    }
  } while(callAgain);

  return TRUE;
}
