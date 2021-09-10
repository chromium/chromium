// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Extract VBA code from Office document.
//
// Sample usage to print code on the screen:
//   vba_extractor --input=/input_dir/example.doc --print_code=true
//
// Printed code looks like
//   ========================================
//   Input: /input_dir/example.doc
//   SHA256: e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
//   Chunks: 2
//   --------------------------------
//   Path: /Macros/VBA/ThisDocument
//   File: ThisDocument.cls
//   MD5: e4bd95e717d9c3db44176d7aff23004b
//   --------------------------------
//   Attribute VB_Name = "ThisDocument"
//   ......
//   =======================================
//
// Sample usage to save code into files:
//   vba_extractor --input=/input_dir/example.doc --output=/output_dir
//
// Saved files look like
//   /output_dir/
//     e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855/
//       e4bd95e717d9c3db44176d7aff23004b
//       c6684c77bce7dfac5e5e12a0b68186ba
//
// Sample usage to extract code from all the office documents in the same dir:
//   vba_extractor --input=/input_dir/* --output=/output_dir

#include <iostream>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/strings/escaping.h"
#include "maldoca/base/digest.h"
#include "maldoca/base/file.h"
#include "maldoca/base/logging.h"
#include "maldoca/base/status.h"
#include "maldoca/ole/oss_utils.h"
#include "maldoca/ole/vba_extract.h"

ABSL_FLAG(std::string, input, "",
          "A file, or a pattern to match multiple files in the same "
          "directory (dir/*) to extract VBA code chunks from. (required)");
ABSL_FLAG(std::string, output, "",
          "Output directory where VBA code chunks are to be saved. When "
          "specified, a folder (named SHA256 of the input file content) "
          "will be automatically created in this output directory, and "
          "each code chunk will be saved as a file (named MD5 of the code "
          "chunk) in that new folder.");
ABSL_FLAG(bool, print_code, false,
          "When set to true, print the code of each VBA code chunk to "
          "stdout.");

using ::maldoca::ExtractVBAFromString;
using ::maldoca::VBACodeChunk;
using ::maldoca::VBACodeChunks;

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);

  std::vector<std::string> files;

#ifndef MALDOCA_CHROME
  CHECK(maldoca::file::Match(absl::GetFlag(FLAGS_input), &files).ok())
      << "Can not list files from pattern match \""
      << absl::GetFlag(FLAGS_input) << "\".";

  if (files.empty()) {
    LOG(ERROR) << "No file matches --input=\"" << absl::GetFlag(FLAGS_input)
               << "\".";
  }

  for (const std::string& filename : files) {
    auto status_or = maldoca::file::GetContents(filename);
    CHECK(status_or.ok()) << "Can not get content for \"" << filename << "\".";
    std::string content = status_or.value();
    VBACodeChunks code_chunks;
    std::string error;
    ExtractVBAFromString(content, &code_chunks, &error);

    std::cout << "========================================" << std::endl;
    std::cout << "Input: " << filename << std::endl;

    std::string sha256_hex = maldoca::Sha256HexString(content);
    std::cout << "SHA256: " << sha256_hex << std::endl;
    std::cout << "Chunks: " << code_chunks.chunk_size() << std::endl;

    if (code_chunks.chunk_size() > 0) {
      std::string output_dir;
      if (!absl::GetFlag(FLAGS_output).empty()) {
        output_dir =
            maldoca::file::JoinPath(absl::GetFlag(FLAGS_output), sha256_hex);
        // Ignore the error when creating a folder that already exists, and
        // defer error checking to individual files. Reusing existing folder
        // makes this function reentrant for low-priority flume jobs.
        maldoca::file::CreateDir(output_dir).IgnoreError();
      }

      for (const VBACodeChunk& chunk : code_chunks.chunk()) {
        std::cout << "--------------------------------" << std::endl;
        std::cout << "Path: " << chunk.path() << std::endl;
        std::cout << "File: " << chunk.filename() << std::endl;

        std::string md5_hex = maldoca::Md5Digest(chunk.code());
        std::cout << "MD5: " << md5_hex << std::endl;

        if (!absl::GetFlag(FLAGS_output).empty()) {
          std::string output_path =
              maldoca::file::JoinPath(output_dir, md5_hex);
          std::cout << "SaveTo: " << output_path << std::endl;
          MALDOCA_CHECK_OK(
              maldoca::file::SetContents(output_path, chunk.code()))
              << "Can not write file " << output_path;
        }

        if (absl::GetFlag(FLAGS_print_code)) {
          std::cout << "--------------------------------" << std::endl;
          std::cout << chunk.code() << std::endl;
        }
      }
    }

    if (!error.empty()) {
      std::cout << "--------------------------------" << std::endl;
      std::cout << "Error: " << error << std::endl;
    }

    std::cout << "========================================" << std::endl;
  }
#endif  // MALDOCA_CHROME

  return 0;
}
