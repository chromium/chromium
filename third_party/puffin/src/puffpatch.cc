// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "puffin/src/include/puffin/puffpatch.h"

#include <inttypes.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/big_endian.h"
#include "zucchini/patch_reader.h"
#include "zucchini/zucchini.h"

#include "puffin/memory_stream.h"
#include "puffin/src/include/puffin/brotli_util.h"
#include "puffin/src/include/puffin/common.h"
#include "puffin/src/include/puffin/huffer.h"
#include "puffin/src/include/puffin/puffer.h"
#include "puffin/src/include/puffin/stream.h"
#include "puffin/src/logging.h"
#include "puffin/src/puffin.pb.h"
#include "puffin/src/puffin_stream.h"

using std::string;
using std::unique_ptr;
using std::vector;

namespace puffin {

const char kMagic[] = "PUF1";
const size_t kMagicLength = 4;

namespace {

template <typename T>
void CopyRpfToVector(
    const google::protobuf::RepeatedPtrField<metadata::BitExtent>& from,
    T* to,
    size_t coef) {
  to->reserve(from.size());
  for (const auto& ext : from) {
    to->emplace_back(ext.offset() / coef, ext.length() / coef);
  }
}

bool DecodePatch(const uint8_t* patch,
                 size_t patch_length,
                 size_t* bsdiff_patch_offset,
                 size_t* bsdiff_patch_size,
                 vector<BitExtent>* src_deflates,
                 vector<BitExtent>* dst_deflates,
                 vector<ByteExtent>* src_puffs,
                 vector<ByteExtent>* dst_puffs,
                 uint64_t* src_puff_size,
                 uint64_t* dst_puff_size,
                 metadata::PatchHeader_PatchType* patch_type) {
  size_t offset = 0;
  uint32_t header_size = 0;
  TEST_AND_RETURN_FALSE(patch_length >= (kMagicLength + sizeof(header_size)));

  string patch_magic(reinterpret_cast<const char*>(patch), kMagicLength);
  if (patch_magic != kMagic) {
    LOG(ERROR) << "Magic number for Puffin patch is incorrect: " << patch_magic;
    return false;
  }
  offset += kMagicLength;

  // Read the header size from big-endian mode.
  memcpy(&header_size, patch + offset, sizeof(header_size));
  base::WriteBigEndian(reinterpret_cast<char*>(&header_size), header_size);
  offset += sizeof(header_size);
  TEST_AND_RETURN_FALSE(header_size <= (patch_length - offset));

  metadata::PatchHeader header;
  TEST_AND_RETURN_FALSE(header.ParseFromArray(patch + offset, header_size));
  offset += header_size;

  CopyRpfToVector(header.src().deflates(), src_deflates, 1);
  CopyRpfToVector(header.dst().deflates(), dst_deflates, 1);
  CopyRpfToVector(header.src().puffs(), src_puffs, 8);
  CopyRpfToVector(header.dst().puffs(), dst_puffs, 8);

  *src_puff_size = header.src().puff_length();
  *dst_puff_size = header.dst().puff_length();

  *bsdiff_patch_offset = offset;
  *bsdiff_patch_size = patch_length - offset;

  *patch_type = header.type();
  return true;
}

bool ApplyZucchiniPatch(UniqueStreamPtr src_stream,
                        size_t src_size,
                        const uint8_t* patch_start,
                        size_t patch_size,
                        UniqueStreamPtr dst_stream) {
  // Read the source data
  Buffer puffed_src(src_size);
  Buffer buffer(1024 * 1024);
  uint64_t bytes_wrote = 0;
  while (bytes_wrote < src_size) {
    auto write_size =
        std::min(static_cast<uint64_t>(buffer.size()), src_size - bytes_wrote);
    TEST_AND_RETURN_FALSE(src_stream->Read(buffer.data(), write_size));
    std::copy(buffer.data(), buffer.data() + write_size,
              puffed_src.data() + bytes_wrote);
    bytes_wrote += write_size;
  }
  // Read the patch
  Buffer zucchini_patch;
  TEST_AND_RETURN_FALSE(BrotliDecode(patch_start, patch_size, &zucchini_patch));
  auto patch_reader = zucchini::EnsemblePatchReader::Create(
      {zucchini_patch.data(), zucchini_patch.size()});
  if (!patch_reader.has_value()) {
    LOG(ERROR) << "Failed to parse the zucchini patch.";
    return false;
  }

  // TODO(197361113) Stream the patched result once zucchini supports it. So we
  // can save some memory when applying patch on device.
  Buffer patched_data(patch_reader->header().new_size);
  auto status = zucchini::ApplyBuffer(
      {puffed_src.data(), puffed_src.size()}, *patch_reader,
      {patched_data.data(), patched_data.size()});
  if (status != zucchini::status::kStatusSuccess) {
    LOG(ERROR) << "Failed to parse the zucchini patch: " << status;
    return false;
  }

  TEST_AND_RETURN_FALSE(
      dst_stream->Write(patched_data.data(), patched_data.size()));
  return true;
}

}  // namespace

bool PuffPatch(UniqueStreamPtr src,
               UniqueStreamPtr dst,
               const uint8_t* patch,
               size_t patch_length,
               size_t max_cache_size) {
  size_t patch_offset;  // raw patch offset in puffin |patch|.
  size_t raw_patch_size = 0;
  vector<BitExtent> src_deflates, dst_deflates;
  vector<ByteExtent> src_puffs, dst_puffs;
  uint64_t src_puff_size, dst_puff_size;

  metadata::PatchHeader_PatchType patch_type;

  // Decode the patch and get the raw patch (e.g. bsdiff, zucchini).
  TEST_AND_RETURN_FALSE(
      DecodePatch(patch, patch_length, &patch_offset, &raw_patch_size,
                  &src_deflates, &dst_deflates, &src_puffs, &dst_puffs,
                  &src_puff_size, &dst_puff_size, &patch_type));
  auto puffer = std::make_shared<Puffer>();
  auto huffer = std::make_shared<Huffer>();

  auto src_stream =
      PuffinStream::CreateForPuff(std::move(src), puffer, src_puff_size,
                                  src_deflates, src_puffs, max_cache_size);
  TEST_AND_RETURN_FALSE(src_stream);
  auto dst_stream = PuffinStream::CreateForHuff(
      std::move(dst), huffer, dst_puff_size, dst_deflates, dst_puffs);
  TEST_AND_RETURN_FALSE(dst_stream);
  if (patch_type == metadata::PatchHeader_PatchType_ZUCCHINI) {
    TEST_AND_RETURN_FALSE(ApplyZucchiniPatch(
        std::move(src_stream), src_puff_size, patch + patch_offset,
        raw_patch_size, std::move(dst_stream)));
  } else {
    LOG(ERROR) << "Unsupported patch type " << patch_type;
    return false;
  }
  return true;
}

}  // namespace puffin
