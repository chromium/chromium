// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "archive_reader.h"

#include <filesystem>
#include <iostream>

#include "tools/android/devil_util/archive_helper.h"

ArchiveReader::ArchiveReader() = default;

ArchiveReader::~ArchiveReader() = default;

void ArchiveReader::ReadMagicBytes(char** input_buffer,
                                   size_t* input_buffer_size) {
  if (*input_buffer_size == 0) {
    return;
  }
  if (magic_bytes_pos_ == kMagicBytesLength) {
    return;
  }

  uint64_t size_to_read = std::min(static_cast<uint64_t>(*input_buffer_size),
                                   kMagicBytesLength - magic_bytes_pos_);
  std::memcpy(magic_bytes_buffer_ + magic_bytes_pos_, *input_buffer,
              size_to_read);
  *input_buffer += size_to_read;
  *input_buffer_size -= size_to_read;
  magic_bytes_pos_ += size_to_read;

  if (magic_bytes_pos_ == kMagicBytesLength) {
    size_t magic_bytes_actual_length =
        strnlen(magic_bytes_buffer_, kMagicBytesLength);
    magic_bytes_ = std::string(magic_bytes_buffer_, magic_bytes_actual_length);
  }
}

void ArchiveReader::ReadCurMemberPathLength(char** input_buffer,
                                            size_t* input_buffer_size) {
  if (*input_buffer_size == 0) {
    return;
  }
  if (cur_member_path_length_pos_ == kPathLengthSize) {
    return;
  }

  uint64_t size_to_read =
      std::min(static_cast<uint64_t>(*input_buffer_size),
               kPathLengthSize - cur_member_path_length_pos_);
  std::memcpy(cur_member_path_length_buffer_ + cur_member_path_length_pos_,
              *input_buffer, size_to_read);
  *input_buffer += size_to_read;
  *input_buffer_size -= size_to_read;
  cur_member_path_length_pos_ += size_to_read;

  if (cur_member_path_length_pos_ == kPathLengthSize) {
    size_t path_length_actual_length =
        strnlen(cur_member_path_length_buffer_, kPathLengthSize);
    std::string path_length_str(cur_member_path_length_buffer_,
                                path_length_actual_length);
    cur_member_path_length_ = stoull(path_length_str);
  }
}

void ArchiveReader::ReadCurMemberPath(char** input_buffer,
                                      size_t* input_buffer_size) {
  if (*input_buffer_size == 0) {
    return;
  }
  if (cur_member_path_pos_ == cur_member_path_length_) {
    return;
  }

  uint64_t size_to_read =
      std::min(static_cast<uint64_t>(*input_buffer_size),
               cur_member_path_length_ - cur_member_path_pos_);
  std::memcpy(cur_member_path_buffer_ + cur_member_path_pos_, *input_buffer,
              size_to_read);
  *input_buffer += size_to_read;
  *input_buffer_size -= size_to_read;
  cur_member_path_pos_ += size_to_read;

  if (cur_member_path_pos_ == cur_member_path_length_) {
    cur_member_path_ =
        std::string(cur_member_path_buffer_, cur_member_path_length_);
    std::filesystem::path path(cur_member_path_);
    // Create the parent directories if necessary.
    std::filesystem::create_directories(path.parent_path());
    // Open the output stream which will also create the file.
    cur_member_ofstream_ =
        std::ofstream(cur_member_path_, std::ios::binary | std::ios::trunc);
    if (cur_member_ofstream_.fail()) {
      std::cerr << "Failed to open the file at: " << cur_member_path_
                << std::endl;
      exit(1);
    }
    // Set the file permission to std::filesystem::perms::all which is 0777.
    std::filesystem::permissions(cur_member_path_, std::filesystem::perms::all);
  }
}

void ArchiveReader::ReadCurMemberContentLength(char** input_buffer,
                                               size_t* input_buffer_size) {
  if (*input_buffer_size == 0) {
    return;
  }
  if (cur_member_content_length_pos_ == kContentLengthSize) {
    return;
  }

  uint64_t size_to_read =
      std::min(static_cast<uint64_t>(*input_buffer_size),
               kContentLengthSize - cur_member_content_length_pos_);
  std::memcpy(
      cur_member_content_length_buffer_ + cur_member_content_length_pos_,
      *input_buffer, size_to_read);
  *input_buffer += size_to_read;
  *input_buffer_size -= size_to_read;
  cur_member_content_length_pos_ += size_to_read;

  if (cur_member_content_length_pos_ == kContentLengthSize) {
    size_t content_length_actual_length =
        strnlen(cur_member_content_length_buffer_, kContentLengthSize);
    std::string content_length_str(cur_member_content_length_buffer_,
                                   content_length_actual_length);
    cur_member_content_length_ = stoull(content_length_str);
  }
}

void ArchiveReader::ReadCurMemberContent(char** input_buffer,
                                         size_t* input_buffer_size) {
  if (*input_buffer_size == 0) {
    return;
  }
  if (cur_member_content_pos_ == cur_member_content_length_) {
    return;
  }

  uint64_t size_to_read =
      std::min(static_cast<uint64_t>(*input_buffer_size),
               cur_member_content_length_ - cur_member_content_pos_);
  cur_member_ofstream_.write(*input_buffer, size_to_read);
  if (cur_member_ofstream_.fail()) {
    std::cerr << "Failed to write to the file at: " << cur_member_path_
              << std::endl;
    exit(1);
  }
  *input_buffer += size_to_read;
  *input_buffer_size -= size_to_read;
  cur_member_content_pos_ += size_to_read;
}

bool ArchiveReader::ExtractArchiveStreaming(char* input_buffer,
                                            size_t input_buffer_size) {
  if (input_buffer_size == 0) {
    return false;
  }

  ReadMagicBytes(&input_buffer, &input_buffer_size);
  if (magic_bytes_pos_ < kMagicBytesLength) {
    return false;
  } else {
    if (magic_bytes_ != kDevilUtilArchiveV1MagicBytes) {
      std::cerr << "The magic bytes at the beginning of the archive does not "
                   "match the expected value!"
                << std::endl;
      std::cerr << "Expected magic bytes: " << kDevilUtilArchiveV1MagicBytes
                << std::endl;
      std::cerr << "Actual magic bytes: " << magic_bytes_ << std::endl;
      exit(1);
    }
  }

  while (true) {
    // If we are processing a new member, reset all internal states.
    if (start_new_member_) {
      cur_member_path_length_pos_ = 0;
      cur_member_path_pos_ = 0;
      cur_member_content_length_pos_ = 0;
      cur_member_content_pos_ = 0;
      start_new_member_ = false;
    }
    ReadCurMemberPathLength(&input_buffer, &input_buffer_size);
    // A path length of 0 indicates the end of the archive.
    if (cur_member_path_length_pos_ == kPathLengthSize &&
        cur_member_path_length_ == 0) {
      return true;
    }
    ReadCurMemberPath(&input_buffer, &input_buffer_size);
    ReadCurMemberContentLength(&input_buffer, &input_buffer_size);
    ReadCurMemberContent(&input_buffer, &input_buffer_size);
    if (cur_member_content_length_pos_ == kContentLengthSize &&
        cur_member_content_pos_ == cur_member_content_length_ &&
        cur_member_path_length_pos_ == kPathLengthSize &&
        cur_member_path_pos_ == cur_member_path_length_) {
      // We have finished reading the current member.
      cur_member_ofstream_.close();
      start_new_member_ = true;
    } else {
      // We have only partially read the current member.
      start_new_member_ = false;
      return false;
    }
  }
}
