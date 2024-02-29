// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/selected_file_info.h"

#include "base/containers/to_vector.h"
#include "base/ranges/algorithm.h"

namespace ui {

SelectedFileInfo::SelectedFileInfo() = default;

SelectedFileInfo::SelectedFileInfo(const base::FilePath& path)
    : SelectedFileInfo(path, path) {}

SelectedFileInfo::SelectedFileInfo(const base::FilePath& in_file_path,
                                   const base::FilePath& in_local_path)
    : file_path(in_file_path),
      local_path(in_local_path),
      display_name(in_file_path.BaseName().value()) {}

SelectedFileInfo::SelectedFileInfo(const SelectedFileInfo& other) = default;
SelectedFileInfo::SelectedFileInfo(SelectedFileInfo&& other) = default;

SelectedFileInfo::~SelectedFileInfo() = default;

SelectedFileInfo& SelectedFileInfo::operator=(const SelectedFileInfo& other) =
    default;
SelectedFileInfo& SelectedFileInfo::operator=(SelectedFileInfo&& other) =
    default;

bool SelectedFileInfo::operator==(const SelectedFileInfo& other) const =
    default;

base::FilePath SelectedFileInfo::path() const {
  return local_path.empty() ? file_path : local_path;
}

std::vector<SelectedFileInfo> FilePathListToSelectedFileInfoList(
    const std::vector<base::FilePath>& paths) {
  return base::ToVector(
      paths, [](const auto& path) { return SelectedFileInfo(path); });
}

std::vector<base::FilePath> SelectedFileInfoListToFilePathList(
    const std::vector<SelectedFileInfo>& files) {
  return base::ToVector(files, &SelectedFileInfo::path);
}

}  // namespace ui
