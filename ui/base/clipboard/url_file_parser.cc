// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/url_file_parser.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace ui::clipboard_util::internal {

std::string ExtractURLFromURLFileContents(std::string_view file_contents) {
  // NB: This code is written with the single goal of obvious correctness. It is
  // deliberately not optimized by any other measure.

  if (file_contents.size() > kMaximumParsableFileSize) {
    return {};
  }

  // Re the file format: The file is in .ini file format, with sections headed
  // by bracketed names, each containing key-value pairs separated by an equal
  // sign. In a .url file, the URL can be found in the [InternetShortcut]
  // section, as the value for the "URL" key.

  const std::string kInternetShortcut("[InternetShortcut]");
  const std::string kURL("URL=");

  // Start by splitting the file content into lines.
  std::vector<std::string_view> lines_vector = base::SplitStringPiece(
      file_contents, "\r\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  base::span<std::string_view> lines(lines_vector);

  // Search for the [InternetShortcut] section by discarding lines one by one
  // until either the section is found or there are no lines left.
  while (!lines.empty() && lines.front() != kInternetShortcut) {
    lines = lines.subspan(1);
  }

  // At this point, either the section was found or there are no lines left. If
  // there are no lines left, there is no URL to find in this file. Return.
  if (lines.empty()) {
    return {};
  }

  // This is now the [InternetShortcut] section. Discard that section header.
  lines = lines.subspan(1);

  // At this point, examine the lines.
  while (!lines.empty()) {
    const auto& line = lines.front();

    // If the line begins with a [ then a new section has begun, and there is no
    // URL to find in this file. Return.
    if (line.length() && line[0] == '[') {
      return {};
    }

    // Otherwise, it should be a key-value pair delimited by "=". However,
    // splitting on "=" doesn't work because URLs can contain that character, so
    // just look for the prefix.
    if (base::StartsWith(line, kURL)) {
      // Success! Strip off the prefix and return what was found.
      return std::string(line.substr(kURL.length()));
    }

    // Otherwise, this isn't a useful line; discard it and move on.
    lines = lines.subspan(1);
  }

  // If control has reached here, the file was searched and it contains no URL.
  // Return.
  return {};
}

}  // namespace ui::clipboard_util::internal
