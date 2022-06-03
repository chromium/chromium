// Copyright 2019 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "util/stream/file_encoder.h"

#include <memory>

#include "base/macros.h"
#include "util/file/file_io.h"
#include "util/file/file_reader.h"
#include "util/file/scoped_remove_file.h"
#include "util/stream/base94_output_stream.h"
#include "util/stream/file_output_stream.h"
#include "util/stream/output_stream_interface.h"
#include "util/stream/zlib_output_stream.h"

namespace crashpad {

FileEncoder::FileEncoder(Mode mode,
                         const base::FilePath& input_path,
                         const base::FilePath& output_path)
    : mode_(mode), input_path_(input_path), output_path_(output_path) {}

FileEncoder::~FileEncoder() {}

bool FileEncoder::Process() {
  ScopedRemoveFile file_remover;
  ScopedFileHandle write_handle(LoggingOpenFileForWrite(
      output_path_, FileWriteMode::kCreateOrFail, FilePermissions::kOwnerOnly));
  if (!write_handle.is_valid())
    return false;

  // Remove the output file on failure.
  file_remover.reset(output_path_);

  std::unique_ptr<OutputStreamInterface> output;
  if (mode_ == Mode::kEncode) {
    output = std::make_unique<ZlibOutputStream>(
        ZlibOutputStream::Mode::kCompress,
        std::make_unique<Base94OutputStream>(
            Base94OutputStream::Mode::kEncode,
            std::make_unique<FileOutputStream>(write_handle.get())));
  } else {
    output = std::make_unique<Base94OutputStream>(
        Base94OutputStream::Mode::kDecode,
        std::make_unique<ZlibOutputStream>(
            ZlibOutputStream::Mode::kDecompress,
            std::make_unique<FileOutputStream>(write_handle.get())));
  }

  FileReader file_reader;
  if (!file_reader.Open(input_path_))
    return false;

  FileOperationResult read_result;
  do {
    uint8_t buffer[4096];
    read_result = file_reader.Read(buffer, sizeof(buffer));
    if (read_result < 0)
      return false;

    if (read_result > 0 && (!output->Write(buffer, read_result)))
      return false;
  } while (read_result > 0);

  if (!output->Flush())
    return false;

  ignore_result(file_remover.release());
  return true;
}

}  // namespace crashpad
