#include <stdint.h>
#include <zip.h>

#include "zip_read_fuzzer_common.h"

#ifdef __cplusplus
extern "C" {
#endif

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    zip_source_t *src;
    zip_error_t error;
    zip_t *za;

    zip_error_init(&error);

    if ((src = zip_source_buffer_create(data, size, 0, &error)) == NULL) {
        zip_error_fini(&error);
        return 0;
    }

    za = zip_open_from_source(src, 0, &error);

    fuzzer_read(za, &error, "secretpassword");

    if (za == NULL) {
        zip_source_free(src);
    }

    return 0;
}

#ifdef __cplusplus
}
#endif
