#ifndef HFSPLUS_H
#define HFSPLUS_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>


#include "common.h"

#define READ(a, b, c, d) ((*((a)->read))(a, b, c, d))
#define WRITE(a, b, c, d) ((*((a)->write))(a, b, c, d))
#define CLOSE(a) ((*((a)->close))(a))
#define COMPARE(a, b, c) ((*((a)->compare))(b, c))
#define READ_KEY(a, b, c) ((*((a)->keyRead))(b, c))
#define WRITE_KEY(a, b, c, d) ((*((a)->keyWrite))(b, c, d))
#define READ_DATA(a, b, c) ((*((a)->dataRead))(b, c))

struct BTKey {
	uint16_t keyLength;
	unsigned char data[0];
} __attribute__((__packed__));

typedef struct BTKey BTKey;

typedef BTKey* (*dataReadFunc)(off_t offset, struct io_func_struct* io);
typedef void (*keyPrintFunc)(BTKey* toPrint);
typedef int (*keyWriteFunc)(off_t offset, BTKey* toWrite, struct io_func_struct* io);
typedef int (*compareFunc)(BTKey* left, BTKey* right);

#define STR_SIZE(str) (sizeof(uint16_t) + (sizeof(uint16_t) * (str).length))

#ifndef __HFS_FORMAT__

typedef uint32_t HFSCatalogNodeID;

enum {
	kHFSRootParentID            = 1,
	kHFSRootFolderID            = 2,
	kHFSExtentsFileID           = 3,
	kHFSCatalogFileID           = 4,
	kHFSBadBlockFileID          = 5,
	kHFSAllocationFileID        = 6,
	kHFSStartupFileID           = 7,
	kHFSAttributesFileID        = 8,
	kHFSRepairCatalogFileID     = 14,
	kHFSBogusExtentFileID       = 15,
	kHFSFirstUserCatalogNodeID  = 16
};

struct HFSUniStr255 {
	uint16_t  length;
	uint16_t unicode[255];
} __attribute__((__packed__));
typedef struct HFSUniStr255 HFSUniStr255;
typedef const  HFSUniStr255 *ConstHFSUniStr255Param;

struct HFSPlusExtentDescriptor {
	uint32_t startBlock;
	uint32_t blockCount;
} __attribute__((__packed__));
typedef struct HFSPlusExtentDescriptor HFSPlusExtentDescriptor;

typedef HFSPlusExtentDescriptor HFSPlusExtentRecord[8];

struct HFSPlusForkData {
	uint64_t logicalSize;
	uint32_t clumpSize;
	uint32_t totalBlocks;
	HFSPlusExtentRecord extents;
} __attribute__((__packed__));
typedef struct HFSPlusForkData HFSPlusForkData;
 
struct HFSPlusVolumeHeader {
	uint16_t signature;
	uint16_t version;
	uint32_t attributes;
	uint32_t lastMountedVersion;
	uint32_t journalInfoBlock;

	uint32_t createDate;
	uint32_t modifyDate;
	uint32_t backupDate;
	uint32_t checkedDate;

	uint32_t fileCount;
	uint32_t folderCount;

	uint32_t blockSize;
	uint32_t totalBlocks;
	uint32_t freeBlocks;

	uint32_t nextAllocation;
	uint32_t rsrcClumpSize;
	uint32_t dataClumpSize;
	HFSCatalogNodeID nextCatalogID;

	uint32_t writeCount;
	uint64_t encodingsBitmap;

	uint32_t finderInfo[8];

	HFSPlusForkData allocationFile;
	HFSPlusForkData extentsFile;
	HFSPlusForkData catalogFile;
	HFSPlusForkData attributesFile;
	HFSPlusForkData startupFile;
} __attribute__((__packed__));
typedef struct HFSPlusVolumeHeader HFSPlusVolumeHeader;

enum {
	kBTLeafNode       = -1,
	kBTIndexNode      =  0,
	kBTHeaderNode     =  1,
	kBTMapNode        =  2
};

struct BTNodeDescriptor {
	uint32_t    fLink;
	uint32_t    bLink;
	int8_t      kind;
	uint8_t     height;
	uint16_t    numRecords;
	uint16_t    reserved;
} __attribute__((__packed__));
typedef struct BTNodeDescriptor BTNodeDescriptor;

#define kHFSCaseFolding 0xCF
#define kHFSBinaryCompare 0xBC

struct BTHeaderRec {
	uint16_t    treeDepth;
	uint32_t    rootNode;
	uint32_t    leafRecords;
	uint32_t    firstLeafNode;
	uint32_t    lastLeafNode;
	uint16_t    nodeSize;
	uint16_t    maxKeyLength;
	uint32_t    totalNodes;
	uint32_t    freeNodes;
	uint16_t    reserved1;
	uint32_t    clumpSize;      // misaligned
	uint8_t     btreeType;
	uint8_t     keyCompareType;
	uint32_t    attributes;     // long aligned again
	uint32_t    reserved3[16];
} __attribute__((__packed__));
typedef struct BTHeaderRec BTHeaderRec;

struct HFSPlusExtentKey {
	uint16_t            keyLength;
	uint8_t             forkType;
	uint8_t             pad;
	HFSCatalogNodeID    fileID;
	uint32_t            startBlock;
} __attribute__((__packed__));
typedef struct HFSPlusExtentKey HFSPlusExtentKey;

struct HFSPlusCatalogKey {
	uint16_t            keyLength;
	HFSCatalogNodeID    parentID;
	HFSUniStr255        nodeName;
} __attribute__((__packed__));
typedef struct HFSPlusCatalogKey HFSPlusCatalogKey;

#ifndef __MACTYPES__
struct Point {
	int16_t              v;
	int16_t              h;
} __attribute__((__packed__));
typedef struct Point  Point;

struct Rect {
	int16_t              top;
	int16_t              left;
	int16_t              bottom;
	int16_t              right;
} __attribute__((__packed__));
typedef struct Rect   Rect;

/* OSType is a 32-bit value made by packing four 1-byte characters 
   together. */
typedef uint32_t        FourCharCode;
typedef FourCharCode    OSType;

#endif

/* Finder flags (finderFlags, fdFlags and frFlags) */
enum {
	kIsOnDesk       = 0x0001,     /* Files and folders (System 6) */
	kColor          = 0x000E,     /* Files and folders */
	kIsShared       = 0x0040,     /* Files only (Applications only) If */
	                              /* clear, the application needs */
	                              /* to write to its resource fork, */
	                              /* and therefore cannot be shared */
	                              /* on a server */
	kHasNoINITs     = 0x0080,     /* Files only (Extensions/Control */
	                              /* Panels only) */
	                              /* This file contains no INIT resource */
	kHasBeenInited  = 0x0100,     /* Files only.  Clear if the file */
	                              /* contains desktop database resources */
	                              /* ('BNDL', 'FREF', 'open', 'kind'...) */
	                              /* that have not been added yet.  Set */
	                              /* only by the Finder. */
	                              /* Reserved for folders */
	kHasCustomIcon  = 0x0400,     /* Files and folders */
	kIsStationery   = 0x0800,     /* Files only */
	kNameLocked     = 0x1000,     /* Files and folders */
	kHasBundle      = 0x2000,     /* Files only */
	kIsInvisible    = 0x4000,     /* Files and folders */
	kIsAlias        = 0x8000      /* Files only */
};

/* Extended flags (extendedFinderFlags, fdXFlags and frXFlags) */
enum {
	kExtendedFlagsAreInvalid    = 0x8000, /* The other extended flags */
	                                      /* should be ignored */
	kExtendedFlagHasCustomBadge = 0x0100, /* The file or folder has a */
	                                      /* badge resource */
	kExtendedFlagHasRoutingInfo = 0x0004  /* The file contains routing */
	                                      /* info resource */
};

enum {
	kSymLinkFileType  = 0x736C6E6B, /* 'slnk' */
	kSymLinkCreator   = 0x72686170  /* 'rhap' */
};

struct FileInfo {
	OSType      fileType;           /* The type of the file */
	OSType      fileCreator;        /* The file's creator */
	uint16_t    finderFlags;
	Point       location;           /* File's location in the folder. */
	uint16_t    reservedField;
} __attribute__((__packed__));
typedef struct FileInfo   FileInfo;

struct ExtendedFileInfo {
	int16_t    reserved1[4];
	uint16_t   extendedFinderFlags;
	int16_t    reserved2;
	int32_t    putAwayFolderID;
} __attribute__((__packed__));
typedef struct ExtendedFileInfo   ExtendedFileInfo;

struct FolderInfo {
	Rect        windowBounds;     /* The position and dimension of the */
	                              /* folder's window */
	uint16_t    finderFlags;
	Point       location;         /* Folder's location in the parent */
	                              /* folder. If set to {0, 0}, the Finder */
	                              /* will place the item automatically */
	uint16_t    reservedField;
} __attribute__((__packed__));
typedef struct FolderInfo   FolderInfo;

struct ExtendedFolderInfo {
	Point      scrollPosition;     /* Scroll position (for icon views) */
	int32_t    reserved1;
	uint16_t   extendedFinderFlags;
	int16_t    reserved2;
	int32_t    putAwayFolderID;
} __attribute__((__packed__));
typedef struct ExtendedFolderInfo   ExtendedFolderInfo;

#ifndef _STAT_H_
#ifndef _SYS_STAT_H
#define S_ISUID 0004000     /* set user id on execution */
#define S_ISGID 0002000     /* set group id on execution */
#define S_ISTXT 0001000     /* sticky bit */

#define S_IRWXU 0000700     /* RWX mask for owner */
#define S_IRUSR 0000400     /* R for owner */
#define S_IWUSR 0000200     /* W for owner */
#define S_IXUSR 0000100     /* X for owner */

#define S_IRWXG 0000070     /* RWX mask for group */
#define S_IRGRP 0000040     /* R for group */
#define S_IWGRP 0000020     /* W for group */
#define S_IXGRP 0000010     /* X for group */

#define S_IRWXO 0000007     /* RWX mask for other */
#define S_IROTH 0000004     /* R for other */
#define S_IWOTH 0000002     /* W for other */
#define S_IXOTH 0000001     /* X for other */

#define S_IFMT   0170000    /* type of file mask */
#define S_IFIFO  0010000    /* named pipe (fifo) */
#define S_IFCHR  0020000    /* character special */
#define S_IFDIR  0040000    /* directory */
#define S_IFBLK  0060000    /* block special */
#define S_IFREG  0100000    /* regular */
#define S_IFLNK  0120000    /* symbolic link */
#define S_IFSOCK 0140000    /* socket */
#define S_IFWHT  0160000    /* whiteout */
#endif
#endif

#define UF_COMPRESSED 040

struct HFSPlusBSDInfo {
	uint32_t  ownerID;
	uint32_t  groupID;
	uint8_t   adminFlags;
	uint8_t   ownerFlags;
	uint16_t  fileMode;
	union {
		uint32_t  iNodeNum;
		uint32_t  linkCount;
		uint32_t  rawDevice;
	} special;
} __attribute__((__packed__));
typedef struct HFSPlusBSDInfo HFSPlusBSDInfo;

enum {
	kHFSPlusFolderRecord        = 0x0001,
	kHFSPlusFileRecord          = 0x0002,
	kHFSPlusFolderThreadRecord  = 0x0003,
	kHFSPlusFileThreadRecord    = 0x0004
};

enum {
	kHFSFileLockedBit       = 0x0000,       /* file is locked and cannot be written to */
	kHFSFileLockedMask      = 0x0001,
	
	kHFSThreadExistsBit     = 0x0001,       /* a file thread record exists for this file */
	kHFSThreadExistsMask    = 0x0002,
	
	kHFSHasAttributesBit    = 0x0002,       /* object has extended attributes */
	kHFSHasAttributesMask   = 0x0004,
	
	kHFSHasSecurityBit      = 0x0003,       /* object has security data (ACLs) */
	kHFSHasSecurityMask     = 0x0008,
	
	kHFSHasFolderCountBit   = 0x0004,       /* only for HFSX, folder maintains a separate sub-folder count */
	kHFSHasFolderCountMask  = 0x0010,       /* (sum of folder records and directory hard links) */
	
	kHFSHasLinkChainBit     = 0x0005,       /* has hardlink chain (inode or link) */
	kHFSHasLinkChainMask    = 0x0020,
	
	kHFSHasChildLinkBit     = 0x0006,       /* folder has a child that's a dir link */
	kHFSHasChildLinkMask    = 0x0040
};

struct HFSPlusCatalogFolder {
	int16_t             recordType;
	uint16_t            flags;
	uint32_t            valence;
	HFSCatalogNodeID    folderID;
	uint32_t            createDate;
	uint32_t            contentModDate;
	uint32_t            attributeModDate;
	uint32_t            accessDate;
	uint32_t            backupDate;
	HFSPlusBSDInfo      permissions;
	FolderInfo          userInfo;
	ExtendedFolderInfo  finderInfo;
	uint32_t            textEncoding;
	uint32_t            folderCount;
} __attribute__((__packed__));
typedef struct HFSPlusCatalogFolder HFSPlusCatalogFolder;

struct HFSPlusCatalogFile {
	int16_t             recordType;
	uint16_t            flags;
	uint32_t            reserved1;
	HFSCatalogNodeID    fileID;
	uint32_t            createDate;
	uint32_t            contentModDate;
	uint32_t            attributeModDate;
	uint32_t            accessDate;
	uint32_t            backupDate;
	HFSPlusBSDInfo      permissions;
	FileInfo            userInfo;
	ExtendedFileInfo    finderInfo;
	uint32_t            textEncoding;
	uint32_t            reserved2;

	HFSPlusForkData     dataFork;
	HFSPlusForkData     resourceFork;
} __attribute__((__packed__));
typedef struct HFSPlusCatalogFile HFSPlusCatalogFile;

struct HFSPlusCatalogThread {
	int16_t             recordType;
	int16_t             reserved;
	HFSCatalogNodeID    parentID;
	HFSUniStr255        nodeName;
} __attribute__((__packed__));
typedef struct HFSPlusCatalogThread HFSPlusCatalogThread;

enum {
	kHFSPlusAttrInlineData	= 0x10,
	kHFSPlusAttrForkData	= 0x20,
	kHFSPlusAttrExtents	= 0x30
};

struct HFSPlusAttrForkData {
	uint32_t 	recordType;
	uint32_t 	reserved;
	HFSPlusForkData theFork;
} __attribute__((__packed__));
typedef struct HFSPlusAttrForkData HFSPlusAttrForkData;

struct HFSPlusAttrExtents {
	uint32_t 		recordType;
	uint32_t 		reserved;
	HFSPlusExtentRecord	extents;
};
typedef struct HFSPlusAttrExtents HFSPlusAttrExtents;

struct HFSPlusAttrData {
	uint32_t    recordType;
	uint32_t    reserved[2];
	uint32_t    size;
	uint8_t     data[0];
} __attribute__((__packed__));
typedef struct HFSPlusAttrData HFSPlusAttrData;

union HFSPlusAttrRecord {
	uint32_t 		recordType;
	HFSPlusAttrData 	attrData;
	HFSPlusAttrForkData 	forkData;
	HFSPlusAttrExtents 	overflowExtents;
};
typedef union HFSPlusAttrRecord HFSPlusAttrRecord;

struct HFSPlusAttrKey {
	uint16_t     keyLength;
	uint16_t     pad;
	uint32_t     fileID;
	uint32_t     startBlock;
	HFSUniStr255 name;
} __attribute__((__packed__));
typedef struct HFSPlusAttrKey HFSPlusAttrKey;

enum {
	kHardLinkFileType = 0x686C6E6B,  /* 'hlnk' */
	kHFSPlusCreator   = 0x6866732B   /* 'hfs+' */
};

#endif

struct HFSPlusCatalogRecord {
	int16_t recordType;
	unsigned char data[0];
} __attribute__((__packed__));
typedef struct HFSPlusCatalogRecord HFSPlusCatalogRecord;

struct CatalogRecordList {
	HFSUniStr255 name;
	HFSPlusCatalogRecord* record;
	struct CatalogRecordList* next;
};
typedef struct CatalogRecordList CatalogRecordList;

struct XAttrList {
  char* name;
  struct XAttrList* next;
};
typedef struct XAttrList XAttrList;

struct Extent {
	uint32_t startBlock;
	uint32_t blockCount;
	struct Extent* next;
};

typedef struct Extent Extent;

typedef struct {
	io_func* io;
	BTHeaderRec *headerRec;
	compareFunc compare;
	dataReadFunc keyRead;
	keyWriteFunc keyWrite;
	keyPrintFunc keyPrint;
	dataReadFunc dataRead;
} BTree;

typedef struct {
	io_func* image;
	HFSPlusVolumeHeader* volumeHeader;

	BTree* extentsTree;
	BTree* catalogTree;
  BTree* attrTree;
	io_func* allocationFile;
  HFSCatalogNodeID metadataDir;
} Volume;


typedef struct {
	HFSCatalogNodeID id;
	HFSPlusCatalogRecord* catalogRecord;
	Volume* volume;
	HFSPlusForkData* forkData;
	Extent* extents;
} RawFile;

#ifdef __cplusplus
extern "C" {
#endif
	void hfs_panic(const char* panicString);

	void printUnicode(HFSUniStr255* str);
	char* unicodeToAscii(HFSUniStr255* str);

	BTNodeDescriptor* readBTNodeDescriptor(uint32_t num, BTree* tree);

	BTHeaderRec* readBTHeaderRec(io_func* io);

	BTree* openBTree(io_func* io, compareFunc compare, dataReadFunc keyRead, keyWriteFunc keyWrite, keyPrintFunc keyPrint, dataReadFunc dataRead);

	void closeBTree(BTree* tree);

	off_t getRecordOffset(int num, uint32_t nodeNum, BTree* tree);

	off_t getNodeNumberFromPointerRecord(off_t offset, io_func* io);

	void* search(BTree* tree, BTKey* searchKey, int *exact, uint32_t *nodeNumber, int *recordNumber);

	io_func* openFlatFile(const char* fileName);
	io_func* openFlatFileRO(const char* fileName);

	io_func* openRawFile(HFSCatalogNodeID id, HFSPlusForkData* forkData, HFSPlusCatalogRecord* catalogRecord, Volume* volume);

	BTree* openAttributesTree(io_func* file);
	size_t getAttribute(Volume* volume, uint32_t fileID, const char* name, uint8_t** data);
	int setAttribute(Volume* volume, uint32_t fileID, const char* name, uint8_t* data, size_t size);
	int unsetAttribute(Volume* volume, uint32_t fileID, const char* name);
	XAttrList* getAllExtendedAttributes(HFSCatalogNodeID CNID, Volume* volume);

	void flipExtentRecord(HFSPlusExtentRecord* extentRecord);

	BTree* openExtentsTree(io_func* file);

	void ASCIIToUnicode(const char* ascii, HFSUniStr255* unistr);

	void flipCatalogFolder(HFSPlusCatalogFolder* record);
	void flipCatalogFile(HFSPlusCatalogFile* record);
	void flipCatalogThread(HFSPlusCatalogThread* record, int out);

	BTree* openCatalogTree(io_func* file);
	int updateCatalog(Volume* volume, HFSPlusCatalogRecord* catalogRecord);
	int move(const char* source, const char* dest, Volume* volume);
	int removeFile(const char* fileName, Volume* volume);
	HFSCatalogNodeID newFolder(const char* pathName, Volume* volume);
	HFSCatalogNodeID newFile(const char* pathName, Volume* volume);
	int chmodFile(const char* pathName, int mode, Volume* volume);
	int chownFile(const char* pathName, uint32_t owner, uint32_t group, Volume* volume);
	int makeSymlink(const char* pathName, const char* target, Volume* volume);
	int attrFile(const char* pathName, const char* flags, Volume* volume);

	HFSCatalogNodeID getMetadataDirectoryID(Volume* volume);
	HFSPlusCatalogRecord* getRecordByCNID(HFSCatalogNodeID CNID, Volume* volume);
	HFSPlusCatalogRecord* getLinkTarget(HFSPlusCatalogRecord* record, HFSCatalogNodeID parentID, HFSPlusCatalogKey *key, Volume* volume);
	CatalogRecordList* getFolderContents(HFSCatalogNodeID CNID, Volume* volume);
	HFSPlusCatalogRecord* getRecordFromPath(const char* path, Volume* volume, char **name, HFSPlusCatalogKey* retKey);
	HFSPlusCatalogRecord* getRecordFromPath2(const char* path, Volume* volume, char **name, HFSPlusCatalogKey* retKey, char traverse);
	HFSPlusCatalogRecord* getRecordFromPath3(const char* path, Volume* volume, char **name, HFSPlusCatalogKey* retKey, char traverse, char returnLink, HFSCatalogNodeID parentID);
	void releaseCatalogRecordList(CatalogRecordList* list);

	int isBlockUsed(Volume* volume, uint32_t block);
	int setBlockUsed(Volume* volume, uint32_t block, int used);
	int allocate(RawFile* rawFile, off_t size);

	void flipForkData(HFSPlusForkData* forkData);

	Volume* openVolume(io_func* io);
	void closeVolume(Volume *volume);
	int updateVolume(Volume* volume);

	int debugBTree(BTree* tree, int displayTree);

	int addToBTree(BTree* tree, BTKey* searchKey, size_t length, unsigned char* content);

	int removeFromBTree(BTree* tree, BTKey* searchKey);

	int32_t FastUnicodeCompare ( register uint16_t str1[], register uint16_t length1,
		                    register uint16_t str2[], register uint16_t length2);
#ifdef __cplusplus
}
#endif

#endif

