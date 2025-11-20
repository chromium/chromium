#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

#include <dmg/dmg.h>
#include <dmg/filevault.h>

char endianness;

void TestByteOrder()
{
	short int word = 0x0001;
	char *byte = (char *) &word;
	endianness = byte[0] ? IS_LITTLE_ENDIAN : IS_BIG_ENDIAN;
}

int buildInOut(const char* source, const char* dest, AbstractFile** in, AbstractFile** out) {
	*in = createAbstractFileFromFile(fopen(source, "rb"));
	if(!(*in)) {
		printf("cannot open source: %s\n", source);
		return FALSE;
	}

	*out = createAbstractFileFromFile(strcmp(dest, "-") == 0 ? stdout : fopen(dest, "wb"));
	if(!(*out)) {
		(*in)->close(*in);
		printf("cannot open destination: %s\n", dest);
		return FALSE;
	}

	return TRUE;
}

void usage(const char *name) {
	printf("usage: %s [OPTIONS] [extract|build|build2048|res|iso|dmg|attribute] <in> <out> (partition)\n", name);
	printf("OPTIONS:\n");
	printf("\t--key, -k           key\n");
	printf("\t--compression, -c   compressor name (%s)\n", compressionNames());
	printf("\t--level, -l         compression level\n");
	printf("\t--run-sectors, -r   run size (in sectors)\n");
	exit(2);
}

int main(int argc, char* argv[]) {
	int partNum;
	AbstractFile* in;
	AbstractFile* out;
	int opt;
	char *cmd, *infile, *outfile;
	char *key = NULL;
	Compressor comp = {.level = COMPRESSION_LEVEL_DEFAULT };
	int ret;
	int runSectors = DEFAULT_SECTORS_AT_A_TIME;

	TestByteOrder();
	initDefaultCompressor(&comp);

	const char *optstring = "k:c:l:r:";
	const struct option longopts[] = {
		{"key", required_argument, NULL, 'k'},
		{"compression", required_argument, NULL, 'c'},
		{"level", required_argument, NULL, 'l'},
		{"run-sectors", required_argument, NULL, 'r'},
		{NULL, 0, NULL, 0},
	};

	while ((opt = getopt_long(argc, argv, optstring, longopts, NULL)) != -1) {
		switch (opt) {
			case 'k':
				key = optarg;
				break;
			case 'c':
				ret = getCompressor(&comp, optarg);
				if (ret != 0) {
					fprintf(stderr, "Unknown compressor \"%s\"\nAllowed options are: %s\n", optarg, compressionNames());
					return 2;
				}
				break;
			case 'l':
				sscanf(optarg, "%d", &comp.level);
				break;
			case 'r':
				sscanf(optarg, "%d", &runSectors);
				if (runSectors < DEFAULT_SECTORS_AT_A_TIME) {
					fprintf(stderr, "Run size must be at least %d sectors\n", DEFAULT_SECTORS_AT_A_TIME);
					return 2;
				}
				break;
			default:
				usage(argv[0]);
		}
	}

	if (argc < optind + 3) {
		usage(argv[0]);
	}
	cmd = argv[optind++];
	infile = argv[optind++];
	outfile = argv[optind++];

	if(!buildInOut(infile, outfile, &in, &out)) {
		return -1;
	}
	if(key != NULL) {
		in = createAbstractFileFromFileVault(in, key);
	}

	if(strcmp(cmd, "extract") == 0) {
		partNum = -1;
		if (optind < argc) {
				sscanf(argv[optind++], "%d", &partNum);
		}
		extractDmg(in, out, partNum);
	} else if(strcmp(cmd, "build") == 0) {
		char *anchor = NULL;
		if (optind < argc) {
			anchor = argv[optind++];
		}
		buildDmg(in, out, SECTOR_SIZE, anchor, &comp, runSectors);
	} else if(strcmp(cmd, "build2048") == 0) {
		buildDmg(in, out, 2048, NULL, &comp, runSectors);
	} else if(strcmp(cmd, "res") == 0) {
		outResources(in, out);
	} else if(strcmp(cmd, "iso") == 0) {
		convertToISO(in, out);
	} else if(strcmp(cmd, "dmg") == 0) {
		convertToDMG(in, out, &comp, runSectors);
	} else if(strcmp(cmd, "attribute") == 0) {
		char *anchor, *data;
		if(argc < optind + 2) {
			printf("Not enough arguments: attribute <in> <out> <sentinel> <string>");
			return 2;
		}
		anchor = argv[optind++];
		data = argv[optind++];
		updateAttribution(in, out, anchor, data, strlen(data));
	}

	return 0;
}
