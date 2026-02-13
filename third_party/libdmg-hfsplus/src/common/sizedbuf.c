#include "sizedbuf.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

SizedBuf* ZAllocBuf(size_t len) {
    SizedBuf* ret = calloc(sizeof(SizedBuf) + len, 1);
    ASSERT(ret, "ZAllocBuf OOM");
    ret->len = ret->cap = len;
    return ret;
}

SizedBuf* AllocBufCopy(const SizedBuf* buf) {
    ASSERT(buf, "AllocBufCopy can't copy out of NULL!");
    ASSERT(buf->len <= buf->cap, "AllocBufCopy observed corrupt input buffer");
    return AllocBufCopyBytes(buf->data, buf->len);
}

SizedBuf* AllocBufCopyBytes(const uint8_t* data, size_t len) {
    ASSERT(data, "AllocBufCopyBytes can't copy out of NULL!");
    SizedBuf* ret = malloc(sizeof(SizedBuf) + len);
    ASSERT(ret, "AllocBufCopyBytes OOM");
    ret->len = ret->cap = len;
    memcpy(ret->data, data, len);
    return ret;
}

SizedBuf* AllocBufCopyString(const char* str) {
    ASSERT(str, "AllocBufCopyString can't copy out of NULL!");
    SizedBuf* ret = AllocBufCopyBytes((const uint8_t*)str, strlen(str) + 1);
    ret->len -= 1;
    return ret;
}

SizedBuf* ReallocBuf(SizedBuf* buf, size_t new_cap) {
    if (!buf) {
        SizedBuf* ret = ZAllocBuf(new_cap);
        ret->len = 0;
        return ret;
    }
    SizedBuf* ret = realloc(buf, sizeof(SizedBuf) + new_cap);
    ASSERT(ret, "ReallocBuf failed");
    ret->cap = new_cap;
    if (ret->len > new_cap) {
        ret->len = new_cap;
    }
    return ret;
}
