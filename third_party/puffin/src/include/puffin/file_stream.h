// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_FILE_STREAM_H_
#define SRC_FILE_STREAM_H_

#include <string>
#include <utility>

#include "puffin/common.h"
#include "puffin/stream.h"

namespace puffin {

// A very simple class for reading and writing data into a file descriptor.
class FileStream : public StreamInterface {
 public:
  explicit FileStream(int fd) : fd_(fd) { Seek(0); }
  ~FileStream() override = default;

  static UniqueStreamPtr Open(const std::string& path, bool read, bool write);

  bool GetSize(uint64_t* size) const override;
  bool GetOffset(uint64_t* offset) const override;
  bool Seek(uint64_t offset) override;
  bool Read(void* buffer, size_t length) override;
  bool Write(const void* buffer, size_t length) override;
  bool Close() override;

 protected:
  FileStream() = default;

 private:
  // The file descriptor.
  int fd_;

  DISALLOW_COPY_AND_ASSIGN(FileStream);
};

}  // namespace puffin

#endif  // SRC_FILE_STREAM_H_
