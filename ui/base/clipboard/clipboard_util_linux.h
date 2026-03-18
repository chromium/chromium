// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_UTIL_LINUX_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_UTIL_LINUX_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "ui/base/clipboard/file_info.h"

namespace ui::clipboard_util {

// Helper to convert a list of absolute file paths to a text/uri-list string.
std::string GetUriListFromPaths(const std::vector<std::string>& paths);

// Helper to convert a text/uri-list string to a list of absolute file paths.
// Ignores comments and non-file URIs.
std::vector<std::string> GetPathsFromUriList(std::string_view uri_list);

// Registers files with the XDG File Transfer Portal and returns a transfer key
// via `callback`.
// Returns an empty string on failure.
void RegisterFilesWithPortal(const std::vector<FileInfo>& filenames,
                             base::OnceCallback<void(std::string)> callback);
void RegisterPathsWithPortal(const std::vector<std::string>& paths,
                             base::OnceCallback<void(std::string)> callback);

// Extracts a list of absolute file paths from an XDG file transfer portal key
// via `callback`.
void ExtractPathsFromPortalKey(
    base::span<const uint8_t> key_data,
    base::OnceCallback<void(std::vector<std::string>)> callback);

}  // namespace ui::clipboard_util

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_UTIL_LINUX_H_
