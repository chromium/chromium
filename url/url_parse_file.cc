// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/check.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_file.h"
#include "url/url_parse_internal.h"

// Interesting IE file:isms...
//
//  INPUT                      OUTPUT
//  =========================  ==============================
//  file:/foo/bar              file:///foo/bar
//      The result here seems totally invalid!?!? This isn't UNC.
//
//  file:/
//  file:// or any other number of slashes
//      IE6 doesn't do anything at all if you click on this link. No error:
//      nothing. IE6's history system seems to always color this link, so I'm
//      guessing that it maps internally to the empty URL.
//
//  C:\                        file:///C:/
//      When on a file: URL source page, this link will work. When over HTTP,
//      the file: URL will appear in the status bar but the link will not work
//      (security restriction for all file URLs).
//
//  file:foo/                  file:foo/     (invalid?!?!?)
//  file:/foo/                 file:///foo/  (invalid?!?!?)
//  file://foo/                file://foo/   (UNC to server "foo")
//  file:///foo/               file:///foo/  (invalid, seems to be a file)
//  file:////foo/              file://foo/   (UNC to server "foo")
//      Any more than four slashes is also treated as UNC.
//
//  file:C:/                   file://C:/
//  file:/C:/                  file://C:/
//      The number of slashes after "file:" don't matter if the thing following
//      it looks like an absolute drive path. Also, slashes and backslashes are
//      equally valid here.

namespace url {

namespace {

// Returns the index of the next slash in the input after the given index, or
// `spec.size()` if the end of the input is reached.
template <typename CharT>
size_t FindNextSlash(std::basic_string_view<CharT> spec, size_t begin_index) {
  size_t idx = begin_index;
  while (idx < spec.size() && !IsSlashOrBackslash(spec[idx])) {
    idx++;
  }
  return idx;
}

// A subcomponent of DoParseFileURL, the input of this function should be a UNC
// path name, with the index of the first character after the slashes following
// the scheme given in `after_slashes`. This will initialize the host, path,
// query, and ref, and leave the other output components untouched
// (DoParseFileURL handles these for us).
template <typename CharT>
void DoParseUNC(std::basic_string_view<CharT> url,
                size_t after_slashes,
                Parsed* parsed) {
  int url_len = base::checked_cast<int>(url.size());
  // The cast is safe because `FindNextSlash` will never return anything longer
  // than `url_len`.
  int next_slash = static_cast<int>(FindNextSlash(url, after_slashes));

  // Everything up until that first slash we found (or end of string) is the
  // host name, which will end up being the UNC host. For example,
  // "file://foo/bar.txt" will get a server name of "foo" and a path of "/bar".
  // Later, on Windows, this should be treated as the filename "\\foo\bar.txt"
  // in proper UNC notation.
  if (after_slashes < static_cast<size_t>(next_slash)) {
    parsed->host = MakeRange(after_slashes, next_slash);
  } else {
    parsed->host.reset();
  }
  if (next_slash < url_len) {
    ParsePathInternal(url.data(), MakeRange(next_slash, url_len), &parsed->path,
                      &parsed->query, &parsed->ref);
  } else {
    parsed->path.reset();
  }
}

// A subcomponent of DoParseFileURL, the input should be a local file, with the
// beginning of the path indicated by the index in `path_begin`. This will
// initialize the host, path, query, and ref, and leave the other output
// components untouched (DoParseFileURL handles these for us).
template <typename CharT>
void DoParseLocalFile(std::basic_string_view<CharT> url,
                      int path_begin,
                      Parsed* parsed) {
  parsed->host.reset();
  ParsePathInternal(url.data(),
                    MakeRange(path_begin, base::checked_cast<int>(url.size())),
                    &parsed->path, &parsed->query, &parsed->ref);
}

// Backend for the external functions that operates on either char type.
// Handles cases where there is a scheme, but also when handed the first
// character following the "file:" at the beginning of the spec. If so,
// this is usually a slash, but needn't be; we allow paths like "file:c:\foo".
template <typename CharT>
Parsed DoParseFileURL(std::basic_string_view<CharT> url) {
  // Strip leading & trailing spaces and control characters.
  int begin = 0;
  int url_len = base::checked_cast<int>(url.size());
  TrimURL(url.data(), &begin, &url_len);

  // Find the scheme, if any.
  int num_slashes = CountConsecutiveSlashes(url.data(), begin, url_len);
  int after_scheme;
  size_t after_slashes;
  Parsed parsed;
#ifdef WIN32
  // See how many slashes there are. We want to handle cases like UNC but also
  // "/c:/foo". This is when there is no scheme, so we can allow pages to do
  // links like "c:/foo/bar" or "//foo/bar". This is also called by the
  // relative URL resolver when it determines there is an absolute URL, which
  // may give us input like "/c:/foo".
  after_slashes = begin + num_slashes;
  if (DoesBeginWindowsDriveSpec(url.data(), after_slashes, url_len)) {
    // Windows path, don't try to extract the scheme (for example, "c:\foo").
    after_scheme = after_slashes;
  } else if (DoesBeginUNCPath(url.data(), begin, url_len, false)) {
    // Windows UNC path: don't try to extract the scheme, but keep the slashes.
    after_scheme = begin;
  } else
#endif
  {
    // ExtractScheme doesn't understand the possibility of filenames with
    // colons in them, in which case it returns the entire spec up to the
    // colon as the scheme. So handle /foo.c:5 as a file but foo.c:5 as
    // the foo.c: scheme.
    if (!num_slashes &&
        ExtractScheme(&url[begin], url_len - begin, &parsed.scheme)) {
      // Offset the results since we gave ExtractScheme a substring.
      parsed.scheme.begin += begin;
      after_scheme = parsed.scheme.end() + 1;
    } else {
      // No scheme found, remember that.
      parsed.scheme.reset();
      after_scheme = begin;
    }
  }

  // Handle empty specs ones that contain only whitespace or control chars,
  // or that are just the scheme (for example "file:").
  if (after_scheme == url_len) {
    return parsed;
  }

  num_slashes = CountConsecutiveSlashes(url.data(), after_scheme, url_len);
  after_slashes = after_scheme + num_slashes;
#ifdef WIN32
  // Check whether the input is a drive again. We checked above for windows
  // drive specs, but that's only at the very beginning to see if we have a
  // scheme at all. This test will be duplicated in that case, but will
  // additionally handle all cases with a real scheme such as "file:///C:/".
  if (!DoesBeginWindowsDriveSpec(url.data(), after_slashes, url_len) &&
      num_slashes != 3) {
    // Anything not beginning with a drive spec ("c:\") on Windows is treated
    // as UNC, with the exception of three slashes which always means a file.
    // Even IE7 treats file:///foo/bar as "/foo/bar", which then fails.
    DoParseUNC(url.substr(0, url_len), after_slashes, &parsed);
    return parsed;
  }
#else
  // file: URL with exactly 2 slashes is considered to have a host component.
  if (num_slashes == 2) {
    DoParseUNC(url.substr(0, url_len), after_slashes, &parsed);
    return parsed;
  }
#endif  // WIN32

  // Easy and common case, the full path immediately follows the scheme
  // (modulo slashes), as in "file://c:/foo". Just treat everything from
  // there to the end as the path. Empty hosts have 0 length instead of -1.
  // We include the last slash as part of the path if there is one.
  DoParseLocalFile(
      url.substr(0, url_len),
      num_slashes > 0 ? after_scheme + num_slashes - 1 : after_scheme, &parsed);
  return parsed;
}

}  // namespace

Parsed ParseFileURL(std::string_view url) {
  return DoParseFileURL(url);
}

Parsed ParseFileURL(std::u16string_view url) {
  return DoParseFileURL(url);
}

}  // namespace url
