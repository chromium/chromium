// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
void DoCanonicalizePathComponent(
    std::optional<std::basic_string_view<CHAR>> source,
    char separator,
    CanonOutput* output,
    Component* new_component) {
  if (source.has_value()) {
    auto& source_value = *source;

    if (separator)
      output->push_back(separator);
    // Copy the path using path URL's more lax escaping rules (think for
    // javascript:). We convert to UTF-8 and escape characters from the
    // C0 control percent-encode set, but leave all other characters alone.
    // This helps readability of JavaScript.
    // https://url.spec.whatwg.org/#cannot-be-a-base-url-path-state
    // https://url.spec.whatwg.org/#c0-control-percent-encode-set
    new_component->begin = output->length();
    for (size_t i = 0; i < source_value.size(); i++) {
      UCHAR uch = static_cast<UCHAR>(source_value[i]);
      if (IsInC0ControlPercentEncodeSet(uch)) {
        AppendUtf8EscapedChar(source_value, &i, output);
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
bool DoCanonicalizePathUrl(const Replacements<CHAR>& source,
                           CanonOutput* output,
                           Parsed* new_parsed) {
  // Scheme: this will append the colon.
  bool success =
      CanonicalizeScheme(source.MaybeScheme(), output, &new_parsed->scheme);

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
  DoCanonicalizePathComponent<CHAR, UCHAR>(source.MaybePath(), '\0', output,
                                           &new_parsed->path);

  // Similar to mailto:, always use the default UTF-8 charset converter for
  // query.
  CanonicalizeQuery(source.MaybeQuery(), nullptr, output, &new_parsed->query);

  CanonicalizeRef(source.MaybeRef(), output, &new_parsed->ref);

  return success;
}

}  // namespace

bool CanonicalizePathUrl(std::string_view spec,
                         const Parsed& parsed,
                         CanonOutput* output,
                         Parsed* new_parsed) {
  return DoCanonicalizePathUrl<char, unsigned char>(
      Replacements<char>(spec, parsed), output, new_parsed);
}

bool CanonicalizePathUrl(std::u16string_view spec,
                         const Parsed& parsed,
                         CanonOutput* output,
                         Parsed* new_parsed) {
  return DoCanonicalizePathUrl<char16_t, char16_t>(
      Replacements<char16_t>(spec, parsed), output, new_parsed);
}

void CanonicalizePathUrlPath(std::optional<std::string_view> source,
                             CanonOutput* output,
                             Component* new_component) {
  DoCanonicalizePathComponent<char, unsigned char>(source, '\0', output,
                                                   new_component);
}

void CanonicalizePathUrlPath(std::optional<std::u16string_view> source,
                             CanonOutput* output,
                             Component* new_component) {
  DoCanonicalizePathComponent<char16_t, char16_t>(source, '\0', output,
                                                  new_component);
}

bool ReplacePathUrl(std::string_view base,
                    const Parsed& base_parsed,
                    const Replacements<char>& replacements,
                    CanonOutput* output,
                    Parsed* new_parsed) {
  Replacements<char> overridden(base, base_parsed);
  SetupOverrideComponents(replacements, overridden);
  return DoCanonicalizePathUrl<char, unsigned char>(overridden, output,
                                                    new_parsed);
}

bool ReplacePathUrl(std::string_view base,
                    const Parsed& base_parsed,
                    const Replacements<char16_t>& replacements,
                    CanonOutput* output,
                    Parsed* new_parsed) {
  RawCanonOutput<1024> utf8;
  Replacements<char> overridden(base, base_parsed);
  SetupUtf16OverrideComponents(replacements, utf8, overridden);
  return DoCanonicalizePathUrl<char, unsigned char>(overridden, output,
                                                    new_parsed);
}

}  // namespace url
