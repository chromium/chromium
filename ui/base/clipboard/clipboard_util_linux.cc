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
  std::vector<std::string_view> uris = base::SplitStringPiece(
      uri_list, "\r\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::vector<std::string> paths;
  paths.reserve(uris.size());

  for (std::string_view uri : uris) {
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

void RegisterFilesWithPortal(const std::vector<FileInfo>& filenames,
                             base::OnceCallback<void(std::string)> callback) {
  if (filenames.empty()) {
    std::move(callback).Run("");
    return;
  }
  std::vector<std::string> paths;
  paths.reserve(filenames.size());
  for (const auto& filename : filenames) {
    paths.push_back(filename.path.value());
  }
  RegisterPathsWithPortal(std::move(paths), std::move(callback));
}

void RegisterPathsWithPortal(const std::vector<std::string>& paths,
                             base::OnceCallback<void(std::string)> callback) {
  if (paths.empty()) {
    std::move(callback).Run("");
    return;
  }

  dbus_xdg::FileTransferPortal::IsAvailable(base::BindOnce(
      [](std::vector<std::string> paths,
         base::OnceCallback<void(std::string)> callback, bool available) {
        if (!available) {
          std::move(callback).Run("");
          return;
        }
        dbus_xdg::FileTransferPortal::RegisterFiles(std::move(paths),
                                                    std::move(callback));
      },
      std::move(paths), std::move(callback)));
}

void ExtractPathsFromPortalKey(
    base::span<const uint8_t> key_data,
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  if (key_data.empty()) {
    std::move(callback).Run({});
    return;
  }

  std::string key(key_data.begin(), key_data.end());
  dbus_xdg::FileTransferPortal::IsAvailable(base::BindOnce(
      [](std::string key,
         base::OnceCallback<void(std::vector<std::string>)> callback,
         bool available) {
        if (!available) {
          std::move(callback).Run({});
          return;
        }
        dbus_xdg::FileTransferPortal::RetrieveFiles(key, std::move(callback));
      },
      std::move(key), std::move(callback)));
}

}  // namespace ui::clipboard_util
