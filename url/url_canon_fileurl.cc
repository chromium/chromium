// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/350788890): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

// Functions for canonicalizing "file:" URLs.

#include <string_view>

#include "base/strings/string_util.h"
#include "url/url_canon.h"
#include "url/url_canon_internal.h"
#include "url/url_file.h"
#include "url/url_parse_internal.h"

namespace url {

namespace {

bool IsLocalhost(const char* spec, int begin, int end) {
  if (begin > end)
    return false;
  return std::string_view(&spec[begin], end - begin) == "localhost";
}

bool IsLocalhost(const char16_t* spec, int begin, int end) {
  if (begin > end)
    return false;
  return std::u16string_view(&spec[begin], end - begin) == u"localhost";
}

template <typename CHAR>
int DoFindWindowsDriveLetter(const CHAR* spec, int begin, int end) {
  if (begin > end)
    return -1;

  // First guess the beginning of the drive letter.
  // If there is something that looks like a drive letter in the spec between
  // begin and end, store its position in drive_letter_pos.
  int drive_letter_pos =
      DoesContainWindowsDriveSpecUntil(spec, begin, end, end);
  if (drive_letter_pos < begin)
    return -1;

  // Check if the path up to the drive letter candidate can be canonicalized as
  // "/".
  Component sub_path = MakeRange(begin, drive_letter_pos);
  RawCanonOutput<1024> output;
  Component output_path;
  bool success = CanonicalizePath(sub_path.maybe_as_string_view_on(spec),
                                  &output, &output_path);
  if (!success || output_path.len != 1 || output.at(output_path.begin) != '/') {
    return -1;
  }

  return drive_letter_pos;
}

#ifdef WIN32

// Given a path string, this copies and canonicalizes the drive letter and
// colon to the `output`, if one is found. If there is not a drive spec, it
// won't do anything. The index of the next character in the input string
// is returned (after the colon when a drive spec is found, zero if one is
// not).
template <typename CHAR>
size_t FileDoDriveSpec(std::basic_string_view<CHAR> path, CanonOutput* output) {
  int drive_letter_pos = FindWindowsDriveLetter(
      path.data(), 0, base::checked_cast<int>(path.length()));
  if (drive_letter_pos < 0) {
    return 0;
  }

  // By now, a valid drive letter is confirmed at position drive_letter_pos,
  // followed by a valid drive letter separator (a colon or a pipe).

  output->push_back('/');

  // Normalize Windows drive letters to uppercase.
  output->push_back(
      static_cast<char>(base::ToUpperASCII(path[drive_letter_pos])));

  // Normalize the character following it to a colon rather than pipe.
  output->push_back(':');
  return static_cast<size_t>(drive_letter_pos + 2);
}

#endif  // WIN32

template <typename CHAR, typename UCHAR>
bool DoFileCanonicalizePath(std::optional<std::basic_string_view<CHAR>> path,
                            CanonOutput* output,
                            Component* out_path) {
  // Copies and normalizes the "c:" at the beginning, if present.
  out_path->begin = output->length();
  size_t after_drive = 0;
#ifdef WIN32
  if (path) {
    after_drive = FileDoDriveSpec(*path, output);
  }
#endif

  // Copies the rest of the path, starting from the slash following the
  // drive colon (if any, Windows only), or the first slash of the path.
  bool success = true;
  if (path && after_drive < path->length()) {
    // Use the regular path canonicalizer to canonicalize the rest of the path
    // after the drive.
    //
    // Give it a fake output component to write into, since we will be
    // calculating the out_path ourselves (consisting of both the drive and the
    // path we canonicalize here).
    Component fake_output_path;
    success = CanonicalizePath(
        path->substr(after_drive, path->length() - after_drive), output,
        &fake_output_path);
  } else if (after_drive == 0) {
    // No input path and no drive spec, canonicalize to a slash.
    output->push_back('/');
  }

  out_path->len = output->length() - out_path->begin;
  return success;
}

template <typename CHAR, typename UCHAR>
bool DoCanonicalizeFileUrl(const URLComponentSource<CHAR>& source,
                           const Parsed& parsed,
                           CharsetConverter* query_converter,
                           CanonOutput* output,
                           Parsed* new_parsed) {
  DCHECK(!parsed.has_opaque_path);

  // Things we don't set in file: URLs.
  new_parsed->username = Component();
  new_parsed->password = Component();
  new_parsed->port = Component();

  // Scheme (known, so we don't bother running it through the more
  // complicated scheme canonicalizer).
  new_parsed->scheme.begin = output->length();
  output->Append("file://");
  new_parsed->scheme.len = 4;

  // If the host is localhost, and the path starts with a Windows drive letter,
  // remove the host component. This does the following transformation:
  //     file://localhost/C:/hello.txt -> file:///C:/hello.txt
  //
  // Note: we do this on every platform per URL Standard, not just Windows.
  //
  // TODO(crbug.com/41299821): According to the latest URL spec, this
  // transformation should be done regardless of the path.
  Component host_range = parsed.host;
  if (IsLocalhost(source.host, host_range.begin, host_range.end()) &&
      FindWindowsDriveLetter(source.path, parsed.path.begin,
                             parsed.path.end()) >= parsed.path.begin) {
    host_range.reset();
  }

  // Append the host. For many file URLs, this will be empty. For UNC, this
  // will be present.
  // TODO(brettw) This doesn't do any checking for host name validity. We
  // should probably handle validity checking of UNC hosts differently than
  // for regular IP hosts.
  bool success = CanonicalizeFileHost(
      std::basic_string_view<CHAR>(
          source.host, host_range.is_valid() ? host_range.end() : 0),
      host_range, *output, new_parsed->host);
  success &= DoFileCanonicalizePath<CHAR, UCHAR>(
      parsed.path.maybe_as_string_view_on(source.path), output,
      &new_parsed->path);

  CanonicalizeQuery(parsed.query.maybe_as_string_view_on(source.query),
                    query_converter, output, &new_parsed->query);
  CanonicalizeRef(parsed.ref.maybe_as_string_view_on(source.ref), output,
                  &new_parsed->ref);

  return success;
}

} // namespace

int FindWindowsDriveLetter(const char* spec, int begin, int end) {
  return DoFindWindowsDriveLetter(spec, begin, end);
}

int FindWindowsDriveLetter(const char16_t* spec, int begin, int end) {
  return DoFindWindowsDriveLetter(spec, begin, end);
}

bool CanonicalizeFileUrl(std::string_view spec,
                         const Parsed& parsed,
                         CharsetConverter* query_converter,
                         CanonOutput* output,
                         Parsed* new_parsed) {
  return DoCanonicalizeFileUrl<char, unsigned char>(
      URLComponentSource<char>(spec.data()), parsed, query_converter, output,
      new_parsed);
}

bool CanonicalizeFileUrl(std::u16string_view spec,
                         const Parsed& parsed,
                         CharsetConverter* query_converter,
                         CanonOutput* output,
                         Parsed* new_parsed) {
  return DoCanonicalizeFileUrl<char16_t, char16_t>(
      URLComponentSource<char16_t>(spec.data()), parsed, query_converter,
      output, new_parsed);
}

bool FileCanonicalizePath(std::optional<std::string_view> path,
                          CanonOutput* output,
                          Component* out_path) {
  return DoFileCanonicalizePath<char, unsigned char>(path, output, out_path);
}

bool FileCanonicalizePath(std::optional<std::u16string_view> path,
                          CanonOutput* output,
                          Component* out_path) {
  return DoFileCanonicalizePath<char16_t, char16_t>(path, output, out_path);
}

bool ReplaceFileUrl(std::string_view base,
                    const Parsed& base_parsed,
                    const Replacements<char>& replacements,
                    CharsetConverter* query_converter,
                    CanonOutput* output,
                    Parsed* new_parsed) {
  URLComponentSource<char> source(base.data());
  Parsed parsed(base_parsed);
  SetupOverrideComponents(base.data(), replacements, &source, &parsed);
  return DoCanonicalizeFileUrl<char, unsigned char>(
      source, parsed, query_converter, output, new_parsed);
}

bool ReplaceFileUrl(std::string_view base,
                    const Parsed& base_parsed,
                    const Replacements<char16_t>& replacements,
                    CharsetConverter* query_converter,
                    CanonOutput* output,
                    Parsed* new_parsed) {
  RawCanonOutput<1024> utf8;
  URLComponentSource<char> source(base.data());
  Parsed parsed(base_parsed);
  SetupUTF16OverrideComponents(base.data(), replacements, &utf8, &source,
                               &parsed);
  return DoCanonicalizeFileUrl<char, unsigned char>(
      source, parsed, query_converter, output, new_parsed);
}

}  // namespace url
