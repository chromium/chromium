// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_RESOURCE_SCOPED_FILE_WRITER_H_
#define UI_BASE_RESOURCE_SCOPED_FILE_WRITER_H_

#include <stdio.h>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"

namespace ui {

// Convenience class to write data to a file. Usage is the following:
// 1) Create a new instance, passing a base::FilePath.
// 2) Call Write() repeatedly to write all desired data to the file.
// 3) Call valid() whenever you want to know if something failed.
// 4) The file is closed automatically on destruction. Though it is possible
//    to call the Close() method before that.
//
// If an I/O error happens, a PLOG(ERROR) message will be generated, and
// a flag will be set in the writer, telling it to ignore future Write()
// requests. This allows the caller to ignore error handling until the
// very end, as in:
//
//   {
//     base::ScopedFileWriter  writer(<some-path>);
//     writer.Write(&foo, sizeof(foo));
//     writer.Write(&bar, sizeof(bar));
//     ....
//     writer.Write(&zoo, sizeof(zoo));
//     if (!writer.valid()) {
//        // An error happened.
//     }
//   }   // closes the file.
//

class ScopedFileWriter {
 public:
  // Constructor takes a |path| parameter and tries to open the file.
  // Call valid() to check if the operation was successful.
  explicit ScopedFileWriter(const base::FilePath& path);

  ScopedFileWriter(const ScopedFileWriter&) = delete;
  ScopedFileWriter& operator=(const ScopedFileWriter&) = delete;

  // Destructor.
  ~ScopedFileWriter();

  // Return true if the last i/o operation was successful.
  bool valid() const { return valid_; }

  // Try to write |data_size| bytes from |data| into the file, if a previous
  // operation didn't already failed.
  void Write(const void* data, size_t data_size);

  // Close the file explicitly. Return true if all previous operations
  // succeeded, including the close, or false otherwise.
  bool Close();

 private:
  bool valid_ = false;
  raw_ptr<FILE, DanglingUntriaged> file_ =
      nullptr;  // base::ScopedFILE doesn't check errors on close.
};

}  // namespace ui

#endif  // UI_BASE_RESOURCE_SCOPED_FILE_WRITER_H_
