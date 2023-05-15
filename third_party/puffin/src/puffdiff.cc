// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "puffin/src/include/puffin/puffdiff.h"

#include <inttypes.h>
#include <string>
#include <vector>

#include "base/big_endian.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "zucchini/buffer_view.h"
#include "zucchini/patch_writer.h"
#include "zucchini/zucchini.h"

#include "puffin/file_stream.h"
#include "puffin/memory_stream.h"
#include "puffin/src/include/puffin/brotli_util.h"
#include "puffin/src/include/puffin/common.h"
#include "puffin/src/include/puffin/puffer.h"
#include "puffin/src/include/puffin/puffpatch.h"
#include "puffin/src/include/puffin/utils.h"
#include "puffin/src/logging.h"
#include "puffin/src/puffin.pb.h"
#include "puffin/src/puffin_stream.h"

using std::string;
using std::vector;

namespace puffin {

namespace {

template <typename T>
void CopyVectorToRpf(
    const T& from,
    google::protobuf::RepeatedPtrField<metadata::BitExtent>* to,
    size_t coef) {
  to->Reserve(from.size());
  for (const auto& ext : from) {
    auto tmp = to->Add();
    tmp->set_offset(ext.offset * coef);
    tmp->set_length(ext.length * coef);
  }
}

// Structure of a Puffin patch
// +-------+------------------+-------------+--------------+
// |P|U|F|1| PatchHeader Size | PatchHeader | raw patch |
// +-------+------------------+-------------+--------------+
bool CreatePatch(const Buffer& raw_patch,
                 const vector<BitExtent>& src_deflates,
                 const vector<BitExtent>& dst_deflates,
                 const vector<ByteExtent>& src_puffs,
                 const vector<ByteExtent>& dst_puffs,
                 uint64_t src_puff_size,
                 uint64_t dst_puff_size,
                 PatchAlgorithm patchAlgorithm,
                 Buffer* patch) {
  metadata::PatchHeader header;
  header.set_version(1);

  CopyVectorToRpf(src_deflates, header.mutable_src()->mutable_deflates(), 1);
  CopyVectorToRpf(dst_deflates, header.mutable_dst()->mutable_deflates(), 1);
  CopyVectorToRpf(src_puffs, header.mutable_src()->mutable_puffs(), 8);
  CopyVectorToRpf(dst_puffs, header.mutable_dst()->mutable_puffs(), 8);

  header.mutable_src()->set_puff_length(src_puff_size);
  header.mutable_dst()->set_puff_length(dst_puff_size);
  header.set_type(static_cast<metadata::PatchHeader_PatchType>(patchAlgorithm));

  const size_t header_size_long = header.ByteSizeLong();
  TEST_AND_RETURN_FALSE(header_size_long <= UINT32_MAX);
  const uint32_t header_size = header_size_long;

  uint64_t offset = 0;
  patch->resize(kMagicLength + sizeof(header_size) + header_size +
                raw_patch.size());

  memcpy(patch->data() + offset, kMagic, kMagicLength);
  offset += kMagicLength;

  // Read header size from big-endian mode.
  uint32_t be_header_size = 0;
  base::ReadBigEndian(reinterpret_cast<const uint8_t*>(&header_size),
                      &be_header_size);
  memcpy(patch->data() + offset, &be_header_size, sizeof(be_header_size));
  offset += 4;

  TEST_AND_RETURN_FALSE(
      header.SerializeToArray(patch->data() + offset, header_size));
  offset += header_size;

  memcpy(patch->data() + offset, raw_patch.data(), raw_patch.size());

  if (raw_patch.size() > patch->size()) {
    LOG(ERROR) << "Puffin patch is invalid";
  }
  return true;
}

}  // namespace

bool PuffDiff(UniqueStreamPtr src,
              UniqueStreamPtr dst,
              const vector<BitExtent>& src_deflates,
              const vector<BitExtent>& dst_deflates,
              const vector<puffin::CompressorType>& compressors,
              PatchAlgorithm patchAlgorithm,
              const string& tmp_filepath,
              Buffer* patch) {
  auto puffer = std::make_shared<Puffer>();
  auto puff_deflate_stream =
      [&puffer](UniqueStreamPtr stream, const vector<BitExtent>& deflates,
                Buffer* puff_buffer, vector<ByteExtent>* puffs) {
        if (![&stream, &deflates, puff_buffer, puffs]() {
              uint64_t puff_size = 0;
              TEST_AND_RETURN_FALSE(stream->Seek(0));
              TEST_AND_RETURN_FALSE(
                  FindPuffLocations(stream, deflates, puffs, &puff_size));
              TEST_AND_RETURN_FALSE(stream->Seek(0));
              puff_buffer->resize(puff_size);
              return true;
            }()) {
          stream->Close();
          return false;
        }
        auto src_puffin_stream = PuffinStream::CreateForPuff(
            std::move(stream), puffer, puff_buffer->size(), deflates, *puffs);
        bool result =
            src_puffin_stream->Read(puff_buffer->data(), puff_buffer->size());
        src_puffin_stream->Close();
        return result;
      };

  Buffer src_puff_buffer;
  Buffer dst_puff_buffer;
  vector<ByteExtent> src_puffs, dst_puffs;
  TEST_AND_RETURN_FALSE(puff_deflate_stream(std::move(src), src_deflates,
                                            &src_puff_buffer, &src_puffs));
  TEST_AND_RETURN_FALSE(puff_deflate_stream(std::move(dst), dst_deflates,
                                            &dst_puff_buffer, &dst_puffs));
  if (patchAlgorithm == PatchAlgorithm::kZucchini) {
    zucchini::ConstBufferView src_bytes(src_puff_buffer.data(),
                                        src_puff_buffer.size());
    zucchini::ConstBufferView dst_bytes(dst_puff_buffer.data(),
                                        dst_puff_buffer.size());

    zucchini::EnsemblePatchWriter patch_writer(src_bytes, dst_bytes);
    auto status = zucchini::GenerateBuffer(src_bytes, dst_bytes, &patch_writer);
    TEST_AND_RETURN_FALSE(status == zucchini::status::kStatusSuccess);

    Buffer zucchini_patch_buf(patch_writer.SerializedSize());
    patch_writer.SerializeInto(
        {zucchini_patch_buf.data(), zucchini_patch_buf.size()});

    // Use brotli to compress the zucchini patch.
    // TODO(197361113) respect the CompressorType parameter for zucchini.
    Buffer compressed_patch;
    TEST_AND_RETURN_FALSE(BrotliEncode(zucchini_patch_buf.data(),
                                       zucchini_patch_buf.size(),
                                       &compressed_patch));

    TEST_AND_RETURN_FALSE(CreatePatch(
        compressed_patch, src_deflates, dst_deflates, src_puffs, dst_puffs,
        src_puff_buffer.size(), dst_puff_buffer.size(), patchAlgorithm, patch));
  } else {
    LOG(ERROR) << "unsupported type " << static_cast<int>(patchAlgorithm);
    return false;
  }

  return true;
}

bool PuffDiff(UniqueStreamPtr src,
              UniqueStreamPtr dst,
              const std::vector<BitExtent>& src_deflates,
              const std::vector<BitExtent>& dst_deflates,
              const std::vector<puffin::CompressorType>& compressors,
              const std::string& tmp_filepath,
              Buffer* patch) {
  return PuffDiff(std::move(src), std::move(dst), src_deflates, dst_deflates,
                  compressors, PatchAlgorithm::kZucchini, tmp_filepath, patch);
}

bool PuffDiff(const Buffer& src,
              const Buffer& dst,
              const vector<BitExtent>& src_deflates,
              const vector<BitExtent>& dst_deflates,
              const vector<puffin::CompressorType>& compressors,
              const string& tmp_filepath,
              Buffer* patch) {
  return PuffDiff(MemoryStream::CreateForRead(src),
                  MemoryStream::CreateForRead(dst), src_deflates, dst_deflates,
                  compressors, PatchAlgorithm::kZucchini, tmp_filepath, patch);
}

bool PuffDiff(const Buffer& src,
              const Buffer& dst,
              const vector<BitExtent>& src_deflates,
              const vector<BitExtent>& dst_deflates,
              const string& tmp_filepath,
              Buffer* patch) {
  return PuffDiff(src, dst, src_deflates, dst_deflates,
                  {puffin::CompressorType::kBrotli}, tmp_filepath, patch);
}

Status PuffDiff(const string& src_file_path,
                const string& dest_file_path,
                const string& output_patch_path) {
  auto src_deflates_byte = vector<ByteExtent>();
  auto dst_deflates_byte = vector<ByteExtent>();
  auto src_deflates_bit = vector<BitExtent>();
  auto dst_deflates_bit = vector<BitExtent>();
  auto src_puffs = vector<ByteExtent>();
  auto dst_puffs = vector<ByteExtent>();
  puffin::UniqueStreamPtr src_stream =
      FileStream::Open(src_file_path, true, false);
  if (!src_stream) {
    LOG(ERROR) << "Invalid source filepath";
    return Status::P_READ_OPEN_ERROR;
  }
  puffin::UniqueStreamPtr dest_stream =
      FileStream::Open(dest_file_path, true, false);
  if (!dest_stream) {
    src_stream->Close();
    LOG(ERROR) << "Invalid destination filepath";
    return Status::P_READ_OPEN_ERROR;
  }

  // Get Src Deflates.
  uint64_t src_stream_size = 0;
  if (!src_stream->GetSize(&src_stream_size)) {
    src_stream->Close();
    dest_stream->Close();
    LOG(ERROR) << "Unable to get streamsize for file: " << src_file_path;
    return Status::P_STREAM_ERROR;
  }
  Buffer src_data(src_stream_size);
  if (!src_stream->Read(src_data.data(), src_data.size())) {
    src_stream->Close();
    dest_stream->Close();
    LOG(ERROR) << "Unable to read stream for file: " << src_file_path;
    return Status::P_STREAM_ERROR;
  }
  if (!puffin::LocateDeflatesInZipArchive(src_data, &src_deflates_bit)) {
    LOG(ERROR) << "No zip deflates found for source filepath: "
               << src_file_path;
  }

  // Get Dest Deflates.
  uint64_t dest_stream_size = 0;
  if (!dest_stream->GetSize(&dest_stream_size)) {
    src_stream->Close();
    dest_stream->Close();
    LOG(ERROR) << "Unable to get streamsize for file: " << dest_file_path;
    return Status::P_STREAM_ERROR;
  }
  Buffer dest_data(dest_stream_size);
  if (!dest_stream->Read(dest_data.data(), dest_data.size())) {
    src_stream->Close();
    dest_stream->Close();
    LOG(ERROR) << "Unable to read stream for file: " << dest_file_path;
    return Status::P_STREAM_ERROR;
  }
  if (!puffin::LocateDeflatesInZipArchive(dest_data, &dst_deflates_bit)) {
    LOG(ERROR) << "No zip deflates for destination filepath: "
               << dest_file_path;
  }

  if (src_deflates_bit.empty()) {
    if (!FindDeflateSubBlocks(src_stream, src_deflates_byte,
                              &src_deflates_bit)) {
      LOG(ERROR) << "Unable to find deflate subblocks for source.";
      return Status::P_STREAM_ERROR;
    }
  }

  if (dst_deflates_bit.empty()) {
    if (!FindDeflateSubBlocks(dest_stream, dst_deflates_byte,
                              &dst_deflates_bit)) {
      LOG(ERROR) << "Unable to find deflate subblocks for destination";
      return Status::P_STREAM_ERROR;
    }
  }

  Buffer puffdiff_delta;
  if (!puffin::PuffDiff(std::move(src_stream), std::move(dest_stream),
                        src_deflates_bit, dst_deflates_bit,
                        {puffin::CompressorType::kBrotli},
                        // TODO(crbug.com/1321247): we are currently just ALWAYS
                        // doing zucchini with brotli, we might come back and
                        // support bsdiff and/or bzip2.
                        puffin::PatchAlgorithm::kZucchini, "/tmp/patch.tmp",
                        &puffdiff_delta)) {
    LOG(ERROR) << "Unable to generate PuffDiff";
    return Status::P_UNABLE_TO_GENERATE_PUFFPATCH;
  }
  LOG(INFO) << "patch_size: " << puffdiff_delta.size();
  puffin::UniqueStreamPtr patch_stream =
      FileStream::Open(output_patch_path, false, true);
  if (!patch_stream) {
    LOG(ERROR) << "Unable to open patch Stream";
    return Status::P_STREAM_ERROR;
  }
  if (!patch_stream->Write(puffdiff_delta.data(), puffdiff_delta.size())) {
    patch_stream->Close();
    LOG(ERROR) << "Unable to write to patch stream to patch filepath.";
    return Status::P_WRITE_ERROR;
  }
  patch_stream->Close();
  return Status::P_OK;
}

}  // namespace puffin
