// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_LZMA_SDK_GOOGLE_SEVEN_ZIP_C_BUF_STREAM_H_
#define THIRD_PARTY_LZMA_SDK_GOOGLE_SEVEN_ZIP_C_BUF_STREAM_H_

#include "third_party/lzma_sdk/C/7zTypes.h"

EXTERN_C_BEGIN

// Z7CBufSeqInStream wraps a flat, fixed-size C-style buffer in lzma_sdk's
// ISeqInStream interface. It does _not_ own its buffer, since it is intended
// to wrap flat C buffers used in existing code when required by lzma_sdk APIs.
//
// If you are using an lzma_sdk API that offers a buffer+size parameter pair as
// well as a stream, prefer the buffer+size parameters; lzma_sdk handles them
// more efficiently.
//
// To use Z7CBufSeqInStream, create it and assign its fields directly.
typedef struct {
  ISeqInStream vt;

  // Data represented by this stream. `Read` calls (via `vt`) read from here.
  // If the output buffer provided to a `Read` call overlaps with `buf`, the
  // behavior is undefined.
  unsigned char* buf;

  // Length of `buf` in bytes.
  size_t bufSz;

  // Next position to start `Read`ing at. Updated by `Read`. This field can
  // be used to find out how much of `buf` has been read by `lzma_sdk`.
  size_t offset;
} Z7CBufSeqInStream;

void Z7CBufSeqInStream_CreateVTable(Z7CBufSeqInStream* p);


// Z7CBufSeqOutStream wraps a flat, fixed-size C-style buffer in lzma_sdk's
// ISeqOutStream interface. It does _not_ own its buffer, since it is intended
// to wrap flat C buffers used in existing code when required by lzma_sdk APIs.
// Attempts to write more data than fit in the buffer fail (no resizing).
//
// If you are using an lzma_sdk API that offers a buffer+size parameter pair as
// well as a stream, prefer the buffer+size parameters; lzma_sdk handles them
// more efficiently.
//
// To use Z7CBufSeqOutStream, create it and assign its fields directly.
typedef struct {
  ISeqOutStream vt;

  // Destination that `Write` calls write into. If the input buffer provided
  // to a `Write` call overlaps with `buf`, the behavior is undefined.
  unsigned char* buf;

  // Capacity of `buf`. `Write` will not write into, or past, `buf + bufSz`.
  size_t bufSz;

  // Location in `buf` where the next `Write` would begin. Updated by `Write`.
  // This field must be used to check the extent of valid data after `lzma_sdk`
  // is done writing to this stream.
  size_t offset;
} Z7CBufSeqOutStream;

void Z7CBufSeqOutStream_CreateVTable(Z7CBufSeqOutStream* p);

EXTERN_C_END

#endif  // THIRD_PARTY_LZMA_SDK_GOOGLE_SEVEN_ZIP_C_BUF_STREAM_H_
