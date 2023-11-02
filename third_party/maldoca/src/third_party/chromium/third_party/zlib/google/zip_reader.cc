// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/zlib/google/zip_reader.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

#include "contrib/minizip/unzip.h"

// This file is a stripped down version of zip_reader.cc from Chromium: https://source.chromium.org/chromium/chromium/src/+/main:third_party/zlib/google/zip_reader.cc.
// It also contains code to wrap memory around the file interface from zip_internal.cc: https://source.chromium.org/chromium/chromium/src/+/main:third_party/zlib/google/zip_internal.cc
// It only contains code required to read a zip file using zlib and replaces the usage of other Chromium code not present in mini_chromium.

namespace zip {

ZipReader::EntryInfo::EntryInfo(const std::string& file_name_in_zip,
                                const unz_file_info& raw_file_info)
#if defined(_WIN32)
    : file_path_(base::UTF8ToWide(file_name_in_zip)),
#else
    : file_path_(file_name_in_zip),
#endif  // _WIN32
      is_directory_(false) {

  original_size_ = raw_file_info.uncompressed_size;

  // Directory entries in zip files end with "/".
  if (file_name_in_zip.size() > 0) {
    is_directory_ = file_name_in_zip.back() == '/';
  }
}

// START from internal.cc

// A struct that contains data required for zlib functions to extract files from
// a zip archive stored in memory directly. The following I/O API functions
// expect their opaque parameters refer to this struct.
struct ZipBuffer {
  const char* data;  // weak
  size_t length;
  size_t offset;
};

// Opens the specified file. When this function returns a non-NULL pointer, zlib
// uses this pointer as a stream parameter while compressing or uncompressing
// data. (Returning NULL represents an error.) This function initializes the
// given opaque parameter and returns it because this parameter stores all
// information needed for uncompressing data. (This function does not support
// writing compressed data and it returns NULL for this case.)
void* OpenZipBuffer(void* opaque, const char* /*filename*/, int mode) {
  if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER) != ZLIB_FILEFUNC_MODE_READ) {
    return NULL;
  }
  ZipBuffer* buffer = static_cast<ZipBuffer*>(opaque);
  if (!buffer || !buffer->data || !buffer->length)
    return NULL;
  buffer->offset = 0;
  return opaque;
}

// Reads compressed data from the specified stream. This function copies data
// refered by the opaque parameter and returns the size actually copied.
uLong ReadZipBuffer(void* opaque, void* /*stream*/, void* buf, uLong size) {
  ZipBuffer* buffer = static_cast<ZipBuffer*>(opaque);
  DCHECK_LE(buffer->offset, buffer->length);
  size_t remaining_bytes = buffer->length - buffer->offset;
  if (!buffer || !buffer->data || !remaining_bytes)
    return 0;
  size = std::min(size, static_cast<uLong>(remaining_bytes));
  memcpy(buf, &buffer->data[buffer->offset], size);
  buffer->offset += size;
  return size;
}

// Returns the offset from the beginning of the data.
long GetOffsetOfZipBuffer(void* opaque, void* /*stream*/) {
  ZipBuffer* buffer = static_cast<ZipBuffer*>(opaque);
  if (!buffer)
    return -1;
  return static_cast<long>(buffer->offset);
}

// Moves the current offset to the specified position.
long SeekZipBuffer(void* opaque, void* /*stream*/, uLong offset, int origin) {
  ZipBuffer* buffer = static_cast<ZipBuffer*>(opaque);
  if (!buffer)
    return -1;
  if (origin == ZLIB_FILEFUNC_SEEK_CUR) {
    buffer->offset = std::min(buffer->offset + static_cast<size_t>(offset),
                              buffer->length);
    return 0;
  }
  if (origin == ZLIB_FILEFUNC_SEEK_END) {
    buffer->offset = (buffer->length > offset) ? buffer->length - offset : 0;
    return 0;
  }
  if (origin == ZLIB_FILEFUNC_SEEK_SET) {
    buffer->offset = std::min(buffer->length, static_cast<size_t>(offset));
    return 0;
  }
  return -1;
}

// Closes the input offset and deletes all resources used for compressing or
// uncompressing data. This function deletes the ZipBuffer object referred by
// the opaque parameter since zlib deletes the unzFile object and it does not
// use this object any longer.
int CloseZipBuffer(void* opaque, void* /*stream*/) {
  if (opaque)
    free(opaque);
  return 0;
}

// Returns the last error happened when reading or writing data. This function
// always returns zero, which means there are not any errors.
int GetErrorOfZipBuffer(void* /*opaque*/, void* /*stream*/) {
  return 0;
}
// END from internal.cc

ZipReader::ZipReader() {
  Reset();
}

ZipReader::~ZipReader() {
  Close();
}

bool ZipReader::OpenFromString(const std::string& data) {
  if (data.empty()) {
    return false;
  }

  ZipBuffer* buffer = static_cast<ZipBuffer*>(malloc(sizeof(ZipBuffer)));
  if (!buffer) {
    return false;
  }
  buffer->data = data.data();
  buffer->length = data.length();
  buffer->offset = 0;

  zlib_filefunc_def zip_functions;
  zip_functions.zopen_file = OpenZipBuffer;
  zip_functions.zread_file = ReadZipBuffer;
  // Removed WriteZipBuffer as it is not required for reading zip files.
  zip_functions.ztell_file = GetOffsetOfZipBuffer;
  zip_functions.zseek_file = SeekZipBuffer;
  zip_functions.zclose_file = CloseZipBuffer;
  zip_functions.zerror_file = GetErrorOfZipBuffer;
  zip_functions.opaque = static_cast<void*>(buffer);
  zip_file_ = unzOpen2(NULL, &zip_functions);

  unz_global_info zip_info = {};  // Zero-clear.
  if (unzGetGlobalInfo(zip_file_, &zip_info) != UNZ_OK) {
    return false;
  }
  num_entries_ = zip_info.number_entry;

  if (num_entries_ < 0)
    return false;

  // We are already at the end if the zip file is empty.
  reached_end_ = (num_entries_ == 0);

  return true;
}

void ZipReader::Close() {
  if (zip_file_) {
    unzClose(zip_file_);
  }
  Reset();
}

bool ZipReader::HasMore() {
  return !reached_end_;
}

bool ZipReader::AdvanceToNextEntry() {
  DCHECK(zip_file_);

  // Should not go further if we already reached the end.
  if (reached_end_)
    return false;

  unz_file_pos position = {};
  if (unzGetFilePos(zip_file_, &position) != UNZ_OK)
    return false;
  const int current_entry_index = position.num_of_file;
  // If we are currently at the last entry, then the next position is the
  // end of the zip file, so mark that we reached the end.
  if (current_entry_index + 1 == num_entries_) {
    reached_end_ = true;
  } else {
    DCHECK_LT(current_entry_index + 1, num_entries_);
    if (unzGoToNextFile(zip_file_) != UNZ_OK) {
      return false;
    }
  }
  current_entry_info_.reset();
  return true;
}

bool ZipReader::OpenCurrentEntryInZip() {
  DCHECK(zip_file_);

  unz_file_info raw_file_info = {};
  constexpr int kZipMaxPath = 256;
  char raw_file_name_in_zip[kZipMaxPath] = {};
  const int result = unzGetCurrentFileInfo(zip_file_,
                                           &raw_file_info,
                                           raw_file_name_in_zip,
                                           sizeof(raw_file_name_in_zip) - 1,
                                           NULL,  // extraField.
                                           0,  // extraFieldBufferSize.
                                           NULL,  // szComment.
                                           0);  // commentBufferSize.

  if (result != UNZ_OK)
    return false;
  if (raw_file_name_in_zip[0] == '\0')
    return false;
  current_entry_info_.reset(
      new EntryInfo(raw_file_name_in_zip, raw_file_info));
  return true;
}

bool ZipReader::ExtractCurrentEntryToString(/* not used*/ uint64_t max_read_bytes,
                                            std::string* output) const {
  DCHECK(output);
  DCHECK(zip_file_);

  output->clear();

  if (max_read_bytes == 0) {
    return true;
  }

  if (current_entry_info_->is_directory()) {
    return true;
  }

  const int open_result = unzOpenCurrentFile(zip_file_);
  if (open_result != UNZ_OK) {
    return false;
  }

  constexpr int kZipBufSize = 8192;
  char buf[kZipBufSize];
  bool entire_file_extracted = false;

  while (true) {
    const int num_bytes_read =
        unzReadCurrentFile(zip_file_, buf, kZipBufSize);

    if (num_bytes_read == 0) {
      entire_file_extracted = true;
      break;
    } else if (num_bytes_read < 0) {
      // If num_bytes_read < 0, then it's a specific UNZ_* error code.
      break;
    } else if (num_bytes_read > 0) {
      output->append(buf, num_bytes_read);
    }
  }

  unzCloseCurrentFile(zip_file_);
  return entire_file_extracted;
}

void ZipReader::Reset() {
  zip_file_ = NULL;
  num_entries_ = 0;
  reached_end_ = false;
  current_entry_info_.reset();
}
}  // namespace zip
