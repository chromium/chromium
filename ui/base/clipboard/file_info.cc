// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/file_info.h"

#include <string_view>

#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"

namespace ui {
namespace {

constexpr char kFileUrlPrefix[] = "file://";
constexpr char kFileSchemePrefix[] = "file:";
constexpr char kURIListSeparator[] = "\r\n";

// Returns true if path starts with a letter, followed by a colon, followed
// by a path separator.
bool StartsWithDriveLetter(std::string_view path) {
  return path.length() > 2 && base::IsAsciiAlpha(path[0]) && path[1] == ':' &&
         base::FilePath::IsSeparator(path[2]);
}

// We implement our own URLToPath() and PathToURL() rather than use
// net::FileUrlToFilePath() or net::FilePathToFileURL() since //net code works
// differently on each platform and is overly strict. In particular, it doesn't
// allow Windows network paths such as //ChromeOS/MyFiles on OS_CHROMEOS.
//
// This code is a little different in nature to most other path handling in that
// we expect this code to roundtrip both posix and windows paths (local or
// network) when running on either platform.
//
// Convert file:// |url| to a FilePath. Returns empty if |url| is invalid.
// This function expects an absolute path since it is not possible to encode
// a relative path as a file:// URL. The third slash in 'file:///' is not
// mandatory, but without it, the path is considered a network path
// (file://host/path). If a drive letter followed by colon and slash is detected
// (file:///C:/path), the path is assumed to be windows path 'C:/path', but
// without the slash (file:///C:path), the path is assumed to posix '/C:path'
// rather than windows relative path 'C:path'.
base::FilePath URLToPath(std::string_view url) {
  // Must start with 'file://' with at least 1 more char.
  std::string prefix(kFileUrlPrefix);
  if (url.size() <= prefix.size() ||
      !base::StartsWith(url, prefix, base::CompareCase::SENSITIVE)) {
    return base::FilePath();
  }

  // Skip slashes after 'file:' if needed:
  size_t path_start;
  if (url[prefix.size()] == '/') {
    // file:///path => /path
    path_start = prefix.size();
    if (StartsWithDriveLetter(url.substr(path_start + 1))) {
      // file:///C:/path => C:/path
      ++path_start;
    }
  } else {
    // file://host/path => //host/path
    DCHECK_EQ(prefix.size(), 7u);
    path_start = prefix.size() - 2;
  }

  std::string result = base::UnescapeBinaryURLComponent(url.substr(path_start));
#if BUILDFLAG(IS_WIN)
  return base::FilePath(base::UTF8ToWide(result)).NormalizePathSeparators();
#else
  return base::FilePath(result);
#endif
}

}  // namespace

FileInfo::FileInfo() = default;

FileInfo::FileInfo(const base::FilePath& path,
                   const base::FilePath& display_name)
    : path(path), display_name(display_name) {}

FileInfo::~FileInfo() = default;

bool FileInfo::operator==(const FileInfo& other) const {
  return path == other.path && display_name == other.display_name;
}

std::vector<FileInfo> URIListToFileInfos(std::string_view uri_list) {
  std::vector<FileInfo> result;
  std::vector<std::string_view> lines =
      base::SplitStringPiece(uri_list, kURIListSeparator, base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
  for (std::string_view line : lines) {
    base::FilePath path = URLToPath(line);
    if (!path.empty()) {
      result.push_back(FileInfo(path, base::FilePath()));
    }
  }
  return result;
}

std::string FilePathToFileURL(const base::FilePath& file_path) {
  std::string url;
#if BUILDFLAG(IS_WIN)
  std::string path = base::WideToUTF8(file_path.value());
#else
  std::string path = file_path.value();
#endif
  // Allocate maximum possible size upfront:
  // 'file:' + '///' + (3 x path.size() for percent encoding).
  url.reserve((sizeof(kFileSchemePrefix) - 1) + 3 + (3 * path.size()));
  url += kFileSchemePrefix;

  // Add slashes after 'file:' if needed:
  if (path.size() > 1 && base::FilePath::IsSeparator(path[0]) &&
      base::FilePath::IsSeparator(path[1])) {
    //  //host/path    => file://host/path
  } else if (path.size() > 0 && base::FilePath::IsSeparator(path[0])) {
    //  /absolute/path => file:///absolute/path
    url += "//";
  } else {
    //  relative/path  => file:///relative/path
    //  C:/path        => file:///C:/path
    // A relative path can't be encoded as a file:// URL, so we will produce an
    // absolute path like GURL() does. We do expect input to be absolute, and
    // ideally, we would DCHECK(file_path.IsAbsolute()), but it is possible that
    // we are interpreting a windows path while running on a posix platform.
    url += "///";
  }

  for (char c : path) {
    // Encode special characters `%;#?\`.
    if (c == '%' || c == ';' || c == '#' || c == '?' ||
#if !BUILDFLAG(IS_WIN)
        // Backslash is percent-encoded on posix platforms.
        c == '\\' ||
#endif
        // Encode space and all control chars.
        c <= ' ') {
      url += '%';
      base::AppendHexEncodedByte(static_cast<uint8_t>(c), url);
#if BUILDFLAG(IS_WIN)
    } else if (c == '\\') {
      // Backslash is converted to slash on windows.
      url += '/';
#endif
    } else {
      url += c;
    }
  }

  return url;
}

std::string FileInfosToURIList(const std::vector<FileInfo>& filenames) {
  std::vector<std::string> uri_list;
  for (const FileInfo& file : filenames) {
    uri_list.push_back(FilePathToFileURL(file.path));
  }
  return base::JoinString(uri_list, kURIListSeparator);
}

}  // namespace ui
