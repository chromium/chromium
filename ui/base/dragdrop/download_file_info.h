// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_DOWNLOAD_FILE_INFO_H_
#define UI_BASE_DRAGDROP_DOWNLOAD_FILE_INFO_H_

#include <memory>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "ui/base/dragdrop/download_file_interface.h"

namespace ui {

// Encapsulates the info about a file to be downloaded.
struct COMPONENT_EXPORT(UI_BASE_DATA_EXCHANGE) DownloadFileInfo {
  DownloadFileInfo(const base::FilePath& filename,
                   std::unique_ptr<DownloadFileProvider> downloader);
  ~DownloadFileInfo();

  base::FilePath filename;
  std::unique_ptr<DownloadFileProvider> downloader;
};

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_DOWNLOAD_FILE_INFO_H_
