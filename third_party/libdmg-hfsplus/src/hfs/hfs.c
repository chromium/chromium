#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <hfs/hfsplus.h>
#include <dirent.h>

#include <hfs/hfslib.h>
#include "abstractfile.h"
#include <inttypes.h>

char endianness;


void cmd_ls(Volume* volume, int argc, const char *argv[]) {
	if(argc > 1)
		hfs_ls(volume, argv[1]);
	else
		hfs_ls(volume, "/");
}

void cmd_cat(Volume* volume, int argc, const char *argv[]) {
	HFSPlusCatalogRecord* record;
	AbstractFile* stdoutFile;

	record = getRecordFromPath(argv[1], volume, NULL, NULL);

	stdoutFile = createAbstractFileFromFile(stdout);
	
	if(record != NULL) {
		if(record->recordType == kHFSPlusFileRecord)
			writeToFile((HFSPlusCatalogFile*)record, stdoutFile, volume);
		else
			printf("Not a file\n");
	} else {
		printf("No such file or directory\n");
	}
	
	free(record);
	free(stdoutFile);
}

void cmd_extract(Volume* volume, int argc, const char *argv[]) {
	HFSPlusCatalogRecord* record;
	AbstractFile *outFile;
	
	if(argc < 3) {
		printf("Not enough arguments");
		return;
	}
	
	outFile = createAbstractFileFromFile(fopen(argv[2], "wb"));
	
	if(outFile == NULL) {
		printf("cannot create file");
	}
	
	record = getRecordFromPath(argv[1], volume, NULL, NULL);
	
	if(record != NULL) {
		if(record->recordType == kHFSPlusFileRecord)
			writeToFile((HFSPlusCatalogFile*)record, outFile, volume);
		else
			printf("Not a file\n");
	} else {
		printf("No such file or directory\n");
	}
	
	outFile->close(outFile);
	free(record);
}

void cmd_mv(Volume* volume, int argc, const char *argv[]) {
	if(argc > 2) {
		move(argv[1], argv[2], volume);
	} else {
		printf("Not enough arguments");
	}
}

void cmd_symlink(Volume* volume, int argc, const char *argv[]) {
	if(argc > 2) {
		makeSymlink(argv[1], argv[2], volume);
	} else {
		printf("Not enough arguments");
	}
}

void cmd_mkdir(Volume* volume, int argc, const char *argv[]) {
	if(argc > 1) {
		newFolder(argv[1], volume);
	} else {
		printf("Not enough arguments");
	}
}

void cmd_add(Volume* volume, int argc, const char *argv[]) {
	AbstractFile *inFile;
	
	if(argc < 3) {
		printf("Not enough arguments");
		return;
	}
	
	inFile = createAbstractFileFromFile(fopen(argv[1], "rb"));
	
	if(inFile == NULL) {
		printf("file to add not found");
	}

	add_hfs(volume, inFile, argv[2]);
}

void cmd_rm(Volume* volume, int argc, const char *argv[]) {
	if(argc > 1) {
		removeFile(argv[1], volume);
	} else {
		printf("Not enough arguments");
	}
}

void cmd_chmod(Volume* volume, int argc, const char *argv[]) {
	int mode;
	
	if(argc > 2) {
		sscanf(argv[1], "%o", &mode);
		chmodFile(argv[2], mode, volume);
	} else {
		printf("Not enough arguments");
	}
}

void cmd_attr(Volume* volume, int argc, const char *argv[]) {
	int mode;

	if(argc > 2) {
		attrFile(argv[1], argv[2], volume);
	} else {
		printf("Not enough arguments");
	}
}

void cmd_extractall(Volume* volume, int argc, const char *argv[]) {
	HFSPlusCatalogRecord* record;
	char cwd[1024];
	char* name;
	
	ASSERT(getcwd(cwd, 1024) != NULL, "cannot get current working directory");
	
	if(argc > 1)
		record = getRecordFromPath(argv[1], volume, &name, NULL);
	else
		record = getRecordFromPath("/", volume, &name, NULL);
	
	if(argc > 2) {
		ASSERT(chdir(argv[2]) == 0, "chdir");
	}

	if(record != NULL) {
		if(record->recordType == kHFSPlusFolderRecord)
			extractAllInFolder(((HFSPlusCatalogFolder*)record)->folderID, volume);  
		else
			printf("Not a folder\n");
	} else {
		printf("No such file or directory\n");
	}
	free(record);
	
	ASSERT(chdir(cwd) == 0, "chdir");
}


void cmd_rmall(Volume* volume, int argc, const char *argv[]) {
	HFSPlusCatalogRecord* record;
	char* name;
	char initPath[1024];
	int lastCharOfPath;
	
	if(argc > 1) {
		record = getRecordFromPath(argv[1], volume, &name, NULL);
		strcpy(initPath, argv[1]);
		lastCharOfPath = strlen(argv[1]) - 1;
		if(argv[1][lastCharOfPath] != '/') {
			initPath[lastCharOfPath + 1] = '/';
			initPath[lastCharOfPath + 2] = '\0';
		}
	} else {
		record = getRecordFromPath("/", volume, &name, NULL);
		initPath[0] = '/';
		initPath[1] = '\0';	
	}
	
	if(record != NULL) {
		if(record->recordType == kHFSPlusFolderRecord) {
			removeAllInFolder(((HFSPlusCatalogFolder*)record)->folderID, volume, initPath);
		} else {
			printf("Not a folder\n");
		}
	} else {
		printf("No such file or directory\n");
	}
	free(record);
}

void cmd_addall(Volume* volume, int argc, const char *argv[]) {   
	if(argc < 2) {
		printf("Not enough arguments");
		return;
	}

	if(argc > 2) {
		addall_hfs(volume, argv[1], argv[2]);
	} else {
		addall_hfs(volume, argv[1], "/");
	}
}

void cmd_grow(Volume* volume, int argc, const char *argv[]) {
	uint64_t newSize;

	if(argc < 2) {
		printf("Not enough arguments\n");
		return;
	}
	
	newSize = 0;
	sscanf(argv[1], "%" PRId64, &newSize);

	grow_hfs(volume, newSize);

	printf("grew volume: %" PRId64 "\n", newSize);
}

void cmd_getattr(Volume* volume, int argc, const char *argv[]) {
	HFSPlusCatalogRecord* record;

	if(argc < 3) {
		printf("Not enough arguments: getattr <path> <attribute-name>");
		return;
	}

	record = getRecordFromPath(argv[1], volume, NULL, NULL);

	if(record != NULL) {
		HFSCatalogNodeID id;
		uint8_t* data;
		size_t size;
		if(record->recordType == kHFSPlusFileRecord)
			id = ((HFSPlusCatalogFile*)record)->fileID;
		else
			id = ((HFSPlusCatalogFolder*)record)->folderID;

		size = getAttribute(volume, id, argv[2], &data);

		if(size > 0) {
			fwrite(data, size, 1, stdout);
			free(data);
		} else {
			printf("No such attribute\n");
		}
	} else {
		printf("No such file or directory\n");
	}

	free(record);
}

void cmd_setattr(Volume* volume, int argc, const char *argv[]) {
	HFSPlusCatalogRecord* record;

	if(argc < 4) {
		printf("Not enough arguments: setattr <path> <attribute-name> <attribute-value>");
		return;
	}

	record = getRecordFromPath(argv[1], volume, NULL, NULL);

	if(record != NULL) {
		HFSCatalogNodeID id;
		uint8_t* data;
		size_t size;
		if(record->recordType == kHFSPlusFileRecord)
			id = ((HFSPlusCatalogFile*)record)->fileID;
		else
			id = ((HFSPlusCatalogFolder*)record)->folderID;

		// Note: this doesn't handle embedded nulls, string encodings, etc.
		size_t dataLen = strlen(argv[3]);
		if (dataLen == 0) {
			// Handle the empty string gracefully.
			dataLen = 1;
		}
		if((dataLen & 0x1) == 0x1) {
			// HFS record sizes must be even.  Pad the given data with one 0 to
			// maintain this invariant.  Note that macOS `xattr` appears to do
			// this silently.
			dataLen += 1;
		}
		data = malloc(sizeof(uint8_t) * (dataLen));
		memset(data, 0, dataLen);
		memcpy(data, argv[3], strlen(argv[3]));

		ASSERT(setAttribute(volume, id, argv[2], data, dataLen), "setAttribute");
	} else {
		printf("No such file or directory\n");
	}

	if(record->recordType == kHFSPlusFolderRecord) {
		((HFSPlusCatalogFolder*)record)->flags |= kHFSHasAttributesMask;
	} else if(record->recordType == kHFSPlusFileRecord) {
		((HFSPlusCatalogFile*)record)->flags |= kHFSHasAttributesMask;
	} else {
		printf("unknown record type %x\n", record->recordType);
	}

	updateCatalog(volume, record);

	free(record);
}

void TestByteOrder()
{
	short int word = 0x0001;
	char *byte = (char *) &word;
	endianness = byte[0] ? IS_LITTLE_ENDIAN : IS_BIG_ENDIAN;
}


int main(int argc, const char *argv[]) {
	io_func* io;
	Volume* volume;
	
	TestByteOrder();
	
	if(argc < 3) {
		printf("usage: %s <image-file> <ls|cat|mv|mkdir|add|rm|chmod|extract|extractall|rmall|addall|attr|debug> <arguments>\n", argv[0]);
		return 0;
	}
	
	io = openFlatFile(argv[1]);
	if(io == NULL) {
		fprintf(stderr, "error: Cannot open image-file.\n");
		return 1;
	}
	
	volume = openVolume(io); 
	if(volume == NULL) {
		fprintf(stderr, "error: Cannot open volume.\n");
		CLOSE(io);
		return 1;
	}
	
	if(argc > 1) {
		if(strcmp(argv[2], "ls") == 0) {
			cmd_ls(volume, argc - 2, argv + 2);
		} else if(strcmp(argv[2], "cat") == 0) {
			cmd_cat(volume, argc - 2, argv + 2);
		} else if(strcmp(argv[2], "mv") == 0) {
			cmd_mv(volume, argc - 2, argv + 2);
		} else if(strcmp(argv[2], "symlink") == 0) {
			cmd_symlink(volume, argc - 2, argv + 2);
		} else if(strcmp(argv[2], "mkdir") == 0) {
			cmd_mkdir(volume, argc - 2, argv + 2);
		} else if(strcmp(argv[2], "add") == 0) {
			cmd_add(volume, argc - 2, argv + 2);
		} else if(strcmp(argv[2], "rm") == 0) {
			cmd_rm(volume, argc - 2, argv + 2);
		} else if(strcmp(argv[2], "chmod") == 0) {
			cmd_chmod(volume, argc - 2, argv + 2);
		} else if(strcmp(argv[2], "attr") == 0) {
			cmd_attr(volume, argc - 2, argv + 2);
		} else if(strcmp(argv[2], "extract") == 0) {
			cmd_extract(volume, argc - 2, argv + 2);
		} else if(strcmp(argv[2], "extractall") == 0) {
			cmd_extractall(volume, argc - 2, argv + 2);
		} else if(strcmp(argv[2], "rmall") == 0) {
			cmd_rmall(volume, argc - 2, argv + 2);
		} else if(strcmp(argv[2], "addall") == 0) {
			cmd_addall(volume, argc - 2, argv + 2);
		} else if(strcmp(argv[2], "grow") == 0) {
			cmd_grow(volume, argc - 2, argv + 2);
		} else if(strcmp(argv[2], "getattr") == 0) {
			cmd_getattr(volume, argc - 2, argv + 2);
		} else if(strcmp(argv[2], "setattr") == 0) {
			cmd_setattr(volume, argc - 2, argv + 2);
		} else if(strcmp(argv[2], "debug") == 0) {
			if(argc > 3 && strcmp(argv[3], "verbose") == 0) {
				debugBTree(volume->catalogTree, TRUE);
			} else {
				debugBTree(volume->catalogTree, FALSE);
			}
		} else if(strcmp(argv[2], "debugattrs") == 0) {
			if(argc > 3 && strcmp(argv[3], "verbose") == 0) {
				debugBTree(volume->attrTree, TRUE);
			} else {
				debugBTree(volume->attrTree, FALSE);
			}
		}
	}
	
	closeVolume(volume);
	CLOSE(io);
	
	return 0;
}
