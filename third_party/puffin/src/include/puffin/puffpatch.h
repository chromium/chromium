// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_INCLUDE_PUFFIN_PUFFPATCH_H_
#define SRC_INCLUDE_PUFFIN_PUFFPATCH_H_

#include "puffin/common.h"
#include "puffin/stream.h"

namespace puffin {

extern const char kMagic[];
extern const size_t kMagicLength;

// Applies the puffin patch to deflate stream |src| to create deflate stream
// |dst|. This function is used in the client and internally uses bspatch to
// apply the patch. The input streams are of type |shared_ptr| because
// |PuffPatch| needs to wrap these streams into another ones and we don't want
// to loose the ownership of the input streams. Optionally one can cache the
// puff buffers individually if non-zero value is passed |max_cache_size|.
//
// |src|           IN  Source deflate stream.
// |dst|           IN  Destination deflate stream.
// |patch|         IN  The input patch.
// |patch_length|  IN  The length of the patch.
// |max_cache_size|IN  The maximum amount of memory to cache puff buffers.
bool PuffPatch(UniqueStreamPtr src,
               UniqueStreamPtr dst,
               const uint8_t* patch,
               size_t patch_length,
               size_t max_cache_size = 0);

}  // namespace puffin

#endif  // SRC_INCLUDE_PUFFIN_PUFFPATCH_H_
