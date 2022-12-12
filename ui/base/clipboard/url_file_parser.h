// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_URL_FILE_PARSER_H_
#define UI_BASE_CLIPBOARD_URL_FILE_PARSER_H_

#include <string>

#include "base/strings/string_piece_forward.h"

namespace ui::ClipboardUtil::internal {

// Given the string contents of a .url file, returns a string version of the URL
// found. Returns an empty string if no URL can be found.
//
// Implementation note: This function does not do full validation of the file
// contents. If a malformed file is passed in, this function may or may not
// manage to find any URLs within.
std::string ExtractURLFromURLFileContents(
    const base::StringPiece& file_contents);

}  // namespace ui::ClipboardUtil::internal

#endif  // UI_BASE_CLIPBOARD_URL_FILE_PARSER_H_
