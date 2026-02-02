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
#include "ui/base/clipboard/file_info.h"

namespace ui::clipboard_util {

// Helper to convert a list of absolute file paths to a text/uri-list string.
std::string GetUriListFromPaths(const std::vector<std::string>& paths);

// Helper to convert a text/uri-list string to a list of absolute file paths.
// Ignores comments and non-file URIs.
std::vector<std::string> GetPathsFromUriList(std::string_view uri_list);

// Registers files with the XDG File Transfer Portal and returns a transfer key.
// Blocks the calling thread to communicate with the DBus portal.
// Returns an empty string on failure.
std::string RegisterFilesWithPortal(const std::vector<FileInfo>& filenames);
std::string RegisterPathsWithPortal(const std::vector<std::string>& paths);

// Extracts a list of absolute file paths from an XDG file transfer portal key.
// Blocks the calling thread to communicate with the DBus portal.
std::vector<std::string> ExtractPathsFromPortalKey(
    base::span<const uint8_t> key_data);

}  // namespace ui::clipboard_util

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_UTIL_LINUX_H_
