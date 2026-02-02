// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_util_linux.h"

#include "base/files/file_path.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/dbus/xdg/file_transfer_portal.h"
#include "net/base/filename_util.h"
#include "url/gurl.h"

namespace ui::clipboard_util {

std::string GetUriListFromPaths(const std::vector<std::string>& paths) {
  std::vector<std::string> uris;
  uris.reserve(paths.size());
  for (const auto& path : paths) {
    uris.push_back(net::FilePathToFileURL(base::FilePath(path)).spec());
  }
  return base::JoinString(uris, "\r\n");
}

std::vector<std::string> GetPathsFromUriList(std::string_view uri_list) {
  std::vector<std::string> uris = base::SplitString(
      uri_list, "\r\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::vector<std::string> paths;
  paths.reserve(uris.size());

  for (const auto& uri : uris) {
    if (uri.empty() || uri[0] == '#') {
      continue;
    }
    base::FilePath file_path;
    if (net::FileURLToFilePath(GURL(uri), &file_path)) {
      paths.push_back(file_path.value());
    }
  }

  return paths;
}

std::string RegisterFilesWithPortal(const std::vector<FileInfo>& filenames) {
  if (filenames.empty()) {
    return "";
  }
  std::vector<std::string> paths;
  paths.reserve(filenames.size());
  for (const auto& filename : filenames) {
    paths.push_back(filename.path.value());
  }
  return RegisterPathsWithPortal(paths);
}

std::string RegisterPathsWithPortal(const std::vector<std::string>& paths) {
  if (paths.empty() || !dbus_xdg::FileTransferPortal::IsAvailableSync()) {
    return "";
  }
  return dbus_xdg::FileTransferPortal::RegisterFilesSync(paths);
}

std::vector<std::string> ExtractPathsFromPortalKey(
    base::span<const uint8_t> key_data) {
  if (key_data.empty() || !dbus_xdg::FileTransferPortal::IsAvailableSync()) {
    return {};
  }
  std::string key(key_data.begin(), key_data.end());
  return dbus_xdg::FileTransferPortal::RetrieveFilesSync(key);
}

}  // namespace ui::clipboard_util
