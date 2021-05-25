// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/zlib/google/zip.h"

#include <queue>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "third_party/zlib/google/zip_internal.h"
#include "third_party/zlib/google/zip_reader.h"
#include "third_party/zlib/google/zip_writer.h"

namespace zip {
namespace {

bool IsHiddenFile(const base::FilePath& file_path) {
  return file_path.BaseName().value()[0] == '.';
}

bool ExcludeNoFilesFilter(const base::FilePath& file_path) {
  return true;
}

bool ExcludeHiddenFilesFilter(const base::FilePath& file_path) {
  return !IsHiddenFile(file_path);
}

// Creates a directory at |extract_dir|/|entry_path|, including any parents.
bool CreateDirectory(const base::FilePath& extract_dir,
                     const base::FilePath& entry_path) {
  return base::CreateDirectory(extract_dir.Append(entry_path));
}

// Creates a WriterDelegate that can write a file at |extract_dir|/|entry_path|.
std::unique_ptr<WriterDelegate> CreateFilePathWriterDelegate(
    const base::FilePath& extract_dir,
    const base::FilePath& entry_path) {
  return std::make_unique<FilePathWriterDelegate>(
      extract_dir.Append(entry_path));
}

class DirectFileAccessor : public FileAccessor {
 public:
  explicit DirectFileAccessor(base::FilePath src_dir)
      : src_dir_(std::move(src_dir)) {}

  ~DirectFileAccessor() override = default;

  bool Open(const Paths paths, std::vector<base::File>* const files) override {
    DCHECK(files);
    files->reserve(files->size() + paths.size());

    for (const base::FilePath& path : paths) {
      DCHECK(!path.IsAbsolute());
      const base::FilePath absolute_path = src_dir_.Append(path);
      if (base::DirectoryExists(absolute_path)) {
        files->emplace_back();
        LOG(ERROR) << "Cannot open '" << path << "': It is a directory";
      } else {
        files->emplace_back(absolute_path,
                            base::File::FLAG_OPEN | base::File::FLAG_READ);
        LOG_IF(ERROR, !files->back().IsValid())
            << "Cannot open '" << path << "'";
      }
    }

    return true;
  }

  bool List(const base::FilePath& path,
            std::vector<base::FilePath>* const files,
            std::vector<base::FilePath>* const subdirs) override {
    DCHECK(!path.IsAbsolute());
    DCHECK(files);
    DCHECK(subdirs);

    base::FileEnumerator file_enumerator(
        src_dir_.Append(path), false /* recursive */,
        base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);

    while (!file_enumerator.Next().empty()) {
      const base::FileEnumerator::FileInfo info = file_enumerator.GetInfo();
      (info.IsDirectory() ? subdirs : files)
          ->push_back(path.Append(info.GetName()));
    }

    return true;
  }

  bool GetInfo(const base::FilePath& path, Info* const info) override {
    DCHECK(!path.IsAbsolute());
    DCHECK(info);

    base::File::Info file_info;
    if (!base::GetFileInfo(src_dir_.Append(path), &file_info)) {
      LOG(ERROR) << "Cannot get info of '" << path << "'";
      return false;
    }

    info->is_directory = file_info.is_directory;
    info->last_modified = file_info.last_modified;

    return true;
  }

 private:
  const base::FilePath src_dir_;
};

}  // namespace

std::ostream& operator<<(std::ostream& out, const Progress& progress) {
  return out << progress.bytes << " bytes, " << progress.files << " files, "
             << progress.directories << " dirs";
}

bool Zip(const ZipParams& params) {
  DirectFileAccessor default_accessor(params.src_dir);
  FileAccessor* const file_accessor = params.file_accessor ?: &default_accessor;

  Paths files_to_add = params.src_files;

  std::vector<base::FilePath> all_files;
  if (files_to_add.empty()) {
    const auto exclude = [&params](const base::FilePath& path) {
      return (!params.include_hidden_files && IsHiddenFile(path)) ||
             (params.filter_callback &&
              !params.filter_callback.Run(params.src_dir.Append(path)));
    };

    // Perform a Breadth First Search (BFS) of the source tree. Note that the
    // BFS order might not be optimal when storing files in a ZIP (either for
    // the storing side, or for the program that will extract this ZIP).
    for (std::queue<base::FilePath> q({{}}); !q.empty(); q.pop()) {
      std::vector<base::FilePath> files, dirs;
      file_accessor->List(q.front(), &files, &dirs);

      for (base::FilePath& path : files) {
        // Skip hidden and filtered files.
        if (!exclude(path))
          all_files.push_back(std::move(path));
      }

      for (base::FilePath& path : dirs) {
        // Skip hidden and filtered subdirs.
        if (!exclude(path)) {
          q.push(path);
          all_files.push_back(std::move(path));
        }
      }
    }

    files_to_add = all_files;
  }

  std::unique_ptr<internal::ZipWriter> zip_writer;

#if defined(OS_POSIX)
  if (params.dest_fd != base::kInvalidPlatformFile) {
    DCHECK(params.dest_file.empty());
    zip_writer =
        internal::ZipWriter::CreateWithFd(params.dest_fd, file_accessor);
    if (!zip_writer)
      return false;
  }
#endif

  if (!zip_writer) {
    zip_writer = internal::ZipWriter::Create(params.dest_file, file_accessor);
    if (!zip_writer)
      return false;
  }

  zip_writer->SetProgressCallback(params.progress_callback,
                                  params.progress_period);

  return zip_writer->WriteEntries(files_to_add);
}

bool Unzip(const base::FilePath& src_file, const base::FilePath& dest_dir) {
  return UnzipWithFilterCallback(
      src_file, dest_dir, base::BindRepeating(&ExcludeNoFilesFilter), true);
}

bool UnzipWithFilterCallback(const base::FilePath& src_file,
                             const base::FilePath& dest_dir,
                             FilterCallback filter_cb,
                             bool log_skipped_files) {
  base::File file(src_file, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    DLOG(WARNING) << "Failed to open " << src_file.value();
    return false;
  }
  return UnzipWithFilterAndWriters(
      file.GetPlatformFile(),
      base::BindRepeating(&CreateFilePathWriterDelegate, dest_dir),
      base::BindRepeating(&CreateDirectory, dest_dir), std::move(filter_cb),
      log_skipped_files);
}

bool UnzipWithFilterAndWriters(const base::PlatformFile& src_file,
                               WriterFactory writer_factory,
                               DirectoryCreator directory_creator,
                               FilterCallback filter_cb,
                               bool log_skipped_files) {
  ZipReader reader;
  if (!reader.OpenFromPlatformFile(src_file)) {
    DLOG(WARNING) << "Failed to open src_file " << src_file;
    return false;
  }
  while (reader.HasMore()) {
    if (!reader.OpenCurrentEntryInZip()) {
      DLOG(WARNING) << "Failed to open the current file in zip";
      return false;
    }
    const base::FilePath& entry_path = reader.current_entry_info()->file_path();
    if (reader.current_entry_info()->is_unsafe()) {
      DLOG(WARNING) << "Found an unsafe file in zip " << entry_path;
      return false;
    }
    if (filter_cb.Run(entry_path)) {
      if (reader.current_entry_info()->is_directory()) {
        if (!directory_creator.Run(entry_path))
          return false;
      } else {
        std::unique_ptr<WriterDelegate> writer = writer_factory.Run(entry_path);
        if (!reader.ExtractCurrentEntry(writer.get(),
                                        std::numeric_limits<uint64_t>::max())) {
          DLOG(WARNING) << "Failed to extract " << entry_path;
          return false;
        }
      }
    } else if (log_skipped_files) {
      DLOG(WARNING) << "Skipped file " << entry_path;
    }

    if (!reader.AdvanceToNextEntry()) {
      DLOG(WARNING) << "Failed to advance to the next file";
      return false;
    }
  }
  return true;
}

bool ZipWithFilterCallback(const base::FilePath& src_dir,
                           const base::FilePath& dest_file,
                           FilterCallback filter_cb) {
  DCHECK(base::DirectoryExists(src_dir));
  return Zip({.src_dir = src_dir,
              .dest_file = dest_file,
              .filter_callback = std::move(filter_cb)});
}

bool Zip(const base::FilePath& src_dir,
         const base::FilePath& dest_file,
         bool include_hidden_files) {
  if (include_hidden_files) {
    return ZipWithFilterCallback(src_dir, dest_file,
                                 base::BindRepeating(&ExcludeNoFilesFilter));
  } else {
    return ZipWithFilterCallback(
        src_dir, dest_file, base::BindRepeating(&ExcludeHiddenFilesFilter));
  }
}

#if defined(OS_POSIX)
bool ZipFiles(const base::FilePath& src_dir,
              Paths src_relative_paths,
              int dest_fd) {
  DCHECK(base::DirectoryExists(src_dir));
  return Zip({.src_dir = src_dir,
              .dest_fd = dest_fd,
              .src_files = src_relative_paths});
}
#endif  // defined(OS_POSIX)

}  // namespace zip
