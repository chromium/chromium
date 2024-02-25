// Copyright 2022 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"

#include "puffin/file_stream.h"
#include "puffin/memory_stream.h"
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

void PrintHelp() {
  LOG(WARNING) << "Main Usage:\n"
               << "puffdiff usage:\n"
               << "  puffin -puffdiff <source_file_path> "
                  "<destination_file_path> <output_patch_file_path>\n"
               << "puffpatch usage:\n"
               << " puffin -puffpatch <source_file_path> <output_file_path> "
                  "<input_patch_file_path>";
}

// An enum representing the type of compressed files.
enum class FileType { kDeflate, kZlib, kGzip, kZip, kRaw, kUnknown };

// Returns a file type based on the input string `file_type` (normally the final
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
  } else if (file_type == "zip" || file_type == "apk" || file_type == "jar" ||
             file_type == "crx" || file_type == "crx2" || file_type == "crx3") {
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
                                   vector<BitExtent>* deflates) {
  auto last_dot = file_name.find_last_of(".");
  if (last_dot == string::npos) {
    // Could not find a dot so we assume there is no extension.
    return false;
  }
  auto extension = file_name.substr(last_dot + 1);
  FileType file_type = StringToFileType(extension);

  if (file_type == FileType::kRaw) {
    // Do not need to populate |deflates|.
    return true;
  }

  uint64_t stream_size = 0;
  if (!stream->GetSize(&stream_size)) {
    LOG(ERROR) << "Unable to get streamsize for file: " << file_name;
    return false;
  }
  Buffer data(stream_size);
  if (!stream->Read(data.data(), data.size())) {
    LOG(ERROR) << "Unable to read stream for file: " << file_name;
    return false;
  }
  switch (file_type) {
    case FileType::kDeflate:
      if (!puffin::LocateDeflatesInDeflateStream(data.data(), data.size(), 0,
                                                 deflates, nullptr)) {
        LOG(ERROR) << "Unable to find deflates in deflate stream for file: "
                   << file_name;
        return false;
      }
      break;
    case FileType::kZlib:
      if (!puffin::LocateDeflatesInZlib(data, deflates)) {
        LOG(ERROR) << "Unable to find zlib deflates for file: " << file_name;
        return false;
      }
      break;
    case FileType::kGzip:
      if (!puffin::LocateDeflatesInGzip(data, deflates)) {
        LOG(ERROR) << "Unable to find gzip deflates for file: " << file_name;
        return false;
      }
      break;
    case FileType::kZip:
      if (!puffin::LocateDeflatesInZipArchive(data, deflates)) {
        LOG(ERROR) << "Unable to find zip deflates for file: " << file_name;
        return false;
      }
      break;
    default:
      LOG(ERROR) << "Unknown file type: (" << extension << ").";
      return false;
  }
  // Return the stream to its zero offset in case we used it.
  if (!stream->Seek(0)) {
    LOG(ERROR) << "Unable to return stream to its zero offset.";
    return false;
  }

  return true;
}

}  // namespace

bool ExecutePuffDiff(const string& src_file_path,
                     const string& dst_file_path,
                     const string& patch_file_path) {
  auto src_deflates_byte = vector<ByteExtent>();
  auto dst_deflates_byte = vector<ByteExtent>();
  auto src_deflates_bit = vector<BitExtent>();
  auto dst_deflates_bit = vector<BitExtent>();
  auto src_puffs = vector<ByteExtent>();
  auto dst_puffs = vector<ByteExtent>();
  puffin::UniqueStreamPtr src_stream =
      FileStream::Open(src_file_path, true, false);
  if (!src_stream) {
    LOG(ERROR) << "Second argument of puffdiff must be a valid source filepath";
    PrintHelp();
    return false;
  }
  puffin::UniqueStreamPtr dst_stream =
      FileStream::Open(dst_file_path, true, false);
  if (!dst_stream) {
    LOG(ERROR) << "Third argument must be a valid destination filepath";
    PrintHelp();
    return false;
  }

  if (!LocateDeflatesBasedOnFileType(src_stream, src_file_path,
                                     &src_deflates_bit)) {
    LOG(ERROR) << "Failed to locate deflates base on source filetype";
    PrintHelp();
    return false;
  }
  if (!LocateDeflatesBasedOnFileType(dst_stream, dst_file_path,
                                     &dst_deflates_bit)) {
    LOG(ERROR) << "Failed to locate deflates base on destination filetype";
    PrintHelp();
    return false;
  }

  if (src_deflates_bit.empty() && src_deflates_byte.empty()) {
    LOG(WARNING) << "Warning: "
                 << "You should pass source deflates, is this intentional?";
  }
  if (dst_deflates_bit.empty() && dst_deflates_byte.empty()) {
    LOG(WARNING) << "Warning: "
                 << "You should pass target deflates, is this intentional?";
  }
  if (src_deflates_bit.empty()) {
    if (!FindDeflateSubBlocks(src_stream, src_deflates_byte,
                              &src_deflates_bit)) {
      LOG(ERROR) << "Unable to find deflate subblocks for source.";
      return false;
    }
  }

  if (dst_deflates_bit.empty()) {
    if (!FindDeflateSubBlocks(dst_stream, dst_deflates_byte,
                              &dst_deflates_bit)) {
      LOG(ERROR) << "Unable to find deflate subblocks for destination";
      return false;
    }
  }

  Buffer puffdiff_delta;
  if (!puffin::PuffDiff(std::move(src_stream), std::move(dst_stream),
                        src_deflates_bit, dst_deflates_bit,
                        {puffin::CompressorType::kBrotli},
                        // TODO(crbug.com/1321247): we are currently just ALWAYS
                        // doing zucchini with brotli, we might come back and
                        // support bsdiff and/or bzip2.
                        puffin::PatchAlgorithm::kZucchini, "/tmp/patch.tmp",
                        &puffdiff_delta)) {
    LOG(ERROR) << "Unable to generate PuffDiff";
    return false;
  }
  LOG(INFO) << "patch_size: " << puffdiff_delta.size();
  puffin::UniqueStreamPtr patch_stream =
      FileStream::Open(patch_file_path, false, true);
  if (!patch_stream) {
    LOG(ERROR) << "Unable to open patch Stream";
    return false;
  }
  if (!patch_stream->Write(puffdiff_delta.data(), puffdiff_delta.size())) {
    LOG(ERROR) << "Unable to write to patch stream";
    return false;
  }
  return true;
}

bool ExecutePuffPatch(const string& src_file_path,
                      const string& dst_file_path,
                      const string& patch_file_path) {
  auto src_deflates_byte = vector<ByteExtent>();
  auto dst_deflates_byte = vector<ByteExtent>();
  auto src_deflates_bit = vector<BitExtent>();
  auto dst_deflates_bit = vector<BitExtent>();
  auto src_puffs = vector<ByteExtent>();
  auto dst_puffs = vector<ByteExtent>();
  puffin::UniqueStreamPtr src_stream =
      FileStream::Open(src_file_path, true, false);
  if (!src_stream) {
    LOG(ERROR) << "Second argument must be a valid source filepath";
    PrintHelp();
    return false;
  }
  puffin::UniqueStreamPtr patch_stream =
      FileStream::Open(patch_file_path, true, false);
  if (!patch_stream) {
    LOG(ERROR) << "Unable to open patch stream";
    return false;
  }
  uint64_t patch_size = 0;
  if (!patch_stream->GetSize(&patch_size)) {
    LOG(ERROR) << "Unable obtain patch stream size";
    return false;
  }

  Buffer puffdiff_delta(patch_size);
  if (!patch_stream->Read(puffdiff_delta.data(), puffdiff_delta.size())) {
    LOG(ERROR) << "Unable to read patch stream";
    return false;
  }
  puffin::UniqueStreamPtr dst_stream =
      FileStream::Open(dst_file_path, false, true);
  if (!dst_stream) {
    LOG(ERROR) << "Unable to open destination stream";
    return false;
  }
  // Apply the patch. Use 50MB cache, it should be enough for most of the
  // operations.
  auto status = puffin::PuffPatch(std::move(src_stream), std::move(dst_stream),
                                  puffdiff_delta.data(), puffdiff_delta.size(),
                                  puffin::kDefaultPuffCacheSize);
  if (status != puffin::Status::P_OK) {
    LOG(ERROR) << "Unable to patch file, failed with error: " << status << ".";
    return false;
  }
  LOG(INFO) << "File Patched successfully!";
  LOG(INFO) << "Output file: " << dst_file_path;
  return true;
}

bool Main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_ALL;
  settings.log_file_path = FILE_PATH_LITERAL("puffin.log");
  std::ignore = logging::InitLogging(settings);
  logging::SetMinLogLevel(logging::LOGGING_VERBOSE);
  bool cmd_puffdiff = command_line.HasSwitch("puffdiff");
  bool cmd_puffpatch = command_line.HasSwitch("puffpatch");

  std::vector<base::FilePath> values;
  for (const auto& arg : command_line.GetArgs()) {
    values.emplace_back(arg);
  }

  if (cmd_puffdiff && !cmd_puffpatch) {
    if (values.size() != 3) {
      PrintHelp();
      return false;
    }

    std::string src_file_path(values[0].AsUTF8Unsafe());
    std::string dst_file_path(values[1].AsUTF8Unsafe());
    std::string patch_file_path(values[2].AsUTF8Unsafe());

    return ExecutePuffDiff(src_file_path, dst_file_path, patch_file_path);
  } else if (cmd_puffpatch && !cmd_puffdiff) {
    if (values.size() != 3) {
      PrintHelp();
      return false;
    }

    std::string src_file_path(values[0].AsUTF8Unsafe());
    std::string dst_file_path(values[1].AsUTF8Unsafe());
    std::string patch_file_path(values[2].AsUTF8Unsafe());

    return ExecutePuffPatch(src_file_path, dst_file_path, patch_file_path);
  }
  // Only one operation must be selected. If zero or both are selected, fail.
  LOG(ERROR) << "First argument must be one of:\n"
                "  -puffdiff or -puffpatch.";
  PrintHelp();
  return false;
}

int main(int argc, char** argv) {
  if (!Main(argc, argv)) {
    return 1;
  }
  return 0;
}
