// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/download_file_info.h"

#include <utility>

namespace ui {

DownloadFileInfo::DownloadFileInfo(
    const base::FilePath& filename,
    std::unique_ptr<DownloadFileProvider> downloader)
    : filename(filename), downloader(std::move(downloader)) {}

DownloadFileInfo::~DownloadFileInfo() = default;

}  // namespace ui
