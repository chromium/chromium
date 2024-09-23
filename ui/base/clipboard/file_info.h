// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_FILE_INFO_H_
#define UI_BASE_CLIPBOARD_FILE_INFO_H_

#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/files/file_path.h"

namespace ui {

// struct that bundles a file's path with an optional display name.
struct COMPONENT_EXPORT(UI_BASE_FILE_INFO) FileInfo {
  FileInfo();
  FileInfo(const base::FilePath& path, const base::FilePath& display_name);
  ~FileInfo();
  bool operator==(const FileInfo& other) const;

  base::FilePath path;
  base::FilePath display_name;  // Optional.
};

// Returns UTF8 file:// URL. |file_path| is expected to be an absolute path, and
// will be encoded as one regardless. E.g. '/path' and 'path' both encode as
// 'file:///path'.
std::string COMPONENT_EXPORT(UI_BASE_FILE_INFO)
    FilePathToFileURL(const base::FilePath& file_path);

// Returns a list of ui::FileInfo from the text/uri-list CRLF-separated file://
// URLs in |uri_list| as per
// https://www.iana.org/assignments/media-types/text/uri-list
// URLs which cannot be parsed are ignored.
std::vector<FileInfo> COMPONENT_EXPORT(UI_BASE_FILE_INFO)
    URIListToFileInfos(std::string_view uri_list);

// Returns UTF8 text/uri-list CRLF-separated file:// URLs from filenames.
std::string COMPONENT_EXPORT(UI_BASE_FILE_INFO)
    FileInfosToURIList(const std::vector<FileInfo>& filenames);

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_FILE_INFO_H_
