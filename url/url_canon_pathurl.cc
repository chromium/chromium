// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/350788890): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

// Functions for canonicalizing "path" URLs. Not to be confused with the path
// of a URL, these are URLs that have no authority section, only a path. For
// example, "javascript:" and "data:".

#include "url/url_canon.h"
#include "url/url_canon_internal.h"

namespace url {

namespace {

// Canonicalize the given |component| from |source| into |output| and
// |new_component|. If |separator| is non-zero, it is pre-pended to |output|
// prior to the canonicalized component; i.e. for the '?' or '#' characters.
template <typename CHAR, typename UCHAR>
void DoCanonicalizePathComponent(const CHAR* source,
                                 const Component& component,
                                 char separator,
                                 CanonOutput* output,
                                 Component* new_component) {
  if (component.is_valid()) {
    if (separator)
      output->push_back(separator);
    // Copy the path using path URL's more lax escaping rules (think for
    // javascript:). We convert to UTF-8 and escape characters from the
    // C0 control percent-encode set, but leave all other characters alone.
    // This helps readability of JavaScript.
    // https://url.spec.whatwg.org/#cannot-be-a-base-url-path-state
    // https://url.spec.whatwg.org/#c0-control-percent-encode-set
    new_component->begin = output->length();
    size_t end = static_cast<size_t>(component.end());
    for (size_t i = static_cast<size_t>(component.begin); i < end; i++) {
      UCHAR uch = static_cast<UCHAR>(source[i]);
      if (IsInC0ControlPercentEncodeSet(uch)) {
        AppendUTF8EscapedChar(source, &i, end, output);
      } else {
        output->push_back(static_cast<char>(uch));
      }
    }
    new_component->len = output->length() - new_component->begin;
  } else {
    // Empty part.
    new_component->reset();
  }
}

template <typename CHAR, typename UCHAR>
bool DoCanonicalizePathURL(const URLComponentSource<CHAR>& source,
                           const Parsed& parsed,
                           CanonOutput* output,
                           Parsed* new_parsed) {
  // Scheme: this will append the colon.
  bool success = CanonicalizeScheme(source.scheme, parsed.scheme,
                                    output, &new_parsed->scheme);

  // We assume there's no authority for path URLs. Note that hosts should never
  // have -1 length.
  new_parsed->username.reset();
  new_parsed->password.reset();
  new_parsed->host.reset();
  new_parsed->port.reset();

  // Canonicalize path via the weaker path URL rules.
  //
  // Note: parsing the path part should never cause a failure, see
  // https://url.spec.whatwg.org/#cannot-be-a-base-url-path-state
  DoCanonicalizePathComponent<CHAR, UCHAR>(source.path, parsed.path, '\0',
                                           output, &new_parsed->path);

  // Similar to mailto:, always use the default UTF-8 charset converter for
  // query.
  CanonicalizeQuery(source.query, parsed.query, nullptr, output,
                    &new_parsed->query);

  CanonicalizeRef(source.ref, parsed.ref, output, &new_parsed->ref);

  return success;
}

}  // namespace

bool CanonicalizePathURL(const char* spec,
                         int spec_len,
                         const Parsed& parsed,
                         CanonOutput* output,
                         Parsed* new_parsed) {
  return DoCanonicalizePathURL<char, unsigned char>(
      URLComponentSource<char>(spec), parsed, output, new_parsed);
}

bool CanonicalizePathURL(const char16_t* spec,
                         int spec_len,
                         const Parsed& parsed,
                         CanonOutput* output,
                         Parsed* new_parsed) {
  return DoCanonicalizePathURL<char16_t, char16_t>(
      URLComponentSource<char16_t>(spec), parsed, output, new_parsed);
}

void CanonicalizePathURLPath(const char* source,
                             const Component& component,
                             CanonOutput* output,
                             Component* new_component) {
  DoCanonicalizePathComponent<char, unsigned char>(source, component, '\0',
                                                   output, new_component);
}

void CanonicalizePathURLPath(const char16_t* source,
                             const Component& component,
                             CanonOutput* output,
                             Component* new_component) {
  DoCanonicalizePathComponent<char16_t, char16_t>(source, component, '\0',
                                                  output, new_component);
}

bool ReplacePathURL(const char* base,
                    const Parsed& base_parsed,
                    const Replacements<char>& replacements,
                    CanonOutput* output,
                    Parsed* new_parsed) {
  URLComponentSource<char> source(base);
  Parsed parsed(base_parsed);
  SetupOverrideComponents(base, replacements, &source, &parsed);
  return DoCanonicalizePathURL<char, unsigned char>(
      source, parsed, output, new_parsed);
}

bool ReplacePathURL(const char* base,
                    const Parsed& base_parsed,
                    const Replacements<char16_t>& replacements,
                    CanonOutput* output,
                    Parsed* new_parsed) {
  RawCanonOutput<1024> utf8;
  URLComponentSource<char> source(base);
  Parsed parsed(base_parsed);
  SetupUTF16OverrideComponents(base, replacements, &utf8, &source, &parsed);
  return DoCanonicalizePathURL<char, unsigned char>(
      source, parsed, output, new_parsed);
}

}  // namespace url
