// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/lzma_sdk/google/seven_zip_c_buf_stream.h"
#include <stddef.h>
#include <string.h>

#include "third_party/lzma_sdk/C/7zTypes.h"

static SRes Z7CBufSeqInStream_Read(ISeqInStreamPtr pp,
                                   void* dest,
                                   size_t* size) {
  Z7CBufSeqInStream* p = Z7_container_of(pp, Z7CBufSeqInStream, vt);

  if (!size || !dest) {
    return SZ_ERROR_PARAM;
  }
  // If nothing is requested, successfully do nothing.
  if (*size == 0) {
    return SZ_OK;
  }

  void* src = p->buf + p->offset;
  size_t remain = p->bufSz - p->offset;

  // If all data is read, report EOF.
  if (remain == 0) {
    *size = 0;
    return SZ_OK;
  }

  size_t n = remain < *size ? remain : *size;
  memcpy(dest, src, n);
  *size = n;
  p->offset += n;
  return SZ_OK;
}

void Z7CBufSeqInStream_CreateVTable(Z7CBufSeqInStream* p) {
  p->vt.Read = Z7CBufSeqInStream_Read;
}

static size_t Z7CBufSeqOutStream_Write(ISeqOutStreamPtr pp,
                                       const void* src,
                                       size_t size) {
  Z7CBufSeqOutStream* p = Z7_container_of(pp, Z7CBufSeqOutStream, vt);

  void* dest = p->buf + p->offset;
  size_t remain = p->bufSz - p->offset;
  size_t n = size < remain ? size : remain;
  if (n == 0) {
    // No input or no room, nothing to do.
    return 0;
  }

  memcpy(dest, src, n);
  p->offset += n;
  return n;
}

void Z7CBufSeqOutStream_CreateVTable(Z7CBufSeqOutStream* p) {
  p->vt.Write = Z7CBufSeqOutStream_Write;
}
