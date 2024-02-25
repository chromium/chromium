// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/logging.h"

#include "puffin/memory_stream.h"
#include "puffin/src/include/puffin/common.h"
#include "puffin/src/include/puffin/puffpatch.h"

using puffin::BitExtent;
using puffin::Buffer;
using puffin::ByteExtent;
using puffin::MemoryStream;
using std::vector;

namespace puffin {
// From puffpatch.cc
bool DecodePatch(const uint8_t* patch,
                 size_t patch_length,
                 size_t* bsdiff_patch_offset,
                 size_t* bsdiff_patch_size,
                 vector<BitExtent>* src_deflates,
                 vector<BitExtent>* dst_deflates,
                 vector<ByteExtent>* src_puffs,
                 vector<ByteExtent>* dst_puffs,
                 uint64_t* src_puff_size,
                 uint64_t* dst_puff_size);
}  // namespace puffin

namespace {
template <typename T>
bool TestExtentsArrayForFuzzer(const vector<T>& extents) {
  const size_t kMaxArraySize = 100;
  if (extents.size() > kMaxArraySize) {
    return false;
  }

  const size_t kMaxBufferSize = 1024;  // 1Kb
  for (const auto& ext : extents) {
    if (ext.length > kMaxBufferSize) {
      return false;
    }
  }
  return true;
}

void FuzzPuffPatch(const uint8_t* data, size_t size) {
  // First decode the header and make sure the deflate and puff buffer sizes do
  // not excede some limits. This is to prevent the fuzzer complain with
  // out-of-memory errors when the fuzz data is in such a way that causes a huge
  // random size memory be allocated.

  size_t bsdiff_patch_offset;
  size_t bsdiff_patch_size = 0;
  vector<BitExtent> src_deflates, dst_deflates;
  vector<ByteExtent> src_puffs, dst_puffs;
  uint64_t src_puff_size, dst_puff_size;
  if (DecodePatch(data, size, &bsdiff_patch_offset, &bsdiff_patch_size,
                  &src_deflates, &dst_deflates, &src_puffs, &dst_puffs,
                  &src_puff_size, &dst_puff_size) &&
      TestExtentsArrayForFuzzer(src_deflates) &&
      TestExtentsArrayForFuzzer(dst_deflates) &&
      TestExtentsArrayForFuzzer(src_puffs) &&
      TestExtentsArrayForFuzzer(dst_puffs)) {
    const size_t kBufferSize = 1000;
    if ((!src_deflates.empty() &&
         kBufferSize <
             src_deflates.back().offset + src_deflates.back().length) ||
        (!dst_deflates.empty() &&
         kBufferSize <
             dst_deflates.back().offset + dst_deflates.back().length)) {
      return;
    }

    Buffer src_buffer(kBufferSize);
    Buffer dst_buffer(kBufferSize);
    auto src = MemoryStream::CreateForRead(src_buffer);
    auto dst = MemoryStream::CreateForWrite(&dst_buffer);
    puffin::PuffPatch(std::move(src), std::move(dst), data, size, kBufferSize);
  }
}

class Environment {
 public:
  Environment() {
    // To turn off the logging.
    logging::SetMinLogLevel(logging::LOGGING_FATAL);

    // To turn off logging for bsdiff library.
    std::cerr.setstate(std::ios_base::failbit);
  }
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  FuzzPuffPatch(data, size);
  return 0;
}
