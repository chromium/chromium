#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <sys/param.h>
#include <sys/types.h>
#include "common.h"
#include "hfs/hfslib.h"
#include "hfs/hfscompress.h"
#include <sys/stat.h>
#include <inttypes.h>
#ifdef WIN32
#include <sys/utime.h>
#define lstat stat
#else
#include <utime.h>
#endif

#define BUFSIZE 1024*1024

static int silence = 0;

void hfs_setsilence(int s) {
	silence = s;
}

void writeToFile(HFSPlusCatalogFile* file, AbstractFile* output, Volume* volume) {
	unsigned char* buffer;
	io_func* io;
	off_t curPosition;
	size_t bytesLeft;

	buffer = (unsigned char*) malloc(BUFSIZE);

	if(file->permissions.ownerFlags & UF_COMPRESSED) {
		io = openHFSPlusCompressed(volume, file);
		if(io == NULL) {
			hfs_panic("error opening file");
			free(buffer);
			return;
		}

		curPosition = 0;
		bytesLeft = ((HFSPlusCompressed*) io->data)->decmpfs->size;
	} else {
		io = openRawFile(file->fileID, &file->dataFork, (HFSPlusCatalogRecord*)file, volume);
		if(io == NULL) {
			hfs_panic("error opening file");
			free(buffer);
			return;
		}

		curPosition = 0;
		bytesLeft = file->dataFork.logicalSize;
	}
	while(bytesLeft > 0) {
		if(bytesLeft > BUFSIZE) {
			if(!READ(io, curPosition, BUFSIZE, buffer)) {
				hfs_panic("error reading");
			}
			if(output->write(output, buffer, BUFSIZE) != BUFSIZE) {
				hfs_panic("error writing");
			}
			curPosition += BUFSIZE;
			bytesLeft -= BUFSIZE;
		} else {
			if(!READ(io, curPosition, bytesLeft, buffer)) {
				hfs_panic("error reading");
			}
			if(output->write(output, buffer, bytesLeft) != bytesLeft) {
				hfs_panic("error writing");
			}
			curPosition += bytesLeft;
			bytesLeft -= bytesLeft;
		}
	}
	CLOSE(io);

	free(buffer);
}

void writeToHFSFile(HFSPlusCatalogFile* file, AbstractFile* input, Volume* volume) {
	unsigned char *buffer;
	io_func* io;
	off_t curPosition;
	off_t bytesLeft;

	buffer = (unsigned char*) malloc(BUFSIZE);

	bytesLeft = input->getLength(input);

	if(file->permissions.ownerFlags & UF_COMPRESSED) {
		io = openHFSPlusCompressed(volume, file);
		if(io == NULL) {
			hfs_panic("error opening file");
			free(buffer);
			return;
		}
	} else {
		io = openRawFile(file->fileID, &file->dataFork, (HFSPlusCatalogRecord*)file, volume);
		if(io == NULL) {
			hfs_panic("error opening file");
			free(buffer);
			return;
		}
		allocate((RawFile*)io->data, bytesLeft);
	}

	curPosition = 0;

	while(bytesLeft > 0) {
		if(bytesLeft > BUFSIZE) {
			if(input->read(input, buffer, BUFSIZE) != BUFSIZE) {
				hfs_panic("error reading");
			}
			if(!WRITE(io, curPosition, BUFSIZE, buffer)) {
				hfs_panic("error writing");
			}
			curPosition += BUFSIZE;
			bytesLeft -= BUFSIZE;
		} else {
			if(input->read(input, buffer, (size_t)bytesLeft) != (size_t)bytesLeft) {
				hfs_panic("error reading");
			}
			if(!WRITE(io, curPosition, (size_t)bytesLeft, buffer)) {
				hfs_panic("error reading");
			}
			curPosition += bytesLeft;
			bytesLeft -= bytesLeft;
		}
	}

	CLOSE(io);

	free(buffer);
}

void get_hfs(Volume* volume, const char* inFileName, AbstractFile* output) {
	HFSPlusCatalogRecord* record;

	record = getRecordFromPath(inFileName, volume, NULL, NULL);

	if(record != NULL) {
		if(record->recordType == kHFSPlusFileRecord)
			writeToFile((HFSPlusCatalogFile*)record,  output, volume);
		else {
			printf("Not a file\n");
			exit(0);
		}
	} else {
		printf("No such file or directory\n");
		exit(0);
	}

	free(record);
}

int add_hfs(Volume* volume, AbstractFile* inFile, const char* outFileName) {
	HFSPlusCatalogRecord* record;
	int ret;

	record = getRecordFromPath(outFileName, volume, NULL, NULL);

	if(record != NULL) {
		if(record->recordType == kHFSPlusFileRecord) {
			writeToHFSFile((HFSPlusCatalogFile*)record, inFile, volume);
			ret = TRUE;
		} else {
			printf("Not a file\n");
			exit(0);
		}
	} else {
		if(newFile(outFileName, volume)) {
			record = getRecordFromPath(outFileName, volume, NULL, NULL);
			writeToHFSFile((HFSPlusCatalogFile*)record, inFile, volume);
			ret = TRUE;
		} else {
			ret = FALSE;
		}
	}

	inFile->close(inFile);
	if(record != NULL) {
		free(record);
	}

	return ret;
}

void grow_hfs(Volume* volume, uint64_t newSize) {
	uint32_t newBlocks;
	uint32_t blocksToGrow;
	uint64_t newMapSize;
	uint64_t i;
	unsigned char zero;

	zero = 0;

	newBlocks = newSize / volume->volumeHeader->blockSize;

	if(newBlocks <= volume->volumeHeader->totalBlocks) {
		printf("Cannot shrink volume\n");
		return;
	}

	blocksToGrow = newBlocks - volume->volumeHeader->totalBlocks;
	newMapSize = newBlocks / 8;

	if(volume->volumeHeader->allocationFile.logicalSize < newMapSize) {
		if(volume->volumeHeader->freeBlocks
		   < ((newMapSize - volume->volumeHeader->allocationFile.logicalSize) / volume->volumeHeader->blockSize)) {
			printf("Not enough room to allocate new allocation map blocks\n");
			exit(0);
		}

		allocate((RawFile*) (volume->allocationFile->data), newMapSize);
	}

	/* unreserve last block */
	setBlockUsed(volume, volume->volumeHeader->totalBlocks - 1, 0);
	/* don't need to increment freeBlocks because we will allocate another alternate volume header later on */

	/* "unallocate" the new blocks */
	for(i = ((volume->volumeHeader->totalBlocks / 8) + 1); i < newMapSize; i++) {
		ASSERT(WRITE(volume->allocationFile, i, 1, &zero), "WRITE");
	}

	/* grow backing store size */
	ASSERT(WRITE(volume->image, newSize - 1, 1, &zero), "WRITE");

	/* write new volume information */
	volume->volumeHeader->totalBlocks = newBlocks;
	volume->volumeHeader->freeBlocks += blocksToGrow;

	/* reserve last block */
	setBlockUsed(volume, volume->volumeHeader->totalBlocks - 1, 1);

	updateVolume(volume);
}

void removeAllInFolder(HFSCatalogNodeID folderID, Volume* volume, const char* parentName) {
	CatalogRecordList* list;
	CatalogRecordList* theList;
	char fullName[1024];
	char* name;
	char* pathComponent;
	int pathLen;
	char isRoot;

	HFSPlusCatalogFolder* folder;
	theList = list = getFolderContents(folderID, volume);

	strcpy(fullName, parentName);
	pathComponent = fullName + strlen(fullName);

	isRoot = FALSE;
	if(strcmp(fullName, "/") == 0) {
		isRoot = TRUE;
	}

	while(list != NULL) {
		name = unicodeToAscii(&list->name);
		if(isRoot && (name[0] == '\0' || strncmp(name, ".HFS+ Private Directory Data", sizeof(".HFS+ Private Directory Data") - 1) == 0)) {
			free(name);
			list = list->next;
			continue;
		}

		strcpy(pathComponent, name);
		pathLen = strlen(fullName);

		if(list->record->recordType == kHFSPlusFolderRecord) {
			folder = (HFSPlusCatalogFolder*)list->record;
			fullName[pathLen] = '/';
			fullName[pathLen + 1] = '\0';
			removeAllInFolder(folder->folderID, volume, fullName);
		} else {
			printf("%s\n", fullName);
			removeFile(fullName, volume);
		}

		free(name);
		list = list->next;
	}

	releaseCatalogRecordList(theList);

	if(!isRoot) {
		*(pathComponent - 1) = '\0';
		printf("%s\n", fullName);
		removeFile(fullName, volume);
	}
}

void addAllInFolder(HFSCatalogNodeID folderID, Volume* volume, const char* parentName) {
	addAllInFolderWithPolicies(folderID, volume, parentName, kIncomingSymlinksTraverse, TRUE);
}

void addAllInFolderWithPolicies(
		HFSCatalogNodeID folderID, Volume* volume, const char* parentName,
		IncomingSymlinksPolicy symlinkPolicy, char assignSpecialPermissions) {
	char fullName[1024];
	size_t nChars = strlen(parentName);
	ASSERT(nChars < 1024, "addAllInFolder: parentName too long");
	memcpy(fullName, parentName, nChars + 1);

	char* pathComponent = fullName + nChars;
	size_t componentBufSz = 1024 - nChars;

	char cwd[1024];
	ASSERT(getcwd(cwd, 1024) != NULL, "cannot get current working directory");

	CatalogRecordList* theList = getFolderContents(folderID, volume);
	DIR* dir = opendir(cwd);
	ASSERT(dir, "addAllInFolder: cannot opendir CWD");

	printf(
		"info: addAllInFolderWithPolicies: running in %s with symlink policy %d and permission setting %d\n",
		cwd, symlinkPolicy, assignSpecialPermissions);

	for (struct dirent* ent = readdir(dir); ent; ent = readdir(dir)) {
		/* Skip `.` and `..` */
		if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) {
			continue;
		}
		nChars = strlen(ent->d_name);
		ASSERT(nChars < componentBufSz, "addAllInFolder: assembled path too long");
		memcpy(pathComponent, ent->d_name, nChars+1);
		size_t pathLen = strlen(fullName);

		/* Look for an existing item to overwrite. */
		HFSCatalogNodeID cnid = 0;
		for (CatalogRecordList* nextEntry = theList; nextEntry; nextEntry = nextEntry->next) {
			char* name = unicodeToAscii(&nextEntry->name);
			if(strcmp(name, ent->d_name) == 0) {
				HFSPlusCatalogFolder* recordAsFolder = tryCatalogRecordAsFolder(nextEntry->record);
				if (recordAsFolder) {
					cnid = recordAsFolder->folderID;
				} else {
					HFSPlusCatalogFile* recordAsFile = tryCatalogRecordAsFile(nextEntry->record);
					ASSERT(recordAsFile, "addAllInFolder: trying to overwrite a strange record");
					cnid = recordAsFile->fileID;
				}
				free(name);
				break;
			}
			free(name);
		}
		struct stat st;
		ASSERT(lstat(ent->d_name, &st) == 0, "addAllInFolder: lstat failed");
		if (S_ISLNK(st.st_mode)) {
			ASSERT(symlinkPolicy != kIncomingSymlinksFail,
					"addAllInFolder: found a symlink when symlink policy is \"fail\"");
			if (symlinkPolicy == kIncomingSymlinksCloneLink) {
				char linkbuf[1024];
				linkbuf[0] = '\0';
				ssize_t zloc = readlink(ent->d_name, linkbuf, 1024);
				ASSERT(zloc >= 0, "addAllInFolder: readlink failed");
				ASSERT(zloc < 1024, "addAllInFolder: readlink overwrote");
				/* readlink doesn't add a null terminator; we have to do it.*/
				linkbuf[zloc] = '\0';
				ASSERT(makeSymlink(fullName, linkbuf, volume),
				    "addAllInFolder: makesymlink failed");
				continue;
			} else if (symlinkPolicy == kIncomingSymlinksTraverse) {
				ASSERT(stat(ent->d_name, &st) == 0, "addallInFolder: stat failed");
				/* Continue processing, using the stat record from the link target. */
			} else {
				ASSERT(0, "addAllInFolder: unrecognized symlink policy");
			}
		}
		/* After traversing a symlink, continue processing the resolved item. */
		if (S_ISDIR(st.st_mode)) {
			printf("folder: %s\n", fullName); fflush(stdout);

			if(cnid == 0) {
				cnid = newFolder(fullName, volume);
			}

			ASSERT(pathLen < 1023, "addAllInFolder: path too long, can't recurse");
			fullName[pathLen] = '/';
			fullName[pathLen + 1] = '\0';
			/* Copy permissions from the source folder */
			chmodFile(fullName, (int)st.st_mode, volume);
			printf("Setting permissions to %06o for %s\n", st.st_mode, fullName);
			/* Recurse */
			ASSERT(chdir(ent->d_name) == 0, "chdir");
			addAllInFolderWithPolicies(cnid, volume, fullName, symlinkPolicy, assignSpecialPermissions);
			ASSERT(chdir(cwd) == 0, "chdir");
		} else if (S_ISREG(st.st_mode)) {
			printf("file: %s\n", fullName);	fflush(stdout);
			if(cnid == 0) {
				cnid = newFile(fullName, volume);
			}
			AbstractFile* file = createAbstractFileFromFile(fopen(ent->d_name, "rb"));
			ASSERT(file != NULL, "fopen");
			HFSPlusCatalogFile* outFile = (HFSPlusCatalogFile*)getRecordByCNID(cnid, volume);
			writeToHFSFile(outFile, file, volume);
			file->close(file);
			free(outFile);
			/* Copy permissions from the source file */
			chmodFile(fullName, (int)st.st_mode, volume);
			printf("Setting permissions to %06o for %s\n", st.st_mode, fullName);

			if (assignSpecialPermissions) {
				if(strncmp(fullName, "/Applications/", sizeof("/Applications/") - 1) == 0) {
					/* warning: buffer size is not checked here! */
					char testBuffer[1024];
					testBuffer[0] = '\0';
					strcpy(testBuffer, "/Applications/");
					strcat(testBuffer, ent->d_name);
					strcat(testBuffer, ".app/");
					strcat(testBuffer, ent->d_name);
					if(strcmp(testBuffer, fullName) == 0) {
						if(strcmp(ent->d_name, "Installer") == 0
						|| strcmp(ent->d_name, "BootNeuter") == 0
						) {
							printf("Giving setuid permissions to %s...\n", fullName); fflush(stdout);
							chmodFile(fullName, 04755, volume);
						} else {
							printf("Giving permissions to %s\n", fullName); fflush(stdout);
							chmodFile(fullName, 0755, volume);
						}
					}
				} else if(strncmp(fullName, "/bin/", sizeof("/bin/") - 1) == 0
					|| strncmp(fullName, "/Applications/BootNeuter.app/bin/", sizeof("/Applications/BootNeuter.app/bin/") - 1) == 0
					|| strncmp(fullName, "/sbin/", sizeof("/sbin/") - 1) == 0
					|| strncmp(fullName, "/usr/sbin/", sizeof("/usr/sbin/") - 1) == 0
					|| strncmp(fullName, "/usr/bin/", sizeof("/usr/bin/") - 1) == 0
					|| strncmp(fullName, "/usr/libexec/", sizeof("/usr/libexec/") - 1) == 0
					|| strncmp(fullName, "/usr/local/bin/", sizeof("/usr/local/bin/") - 1) == 0
					|| strncmp(fullName, "/usr/local/sbin/", sizeof("/usr/local/sbin/") - 1) == 0
					|| strncmp(fullName, "/usr/local/libexec/", sizeof("/usr/local/libexec/") - 1) == 0
					) {
					chmodFile(fullName, 0755, volume);
					printf("Giving permissions to %s\n", fullName); fflush(stdout);
				}
			}
		} else {
			ASSERT(0, "addAllInFolder: cannot handle special file objects");
		}  /* if (S_IS<something>(st.st_mode) */
	}  /* for (... ent = readdir(dir)) */

	closedir(dir);

	releaseCatalogRecordList(theList);
}

static void extractOne(HFSCatalogNodeID folderID, char* name, HFSPlusCatalogRecord* record, Volume* volume, char* cwd) {
	HFSPlusCatalogFolder* folder;
	HFSPlusCatalogFile* file;
	AbstractFile* outFile;
	struct stat status;
	uint16_t fileType;
	size_t size;
	char* linkTarget;
#ifdef WIN32
	HFSPlusCatalogRecord* targetRecord;
#endif
	struct utimbuf times;

	if(strncmp(name, ".HFS+ Private Directory Data", sizeof(".HFS+ Private Directory Data") - 1) == 0 || name[0] == '\0') {
		return;
	}

	if(record->recordType == kHFSPlusFolderRecord) {
		folder = (HFSPlusCatalogFolder*)record;
		printf("folder: %s\n", name);
		if(stat(name, &status) != 0) {
			ASSERT(mkdir(name, 0755) == 0, "mkdir");
		}
		ASSERT(chdir(name) == 0, "chdir");
		extractAllInFolder(folder->folderID, volume);
		// TODO: chown . now that contents are extracted
		ASSERT(chdir(cwd) == 0, "chdir");
		chmod(name, folder->permissions.fileMode & 07777);
		times.actime = APPLE_TO_UNIX_TIME(folder->accessDate);
		times.modtime = APPLE_TO_UNIX_TIME(folder->contentModDate);
		utime(name, &times);
	} else if(record->recordType == kHFSPlusFileRecord) {
		file = (HFSPlusCatalogFile*)record;
		fileType = file->permissions.fileMode & S_IFMT;
		if(fileType == S_IFLNK) {
			// Symlinks are stored as a file with the symlink target in the file's data fork.
			// We read the target into a data buffer, then pass that filename to symlink().
			printf("symlink: %s\n", name);
			size = file->dataFork.logicalSize;
			if (size > 1024) {
				printf("WARNING: symlink target for %s longer than PATH_MAX?  Skipping.\n", name);
			} else {
				// symlink(3) needs a null terminator, which the file contents do not include
				linkTarget = (char*)malloc(size + 1);
				outFile = createAbstractFileFromMemory((void**)(&linkTarget), size);
				// write target from volume into linkTarget
				writeToFile(file, outFile, volume);
				linkTarget[size] = 0; // null terminator
				outFile->close(outFile);
#ifndef WIN32
				symlink(linkTarget, name);
#else
				// create copies instead of symlinks on Windows
				targetRecord = getRecordFromPath3(linkTarget, volume, NULL, NULL, TRUE, TRUE, folderID);
				if (targetRecord != NULL) {
					extractOne(folderID, name, targetRecord, volume, cwd);
				}
#endif
				free(linkTarget);
			}
		} else if(fileType == S_IFREG) {
			printf("file: %s\n", name);
			outFile = createAbstractFileFromFile(fopen(name, "wb"));
			if(outFile != NULL) {
				writeToFile(file, outFile, volume);
				// TODO: fchown to replicate ownership
#ifndef WIN32
				fchmod(fileno((FILE*)outFile->data), file->permissions.fileMode & 07777);
#endif
				outFile->close(outFile);
#ifdef WIN32
				chmod(name, file->permissions.fileMode & 07777);
#endif
				times.actime = APPLE_TO_UNIX_TIME(file->accessDate);
				times.modtime = APPLE_TO_UNIX_TIME(file->contentModDate);
				utime(name, &times);
			} else {
				printf("WARNING: cannot fopen %s\n", name);
			}
		} else {
			printf("unsupported: %s\n", name);
		}
	}
}

void extractAllInFolder(HFSCatalogNodeID folderID, Volume* volume) {
	CatalogRecordList* list;
	CatalogRecordList* theList;
	char cwd[1024];
	char* name;

	ASSERT(getcwd(cwd, 1024) != NULL, "cannot get current working directory");

	theList = list = getFolderContents(folderID, volume);

	while(list != NULL) {
		name = unicodeToAscii(&list->name);
		extractOne(folderID, name, list->record, volume, cwd);
		free(name);
		list = list->next;
	}
	releaseCatalogRecordList(theList);
}


void addall_hfs(Volume* volume, const char* dirToMerge, const char* dest) {
	addall_hfs_with_policies(volume, dirToMerge, dest, kIncomingSymlinksTraverse, TRUE);
}

void addall_hfs_with_policies(
			Volume* volume, const char* dirToMerge, const char* dest,
			IncomingSymlinksPolicy symlinkPolicy, char assignSpecialPermissions) {
	char cwd[1024];
	ASSERT(getcwd(cwd, 1024) != NULL, "addall_hfs_with_policies: getcwd failed");

	if(chdir(dirToMerge) != 0) {
		printf("Cannot open that directory: %s\n", dirToMerge);
		exit(1);
	}

	HFSPlusCatalogRecord* record = getRecordFromPath(dest, volume, NULL, NULL);
	ASSERT(record, "No such file or directory");
	HFSPlusCatalogFolder* folderRecord = tryCatalogRecordAsFolder(record);
	ASSERT(folderRecord, "Not a folder");
	
	/* reduced limit leaves room for appending '/' */
	size_t nChars = strlen(dest);
	ASSERT(nChars < 1023, "addall_hfs_with_policies: dest too long");
	char pathWithSlash[1024];
	memcpy(pathWithSlash, dest, nChars+1);
	if (pathWithSlash[nChars-1] != '/') {
		pathWithSlash[nChars] = '/';
		pathWithSlash[nChars+1] = '\0';
	}

	addAllInFolderWithPolicies(folderRecord->folderID, volume,  pathWithSlash,
					symlinkPolicy, assignSpecialPermissions);
	ASSERT(chdir(cwd) == 0, "chdir");
	free(record);
}

int copyAcrossVolumes(Volume* volume1, Volume* volume2, char* path1, char* path2) {
	void* buffer;
	size_t bufferSize;
	AbstractFile* tmpFile;
	int ret;

	buffer = malloc(1);
	bufferSize = 0;
	tmpFile = createAbstractFileFromMemoryFile((void**)&buffer, &bufferSize);

	if(!silence)
	{
		printf("retrieving... "); fflush(stdout);
	}

	get_hfs(volume1, path1, tmpFile);
	tmpFile->seek(tmpFile, 0);

	if(!silence)
	{
		printf("writing (%ld)... ", (long) tmpFile->getLength(tmpFile)); fflush(stdout);
	}

	ret = add_hfs(volume2, tmpFile, path2);

	if(!silence)
	{
		printf("done\n");
	}

	free(buffer);

	return ret;
}

void displayFolder(HFSCatalogNodeID folderID, Volume* volume) {
	CatalogRecordList* list;
	CatalogRecordList* theList;
	HFSPlusCatalogFolder* folder;
	HFSPlusCatalogFile* file;
	time_t fileTime;
	struct tm *date;
	HFSPlusDecmpfs* compressData;
	size_t attrSize;

	theList = list = getFolderContents(folderID, volume);

	while(list != NULL) {
		if(list->record->recordType == kHFSPlusFolderRecord) {
			folder = (HFSPlusCatalogFolder*)list->record;
			printf("%06o ", folder->permissions.fileMode);
			printf("%3d ", folder->permissions.ownerID);
			printf("%3d ", folder->permissions.groupID);
			printf("%12d ", folder->valence);
			fileTime = APPLE_TO_UNIX_TIME(folder->contentModDate);
		} else if(list->record->recordType == kHFSPlusFileRecord) {
			file = (HFSPlusCatalogFile*)list->record;
			printf("%06o ", file->permissions.fileMode);
			printf("%3d ", file->permissions.ownerID);
			printf("%3d ", file->permissions.groupID);
			if(file->permissions.ownerFlags & UF_COMPRESSED) {
				attrSize = getAttribute(volume, file->fileID, "com.apple.decmpfs", (uint8_t**)(&compressData));
				flipHFSPlusDecmpfs(compressData);
				printf("%12" PRId64 " ", compressData->size);
				free(compressData);
			} else {
				printf("%12" PRId64 " ", file->dataFork.logicalSize);
			}
			fileTime = APPLE_TO_UNIX_TIME(file->contentModDate);
		}

		date = localtime(&fileTime);
		if(date != NULL) {
			printf("%2d/%2d/%4d %02d:%02d ", date->tm_mon, date->tm_mday, date->tm_year + 1900, date->tm_hour, date->tm_min);
		} else {
			printf("                 ");
		}

		printUnicode(&list->name);
		printf("\n");

		list = list->next;
	}

	releaseCatalogRecordList(theList);
}

void displayFileLSLine(Volume* volume, HFSPlusCatalogFile* file, const char* name) {
	time_t fileTime;
	struct tm *date;
	HFSPlusDecmpfs* compressData;

	printf("%06o ", file->permissions.fileMode);
	printf("%3d ", file->permissions.ownerID);
	printf("%3d ", file->permissions.groupID);

	if(file->permissions.ownerFlags & UF_COMPRESSED) {
		getAttribute(volume, file->fileID, "com.apple.decmpfs", (uint8_t**)(&compressData));
		flipHFSPlusDecmpfs(compressData);
		printf("%12" PRId64 " ", compressData->size);
		free(compressData);
	} else {
		printf("%12" PRId64 " ", file->dataFork.logicalSize);
	}

	fileTime = APPLE_TO_UNIX_TIME(file->contentModDate);
	date = localtime(&fileTime);
	if(date != NULL) {
		printf("%2d/%2d/%4d %2d:%02d ", date->tm_mon, date->tm_mday, date->tm_year + 1900, date->tm_hour, date->tm_min);
	} else {
		printf("                 ");
	}
	printf("%s\n", name);

	XAttrList* next;
	XAttrList* attrs = getAllExtendedAttributes(file->fileID, volume);
	if(attrs != NULL) {
		printf("Extended attributes\n");
		while(attrs != NULL) {
			next = attrs->next;
			printf("\t%s\n", attrs->name);
			free(attrs->name);
			free(attrs);
			attrs = next;
		}
	}
}

void hfs_ls(Volume* volume, const char* path) {
	HFSPlusCatalogRecord* record;
	char* name;

	record = getRecordFromPath(path, volume, &name, NULL);

	printf("%s: \n", name);
	if(record != NULL) {
		if(record->recordType == kHFSPlusFolderRecord)
			displayFolder(((HFSPlusCatalogFolder*)record)->folderID, volume);
		else
			displayFileLSLine(volume, (HFSPlusCatalogFile*)record, name);
	} else {
		printf("No such file or directory\n");
	}

	printf("Total filesystem size: %d, free: %d\n", (volume->volumeHeader->totalBlocks - volume->volumeHeader->freeBlocks) * volume->volumeHeader->blockSize, volume->volumeHeader->freeBlocks * volume->volumeHeader->blockSize);

	free(record);
}

void hfs_untar(Volume* volume, AbstractFile* tarFile) {
	size_t tarSize = tarFile->getLength(tarFile);
	size_t curRecord = 0;
	char block[512];

	while(curRecord < tarSize) {
		tarFile->seek(tarFile, curRecord);
		tarFile->read(tarFile, block, 512);

		uint32_t mode = 0;
		char* fileName = NULL;
		const char* target = NULL;
		uint32_t type = 0;
		uint32_t size;
		uint32_t uid;
		uint32_t gid;

		sscanf(&block[100], "%o", &mode);
		fileName = &block[0];
		sscanf(&block[156], "%o", &type);
		target = &block[157];
		sscanf(&block[124], "%o", &size);
		sscanf(&block[108], "%o", &uid);
		sscanf(&block[116], "%o", &gid);

		if(fileName[0] == '\0')
			break;

		if(fileName[0] == '.' && fileName[1] == '/') {
			fileName += 2;
		}

		if(fileName[0] == '\0')
			goto loop;

		if(fileName[strlen(fileName) - 1] == '/')
			fileName[strlen(fileName) - 1] = '\0';

		HFSPlusCatalogRecord* record = getRecordFromPath3(fileName, volume, NULL, NULL, TRUE, FALSE, kHFSRootFolderID);
		if(record) {
			if(record->recordType == kHFSPlusFolderRecord || type == 5) {
				if(!silence)
					printf("ignoring %s, type = %d\n", fileName, type);
				free(record);
				goto loop;
			} else {
				printf("replacing %s\n", fileName);
				free(record);
				removeFile(fileName, volume);
			}
		}

		if(type == 0) {
			if(!silence)
				printf("file: %s (%04o), size = %d\n", fileName, mode, size);
			void* buffer = malloc(size);
			tarFile->seek(tarFile, curRecord + 512);
			tarFile->read(tarFile, buffer, size);
			AbstractFile* inFile = createAbstractFileFromMemory(&buffer, size);
			add_hfs(volume, inFile, fileName);
			free(buffer);
		} else if(type == 5) {
			if(!silence)
				printf("directory: %s (%04o)\n", fileName, mode);
			newFolder(fileName, volume);
		} else if(type == 2) {
			if(!silence)
				printf("symlink: %s (%04o) -> %s\n", fileName, mode, target);
			makeSymlink(fileName, target, volume);
		}

		chmodFile(fileName, mode, volume);
		chownFile(fileName, uid, gid, volume);

loop:

		curRecord = (curRecord + 512) + ((size + 511) / 512 * 512);
	}

}

