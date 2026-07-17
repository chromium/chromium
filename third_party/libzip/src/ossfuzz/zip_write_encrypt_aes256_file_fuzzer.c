#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zip.h>

/**
   This fuzzing target takes input data, creates a ZIP archive, load
   it to a buffer, adds a file to it with AES-256 encryption and a
   specified password, and then closes and removes the archive.

   The purpose of this fuzzer is to test security of ZIP archive
   handling and encryption in the libzip by subjecting it to various
   inputs, including potentially malicious or malformed data of
   different file types.
 **/

#ifdef __cplusplus
extern "C" {
#endif

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    const char *path = "test_aes256.zip";
    const char *password = "password";
    const char *file = "filename";
    int error = 0;
    struct zip *archive;

    (void)remove(path);
    if ((archive = zip_open(path, ZIP_CREATE, &error)) == NULL) {
        return -1;
    }

    struct zip_source *source = zip_source_buffer(archive, data, size, 0);
    if (source == NULL) {
        fprintf(stderr, "failed to create source buffer. %s\n", zip_strerror(archive));
        zip_discard(archive);
        return -1;
    }

    int index = (int)zip_file_add(archive, file, source, ZIP_FL_OVERWRITE);
    if (index < 0) {
        fprintf(stderr, "failed to add file to archive: %s\n", zip_strerror(archive));
        zip_source_free(source);
        zip_discard(archive);
        return -1;
    }
    if (zip_file_set_encryption(archive, index, ZIP_EM_AES_256, password) < 0) {
        fprintf(stderr, "failed to set file encryption: %s\n", zip_strerror(archive));
        zip_discard(archive);
        return -1;
    }
    if (zip_close(archive) < 0) {
        fprintf(stderr, "error closing archive: %s\n", zip_strerror(archive));
        zip_discard(archive);
        return -1;
    }
    (void)remove(path);

    return 0;
}

#ifdef __cplusplus
}
#endif
