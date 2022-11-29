// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/selected_file_info.h"

namespace ui {

SelectedFileInfo::SelectedFileInfo() {}

SelectedFileInfo::SelectedFileInfo(const base::FilePath& in_file_path,
                                   const base::FilePath& in_local_path)
    : file_path(in_file_path),
      local_path(in_local_path) {
  display_name = in_file_path.BaseName().value();
}

SelectedFileInfo::SelectedFileInfo(const SelectedFileInfo& other) = default;
SelectedFileInfo::SelectedFileInfo(SelectedFileInfo&& other) = default;

SelectedFileInfo::~SelectedFileInfo() {}

SelectedFileInfo& SelectedFileInfo::operator=(const SelectedFileInfo& other) =
    default;
SelectedFileInfo& SelectedFileInfo::operator=(SelectedFileInfo&& other) =
    default;

bool SelectedFileInfo::operator==(const SelectedFileInfo& other) const {
  return file_path == other.file_path && local_path == other.local_path &&
         display_name == other.display_name && url == other.url &&
         virtual_path == other.virtual_path;
}

// Converts a list of FilePaths to a list of ui::SelectedFileInfo.
std::vector<SelectedFileInfo> FilePathListToSelectedFileInfoList(
    const std::vector<base::FilePath>& paths) {
  std::vector<SelectedFileInfo> selected_files;
  for (const auto& path : paths)
    selected_files.push_back(SelectedFileInfo(path, path));
  return selected_files;
}

}  // namespace ui
