// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_ANDROID_DEVIL_UTIL_TAR_READER_H_
#define TOOLS_ANDROID_DEVIL_UTIL_TAR_READER_H_

#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Forward declarations.
struct TarHeader;

class TarReader {
 private:
  // The root extraction directory. If this is empty string, the input archive
  // must contain absolute paths. Otherwise it must contain relative paths.
  std::string root_dir_;
  // The path of the file that is currently being extracted. This is the path
  // where we are putting the extracted file to.
  std::string cur_file_path_;
  // The size of the actual contents of the file that is being extracted.
  uint64_t cur_file_content_size_;
  // The size of the trailing paddings of the file that is being extracted.
  // The tar format adds trailing paddings after the actual contents of each
  // file, so that each file is 512-byte aligned.
  uint64_t cur_file_padding_size_;
  // The size of the currently extracted file that we have already read.
  uint64_t cur_file_already_read_size_;
  // The output file stream of the currently extracted file.
  std::ofstream cur_file_;
  // A pointer to a buffer containing the contents of a TarHeader that we have
  // partially read.
  std::unique_ptr<char[]> partial_header_;
  // The size of the partially read TarHeader. This equals 0 if there is no
  // partially read header.
  size_t partial_header_size_;
  // The number of consecutive all-zero blocks that we have seen immediately
  // before the current block. Two consecutive all-zero blocks indicate the end
  // of the tar file.
  int num_zero_block_;

  // Return true if this is a regular file, false if this is a directory.
  bool IsRegularFile(char typeflag, const std::string& file_path);
  // Set the cur_file_original_path and cur_file_path based on the header.
  void GetFilePath(TarHeader* header);
  // Read a char array containing an octal base-8 number and return the octal
  // number as uint64_t.
  uint64_t ReadOctalNumber(const char* octal_number, size_t length);
  // A helper function used to read the file that is currently being extracted.
  // It is used to either read the file from the very beginning, or resume
  // reading the file after reading some of it previously.
  void ReadCurrentFile(char** input_buffer, size_t* input_buffer_size);

 public:
  explicit TarReader(const std::string& extraction_root_dir);
  ~TarReader();

  // Extract the next portion of the input tar file. Repeatedly call this
  // function to extract the entirety of the input tar file. Return true if the
  // entirety of input tar file has been processed, and false otherwise.
  bool UntarStreaming(char* input_buffer, size_t input_buffer_size);
};

#endif  // TOOLS_ANDROID_DEVIL_UTIL_TAR_READER_H_
