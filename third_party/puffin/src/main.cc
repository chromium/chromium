// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

#ifdef USE_BRILLO
#include "brillo/flag_helper.h"
#else
#include "gflags/gflags.h"
#endif

#include "puffin/file_stream.h"
#include "puffin/memory_stream.h"
#include "puffin/src/extent_stream.h"
#include "puffin/src/include/puffin/common.h"
#include "puffin/src/include/puffin/huffer.h"
#include "puffin/src/include/puffin/puffdiff.h"
#include "puffin/src/include/puffin/puffer.h"
#include "puffin/src/include/puffin/puffpatch.h"
#include "puffin/src/include/puffin/utils.h"
#include "puffin/src/logging.h"
#include "puffin/src/puffin_stream.h"

using puffin::BitExtent;
using puffin::Buffer;
using puffin::ByteExtent;
using puffin::ExtentStream;
using puffin::FileStream;
using puffin::Huffer;
using puffin::MemoryStream;
using puffin::Puffer;
using puffin::PuffinStream;
using puffin::UniqueStreamPtr;
using std::string;
using std::stringstream;
using std::vector;

namespace {

constexpr char kExtentDelimeter = ',';
constexpr char kOffsetLengthDelimeter = ':';

template <typename T>
vector<T> StringToExtents(const string& str) {
  vector<T> extents;
  if (!str.empty()) {
    stringstream ss(str);
    string extent_str;
    while (getline(ss, extent_str, kExtentDelimeter)) {
      stringstream extent_ss(extent_str);
      string offset_str, length_str;
      getline(extent_ss, offset_str, kOffsetLengthDelimeter);
      getline(extent_ss, length_str, kOffsetLengthDelimeter);
      extents.emplace_back(stoull(offset_str), stoull(length_str));
    }
  }
  return extents;
}

const uint64_t kDefaultPuffCacheSize = 50 * 1024 * 1024;  // 50 MB

// An enum representing the type of compressed files.
enum class FileType { kDeflate, kZlib, kGzip, kZip, kRaw, kUnknown };

// Returns a file type based on the input string |file_type| (normally the final
// extension of the file).
FileType StringToFileType(const string& file_type) {
  if (file_type == "raw") {
    return FileType::kRaw;
  }
  if (file_type == "deflate") {
    return FileType::kDeflate;
  } else if (file_type == "zlib") {
    return FileType::kZlib;
  } else if (file_type == "gzip" || file_type == "gz" || file_type == "tgz") {
    return FileType::kGzip;
  } else if (file_type == "zip" || file_type == "apk" || file_type == "jar") {
    return FileType::kZip;
  }
  return FileType::kUnknown;
}

// Finds the location of deflates in |stream|. If |file_type_to_override| is
// non-empty, it infers the file type based on that, otherwise, it infers the
// file type based on the final extension of |file_name|. It returns false if
// file type cannot be inferred from any of the input arguments. |deflates|
// is filled with byte-aligned location of deflates.
bool LocateDeflatesBasedOnFileType(const UniqueStreamPtr& stream,
                                   const string& file_name,
                                   const string& file_type_to_override,
                                   vector<BitExtent>* deflates) {
  auto file_type = FileType::kUnknown;

  auto last_dot = file_name.find_last_of(".");
  if (last_dot == string::npos) {
    // Could not find a dot so we assume there is no extension.
    return false;
  }
  auto extension = file_name.substr(last_dot + 1);
  file_type = StringToFileType(extension);

  if (!file_type_to_override.empty()) {
    auto override_file_type = StringToFileType(file_type_to_override);
    if (override_file_type == FileType::kUnknown) {
      LOG(ERROR) << "Overriden file type " << file_type_to_override
                 << " does not exist.";
      return false;
    }
    if (file_type != FileType::kUnknown && file_type != override_file_type) {
      LOG(WARNING) << "Based on the file name, the file type is " << extension
                   << ", But the overriden file type is "
                   << file_type_to_override << ". Is this intentional?";
    }
    file_type = override_file_type;
  }

  if (file_type == FileType::kRaw) {
    // Do not need to populate |deflates|.
    return true;
  }

  uint64_t stream_size;
  TEST_AND_RETURN_FALSE(stream->GetSize(&stream_size));
  Buffer data(stream_size);
  TEST_AND_RETURN_FALSE(stream->Read(data.data(), data.size()));
  switch (file_type) {
    case FileType::kDeflate:
      TEST_AND_RETURN_FALSE(puffin::LocateDeflatesInDeflateStream(
          data.data(), data.size(), 0, deflates, nullptr));
      break;
    case FileType::kZlib:
      TEST_AND_RETURN_FALSE(puffin::LocateDeflatesInZlib(data, deflates));
      break;
    case FileType::kGzip:
      TEST_AND_RETURN_FALSE(puffin::LocateDeflatesInGzip(data, deflates));
      break;
    case FileType::kZip:
      TEST_AND_RETURN_FALSE(puffin::LocateDeflatesInZipArchive(data, deflates));
      break;
    default:
      LOG(ERROR) << "Unknown file type: (" << file_type_to_override << ") nor ("
                 << extension << ").";
      return false;
  }
  // Return the stream to its zero offset in case we used it.
  TEST_AND_RETURN_FALSE(stream->Seek(0));

  return true;
}

}  // namespace

#define SETUP_FLAGS                                                          \
  DEFINE_string(src_file, "", "Source file");                                \
  DEFINE_string(dst_file, "", "Target file");                                \
  DEFINE_string(patch_file, "", "patch file");                               \
  DEFINE_string(                                                             \
      src_deflates_byte, "",                                                 \
      "Source deflate byte locations in the format offset:length,...");      \
  DEFINE_string(                                                             \
      dst_deflates_byte, "",                                                 \
      "Target deflate byte locations in the format offset:length,...");      \
  DEFINE_string(                                                             \
      src_deflates_bit, "",                                                  \
      "Source deflate bit locations in the format offset:length,...");       \
  DEFINE_string(                                                             \
      dst_deflates_bit, "",                                                  \
      "Target deflatebit locations in the format offset:length,...");        \
  DEFINE_string(src_puffs, "",                                               \
                "Source puff locations in the format offset:length,...");    \
  DEFINE_string(dst_puffs, "",                                               \
                "Target puff locations in the format offset:length,...");    \
  DEFINE_string(src_extents, "",                                             \
                "Source extents in the format of offset:length,...");        \
  DEFINE_string(dst_extents, "",                                             \
                "Target extents in the format of offset:length,...");        \
  DEFINE_string(operation, "",                                               \
                "Type of the operation: puff, huff, puffdiff, puffpatch, "   \
                "puffhuff");                                                 \
  DEFINE_string(src_file_type, "",                                           \
                "Type of the input source file: deflate, gzip, "             \
                "zlib or zip");                                              \
  DEFINE_string(dst_file_type, "",                                           \
                "Same as src_file_type but for the target file");            \
  DEFINE_bool(verbose, false,                                                \
              "Logs all the given parameters including internally "          \
              "generated ones");                                             \
  DEFINE_uint64(cache_size, kDefaultPuffCacheSize,                           \
                "Maximum size to cache the puff stream. Used in puffpatch"); \
  DEFINE_int32(patch_algorithm, 0,                                           \
               "Type of raw diff algorithm to use. The current supported "   \
               "ones are 0: bsdiff, 1: zucchini.");
#ifndef USE_BRILLO
SETUP_FLAGS;
#endif

// Main entry point to the application.
bool Main(int argc, char** argv) {
#ifdef USE_BRILLO
  SETUP_FLAGS;
  brillo::FlagHelper::Init(argc, argv, "Puffin tool");
#else
  // google::InitGoogleLogging(argv[0]);
  google::ParseCommandLineFlags(&argc, &argv, true);
#endif

  TEST_AND_RETURN_FALSE(!FLAGS_operation.empty());
  TEST_AND_RETURN_FALSE(!FLAGS_src_file.empty());
  TEST_AND_RETURN_FALSE(!FLAGS_dst_file.empty());

  auto src_deflates_byte = StringToExtents<ByteExtent>(FLAGS_src_deflates_byte);
  auto dst_deflates_byte = StringToExtents<ByteExtent>(FLAGS_dst_deflates_byte);
  auto src_deflates_bit = StringToExtents<BitExtent>(FLAGS_src_deflates_bit);
  auto dst_deflates_bit = StringToExtents<BitExtent>(FLAGS_dst_deflates_bit);
  auto src_puffs = StringToExtents<ByteExtent>(FLAGS_src_puffs);
  auto dst_puffs = StringToExtents<ByteExtent>(FLAGS_dst_puffs);
  auto src_extents = StringToExtents<ByteExtent>(FLAGS_src_extents);
  auto dst_extents = StringToExtents<ByteExtent>(FLAGS_dst_extents);

  auto src_stream = FileStream::Open(FLAGS_src_file, true, false);
  TEST_AND_RETURN_FALSE(src_stream);
  if (!src_extents.empty()) {
    src_stream =
        ExtentStream::CreateForRead(std::move(src_stream), src_extents);
    TEST_AND_RETURN_FALSE(src_stream);
  }

  if (FLAGS_operation == "puff" || FLAGS_operation == "puffhuff") {
    TEST_AND_RETURN_FALSE(LocateDeflatesBasedOnFileType(
        src_stream, FLAGS_src_file, FLAGS_src_file_type, &src_deflates_bit));

    if (src_deflates_bit.empty() && src_deflates_byte.empty()) {
      LOG(WARNING) << "You should pass source deflates, is this intentional?";
    }
    if (src_deflates_bit.empty()) {
      TEST_AND_RETURN_FALSE(FindDeflateSubBlocks(src_stream, src_deflates_byte,
                                                 &src_deflates_bit));
    }
    TEST_AND_RETURN_FALSE(dst_puffs.empty());
    uint64_t dst_puff_size;
    TEST_AND_RETURN_FALSE(FindPuffLocations(src_stream, src_deflates_bit,
                                            &dst_puffs, &dst_puff_size));

    auto dst_stream = FileStream::Open(FLAGS_dst_file, false, true);
    TEST_AND_RETURN_FALSE(dst_stream);
    auto puffer = std::make_shared<Puffer>();
    auto reader =
        PuffinStream::CreateForPuff(std::move(src_stream), puffer,
                                    dst_puff_size, src_deflates_bit, dst_puffs);

    Buffer puff_buffer;
    auto writer = FLAGS_operation == "puffhuff"
                      ? MemoryStream::CreateForWrite(&puff_buffer)
                      : std::move(dst_stream);

    Buffer buffer(1024 * 1024);
    uint64_t bytes_wrote = 0;
    while (bytes_wrote < dst_puff_size) {
      auto write_size = std::min(static_cast<uint64_t>(buffer.size()),
                                 dst_puff_size - bytes_wrote);
      TEST_AND_RETURN_FALSE(reader->Read(buffer.data(), write_size));
      TEST_AND_RETURN_FALSE(writer->Write(buffer.data(), write_size));
      bytes_wrote += write_size;
    }

    // puffhuff operation puffs a stream and huffs it back to the target stream
    // to make sure we can get to the original stream.
    if (FLAGS_operation == "puffhuff") {
      src_puffs = dst_puffs;
      dst_deflates_byte = src_deflates_byte;
      dst_deflates_bit = src_deflates_bit;

      auto read_puff_stream = MemoryStream::CreateForRead(puff_buffer);
      auto huffer = std::make_shared<Huffer>();
      auto huff_writer = PuffinStream::CreateForHuff(
          std::move(dst_stream), huffer, dst_puff_size, dst_deflates_bit,
          src_puffs);

      uint64_t bytes_read = 0;
      while (bytes_read < dst_puff_size) {
        auto read_size = std::min(static_cast<uint64_t>(buffer.size()),
                                  dst_puff_size - bytes_read);
        TEST_AND_RETURN_FALSE(read_puff_stream->Read(buffer.data(), read_size));
        TEST_AND_RETURN_FALSE(huff_writer->Write(buffer.data(), read_size));
        bytes_read += read_size;
      }
    }
  } else if (FLAGS_operation == "huff") {
    if (dst_deflates_bit.empty() && src_puffs.empty()) {
      LOG(WARNING) << "You should pass source puffs and destination deflates"
                   << ", is this intentional?";
    }
    TEST_AND_RETURN_FALSE(src_puffs.size() == dst_deflates_bit.size());
    uint64_t src_stream_size;
    TEST_AND_RETURN_FALSE(src_stream->GetSize(&src_stream_size));
    auto dst_file = FileStream::Open(FLAGS_dst_file, false, true);
    TEST_AND_RETURN_FALSE(dst_file);

    auto huffer = std::make_shared<Huffer>();
    auto dst_stream = PuffinStream::CreateForHuff(std::move(dst_file), huffer,
                                                  src_stream_size,
                                                  dst_deflates_bit, src_puffs);

    Buffer buffer(1024 * 1024);
    uint64_t bytes_read = 0;
    while (bytes_read < src_stream_size) {
      auto read_size = std::min(static_cast<uint64_t>(buffer.size()),
                                src_stream_size - bytes_read);
      TEST_AND_RETURN_FALSE(src_stream->Read(buffer.data(), read_size));
      TEST_AND_RETURN_FALSE(dst_stream->Write(buffer.data(), read_size));
      bytes_read += read_size;
    }
  } else if (FLAGS_operation == "puffdiff") {
    auto dst_stream = FileStream::Open(FLAGS_dst_file, true, false);
    TEST_AND_RETURN_FALSE(dst_stream);

    TEST_AND_RETURN_FALSE(LocateDeflatesBasedOnFileType(
        src_stream, FLAGS_src_file, FLAGS_src_file_type, &src_deflates_bit));
    TEST_AND_RETURN_FALSE(LocateDeflatesBasedOnFileType(
        dst_stream, FLAGS_dst_file, FLAGS_dst_file_type, &dst_deflates_bit));

    if (src_deflates_bit.empty() && src_deflates_byte.empty()) {
      LOG(WARNING) << "You should pass source deflates, is this intentional?";
    }
    if (dst_deflates_bit.empty() && dst_deflates_byte.empty()) {
      LOG(WARNING) << "You should pass target deflates, is this intentional?";
    }
    if (!dst_extents.empty()) {
      dst_stream =
          ExtentStream::CreateForWrite(std::move(dst_stream), dst_extents);
      TEST_AND_RETURN_FALSE(dst_stream);
    }

    if (src_deflates_bit.empty()) {
      TEST_AND_RETURN_FALSE(FindDeflateSubBlocks(src_stream, src_deflates_byte,
                                                 &src_deflates_bit));
    }

    if (dst_deflates_bit.empty()) {
      TEST_AND_RETURN_FALSE(FindDeflateSubBlocks(dst_stream, dst_deflates_byte,
                                                 &dst_deflates_bit));
    }

    if (FLAGS_patch_algorithm != 0 && FLAGS_patch_algorithm != 1) {
      LOG(ERROR)
          << "The supported patch algorithms are 0: bsdiff, 1: zucchini.";
      return false;
    }
    // TODO(xunchang) add flags to select the bsdiff compressors.
    Buffer puffdiff_delta;
    TEST_AND_RETURN_FALSE(puffin::PuffDiff(
        std::move(src_stream), std::move(dst_stream), src_deflates_bit,
        dst_deflates_bit,
        {bsdiff::CompressorType::kBZ2, bsdiff::CompressorType::kBrotli},
        static_cast<puffin::PatchAlgorithm>(FLAGS_patch_algorithm),
        "/tmp/patch.tmp", &puffdiff_delta));
    if (FLAGS_verbose) {
      LOG(INFO) << "patch_size: " << puffdiff_delta.size();
    }
    auto patch_stream = FileStream::Open(FLAGS_patch_file, false, true);
    TEST_AND_RETURN_FALSE(patch_stream);
    TEST_AND_RETURN_FALSE(
        patch_stream->Write(puffdiff_delta.data(), puffdiff_delta.size()));
  } else if (FLAGS_operation == "puffpatch") {
    auto patch_stream = FileStream::Open(FLAGS_patch_file, true, false);
    TEST_AND_RETURN_FALSE(patch_stream);
    uint64_t patch_size;
    TEST_AND_RETURN_FALSE(patch_stream->GetSize(&patch_size));

    Buffer puffdiff_delta(patch_size);
    TEST_AND_RETURN_FALSE(
        patch_stream->Read(puffdiff_delta.data(), puffdiff_delta.size()));
    auto dst_stream = FileStream::Open(FLAGS_dst_file, false, true);
    TEST_AND_RETURN_FALSE(dst_stream);
    if (!dst_extents.empty()) {
      dst_stream =
          ExtentStream::CreateForWrite(std::move(dst_stream), dst_extents);
      TEST_AND_RETURN_FALSE(dst_stream);
    }
    // Apply the patch. Use 50MB cache, it should be enough for most of the
    // operations.
    TEST_AND_RETURN_FALSE(puffin::PuffPatch(
        std::move(src_stream), std::move(dst_stream), puffdiff_delta.data(),
        puffdiff_delta.size(), FLAGS_cache_size));
  }

  if (FLAGS_verbose) {
    LOG(INFO) << "src_deflates_byte: "
              << puffin::ExtentsToString(src_deflates_byte);
    LOG(INFO) << "dst_deflates_byte: "
              << puffin::ExtentsToString(dst_deflates_byte);
    LOG(INFO) << "src_deflates_bit: "
              << puffin::ExtentsToString(src_deflates_bit);
    LOG(INFO) << "dst_deflates_bit: "
              << puffin::ExtentsToString(dst_deflates_bit);
    LOG(INFO) << "src_puffs: " << puffin::ExtentsToString(src_puffs);
    LOG(INFO) << "dst_puffs: " << puffin::ExtentsToString(dst_puffs);
    LOG(INFO) << "src_extents: " << puffin::ExtentsToString(src_extents);
    LOG(INFO) << "dst_extents: " << puffin::ExtentsToString(dst_extents);
  }
  return true;
}

int main(int argc, char** argv) {
  if (!Main(argc, argv)) {
    return 1;
  }
  return 0;
}
