// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_DOWNLOAD_FILE_INTERFACE_H_
#define UI_BASE_DRAGDROP_DOWNLOAD_FILE_INTERFACE_H_

#include "build/build_config.h"

#include "base/memory/ref_counted.h"

#include "ui/base/ui_base_export.h"

#if defined(OS_WIN)
#include <objidl.h>
#endif

namespace base {
class FilePath;
}

namespace ui {

// Defines the interface to observe the status of file download.
class UI_BASE_EXPORT DownloadFileObserver
    : public base::RefCountedThreadSafe<DownloadFileObserver> {
 public:
  virtual void OnDownloadCompleted(const base::FilePath& file_path) = 0;
  virtual void OnDownloadAborted() = 0;

 protected:
  friend class base::RefCountedThreadSafe<DownloadFileObserver>;
  virtual ~DownloadFileObserver() = default;
};

// Defines the interface to control how a file is downloaded.
class UI_BASE_EXPORT DownloadFileProvider {
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
