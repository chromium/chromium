#include "sizedbuf.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

SizedBuf* AllocBuf(size_t cap) {
    // Use calloc to make incorrect code a little more predictable.
    // ZAllocBuf relies on AllocBuf actually using calloc, even though the
    // header does not promise it to callers outside of sizedbuf.c itself.
    SizedBuf* ret = calloc(sizeof(SizedBuf) + cap, 1);
    ASSERT(ret, "AllocBuf OOM");
    ret->cap = cap;
    return ret;
}

SizedBuf* ZAllocBuf(size_t len) {
    // AllocBuf uses calloc, so our promise that this one zeroes out `data`
    // up to `len` is already met.
    SizedBuf* ret = AllocBuf(len);
    ASSERT(ret, "ZAllocBuf OOM");
    ret->len = len;
    return ret;
}

SizedBuf* AllocBufCopy(const SizedBuf* buf) {
    ASSERT(buf, "AllocBufCopy can't copy out of NULL!");
    size_t blen = buf->len;
    ASSERT(blen <= buf->cap, "AllocBufCopy observed corrupt input buffer");
    return AllocBufCopyBytes(buf->data, blen);
}

SizedBuf* AllocBufCopyBytes(const char* data, size_t len) {
    ASSERT(data, "AllocBufCopyBytes can't copy out of NULL!");
    SizedBuf* ret = malloc(sizeof(SizedBuf) + len);
    ASSERT(ret, "AllocBufCopyBytes OOM");
    ret->len = ret->cap = len;
    memcpy(ret->data, data, len);
    return ret;
}

SizedBuf* AllocBufCopyString(const char* str) {
    ASSERT(str, "AllocBufCopyString can't copy out of NULL!");
    SizedBuf* ret = AllocBufCopyBytes(str, strlen(str)+1);
    ret->len -= 1;
    return ret;
}
