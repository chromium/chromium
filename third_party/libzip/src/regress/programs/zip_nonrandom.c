#include "zipint.h"

#ifndef REGRESSION_NO_RANDOM
#error "This file should only be compiled when REGRESSION_NO_RANDOM is defined"
#endif

bool zip_secure_nonrandom(zip_uint8_t *buffer, zip_uint16_t length) {
    memset(buffer, 0, length);

    return true;
}
