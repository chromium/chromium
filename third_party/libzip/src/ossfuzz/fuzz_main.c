#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* fuzz target entry point, works without libFuzzer */

extern int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int main(int argc, char **argv) {
    FILE *f = NULL;
    char *buf = NULL;
    long siz_buf;

    if (argc < 2) {
        fprintf(stderr, "no input file\n");
        goto err;
    }

    f = fopen(argv[1], "rb");
    if (f == NULL) {
        fprintf(stderr, "error opening input file %s\n", argv[1]);
        goto err;
    }

    fseek(f, 0, SEEK_END);

    siz_buf = ftell(f);
    rewind(f);

    if (siz_buf < 1) {
        fprintf(stderr, "zero-byte file not supported\n");
        goto err;
    }

    buf = (char *)malloc(siz_buf);
    if (buf == NULL) {
        fprintf(stderr, "malloc() failed\n");
        goto err;
    }

    if (fread(buf, siz_buf, 1, f) != 1) {
        fprintf(stderr, "fread() failed\n");
        goto err;
    }
    fclose(f);
    f = NULL;

    (void)LLVMFuzzerTestOneInput((uint8_t *)buf, siz_buf);

err:
    if (f) {
        fclose(f);
    }
    free(buf);

    return 0;
}
