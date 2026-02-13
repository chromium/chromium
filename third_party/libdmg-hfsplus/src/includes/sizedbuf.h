#ifndef _LIBDMG_HFSPLUS_SIZEDBUF_H
#define _LIBDMG_HFSPLUS_SIZEDBUF_H

#include <stddef.h>
#include <stdint.h>

// SizedBuf is an arbitrary-length buffer for arbitrary data. SizedBuf
// instances can be allocated with AllocBuf, ZAllocBuf, AllocCopyBuf, or
// AllocCopyBytes. Free them with `free`.
typedef struct {
  // Length of valid data in `data` (usage-specific).
  size_t len;

  // Allocated length of `data`.
  size_t cap;

  // Arbitrary data storage; allocated length is `cap`.
  uint8_t data[];
} SizedBuf;

#ifdef __cplusplus
extern "C" {
#endif

// ZAllocBuf allocates a new SizedBuf with capacity and len both `len`.
// It is filled with zeroes.
SizedBuf* ZAllocBuf(size_t len);

// AllocBufCopy allocates a new SizedBuf with data copied from the provided
// buffer up to its `len`. The new SizedBuf has `cap` at least equal to the
// `len` of the provided `buf` (not necessarily up to its `cap`).
SizedBuf* AllocBufCopy(const SizedBuf* buf);

// AllocBufCopyBytes allocates a new SizedBuf with data copied from the provided
// byte buffer.
SizedBuf* AllocBufCopyBytes(const uint8_t* data, size_t len);

// AllocBufCopyString allocates a new SizedBuf with data copied from the
// provided NUL-terminated C string. The `len` of the buffer excludes the
// NUL terminator, but the buffer's `cap` includes it.
SizedBuf* AllocBufCopyString(const char* str);

// ReallocBuf uses realloc to resize buf such that is capacity is now `new_cap`.
// If `new_cap` is shorter than `buf->len`, `buf->len` is shortened. If `buf`
// is NULL, this calls `AllocBuf` instead.
//
// Do not use `buf` after realloc. The returned pointer may or may not refer
// to the same memory location.
SizedBuf* ReallocBuf(SizedBuf* buf, size_t new_cap);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // _LIBDMG_HFSPLUS_SIZEDBUF_H