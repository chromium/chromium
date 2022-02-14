// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_ZLIB_GOOGLE_ZIP_READER_H_
#define THIRD_PARTY_ZLIB_GOOGLE_ZIP_READER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

#if defined(USE_SYSTEM_MINIZIP)
#include <minizip/unzip.h>
#else
#include "third_party/zlib/contrib/minizip/unzip.h"
#endif

namespace zip {

// A delegate interface used to stream out an entry; see
// ZipReader::ExtractCurrentEntry.
class WriterDelegate {
 public:
  virtual ~WriterDelegate() {}

  // Invoked once before any data is streamed out to pave the way (e.g., to open
  // the output file). Return false on failure to cancel extraction.
  virtual bool PrepareOutput() = 0;

  // Invoked to write the next chunk of data. Return false on failure to cancel
  // extraction.
  virtual bool WriteBytes(const char* data, int num_bytes) = 0;

  // Sets the last-modified time of the data.
  virtual void SetTimeModified(const base::Time& time) = 0;

  // Called with the POSIX file permissions of the data; POSIX implementations
  // may apply some of the permissions (for example, the executable bit) to the
  // output file.
  virtual void SetPosixFilePermissions(int mode) = 0;
};

// This class is used for reading ZIP archives. A typical use case of this class
// is to scan entries in a ZIP archive and extract them. The code will look
// like:
//
//   ZipReader reader;
//   if (!reader.Open(zip_path)) {
//     // Cannot open
//     return;
//   }
//
//   while (const ZipReader::entry* entry = reader.Next()) {
//     auto writer = CreateFilePathWriterDelegate(extract_dir, entry->path);
//     if (!reader.ExtractCurrentEntry(
//         writer, std::numeric_limits<uint64_t>::max())) {
//           // Cannot extract
//           return;
//     }
//   }
//
//   if (!reader.ok()) {
//     // Error while enumerating entries
//     return;
//   }
//
class ZipReader {
 public:
  // A callback that is called when the operation is successful.
  using SuccessCallback = base::OnceClosure;
  // A callback that is called when the operation fails.
  using FailureCallback = base::OnceClosure;
  // A callback that is called periodically during the operation with the number
  // of bytes that have been processed so far.
  using ProgressCallback = base::RepeatingCallback<void(int64_t)>;

  // Information of an entry (file or directory) in a ZIP archive.
  struct Entry {
    // Path of this entry, in its original encoding as it is stored in the ZIP
    // archive. The encoding is not specified here. It might or might not be
    // UTF-8, and the caller needs to use other means to determine the encoding
    // if it wants to interpret this path correctly.
    std::string path_in_original_encoding;

    // Path of the entry, converted to Unicode. This path is usually relative
    // (eg "foo/bar.txt"), but it can also be absolute (eg "/foo/bar.txt") or
    // parent-relative (eg "../foo/bar.txt"). See also |is_unsafe|.
    base::FilePath path;

    // Size of the original uncompressed file, or 0 if the entry is a directory.
    // This value should not be trusted, because it is stored as metadata in the
    // ZIP archive and can be different from the real uncompressed size.
    int64_t original_size;

    // Last modified time. If the timestamp stored in the ZIP archive is not
    // valid, the Unix epoch will be returned.
    //
    // The timestamp stored in the ZIP archive uses the MS-DOS date and time
    // format.
    //
    // http://msdn.microsoft.com/en-us/library/ms724247(v=vs.85).aspx
    //
    // As such the following limitations apply:
    // * Only years from 1980 to 2107 can be represented.
    // * The timestamp has a 2-second resolution.
    // * There is no timezone information, so the time is interpreted as UTC.
    base::Time last_modified;

    // True if the entry is a directory.
    // False if the entry is a file.
    bool is_directory;

    // True if the entry path is considered unsafe, ie if it is absolute or if
    // it contains "..".
    bool is_unsafe;

    // True if the file content is encrypted.
    bool is_encrypted;

    // Entry POSIX permissions (POSIX systems only).
    int posix_mode;
  };

  // TODO(crbug.com/1295127) Remove this struct once transition to Entry is
  // finished.
  struct EntryInfo : Entry {
    const Entry& entry() const { return *this; }
    const std::string& file_path_in_original_encoding() const {
      return entry().path_in_original_encoding;
    }
    const base::FilePath& file_path() const { return entry().path; }
    int64_t original_size() const { return entry().original_size; }
    base::Time last_modified() const { return entry().last_modified; }
    bool is_directory() const { return entry().is_directory; }
    bool is_unsafe() const { return entry().is_unsafe; }
    bool is_encrypted() const { return entry().is_encrypted; }
    int posix_mode() const { return entry().posix_mode; }
  };

  ZipReader();

  ZipReader(const ZipReader&) = delete;
  ZipReader& operator=(const ZipReader&) = delete;

  ~ZipReader();

  // Opens the ZIP archive specified by |zip_path|. Returns true on
  // success.
  bool Open(const base::FilePath& zip_path);

  // Opens the ZIP archive referred to by the platform file |zip_fd|, without
  // taking ownership of |zip_fd|. Returns true on success.
  bool OpenFromPlatformFile(base::PlatformFile zip_fd);

  // Opens the zip data stored in |data|. This class uses a weak reference to
  // the given sring while extracting files, i.e. the caller should keep the
  // string until it finishes extracting files.
  bool OpenFromString(const std::string& data);

  // Closes the currently opened ZIP archive. This function is called in the
  // destructor of the class, so you usually don't need to call this.
  void Close();

  // Sets the encoding of entry paths in the ZIP archive.
  // By default, paths are assumed to be in UTF-8.
  void SetEncoding(std::string encoding) { encoding_ = std::move(encoding); }

  // Sets the decryption password that will be used to decrypt encrypted file in
  // the ZIP archive.
  void SetPassword(std::string password) { password_ = std::move(password); }

  // Gets the next entry. Returns null if there is no more entry. The returned
  // Entry is owned by this ZipReader, and is valid until Next() is called
  // again or until this ZipReader is closed.
  //
  // This function is used to scan entries:
  // while (const ZipReader::Entry* entry = reader.Next()) {
  //   // Do something with the current entry here.
  //   ...
  // }
  const Entry* Next();

  // Returns true if the enumeration of entries was successful.
  bool ok() const { return ok_; }

  // Returns true if there is at least one entry to read. This function is
  // used to scan entries with AdvanceToNextEntry(), like:
  //
  // while (reader.HasMore()) {
  //   // Do something with the current file here.
  //   reader.AdvanceToNextEntry();
  // }
  //
  // TODO(crbug.com/1295127) Remove this method.
  bool HasMore();

  // Advances the next entry. Returns true on success.
  //
  // TODO(crbug.com/1295127) Remove this method.
  bool AdvanceToNextEntry();

  // Opens the current entry in the ZIP archive. On success, returns true and
  // updates the current entry state (i.e. current_entry_info() is updated).
  // This function should be called before operations over the current entry
  // like ExtractCurrentEntryToFile().
  //
  // Note that there is no CloseCurrentEntryInZip(). The current entry state is
  // reset automatically as needed.
  //
  // TODO(crbug.com/1295127) Remove this method.
  bool OpenCurrentEntryInZip();

  // Extracts |num_bytes_to_extract| bytes of the current entry to |delegate|,
  // starting from the beginning of the entry. Return value specifies whether
  // the entire file was extracted.
  bool ExtractCurrentEntry(WriterDelegate* delegate,
                           uint64_t num_bytes_to_extract) const;

  // Asynchronously extracts the current entry to the given output file path.
  // If the current entry is a directory it just creates the directory
  // synchronously instead.  OpenCurrentEntryInZip() must be called beforehand.
  // success_callback will be called on success and failure_callback will be
  // called on failure.  progress_callback will be called at least once.
  // Callbacks will be posted to the current MessageLoop in-order.
  void ExtractCurrentEntryToFilePathAsync(
      const base::FilePath& output_file_path,
      SuccessCallback success_callback,
      FailureCallback failure_callback,
      const ProgressCallback& progress_callback);

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
  //
  // TODO(crbug.com/1295127) Remove this method.
  EntryInfo* current_entry_info() const { return current_entry_; }

  // Returns the number of entries in the ZIP archive.
  // Open() must be called beforehand.
  int num_entries() const { return num_entries_; }

 private:
  // Common code used both in Open and OpenFromFd.
  bool OpenInternal();

  // Resets the internal state.
  void Reset();

  // Extracts a chunk of the file to the target.  Will post a task for the next
  // chunk and success/failure/progress callbacks as necessary.
  void ExtractChunk(base::File target_file,
                    SuccessCallback success_callback,
                    FailureCallback failure_callback,
                    const ProgressCallback& progress_callback,
                    const int64_t offset);

  std::string encoding_;
  std::string password_;
  unzFile zip_file_;
  int num_entries_;
  int next_index_;
  bool reached_end_;
  bool ok_;
  EntryInfo entry_ = {};
  EntryInfo* current_entry_ = nullptr;

  base::WeakPtrFactory<ZipReader> weak_ptr_factory_{this};
};

// A writer delegate that writes to a given File.
class FileWriterDelegate : public WriterDelegate {
 public:
  // Constructs a FileWriterDelegate that manipulates |file|. The delegate will
  // not own |file|, therefore the caller must guarantee |file| will outlive the
  // delegate.
  explicit FileWriterDelegate(base::File* file);

  // Constructs a FileWriterDelegate that takes ownership of |file|.
  explicit FileWriterDelegate(std::unique_ptr<base::File> file);

  FileWriterDelegate(const FileWriterDelegate&) = delete;
  FileWriterDelegate& operator=(const FileWriterDelegate&) = delete;

  // Truncates the file to the number of bytes written.
  ~FileWriterDelegate() override;

  // WriterDelegate methods:

  // Seeks to the beginning of the file, returning false if the seek fails.
  bool PrepareOutput() override;

  // Writes |num_bytes| bytes of |data| to the file, returning false on error or
  // if not all bytes could be written.
  bool WriteBytes(const char* data, int num_bytes) override;

  // Sets the last-modified time of the data.
  void SetTimeModified(const base::Time& time) override;

  // On POSIX systems, sets the file to be executable if the source file was
  // executable.
  void SetPosixFilePermissions(int mode) override;

  // Return the actual size of the file.
  int64_t file_length() { return file_length_; }

 private:
  // The file the delegate modifies.
  base::File* file_;

  // The delegate can optionally own the file it modifies, in which case
  // owned_file_ is set and file_ is an alias for owned_file_.
  std::unique_ptr<base::File> owned_file_;

  int64_t file_length_ = 0;
};

// A writer delegate that writes a file at a given path.
class FilePathWriterDelegate : public WriterDelegate {
 public:
  explicit FilePathWriterDelegate(const base::FilePath& output_file_path);

  FilePathWriterDelegate(const FilePathWriterDelegate&) = delete;
  FilePathWriterDelegate& operator=(const FilePathWriterDelegate&) = delete;

  ~FilePathWriterDelegate() override;

  // WriterDelegate methods:

  // Creates the output file and any necessary intermediate directories.
  bool PrepareOutput() override;

  // Writes |num_bytes| bytes of |data| to the file, returning false if not all
  // bytes could be written.
  bool WriteBytes(const char* data, int num_bytes) override;

  // Sets the last-modified time of the data.
  void SetTimeModified(const base::Time& time) override;

  // On POSIX systems, sets the file to be executable if the source file was
  // executable.
  void SetPosixFilePermissions(int mode) override;

 private:
  base::FilePath output_file_path_;
  base::File file_;
};

}  // namespace zip

#endif  // THIRD_PARTY_ZLIB_GOOGLE_ZIP_READER_H_
