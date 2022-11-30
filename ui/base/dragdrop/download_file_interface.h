// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_DOWNLOAD_FILE_INTERFACE_H_
#define UI_BASE_DRAGDROP_DOWNLOAD_FILE_INTERFACE_H_

#include "build/build_config.h"

#include "base/component_export.h"
#include "base/memory/ref_counted.h"

namespace base {
class FilePath;
}

namespace ui {

// Defines the interface to observe the status of file download.
class COMPONENT_EXPORT(UI_BASE_DATA_EXCHANGE) DownloadFileObserver
    : public base::RefCountedThreadSafe<DownloadFileObserver> {
 public:
  virtual void OnDownloadCompleted(const base::FilePath& file_path) = 0;
  virtual void OnDownloadAborted() = 0;

 protected:
  friend class base::RefCountedThreadSafe<DownloadFileObserver>;
  virtual ~DownloadFileObserver() = default;
};

// Defines the interface to control how a file is downloaded.
class COMPONENT_EXPORT(UI_BASE_DATA_EXCHANGE) DownloadFileProvider {
 public:
  virtual ~DownloadFileProvider() = default;

  // Starts the download asynchronously and returns immediately.
  virtual void Start(DownloadFileObserver* observer) = 0;

  // Returns true if the download succeeded and false otherwise. Waits until the
  // download is completed/cancelled/interrupted before returning.
  virtual bool Wait() = 0;

  // Cancels the download.
  virtual void Stop() = 0;
};

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_DOWNLOAD_FILE_INTERFACE_H_
