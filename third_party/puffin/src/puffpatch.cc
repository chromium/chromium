// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <inttypes.h>

#include <algorithm>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/string_view_util.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "puffin/memory_stream.h"
#include "puffin/src/include/puffin/brotli_util.h"
#include "puffin/src/include/puffin/common.h"
#include "puffin/src/include/puffin/file_stream.h"
#include "puffin/src/include/puffin/huffer.h"
#include "puffin/src/include/puffin/puffer.h"
#include "puffin/src/include/puffin/puffpatch.h"
#include "puffin/src/include/puffin/stream.h"
#include "puffin/src/logging.h"
#include "puffin/src/puffin.pb.h"
#include "puffin/src/puffin_stream.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "zucchini/patch_reader.h"
#include "zucchini/zucchini.h"

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

Status DecodePatch(const uint8_t* patch,
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
  uint32_t header_size = 0;
  TEST_AND_RETURN_VALUE(patch_length >= (kMagicLength + sizeof(header_size)),
                        Status::P_BAD_PUFFIN_CORRUPT);
  // SAFETY: Caller is required to provide at least `patch_length` valid bytes
  // at `patch`.
  UNSAFE_BUFFERS(const base::span patch_span(patch, patch_length));

  const auto patch_magic =
      base::as_string_view(patch_span.first<kMagicLength>());
  if (patch_magic != kMagic) {
    return Status::P_BAD_PUFFIN_MAGIC;
  }
  auto header_span = patch_span.subspan<kMagicLength>();

  // Read the header size from big-endian mode.
  const auto header_size_span = header_span.first<sizeof header_size>();
  header_size = base::U32FromBigEndian(header_size_span);
  header_span = header_span.subspan<sizeof header_size>();
  TEST_AND_RETURN_VALUE(header_size <= header_span.size(),
                        Status::P_BAD_PUFFIN_HEADER);

  metadata::PatchHeader header;
  TEST_AND_RETURN_VALUE(header.ParseFromArray(header_span.data(), header_size),
                        Status::P_BAD_PUFFIN_HEADER);
  header_span = header_span.subspan(header_size);

  CopyRpfToVector(header.src().deflates(), src_deflates, 1);
  CopyRpfToVector(header.dst().deflates(), dst_deflates, 1);
  CopyRpfToVector(header.src().puffs(), src_puffs, 8);
  CopyRpfToVector(header.dst().puffs(), dst_puffs, 8);

  *src_puff_size = header.src().puff_length();
  *dst_puff_size = header.dst().puff_length();

  *bsdiff_patch_offset = patch_span.size() - header_span.size();
  *bsdiff_patch_size = header_span.size();

  *patch_type = header.type();
  return Status::P_OK;
}

#if !BUILDFLAG(IS_ANDROID)
// Creates a memory-mapped temporary file of the specified size. Returns a
// unique pointer to the memory-mapped file on success, or nullptr if temporary
// file creation or memory mapping fails.
std::unique_ptr<base::MemoryMappedFile> CreateMemoryMappedTemporaryFile(
    size_t size) {
  base::FilePath temp_path;
  if (!base::CreateTemporaryFile(&temp_path)) {
    return nullptr;
  }

  base::File temp_file(
      temp_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                     base::File::FLAG_WRITE | base::File::FLAG_WIN_TEMPORARY |
                     base::File::FLAG_WIN_SHARE_DELETE |
                     base::File::FLAG_DELETE_ON_CLOSE);

  if (!temp_file.IsValid()) {
    base::DeleteFile(temp_path);
    return nullptr;
  }

  auto mapped_file = std::make_unique<base::MemoryMappedFile>();
  if (!mapped_file->Initialize(std::move(temp_file), {0, size},
                               base::MemoryMappedFile::READ_WRITE_EXTEND)) {
    return nullptr;
  }

  return mapped_file;
}
#endif  // !BUILDFLAG(IS_ANDROID)

using PatchBufferBacking =
    std::variant<std::unique_ptr<base::MemoryMappedFile>, Buffer>;

PatchBufferBacking AllocatePatchBuffer(size_t size) {
  // Accessing PathUtils from a sandboxed process on Android might crash.
  // Consider enabling this code path in the future, though patching only runs
  // in sandboxed processes on Android and is expected to fall back to in-memory
  // buffers. See: crbug.com/513197224
#if !BUILDFLAG(IS_ANDROID)
  // The minimum size in bytes at which memory-mapped file backing is attempted.
  constexpr size_t kMemoryMappedFileCutoff = 1024 * 1024;
  if (size < kMemoryMappedFileCutoff) {
    return Buffer(size);
  }

  if (std::unique_ptr<base::MemoryMappedFile> mapped_file =
          CreateMemoryMappedTemporaryFile(size)) {
    return std::move(mapped_file);
  }
#endif
  return Buffer(size);
}

Status ApplyZucchiniPatch(UniqueStreamPtr src_stream,
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
    if (!src_stream->Read(buffer.data(), write_size)) {
      src_stream->Close();
      dst_stream->Close();
      return Status::P_READ_ERROR;
    }
    std::copy(buffer.data(), buffer.data() + write_size,
              puffed_src.data() + bytes_wrote);
    bytes_wrote += write_size;
  }
  src_stream->Close();
  // Read the patch
  Buffer zucchini_patch;
  TEST_AND_RETURN_VALUE(BrotliDecode(patch_start, patch_size, &zucchini_patch),
                        Status::P_BAD_PUFFIN_CORRUPT);
  auto patch_reader = zucchini::EnsemblePatchReader::Create(
      {zucchini_patch.data(), zucchini_patch.size()});
  if (!patch_reader.has_value()) {
    dst_stream->Close();
    return Status::P_BAD_ZUCC_CORRUPT;
  }

  // TODO(197361113) Stream the patched result once zucchini supports it. So we
  // can save some memory when applying patch on device.
  PatchBufferBacking buffer_backing =
      AllocatePatchBuffer(patch_reader->header().new_size);
  base::span<uint8_t> patched_span = std::visit(
      absl::Overload{
          [](const std::unique_ptr<base::MemoryMappedFile>& backing) {
            return backing->mutable_bytes();
          },
          [](Buffer& backing) { return base::span<uint8_t>(backing); }},
      buffer_backing);

  auto status = zucchini::ApplyBuffer(
      {puffed_src.data(), puffed_src.size()}, *patch_reader,
      {patched_span.data(), patched_span.size()});
  Status result = Status::P_OK;
  switch (status) {
    case zucchini::status::kStatusSuccess:
      if (!dst_stream->Write(patched_span.data(), patched_span.size())) {
        result = Status::P_WRITE_ERROR;
      }
      break;
    case zucchini::status::kStatusInvalidParam:
      result = Status::P_INPUT_NOT_RECOGNIZED;
      break;
    case zucchini::status::kStatusFileReadError:
      [[fallthrough]];
    case zucchini::status::kStatusPatchReadError:
      result = Status::P_READ_ERROR;
      break;
    case zucchini::status::kStatusFileWriteError:
      [[fallthrough]];
    case zucchini::status::kStatusPatchWriteError:
      result = Status::P_WRITE_ERROR;
      break;
    case zucchini::status::kStatusInvalidOldImage:
      result = Status::P_BAD_ZUCC_OLD_IMAGE;
      break;
    case zucchini::status::kStatusInvalidNewImage:
      result = Status::P_BAD_ZUCC_NEW_IMAGE;
      break;
    case zucchini::status::kStatusFatal:
      [[fallthrough]];
    default:
      result = Status::P_UNKNOWN_ERROR;
  }
  dst_stream->Close();
  return result;
}

}  // namespace

Status PuffPatch(UniqueStreamPtr src,
                 UniqueStreamPtr dst,
                 const uint8_t* patch,
                 size_t patch_length,
                 size_t max_cache_size) {
  size_t patch_offset;  // raw patch offset in Puffin |patch|.
  size_t raw_patch_size = 0;
  vector<BitExtent> src_deflates, dst_deflates;
  vector<ByteExtent> src_puffs, dst_puffs;
  uint64_t src_puff_size, dst_puff_size;

  metadata::PatchHeader_PatchType patch_type;

  // Decode the patch and get the raw patch (e.g. bsdiff, zucchini).
  auto decode_status =
      DecodePatch(patch, patch_length, &patch_offset, &raw_patch_size,
                  &src_deflates, &dst_deflates, &src_puffs, &dst_puffs,
                  &src_puff_size, &dst_puff_size, &patch_type);
  if (Status::P_OK != decode_status) {
    src->Close();
    dst->Close();
    return decode_status;
  }
  auto puffer = std::make_shared<Puffer>();
  auto huffer = std::make_shared<Huffer>();

  auto src_stream =
      PuffinStream::CreateForPuff(std::move(src), puffer, src_puff_size,
                                  src_deflates, src_puffs, max_cache_size);
  if (!src_stream) {
    dst->Close();
    return Status::P_READ_ERROR;
  }
  auto dst_stream = PuffinStream::CreateForHuff(
      std::move(dst), huffer, dst_puff_size, dst_deflates, dst_puffs);
  if (!dst_stream) {
    src_stream->Close();
    return Status::P_WRITE_ERROR;
  }
  if (patch_type == metadata::PatchHeader_PatchType_ZUCCHINI) {
    auto zucc_status = ApplyZucchiniPatch(std::move(src_stream), src_puff_size,
                                          patch + patch_offset, raw_patch_size,
                                          std::move(dst_stream));
    if (Status::P_OK != zucc_status) {
      return zucc_status;
    }
  } else {
    return Status::P_BAD_PUFFIN_PATCH_TYPE;
  }
  return Status::P_OK;
}

Status ApplyPuffPatch(const base::FilePath& input_path,
                      const base::FilePath& patch_path,
                      const base::FilePath& output_path) {
  puffin::UniqueStreamPtr input_stream =
      puffin::FileStream::Open(input_path.AsUTF8Unsafe(), true, false);
  if (!input_stream) {
    return Status::P_READ_OPEN_ERROR;
  }
  puffin::UniqueStreamPtr output_stream =
      puffin::FileStream::Open(output_path.AsUTF8Unsafe(), false, true);
  if (!output_stream) {
    input_stream->Close();
    return Status::P_WRITE_OPEN_ERROR;
  }
  puffin::UniqueStreamPtr patch_stream =
      puffin::FileStream::Open(patch_path.AsUTF8Unsafe(), true, false);
  if (!patch_stream) {
    input_stream->Close();
    output_stream->Close();
    return Status::P_READ_OPEN_ERROR;
  }
  uint64_t patch_size = 0;
  if (!patch_stream->GetSize(&patch_size)) {
    input_stream->Close();
    output_stream->Close();
    patch_stream->Close();
    return Status::P_STREAM_ERROR;
  }
  puffin::Buffer puffdiff_delta(patch_size);
  if (!patch_stream->Read(puffdiff_delta.data(), puffdiff_delta.size())) {
    input_stream->Close();
    output_stream->Close();
    patch_stream->Close();
    return Status::P_READ_ERROR;
  }
  patch_stream->Close();
  return puffin::PuffPatch(std::move(input_stream), std::move(output_stream),
                           std::move(puffdiff_delta.data()),
                           puffdiff_delta.size(), kDefaultPuffCacheSize);
}

Status ApplyPuffPatch(base::File input_file,
                      base::File patch_file,
                      base::File output_file) {
  puffin::UniqueStreamPtr input_stream =
      puffin::FileStream::CreateStreamFromFile(std::move(input_file));
  if (!input_stream) {
    return Status::P_READ_OPEN_ERROR;
  }
  puffin::UniqueStreamPtr output_stream =
      puffin::FileStream::CreateStreamFromFile(std::move(output_file));
  if (!output_stream) {
    input_stream->Close();
    return Status::P_WRITE_OPEN_ERROR;
  }
  puffin::UniqueStreamPtr patch_stream =
      puffin::FileStream::CreateStreamFromFile(std::move(patch_file));
  if (!patch_stream) {
    input_stream->Close();
    output_stream->Close();
    return Status::P_READ_OPEN_ERROR;
  }
  uint64_t patch_size = 0;
  if (!patch_stream->GetSize(&patch_size)) {
    input_stream->Close();
    output_stream->Close();
    patch_stream->Close();
    return Status::P_STREAM_ERROR;
  }
  puffin::Buffer puffdiff_delta(patch_size);
  if (!patch_stream->Read(puffdiff_delta.data(), puffdiff_delta.size())) {
    input_stream->Close();
    output_stream->Close();
    patch_stream->Close();
    LOG(ERROR) << "Unable to read patch stream";
    return Status::P_READ_ERROR;
  }
  patch_stream->Close();
  return puffin::PuffPatch(std::move(input_stream), std::move(output_stream),
                           std::move(puffdiff_delta.data()),
                           puffdiff_delta.size(), kDefaultPuffCacheSize);
}

}  // namespace puffin
