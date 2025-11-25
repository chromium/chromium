// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Functions for canonicalizing "mailto:" URLs.

#include "base/compiler_specific.h"
#include "url/url_canon.h"
#include "url/url_canon_internal.h"
#include "url/url_file.h"
#include "url/url_parse_internal.h"

namespace url {

namespace {

// Certain characters should be percent-encoded when they appear in the path
// component of a mailto URL, to improve compatibility and mitigate against
// command-injection attacks on mailto handlers. See https://crbug.com/711020.
template <typename UCHAR>
bool ShouldEncodeMailboxCharacter(UCHAR uch) {
  if (uch < 0x21 ||                              // space & control characters.
      uch > 0x7e ||                              // high-ascii characters.
      uch == 0x22 ||                             // quote.
      uch == 0x3c || uch == 0x3e ||              // angle brackets.
      uch == 0x60 ||                             // backtick.
      uch == 0x7b || uch == 0x7c || uch == 0x7d  // braces and pipe.
      ) {
    return true;
  }
  return false;
}

template <typename CHAR, typename UCHAR>
bool DoCanonicalizeMailtoUrl(const Replacements<CHAR>& source,
                             CanonOutput* output,
                             Parsed* new_parsed) {
  // mailto: only uses {scheme, path, query} -- clear the rest.
  new_parsed->username = Component();
  new_parsed->password = Component();
  new_parsed->host = Component();
  new_parsed->port = Component();
  new_parsed->ref = Component();

  // Scheme (known, so we don't bother running it through the more
  // complicated scheme canonicalizer).
  new_parsed->scheme.begin = output->length();
  output->Append("mailto:");
  new_parsed->scheme.len = 6;

  bool success = true;

  // Path
  if (source.components().path.is_valid()) {
    new_parsed->path.begin = output->length();

    // Copy the path using path URL's more lax escaping rules.
    // We convert to UTF-8 and escape non-ASCII, but leave most
    // ASCII characters alone.
    auto path_view = *source.MaybePath();
    for (size_t i = 0; i < path_view.length(); ++i) {
      UCHAR uch = static_cast<UCHAR>(path_view[i]);
      if (ShouldEncodeMailboxCharacter<UCHAR>(uch))
        success &= AppendUtf8EscapedChar(path_view, &i, output);
      else
        output->push_back(static_cast<char>(uch));
    }

    new_parsed->path.len = output->length() - new_parsed->path.begin;
  } else {
    // No path at all
    new_parsed->path.reset();
  }

  // Query -- always use the default UTF8 charset converter.
  CanonicalizeQuery(source.MaybeQuery(), nullptr, output, &new_parsed->query);

  return success;
}

} // namespace

bool CanonicalizeMailtoUrl(std::string_view spec,
                           const Parsed& parsed,
                           CanonOutput* output,
                           Parsed* new_parsed) {
  return DoCanonicalizeMailtoUrl<char, unsigned char>(
      Replacements<char>(spec, parsed), output, new_parsed);
}

bool CanonicalizeMailtoUrl(std::u16string_view spec,
                           const Parsed& parsed,
                           CanonOutput* output,
                           Parsed* new_parsed) {
  return DoCanonicalizeMailtoUrl<char16_t, char16_t>(
      Replacements<char16_t>(spec, parsed), output, new_parsed);
}

bool ReplaceMailtoUrl(std::string_view base,
                      const Parsed& base_parsed,
                      const Replacements<char>& replacements,
                      CanonOutput* output,
                      Parsed* new_parsed) {
  Replacements<char> overridden(base, base_parsed);
  SetupOverrideComponents(replacements, overridden);
  return DoCanonicalizeMailtoUrl<char, unsigned char>(overridden, output,
                                                      new_parsed);
}

bool ReplaceMailtoUrl(std::string_view base,
                      const Parsed& base_parsed,
                      const Replacements<char16_t>& replacements,
                      CanonOutput* output,
                      Parsed* new_parsed) {
  RawCanonOutput<1024> utf8;
  Replacements<char> overridden(base, base_parsed);
  SetupUtf16OverrideComponents(replacements, utf8, overridden);
  return DoCanonicalizeMailtoUrl<char, unsigned char>(overridden, output,
                                                      new_parsed);
}

}  // namespace url
