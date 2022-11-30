// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/scoped_file_writer.h"

#include "base/files/file_util.h"
#include "base/logging.h"

namespace ui {

// ScopedFileWriter implementation.
ScopedFileWriter::ScopedFileWriter(const base::FilePath& path)
    : valid_(true), file_(base::OpenFile(path, "wb")) {
  if (!file_) {
    PLOG(ERROR) << "Could not open pak file for writing";
    valid_ = false;
  }
}

ScopedFileWriter::~ScopedFileWriter() {
  Close();
}

void ScopedFileWriter::Write(const void* data, size_t data_size) {
  if (!data_size)
    return;

  if (valid_ && fwrite(data, data_size, 1, file_) != 1) {
    PLOG(ERROR) << "Could not write to pak file";
    valid_ = false;
  }
}

bool ScopedFileWriter::Close() {
  if (file_) {
    valid_ = (fclose(file_) == 0);
    file_ = nullptr;
    if (!valid_) {
      PLOG(ERROR) << "Could not close pak file";
    }
  }
  return valid_;
}

}  // namespace ui
