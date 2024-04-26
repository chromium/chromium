// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_SELECTED_FILE_INFO_H_
#define UI_SHELL_DIALOGS_SELECTED_FILE_INFO_H_

#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "ui/shell_dialogs/shell_dialogs_export.h"
#include "url/gurl.h"

namespace ui {

// Struct used for returning selected file info.
struct SHELL_DIALOGS_EXPORT SelectedFileInfo {
  // The selected file's user friendly path as seen in the UI.
  base::FilePath file_path;

  // The actual local path to the selected file. This can be a snapshot file
  // with an unreadable name like "/blah/.d41d8cd98f00b204e9800998ecf8427e".
  // `local_path` can differ from `file_path` for drive files (e.g.
  // "/drive_cache/temporary/d41d8cd98f00b204e9800998ecf8427e" vs.
  // "/special/drive/foo.txt").
  base::FilePath local_path;

  // TODO(crbug.com/41486940): Use a variant for the `local_path`, `url`,
  // and `virtual_path` fields, as those are logically mutually exclusive, and
  // code should be used to indicate which to use rather than relying on
  // comments.

  // This field is optional. The display name contains only the base name
  // portion of a file name (ex. no path separators), and used for displaying
  // selected file names. If this field is empty, the base name portion of
  // `path` is used for displaying.
  base::FilePath::StringType display_name;

  // If set, this URL may be used to access the file. If the user is capable of
  // using a URL to access the file, it should be used in preference to
  // `local_path`. For example, when opening a .gdoc file from Google Drive the
  // file is opened by navigating to a docs.google.com URL.
  std::optional<GURL> url;

  // If set, this virtual path may be used to access the file. If the user is
  // capable of using a virtual path to access the file (using the file system
  // abstraction in //storage/browser/file_system with a
  // storage::kFileSystemTypeExternal FileSystemURL), it should be used in
  // preference over `local_path` and `url`.
  std::optional<base::FilePath> virtual_path;

#if BUILDFLAG(IS_MAC)
  // A list of tags specified by the user to be set on the file upon the
  // completion of it being written to disk.
  std::vector<std::string> file_tags;
#endif

  // Constructs an empty object.
  SelectedFileInfo();

  // Constructs an object with both the `file_path` and `local_path` set to the
  // provided `path` value. This also sets a default `display_name` derived from
  // that value.
  explicit SelectedFileInfo(const base::FilePath& path);

  // Constructs an object with both the `file_path` and `local_path` set to the
  // provided values. This also sets a default `display_name` derived from the
  // `in_file_path` value.
  SelectedFileInfo(const base::FilePath& in_file_path,
                   const base::FilePath& in_local_path);
  SelectedFileInfo(const SelectedFileInfo& other);
  SelectedFileInfo(SelectedFileInfo&& other);
  ~SelectedFileInfo();

  SelectedFileInfo& operator=(const SelectedFileInfo& other);
  SelectedFileInfo& operator=(SelectedFileInfo&& other);

  bool operator==(const SelectedFileInfo& other) const;

  // A utility function to return the path to use in most cases; returns
  // `local_path` if not empty, else `file_path`.
  // TODO(crbug.com/41486940): Clean this up; the different path options
  // should be clearer.
  base::FilePath path() const;
};

// Converts a list of FilePaths to a list of ui::SelectedFileInfo.
SHELL_DIALOGS_EXPORT std::vector<SelectedFileInfo>
FilePathListToSelectedFileInfoList(const std::vector<base::FilePath>& paths);

// Converts a list of ui::SelectedFileInfos to a list of FilePaths. This uses
// the path() accessor to use the `local_path` if not empty, else the
// `file_path`.
SHELL_DIALOGS_EXPORT std::vector<base::FilePath>
SelectedFileInfoListToFilePathList(const std::vector<SelectedFileInfo>& files);

}  // namespace ui

#endif  // UI_SHELL_DIALOGS_SELECTED_FILE_INFO_H_
