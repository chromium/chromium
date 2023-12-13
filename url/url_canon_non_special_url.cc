// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Functions to canonicalize non-special URLs.

#include "url/url_canon.h"
#include "url/url_canon_internal.h"

namespace url {

namespace {

template <typename CHAR>
bool DoCanonicalizeNonSpecialURL(const URLComponentSource<CHAR>& source,
                                 const Parsed& parsed,
                                 CharsetConverter* query_converter,
                                 CanonOutput& output,
                                 Parsed& new_parsed) {
  // The implementation is similar to `DoCanonicalizeStandardURL()`, but there
  // are many subtle differences. So we have a different function for
  // canonicalizing non-special URLs.

  DCHECK(!parsed.has_opaque_path);

  // Scheme: this will append the colon.
  bool success = CanonicalizeScheme(source.scheme, parsed.scheme, &output,
                                    &new_parsed.scheme);
  bool have_authority =
      (parsed.username.is_valid() || parsed.password.is_valid() ||
       parsed.host.is_valid() || parsed.port.is_valid());

  // Non-special URL examples which should be carefully handled:
  //
  // | URL      | parsed.user   | parsed.host   | have_authority | Valid URL? |
  // |----------+---------------+---------------+----------------+------------|
  // | git:/a   | invalid       | invalid       | false          | valid      |
  // | git://@/ | valid (empty) | invalid       | true           | invalid    |
  // | git:///  | invalid       | valid (empty) | true           | valid      |

  if (have_authority) {
    // Only write the authority separators when we have a scheme.
    if (parsed.scheme.is_valid()) {
      output.push_back('/');
      output.push_back('/');
    }

    // User info: the canonicalizer will handle the : and @.
    success &= CanonicalizeUserInfo(source.username, parsed.username,
                                    source.password, parsed.password, &output,
                                    &new_parsed.username, &new_parsed.password);

    // Host
    if (parsed.host.is_valid()) {
      success &= CanonicalizeNonSpecialHost(source.host, parsed.host, output,
                                            new_parsed.host);
    } else {
      // URL is invalid if `have_authority` is true, but `parsed.host` is
      // invalid. Example: "git://@/".
      success = false;
    }

    // Port
    success &= CanonicalizePort(source.port, parsed.port, PORT_UNSPECIFIED,
                                &output, &new_parsed.port);
  } else {
    // No authority, clear the components.
    new_parsed.host.reset();
    new_parsed.username.reset();
    new_parsed.password.reset();
    new_parsed.port.reset();
  }

  // Path
  if (parsed.path.is_valid()) {
    success &=
        CanonicalizePath(source.path, parsed.path, CanonMode::kNonSpecialURL,
                         &output, &new_parsed.path);
  } else {
    new_parsed.path.reset();
  }

  // Query
  CanonicalizeQuery(source.query, parsed.query, query_converter, &output,
                    &new_parsed.query);

  // Ref: ignore failure for this, since the page can probably still be loaded.
  CanonicalizeRef(source.ref, parsed.ref, &output, &new_parsed.ref);

  // Carry over the flag for potentially dangling markup:
  if (parsed.potentially_dangling_markup) {
    new_parsed.potentially_dangling_markup = true;
  }

  return success;
}

}  // namespace

bool CanonicalizeNonSpecialURL(const char* spec,
                               int spec_len,
                               const Parsed& parsed,
                               CharsetConverter* query_converter,
                               CanonOutput& output,
                               Parsed& new_parsed) {
  // Carry over the flag.
  new_parsed.has_opaque_path = parsed.has_opaque_path;

  if (parsed.has_opaque_path) {
    return CanonicalizePathURL(spec, spec_len, parsed, &output, &new_parsed);
  }
  return DoCanonicalizeNonSpecialURL(URLComponentSource(spec), parsed,
                                     query_converter, output, new_parsed);
}

bool CanonicalizeNonSpecialURL(const char16_t* spec,
                               int spec_len,
                               const Parsed& parsed,
                               CharsetConverter* query_converter,
                               CanonOutput& output,
                               Parsed& new_parsed) {
  // Carry over the flag.
  new_parsed.has_opaque_path = parsed.has_opaque_path;

  if (parsed.has_opaque_path) {
    return CanonicalizePathURL(spec, spec_len, parsed, &output, &new_parsed);
  }
  return DoCanonicalizeNonSpecialURL(URLComponentSource(spec), parsed,
                                     query_converter, output, new_parsed);
}

}  // namespace url
