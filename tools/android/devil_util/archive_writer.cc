// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "archive_writer.h"

#include <filesystem>
#include <iostream>

#include "base/strings/string_number_conversions.h"
#include "tools/android/devil_util/archive_helper.h"

ArchiveWriter::ArchiveWriter(std::vector<ArchiveMember> members)
    : members_(std::move(members)) {}

ArchiveWriter::~ArchiveWriter() = default;

void ArchiveWriter::WriteMagicBytes(char** output_buffer,
                                    size_t* output_buffer_size) {
  if (*output_buffer_size == 0) {
    return;
  }
  if (magic_bytes_pos_ == kMagicBytesLength) {
    return;
  }

  // Create a string containing the magic bytes that is exactly
  // kMagicBytesLength characters long. Add \0 at the end if needed.
  std::string magic_bytes_str = std::string(kDevilUtilArchiveV1MagicBytes);
  std::string magic_bytes_padding(kMagicBytesLength - magic_bytes_str.length(),
                                  '\0');
  std::string magic_bytes_str_padded = magic_bytes_str + magic_bytes_padding;

  uint64_t size_to_write = std::min(static_cast<uint64_t>(*output_buffer_size),
                                    kMagicBytesLength - magic_bytes_pos_);
  magic_bytes_str_padded.copy(*output_buffer, size_to_write, magic_bytes_pos_);
  *output_buffer += size_to_write;
  *output_buffer_size -= size_to_write;
  magic_bytes_pos_ += size_to_write;
}

void ArchiveWriter::WriteCurMemberPathLength(char** output_buffer,
                                             size_t* output_buffer_size) {
  if (*output_buffer_size == 0) {
    return;
  }
  if (cur_member_path_length_pos_ == kPathLengthSize) {
    return;
  }

  // Create a string containing the path length that is exactly kPathLengthSize
  // characters long. Add \0 at the end if needed.
  size_t path_length = cur_member_path_.length();
  std::string path_length_str = base::NumberToString(path_length);
  if (path_length_str.length() > kPathLengthSize) {
    std::cerr << "This archive path is way too long: " << cur_member_path_
              << std::endl;
    exit(1);
  }
  std::string path_length_padding(kPathLengthSize - path_length_str.length(),
                                  '\0');
  std::string path_length_str_padded = path_length_str + path_length_padding;

  uint64_t size_to_write =
      std::min(static_cast<uint64_t>(*output_buffer_size),
               kPathLengthSize - cur_member_path_length_pos_);
  path_length_str_padded.copy(*output_buffer, size_to_write,
                              cur_member_path_length_pos_);
  *output_buffer += size_to_write;
  *output_buffer_size -= size_to_write;
  cur_member_path_length_pos_ += size_to_write;
}

void ArchiveWriter::WriteCurMemberPath(char** output_buffer,
                                       size_t* output_buffer_size) {
  if (*output_buffer_size == 0) {
    return;
  }
  uint64_t path_length = cur_member_path_.length();
  if (cur_member_path_pos_ == path_length) {
    return;
  }

  uint64_t size_to_write = std::min(static_cast<uint64_t>(*output_buffer_size),
                                    path_length - cur_member_path_pos_);
  cur_member_path_.copy(*output_buffer, size_to_write, cur_member_path_pos_);
  *output_buffer += size_to_write;
  *output_buffer_size -= size_to_write;
  cur_member_path_pos_ += size_to_write;
}

void ArchiveWriter::WriteCurMemberContentLength(char** output_buffer,
                                                size_t* output_buffer_size) {
  if (*output_buffer_size == 0) {
    return;
  }
  if (cur_member_content_length_pos_ == kContentLengthSize) {
    return;
  }

  // Create a string containing the content length that is exactly
  // kContentLengthSize characters long. Add \0 at the end if needed.
  std::string content_length_str =
      base::NumberToString(cur_member_content_length_);
  if (content_length_str.length() > kContentLengthSize) {
    std::cerr << "This file is way too large: "
              << members_[cur_member_index_].file_path_in_host << std::endl;
    exit(1);
  }
  std::string content_length_padding(
      kContentLengthSize - content_length_str.length(), '\0');
  std::string content_length_str_padded =
      content_length_str + content_length_padding;

  uint64_t size_to_write =
      std::min(static_cast<uint64_t>(*output_buffer_size),
               kContentLengthSize - cur_member_content_length_pos_);
  content_length_str_padded.copy(*output_buffer, size_to_write,
                                 cur_member_content_length_pos_);
  *output_buffer += size_to_write;
  *output_buffer_size -= size_to_write;
  cur_member_content_length_pos_ += size_to_write;
}

void ArchiveWriter::WriteCurMemberContent(char** output_buffer,
                                          size_t* output_buffer_size) {
  if (*output_buffer_size == 0) {
    return;
  }
  if (cur_member_content_pos_ == cur_member_content_length_) {
    return;
  }

  uint64_t size_to_write =
      std::min(static_cast<uint64_t>(*output_buffer_size),
               cur_member_content_length_ - cur_member_content_pos_);
  cur_member_ifstream_.read(*output_buffer, size_to_write);
  if (cur_member_ifstream_.fail()) {
    std::cerr << "Failed to read the file at: "
              << members_[cur_member_index_].file_path_in_host << std::endl;
    exit(1);
  }
  *output_buffer += size_to_write;
  *output_buffer_size -= size_to_write;
  cur_member_content_pos_ += size_to_write;
}

size_t ArchiveWriter::CreateArchiveStreaming(char* output_buffer,
                                             size_t output_buffer_size) {
  if (output_buffer_size == 0) {
    return 0;
  }
  size_t output_buffer_size_original = output_buffer_size;

  WriteMagicBytes(&output_buffer, &output_buffer_size);
  if (magic_bytes_pos_ < kMagicBytesLength) {
    return output_buffer_size_original;
  }

  for (; cur_member_index_ < members_.size(); ++cur_member_index_) {
    // If we are processing a new member, reset all internal states.
    if (start_next_member_) {
      cur_member_path_ = members_[cur_member_index_].file_path_in_archive;
      const std::string& path = members_[cur_member_index_].file_path_in_host;
      cur_member_content_length_ = std::filesystem::file_size(path);
      cur_member_ifstream_ = std::ifstream(path, std::ios::binary);
      if (cur_member_ifstream_.fail()) {
        std::cerr << "Failed to open the file at: " << path << std::endl;
        exit(1);
      }
      cur_member_path_length_pos_ = 0;
      cur_member_path_pos_ = 0;
      cur_member_content_length_pos_ = 0;
      cur_member_content_pos_ = 0;
      start_next_member_ = false;
    }
    WriteCurMemberPathLength(&output_buffer, &output_buffer_size);
    WriteCurMemberPath(&output_buffer, &output_buffer_size);
    WriteCurMemberContentLength(&output_buffer, &output_buffer_size);
    WriteCurMemberContent(&output_buffer, &output_buffer_size);
    if (cur_member_content_pos_ == cur_member_content_length_ &&
        cur_member_content_length_pos_ == kContentLengthSize &&
        cur_member_path_pos_ == cur_member_path_.length() &&
        cur_member_path_length_pos_ == kPathLengthSize) {
      // We have finished processing the current member.
      cur_member_ifstream_.close();
      start_next_member_ = true;
    } else {
      // We have only partially processed the current member.
      start_next_member_ = false;
      return output_buffer_size_original;
    }
  }

  // End the archive with a path length of 0.
  if (start_next_member_) {
    cur_member_path_ = "";
    cur_member_path_length_pos_ = 0;
    start_next_member_ = false;
  }
  WriteCurMemberPathLength(&output_buffer, &output_buffer_size);
  if (cur_member_path_length_pos_ == kPathLengthSize) {
    return output_buffer_size_original - output_buffer_size;
  } else {
    return output_buffer_size_original;
  }
}
