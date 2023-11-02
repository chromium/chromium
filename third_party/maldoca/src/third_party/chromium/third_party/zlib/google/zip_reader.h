// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_ZLIB_GOOGLE_ZIP_READER_H_
#define THIRD_PARTY_ZLIB_GOOGLE_ZIP_READER_H_

#include <memory>

#include "absl/strings/string_view.h"
#include "base/files/file_path.h"
#include "contrib/minizip/unzip.h"

// This file is a stripped down version of zip_reader.h from Chromium:
// https://source.chromium.org/chromium/chromium/src/+/main:third_party/zlib/google/zip_reader.h
// It only contains code required to read a zip file using zlib and replaces the
// usage of other Chromium code not present in mini_chromium.

namespace zip {

// This class is used for reading zip files. A typical use case of this
// class is to scan entries in a zip file and extract them. The code will
// look like:
//
//   ZipReader reader;
//   reader.Open(zip_file_path);
//   while (reader.HasMore()) {
//     reader.OpenCurrentEntryInZip();
//     const base::FilePath& entry_path =
//        reader.current_entry_info()->file_path();
//     auto writer = CreateFilePathWriterDelegate(extract_dir, entry_path);
//     reader.ExtractCurrentEntry(writer, std::numeric_limits<uint64_t>::max());
//     reader.AdvanceToNextEntry();
//   }
//
// For simplicity, error checking is omitted in the example code above. The
// production code should check return values from all of these functions.
//
class ZipReader {
 public:

  // This class represents information of an entry (file or directory) in
  // a zip file.
class EntryInfo {
   public:
    EntryInfo(const std::string& filename_in_zip,
              const unz_file_info& raw_file_info);
    EntryInfo(const EntryInfo&) = delete;
    EntryInfo& operator=(const EntryInfo&) = delete;

    // Returns the file path. The path is usually relative like
    // "foo/bar.txt", but if it's absolute, is_unsafe() returns true.
    inline const base::FilePath& file_path() const { return file_path_; }

    // Returns the size of the original file (i.e. after uncompressed).
    // Returns 0 if the entry is a directory.
    // Note: this value should not be trusted, because it is stored as metadata
    // in the zip archive and can be different from the real uncompressed size.
    inline int64_t original_size() const { return original_size_; }

    // Returns true if the entry is a directory.
    inline bool is_directory() const { return is_directory_; }

   private:
    const base::FilePath file_path_;
    int64_t original_size_;
    bool is_directory_;
  };

  ZipReader();
  ZipReader(const ZipReader&) = delete;
  ZipReader& operator=(const ZipReader&) = delete;

  ~ZipReader();

  // Opens the zip data stored in |data|. This class uses a weak reference to
  // the given sring while extracting files, i.e. the caller should keep the
  // string until it finishes extracting files.
  bool OpenFromString(const std::string& data);

  // Closes the currently opened zip file. This function is called in the
  // destructor of the class, so you usually don't need to call this.
  void Close();

  // Returns true if there is at least one entry to read. This function is
  // used to scan entries with AdvanceToNextEntry(), like:
  //
  // while (reader.HasMore()) {
  //   // Do something with the current file here.
  //   reader.AdvanceToNextEntry();
  // }
  bool HasMore();

  // Advances the next entry. Returns true on success.
  bool AdvanceToNextEntry();

  // Opens the current entry in the zip file. On success, returns true and
  // updates the the current entry state (i.e. current_entry_info() is
  // updated). This function should be called before operations over the
  // current entry like ExtractCurrentEntryToFile().
  //
  // Note that there is no CloseCurrentEntryInZip(). The the current entry
  // state is reset automatically as needed.
  bool OpenCurrentEntryInZip();

  // Extracts the current entry into memory. If the current entry is a
  // directory, the |output| parameter is set to the empty string. If the
  // current entry is a file, the |output| parameter is filled with its
  // contents. OpenCurrentEntryInZip() must be called beforehand. Note: the
  // |output| parameter can be filled with a big amount of data, avoid passing
  // it around by value, but by reference or pointer. Note: the value returned
  // by EntryInfo::original_size() cannot be trusted, so the real size of the
  // uncompressed contents can be different. |max_read_bytes| limits the ammount
  // of memory used to carry the entry. Returns true if the entire content is
  // read. If the entry is bigger than |max_read_bytes|, returns false and
  // |output| is filled with |max_read_bytes| of data. If an error occurs,
  // returns false, and |output| is set to the empty string.
  bool ExtractCurrentEntryToString(uint64_t max_read_bytes,
                                   std::string* output) const;

  // Returns the current entry info. Returns NULL if the current entry is
  // not yet opened. OpenCurrentEntryInZip() must be called beforehand.
  inline EntryInfo* current_entry_info() const {
    return current_entry_info_.get();
  }

  // Returns the number of entries in the zip file.
  // Open() must be called beforehand.
  int num_entries() const { return num_entries_; }

 private:
  // Resets the internal state.
  void Reset();

  unzFile zip_file_;
  int num_entries_;
  bool reached_end_;
  std::unique_ptr<EntryInfo> current_entry_info_;
};

}  // namespace zip

#endif  // THIRD_PARTY_ZLIB_GOOGLE_ZIP_READER_H_
