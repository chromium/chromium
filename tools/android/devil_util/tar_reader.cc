// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tar_reader.h"

#include <filesystem>
#include <iostream>

static const size_t kTarBlockSize = 512;
static const size_t kTarNameLength = 100;
static const size_t kTarSizeLength = 12;
static const size_t kTarMagicLength = 6;
static const size_t kTarPrefixLength = 155;
static const std::string_view kTarMagicUstar = "ustar";

// Below are all the type flags that can be found in a tar header.
// Unused type flags are commented out to prevent unused variable warnings.
static const char kRegType = '0';
static const char kAregType = '\0';
// static const char kLinkType = '1';
// static const char kSymType = '2';
// static const char kChrType = '3';
// static const char kBlkType = '4';
static const char kDirType = '5';
// static const char kFifoType = '6';
// static const char kContType = '7';

struct __attribute__((packed)) TarHeader {
  char name[100];
  char mode[8];
  char uid[8];
  char gid[8];
  char size[12];
  char mtime[12];
  char chksum[8];
  char typeflag;
  char linkname[100];
  char magic[6];
  char version[2];
  char uname[32];
  char gname[32];
  char devmajor[8];
  char devminor[8];
  char prefix[155];
  char padding[12];
};

TarReader::TarReader(const std::string& extraction_root_dir) {
  if (extraction_root_dir.empty()) {
    // The tar file should contain absolute file paths.
    root_dir_ = "";
  } else {
    // The tar file should contain relative file paths.
    if (extraction_root_dir.front() != '/') {
      std::cerr
          << "The extraction_root_dir passed to the tar reader must be an "
             "absolute path (start with slash) or an empty string!"
          << std::endl;
      exit(1);
    }
    if (extraction_root_dir.back() == '/') {
      root_dir_ = extraction_root_dir;
    } else {
      root_dir_ = extraction_root_dir + '/';
    }
  }

  cur_file_path_ = "";
  cur_file_content_size_ = 0;
  cur_file_padding_size_ = 0;
  cur_file_already_read_size_ = 0;
  cur_file_ = std::ofstream();
  partial_header_ = std::make_unique<char[]>(kTarBlockSize);
  partial_header_size_ = 0;
  num_zero_block_ = 0;
}

TarReader::~TarReader() = default;

bool TarReader::IsRegularFile(char typeflag, const std::string& file_path) {
  if (typeflag == kRegType) {
    return true;
  } else if (typeflag == kAregType) {
    if (file_path.back() == '/') {
      return false;
    } else {
      return true;
    }
  } else if (typeflag == kDirType) {
    return false;
  } else {
    std::cerr << "This tar file uses an advanced feature that is currently not "
                 "supported by this tar reader. More specifically, the file at "
              << file_path << " has a type flag of " << typeflag << std::endl;
    exit(1);
  }
}

void TarReader::GetFilePath(TarHeader* header) {
  size_t name_actual_length = strnlen(header->name, kTarNameLength);
  std::string name(header->name, name_actual_length);
  size_t magic_actual_length = strnlen(header->magic, kTarMagicLength);
  std::string magic(header->magic, magic_actual_length);
  size_t prefix_actual_length = strnlen(header->prefix, kTarPrefixLength);
  std::string prefix(header->prefix, prefix_actual_length);

  // The name field in the header is 100 characters in size, which means the
  // file paths can be no more than 100 characters long. The "ustar" tar format
  // uses the prefix field to store the file path when the file path is over 100
  // characters. Since the prefix field is 155 characters in size, this allows
  // us to have file paths that are up to 255 characters long. This is still not
  // a lot, but seems good enough for our use cases. If needed, we can support
  // more modern tar formats that use more complicated schema to allow for
  // unlimitedly file paths.
  if (magic == kTarMagicUstar && prefix != "") {
    cur_file_path_ = prefix + "/" + name;
  } else {
    cur_file_path_ = name;
  }

  if (root_dir_.empty()) {
    if (cur_file_path_.front() != '/') {
      std::cerr
          << "Tar reader received an empty string as extraction_root_dir, "
             "so the tar file should contain absolute paths, but it contains "
             "this relative path: "
          << cur_file_path_ << std::endl;
      exit(1);
    }
  } else {
    if (cur_file_path_.front() == '/') {
      std::cerr
          << "Tar reader received a non empty string as extraction_root_dir, "
             "so the tar file should contain relative paths, but it contains "
             "this absolute path: "
          << cur_file_path_ << std::endl;
      exit(1);
    }
    cur_file_path_ = root_dir_ + cur_file_path_;
  }
}

uint64_t TarReader::ReadOctalNumber(const char* octal_number, size_t length) {
  uint64_t ret = 0;
  for (size_t i = 0; i < length; ++i) {
    char cur_char = octal_number[i];
    if (cur_char == '\0') {
      break;
    }
    ret *= 8;
    ret += cur_char - '0';
  }
  return ret;
}

void TarReader::ReadCurrentFile(char** input_buffer,
                                size_t* input_buffer_size) {
  if (cur_file_content_size_ == 0 && cur_file_padding_size_ == 0) {
    cur_file_.close();
    return;
  }
  if (*input_buffer_size == 0) {
    return;
  }

  // Read the actual contents of the file being extracted.
  if (cur_file_already_read_size_ < cur_file_content_size_) {
    uint64_t size_to_read =
        std::min(cur_file_content_size_ - cur_file_already_read_size_,
                 (uint64_t)*input_buffer_size);
    cur_file_.write(*input_buffer, size_to_read);
    if (cur_file_.fail()) {
      std::cerr << "Tar reader failed to write to the file at "
                << cur_file_path_ << std::endl;
      exit(1);
    }
    *input_buffer += size_to_read;
    *input_buffer_size -= size_to_read;
    cur_file_already_read_size_ += size_to_read;
    if (cur_file_already_read_size_ < cur_file_content_size_) {
      return;
    }
  }

  if (cur_file_padding_size_ == 0) {
    cur_file_.close();
    return;
  }
  if (*input_buffer_size == 0) {
    return;
  }

  // Read the trailing paddings of the file being extracted.
  uint64_t size_to_read =
      std::min(cur_file_content_size_ + cur_file_padding_size_ -
                   cur_file_already_read_size_,
               (uint64_t)*input_buffer_size);
  *input_buffer += size_to_read;
  *input_buffer_size -= size_to_read;
  cur_file_already_read_size_ += size_to_read;

  // If this is true, then we have finished reading the current file.
  if (cur_file_already_read_size_ ==
      cur_file_content_size_ + cur_file_padding_size_) {
    cur_file_.close();
  }
}

bool TarReader::UntarStreaming(char* input_buffer, size_t input_buffer_size) {
  if (input_buffer_size == 0) {
    return false;
  }

  TarHeader* header = nullptr;
  if (partial_header_size_ > 0) {
    // If we have a partially read header, continue where we left off.
    size_t header_remaining_size = kTarBlockSize - partial_header_size_;
    if (input_buffer_size >= header_remaining_size) {
      std::memcpy(partial_header_.get() + partial_header_size_, input_buffer,
                  header_remaining_size);
      header = reinterpret_cast<TarHeader*>(partial_header_.get());
      input_buffer += header_remaining_size;
      input_buffer_size -= header_remaining_size;
      partial_header_size_ = 0;
    } else {
      std::memcpy(partial_header_.get() + partial_header_size_, input_buffer,
                  input_buffer_size);
      partial_header_size_ += input_buffer_size;
      return false;
    }

  } else if (cur_file_content_size_ + cur_file_padding_size_ >
             cur_file_already_read_size_) {
    // If we have a partially read file, continue where we left off.
    ReadCurrentFile(&input_buffer, &input_buffer_size);
    if (input_buffer_size == 0) {
      return false;
    }
  }

  // After handling partially read headers and files, we now have a fresh start
  // and we should start reading the next file.
  // A tar archive is divided into blocks and each block is 512 bytes in size.
  // Each file in the tar archive starts with a header block containing some
  // metadata of the file, followed by a number of blocks containing the actual
  // contents of the file. If needed, trailing paddings are added immediately
  // after the actual contents of the file to make up a full block.
  for (;; header = nullptr) {
    // As the first step of reading a new file, we read the header block.
    if (header == nullptr) {
      if (input_buffer_size == 0) {
        return false;
      }
      if (input_buffer_size < kTarBlockSize) {
        std::memcpy(partial_header_.get(), input_buffer, input_buffer_size);
        partial_header_size_ = input_buffer_size;
        return false;
      } else {
        header = reinterpret_cast<TarHeader*>(input_buffer);
        input_buffer += kTarBlockSize;
        input_buffer_size -= kTarBlockSize;
      }
    }

    // Take care of all-zero blocks. If there are two consecutive all-zero
    // blocks, we have reached the end of the tar file.
    if (header->name[0] == '\0') {
      num_zero_block_++;
      if (num_zero_block_ >= 2) {
        return true;
      } else {
        continue;
      }
    } else {
      num_zero_block_ = 0;
    }

    GetFilePath(header);

    // If the current file is a directory, it will only have a header block in
    // the tar archive. We create the directory and all parent directories.
    if (!IsRegularFile(header->typeflag, cur_file_path_)) {
      std::filesystem::path path(cur_file_path_);
      // Create the directory and all parent directories with permission 0777.
      std::filesystem::create_directories(path);
      // The above call can leave the permission unchanged if the directory
      // already exists, so we manually set the permission to 0777.
      std::filesystem::permissions(path, std::filesystem::perms::all);
      continue;
    }

    // If the current file is a regular file, we read the contents of the file
    // from the tar archive and write it to the output file stream.
    cur_file_content_size_ = ReadOctalNumber(header->size, kTarSizeLength);
    uint64_t last_block_size = cur_file_content_size_ % kTarBlockSize;
    cur_file_padding_size_ =
        last_block_size == 0 ? 0 : kTarBlockSize - last_block_size;
    cur_file_already_read_size_ = 0;
    std::filesystem::path path(cur_file_path_);
    // Create the parent directories if necessary.
    std::filesystem::create_directories(path.parent_path());
    cur_file_ = std::ofstream(path, std::ios::trunc);
    if (cur_file_.fail()) {
      std::cerr << "Tar reader failed to create and open the file at "
                << cur_file_path_ << std::endl;
      exit(1);
    }
    // Set the file permission to std::filesystem::perms::all which is 0777.
    std::filesystem::permissions(path, std::filesystem::perms::all);
    ReadCurrentFile(&input_buffer, &input_buffer_size);
    if (input_buffer_size == 0) {
      return false;
    }
  }
}
