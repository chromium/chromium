#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <getopt.h>

#include "common.h"
#include "hfs/hfslib.h"
#include "hfs/hfsplus.h"
#include "abstractfile.h"
#include "parse_data_param.h"
#include "sizedbuf.h"
#include <inttypes.h>

char endianness;


void cmd_ls(Volume* volume, int argc, char *argv[]) {
	if (argc > 1) {
		hfs_ls(volume, argv[1]);
	} else {
		hfs_ls(volume, "/");
	}
}

void cmd_cat(Volume* volume, int argc, char *argv[]) {
	HFSPlusCatalogRecord* record;
	AbstractFile* stdoutFile;

	record = getRecordFromPath(argv[1], volume, NULL, NULL);

	stdoutFile = createAbstractFileFromFile(stdout);

	ASSERT(record != NULL, "No such file or directory");
	ASSERT(record->recordType == kHFSPlusFileRecord, "Not a file");
	writeToFile((HFSPlusCatalogFile*)record, stdoutFile, volume);

	free(record);
	free(stdoutFile);
}

void cmd_extract(Volume* volume, int argc, char *argv[]) {
	HFSPlusCatalogRecord* record;
	AbstractFile *outFile;

	if (argc < 3) {
		fprintf(stderr, "Not enough arguments\n");
		exit(2);
	}

	outFile = createAbstractFileFromFile(fopen(argv[2], "wb"));
	ASSERT(outFile != NULL, "cannot create file");

	record = getRecordFromPath(argv[1], volume, NULL, NULL);

	ASSERT(record != NULL, "No such file or directory");
	ASSERT(record->recordType == kHFSPlusFileRecord, "Not a file");
	writeToFile((HFSPlusCatalogFile*)record, outFile, volume);

	outFile->close(outFile);
	free(record);
}

void cmd_mv(Volume* volume, int argc, char *argv[]) {
	if (argc > 2) {
		move(argv[1], argv[2], volume);
	} else {
		fprintf(stderr, "Not enough arguments\n");
		exit(2);
	}
}

void cmd_symlink(Volume* volume, int argc, char *argv[]) {
	if (argc > 2) {
		makeSymlink(argv[1], argv[2], volume);
	} else {
		fprintf(stderr, "Not enough arguments\n");
		exit(2);
	}
}

void cmd_mkdir(Volume* volume, int argc, char *argv[]) {
	if (argc > 1) {
		newFolder(argv[1], volume);
	} else {
		fprintf(stderr, "Not enough arguments\n");
		exit(2);
	}
}

void cmd_add(Volume* volume, int argc, char *argv[]) {
	AbstractFile *inFile;

	if (argc < 3) {
		fprintf(stderr, "Not enough arguments\n");
		exit(2);
	}

	inFile = createAbstractFileFromFile(fopen(argv[1], "rb"));
	ASSERT(inFile != NULL, "File to add not found");

	add_hfs(volume, inFile, argv[2]);
}

void cmd_rm(Volume* volume, int argc, char *argv[]) {
	if (argc > 1) {
		removeFile(argv[1], volume);
	} else {
		fprintf(stderr, "Not enough arguments\n");
		exit(2);
	}
}

void cmd_chmod(Volume* volume, int argc, char *argv[]) {
	int mode;

	if (argc > 2) {
		sscanf(argv[1], "%o", &mode);
		chmodFile(argv[2], mode, volume);
	} else {
		fprintf(stderr, "Not enough arguments\n");
		exit(2);
	}
}

void cmd_attr(Volume* volume, int argc, char *argv[]) {
	int mode;

	if (argc > 2) {
		attrFile(argv[1], argv[2], volume);
	} else {
		fprintf(stderr, "Not enough arguments\n");
		exit(2);
	}
}

void cmd_extractall(Volume* volume, int argc, char *argv[]) {
	HFSPlusCatalogRecord* record;
	char cwd[1024];
	char* name;

	ASSERT(getcwd(cwd, 1024) != NULL, "cannot get current working directory");

	if (argc > 1) {
		record = getRecordFromPath(argv[1], volume, &name, NULL);
	} else {
		record = getRecordFromPath("/", volume, &name, NULL);
	}

	if (argc > 2) {
		ASSERT(chdir(argv[2]) == 0, "chdir");
	}

	ASSERT(record != NULL, "No such file or directory");
	ASSERT(record->recordType == kHFSPlusFolderRecord, "Not a folder");
	extractAllInFolder(((HFSPlusCatalogFolder*)record)->folderID, volume);

	free(record);
	ASSERT(chdir(cwd) == 0, "chdir (reset)");
}


void cmd_rmall(Volume* volume, int argc, char *argv[]) {
	HFSPlusCatalogRecord* record;
	char* name;
	char initPath[1024];
	int lastCharOfPath;

	if (argc > 1) {
		record = getRecordFromPath(argv[1], volume, &name, NULL);
		strcpy(initPath, argv[1]);
		lastCharOfPath = strlen(argv[1]) - 1;
		if (argv[1][lastCharOfPath] != '/') {
			initPath[lastCharOfPath + 1] = '/';
			initPath[lastCharOfPath + 2] = '\0';
		}
	} else {
		record = getRecordFromPath("/", volume, &name, NULL);
		initPath[0] = '/';
		initPath[1] = '\0';
	}

	ASSERT(record != NULL, "No such file or directory");
	ASSERT(record->recordType == kHFSPlusFolderRecord, "Not a folder");
	removeAllInFolder(((HFSPlusCatalogFolder*)record)->folderID, volume, initPath);

	free(record);
}

void cmd_addall(Volume* volume, IncomingSymlinksPolicy symlink_policy,
                char assign_special_permissions, int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Not enough arguments\n");
		exit(2);
	}

	if (argc > 2) {
		addall_hfs_with_policies(volume, argv[1], argv[2], symlink_policy, assign_special_permissions);
	} else {
		addall_hfs_with_policies(volume, argv[1], "/", symlink_policy, assign_special_permissions);
	}
}

void cmd_grow(Volume* volume, int argc, char *argv[]) {
	uint64_t newSize;

	if (argc < 2) {
		fprintf(stderr, "Not enough arguments\n");
		exit(2);
	}

	newSize = 0;
	sscanf(argv[1], "%" PRId64, &newSize);

	grow_hfs(volume, newSize);

	printf("grew volume: %" PRId64 "\n", newSize);
}

void cmd_getattr(Volume* volume, int argc, char *argv[]) {
	HFSPlusCatalogRecord* record;

	if (argc < 3) {
		fprintf(stderr, "Not enough arguments: getattr <path> <attribute-name>");
		exit(2);
	}

	record = getRecordFromPath(argv[1], volume, NULL, NULL);

	ASSERT(record != NULL, "No such file or directory");

	HFSCatalogNodeID id;

	HFSPlusCatalogFile* file_record = tryCatalogRecordAsFile(record);
	if (file_record != NULL) {
		id = file_record->fileID;
	} else {
		HFSPlusCatalogFolder* folder_record = tryCatalogRecordAsFolder(record);
		ASSERT(folder_record != NULL, "Not a file or folder");
		id = folder_record->folderID;
	}

	uint8_t* data;
	size_t size = getAttribute(volume, id, argv[2], &data);
	ASSERT(size > 0, "No such attribute");
	fwrite(data, size, 1, stdout);
	free(data);

	free(record);
}

void cmd_setattr(Volume* volume, DataParamParserPtr data_param_parser, int argc, char *argv[]) {
	HFSPlusCatalogRecord* record;

	if (argc < 4) {
		fprintf(stderr, "Not enough arguments: setattr <path> <attribute-name> <attribute-value>\n");
		exit(2);
	}

	record = getRecordFromPath(argv[1], volume, NULL, NULL);
	ASSERT(record != NULL, "No such file or directory");

	HFSCatalogNodeID id;
	HFSPlusCatalogFile* fileRecord = tryCatalogRecordAsFile(record);
	HFSPlusCatalogFolder* folderRecord = tryCatalogRecordAsFolder(record);
	if (fileRecord != NULL) {
		id = fileRecord->fileID;
	} else {
		ASSERT(folderRecord != NULL, "setattr: unidentified record type");
		id = folderRecord -> folderID;
	}

	SizedBuf* value = data_param_parser(argv[3]);
	// Write empty data as two zero-value bytes instead. This produces an
	// empty string if string decoding is intended, then rounds up to an even
	// record size.
	if (value->len == 0) {
		free(value);
		value = ZAllocBuf(2);
	}

	if ((value->len & 0x1) == 0x1) {
		// HFS record sizes must be even.  Pad the given data with one 0 to
		// maintain this invariant.  Note that macOS `xattr` appears to do
		// this silently.
		ASSERT(value->cap >= value->len, "SizedBuf cap below len is bad news");
		if (value->cap == value->len) {
			// No space for the 0. Realloc.
			value = ReallocBuf(value, value->len + 1);
		}
		// There is (now) definitely room to extend by one byte.
		value->data[value->len] = 0;
		value->len += 1;
	}

	ASSERT(setAttribute(volume, id, argv[2], value->data, value->len), "setAttribute");

	if (fileRecord != NULL) {
		fileRecord -> flags |= kHFSHasAttributesMask;
	} else {
		ASSERT(folderRecord != NULL, "unknown record type the second time?!");
		folderRecord -> flags |= kHFSHasAttributesMask;
	}

	// record is still the same object as fileRecord and folderRecord; the
	// pointers of alternate types are aliases. This is one of the reasons we
	// require -fno-strict-aliasing.
	updateCatalog(volume, record);
	free(record);
}

void TestByteOrder()
{
	short int word = 0x0001;
	char *byte = (char *) &word;
	endianness = byte[0] ? IS_LITTLE_ENDIAN : IS_BIG_ENDIAN;
}

void usage(const char* name) {
	char dataFormats[256] = {0};
	size_t needed = dataParamFormats(dataFormats, 256);
	if (needed > 256) {
		fprintf(stderr, "warning: data format list truncated, needed %zu bytes\n",
			    needed);
	}
	printf("usage: %s <image-file> <ls|cat|mv|mkdir|add|rm|chmod|extract|extractall|rmall|addall|attr|setattr|debug> <arguments>\n", name);
	printf("OPTIONS:\n");
	printf("\t--symlinks, -s        <fail, traverse, clone_link>: how to handle symlinks\n");
	printf("\t                      in the input directory in command `addall`\n");
	printf("\t--special-modes, -m   <yes, no>: whether to chmod files in the volume\n");
	printf("\t                      when they are recognized with a name or path\n");
	printf("\t                      where OS setup or iPhone jailbreaking would\n");
	printf("\t                      require special permissions; specific to `addall`\n");
	printf("\t--data-format, -d     `setattr` only: encoding format for xattr value\n");
	printf("\t                       known formats: %s\n", dataFormats);
}

IncomingSymlinksPolicy must_parse_symlink_policy(const char* policy, const char* bin_name) {
	if (strcmp(policy, "fail") == 0) {
		return kIncomingSymlinksFail;
	}
	if (strcmp(policy, "traverse") == 0) {
		return kIncomingSymlinksTraverse;
	}
	if (strcmp(policy, "clone_link") == 0) {
		return kIncomingSymlinksCloneLink;
	}

	fprintf(stderr, "error: Unrecognized value for --symlinks: %s\n", policy);
	usage(bin_name);
	exit(1);
}

char must_parse_bool(const char* str, const char* flag_name, const char* bin_name) {
	if (str == NULL || str[0] == '\0') {
		fprintf(stderr, "no value provided for --%s\n", flag_name);
		usage(bin_name);
		exit(1);
	}
	if (strchr("yYtT1", str[0])) {
		return TRUE;
	}
	if (strchr("nNfF0", str[0])) {
		return FALSE;
	}
	fprintf(stderr, "error: Cannot recognize a bool in value for --%s: %s\n", flag_name, str);
	usage(bin_name);
	exit(1);
}

int main(int argc, char *argv[]) {
	const char* bin_name = argv[0];

	// Default values, may be overridden by flags
	IncomingSymlinksPolicy symlink_policy = kIncomingSymlinksTraverse;
	char assign_special_permissions = TRUE;
	DataParamParserPtr data_param_parser = dataParamParserForFormat("literal");

	TestByteOrder();

	const char* optstring = "s:m:d:";
	const struct option longopts[] = {
		{"symlinks", required_argument, NULL, 's'},
		{"special-modes", required_argument, NULL, 'm'},
		{"data-format", required_argument, NULL, 'd'},
		{NULL, 0, NULL, 0}
	};
	for(
		int opt = getopt_long(argc, argv, optstring, longopts, NULL);
		opt != -1;
		opt = getopt_long(argc, argv, optstring, longopts, NULL)
	){
		switch (opt) {
			case 's':
				symlink_policy = must_parse_symlink_policy(optarg, bin_name);
				break;
			case 'm':
				assign_special_permissions = must_parse_bool(optarg, "special-modes", bin_name);
				break;
			case 'd':
				data_param_parser = dataParamParserForFormat(optarg);
				break;
			default:
				usage(bin_name);
				exit(2);
		}
	}
	argc -= optind - 1;
	argv += optind - 1;

	if (argc < 3) {
		usage(bin_name);
		return 2;
	}

	io_func* io = openFlatFile(argv[1]);
	ASSERT(io != NULL, "Cannot open image file.");
	Volume* volume = openVolume(io);
	ASSERT(volume != NULL, "Cannot open volume.");


	if (argc > 1) {
		if (strcmp(argv[2], "ls") == 0) {
			cmd_ls(volume, argc - 2, argv + 2);
		} else if (strcmp(argv[2], "cat") == 0) {
			cmd_cat(volume, argc - 2, argv + 2);
		} else if (strcmp(argv[2], "mv") == 0) {
			cmd_mv(volume, argc - 2, argv + 2);
		} else if (strcmp(argv[2], "symlink") == 0) {
			cmd_symlink(volume, argc - 2, argv + 2);
		} else if (strcmp(argv[2], "mkdir") == 0) {
			cmd_mkdir(volume, argc - 2, argv + 2);
		} else if (strcmp(argv[2], "add") == 0) {
			cmd_add(volume, argc - 2, argv + 2);
		} else if (strcmp(argv[2], "rm") == 0) {
			cmd_rm(volume, argc - 2, argv + 2);
		} else if (strcmp(argv[2], "chmod") == 0) {
			cmd_chmod(volume, argc - 2, argv + 2);
		} else if (strcmp(argv[2], "attr") == 0) {
			cmd_attr(volume, argc - 2, argv + 2);
		} else if (strcmp(argv[2], "extract") == 0) {
			cmd_extract(volume, argc - 2, argv + 2);
		} else if (strcmp(argv[2], "extractall") == 0) {
			cmd_extractall(volume, argc - 2, argv + 2);
		} else if (strcmp(argv[2], "rmall") == 0) {
			cmd_rmall(volume, argc - 2, argv + 2);
		} else if (strcmp(argv[2], "addall") == 0) {
			cmd_addall(volume, symlink_policy, assign_special_permissions, argc - 2, argv + 2);
		} else if (strcmp(argv[2], "grow") == 0) {
			cmd_grow(volume, argc - 2, argv + 2);
		} else if (strcmp(argv[2], "getattr") == 0) {
			cmd_getattr(volume, argc - 2, argv + 2);
		} else if (strcmp(argv[2], "setattr") == 0) {
			cmd_setattr(volume, data_param_parser, argc - 2, argv + 2);
		} else if (strcmp(argv[2], "debug") == 0) {
			if (argc > 3 && strcmp(argv[3], "verbose") == 0) {
				debugBTree(volume->catalogTree, TRUE);
			} else {
				debugBTree(volume->catalogTree, FALSE);
			}
		} else if (strcmp(argv[2], "debugattrs") == 0) {
			if (argc > 3 && strcmp(argv[3], "verbose") == 0) {
				debugBTree(volume->attrTree, TRUE);
			} else {
				debugBTree(volume->attrTree, FALSE);
			}
		} else {
			fprintf(stderr, "unrecognized verb: %s\n", argv[2]);
			usage(bin_name);
			exit(2);
		}
	}

	closeVolume(volume);
	CLOSE(io);

	return 0;
}
