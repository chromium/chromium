#include <stdint.h>
#include <string.h>

#include "omaha_tag_format.h"

#include "common.h"
#include "sizedbuf.h"

// See Chromium's /src/chrome/updater/tag.h for documentation on the Omaha 4
// tag data format.

const char kOmahaTagSignature[] = "Gact2.0Omaha";
const size_t kOmahaTagSignatureSize = sizeof(kOmahaTagSignature) - 1;

const size_t kOmahaTagDataSize = 8192;

const size_t kOmahaFullTagZoneSize = kOmahaTagSignatureSize + 2 + kOmahaTagDataSize;

SizedBuf* ParseOmahaTagZone (const char* arg) {
    size_t tag_len = strlen(arg);
    ASSERT(tag_len <= kOmahaTagDataSize, "Omaha tag data size exceeded");
    SizedBuf* ret = ZAllocBuf(kOmahaFullTagZoneSize);
    uint8_t* target = ret->data;

    memcpy(target, kOmahaTagSignature, kOmahaTagSignatureSize);
    target += kOmahaTagSignatureSize;

    // Omaha's max tag size should have already enforced this, might as well check
    ASSERT(tag_len == (size_t)(uint16_t)tag_len, "tag data size does not fit in uint16");
    // Omaha tag size is big-endian.
    *target++ = (uint8_t)((tag_len & (size_t)0xFF00) >> 8);
    *target++ = (uint8_t)(tag_len & (size_t)0xFF);

    memcpy(target, arg, tag_len);

    return ret;
}
