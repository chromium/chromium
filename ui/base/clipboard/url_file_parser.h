// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_URL_FILE_PARSER_H_
#define UI_BASE_CLIPBOARD_URL_FILE_PARSER_H_

#include <cstddef>
#include <string>
#include <string_view>

namespace ui::clipboard_util::internal {

// A completely arbitrary cut-off size (16kB), above which
// `ExtractURLFromURLFileContents` will refuse to parse. Because parsing
// is done in-memory by string splitting, dealing with large files invites
// hangs. See an example at https://crbug.com/1401639.
constexpr size_t kMaximumParsableFileSize = 16'384;

// Given the string contents of a .url file, returns a string version of the URL
// found. Returns an empty string if no URL can be found.
//
// Implementation note: This function does not do full validation of the file
// contents. If a malformed file is passed in, this function may or may not
// manage to find any URLs within.
std::string ExtractURLFromURLFileContents(std::string_view file_contents);

}  // namespace ui::clipboard_util::internal

#endif  // UI_BASE_CLIPBOARD_URL_FILE_PARSER_H_
