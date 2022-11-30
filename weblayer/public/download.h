// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_DOWNLOAD_H_
#define WEBLAYER_PUBLIC_DOWNLOAD_H_

#include <string>

namespace base {
class FilePath;
}

namespace weblayer {

// These types are sent over IPC and across different versions. Never remove
// or change the order.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.weblayer_private
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: ImplDownloadState
enum class DownloadState {
  // Download is actively progressing.
  kInProgress = 0,
  // Download is completely finished.
  kComplete = 1,
  // Download is paused by the user.
  kPaused = 2,
  // Download has been cancelled by the user.
  kCancelled = 3,
  // Download has failed (e.g. server or connection problem).
  kFailed = 4,
};

// These types are sent over IPC and across different versions. Never remove
// or change the order.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.weblayer_private
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: ImplDownloadError
enum class DownloadError {
  kNoError = 0,            // Download completed successfully.
  kServerError = 1,        // Server failed, e.g. unauthorized or forbidden,
                           // server unreachable,
  kSSLError = 2,           // Certificate error.
  kConnectivityError = 3,  // A network error occur. e.g. disconnected, timed
                           // out, invalid request.
  kNoSpace = 4,            // There isn't enough room in the storage location.
  kFileError = 5,          // Various errors related to file access. e.g.
                           // access denied, directory or filename too long,
                           // file is too large for file system, file in use,
                           // too many files open at once etc...
  kCancelled = 6,          // The user cancelled the download.
  kOtherError = 7,         // An error not listed above occurred.
};

// Contains information about a single download that's in progress.
class Download {
 public:
  virtual ~Download() {}

  virtual DownloadState GetState() = 0;

  // Returns the total number of expected bytes. Returns -1 if the total size is
  // not known.
  virtual int64_t GetTotalBytes() = 0;

  // Total number of bytes that have been received and written to the download
  // file.
  virtual int64_t GetReceivedBytes() = 0;

  // Pauses the download.
  virtual void Pause() = 0;

  // Resumes the download.
  virtual void Resume() = 0;

  // Cancels the download.
  virtual void Cancel() = 0;

  // Returns the location of the downloaded file. This may be empty if the
  // target path hasn't been determined yet. The file it points to won't be
  // available until the download completes successfully.
  virtual base::FilePath GetLocation() = 0;

  // Returns the display name for the download.
  virtual std::u16string GetFileNameToReportToUser() = 0;

  // Returns the effective MIME type of downloaded content.
  virtual std::string GetMimeType() = 0;

  // Return information about the error, if any, that was encountered during the
  // download.
  virtual DownloadError GetError() = 0;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_DOWNLOAD_H_
