// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/350788890): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

// Functions for canonicalizing "mailto:" URLs.

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
bool DoCanonicalizeMailtoURL(const URLComponentSource<CHAR>& source,
                             const Parsed& parsed,
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
  if (parsed.path.is_valid()) {
    new_parsed->path.begin = output->length();

    // Copy the path using path URL's more lax escaping rules.
    // We convert to UTF-8 and escape non-ASCII, but leave most
    // ASCII characters alone.
    size_t end = static_cast<size_t>(parsed.path.end());
    for (size_t i = static_cast<size_t>(parsed.path.begin); i < end; ++i) {
      UCHAR uch = static_cast<UCHAR>(source.path[i]);
      if (ShouldEncodeMailboxCharacter<UCHAR>(uch))
        success &= AppendUTF8EscapedChar(source.path, &i, end, output);
      else
        output->push_back(static_cast<char>(uch));
    }

    new_parsed->path.len = output->length() - new_parsed->path.begin;
  } else {
    // No path at all
    new_parsed->path.reset();
  }

  // Query -- always use the default UTF8 charset converter.
  CanonicalizeQuery(source.query, parsed.query, NULL,
                    output, &new_parsed->query);

  return success;
}

} // namespace

bool CanonicalizeMailtoURL(const char* spec,
                           int spec_len,
                           const Parsed& parsed,
                           CanonOutput* output,
                           Parsed* new_parsed) {
  return DoCanonicalizeMailtoURL<char, unsigned char>(
      URLComponentSource<char>(spec), parsed, output, new_parsed);
}

bool CanonicalizeMailtoURL(const char16_t* spec,
                           int spec_len,
                           const Parsed& parsed,
                           CanonOutput* output,
                           Parsed* new_parsed) {
  return DoCanonicalizeMailtoURL<char16_t, char16_t>(
      URLComponentSource<char16_t>(spec), parsed, output, new_parsed);
}

bool ReplaceMailtoURL(const char* base,
                      const Parsed& base_parsed,
                      const Replacements<char>& replacements,
                      CanonOutput* output,
                      Parsed* new_parsed) {
  URLComponentSource<char> source(base);
  Parsed parsed(base_parsed);
  SetupOverrideComponents(base, replacements, &source, &parsed);
  return DoCanonicalizeMailtoURL<char, unsigned char>(
      source, parsed, output, new_parsed);
}

bool ReplaceMailtoURL(const char* base,
                      const Parsed& base_parsed,
                      const Replacements<char16_t>& replacements,
                      CanonOutput* output,
                      Parsed* new_parsed) {
  RawCanonOutput<1024> utf8;
  URLComponentSource<char> source(base);
  Parsed parsed(base_parsed);
  SetupUTF16OverrideComponents(base, replacements, &utf8, &source, &parsed);
  return DoCanonicalizeMailtoURL<char, unsigned char>(
      source, parsed, output, new_parsed);
}

}  // namespace url
