// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Functions for canonicalizing "filesystem:file:" URLs.

#include <optional>

#include "url/url_canon.h"
#include "url/url_canon_internal.h"
#include "url/url_file.h"
#include "url/url_parse_internal.h"
#include "url/url_util.h"
#include "url/url_util_internal.h"

namespace url {

namespace {

// We use the URLComponentSource for the outer URL, as it can have replacements,
// whereas the inner_url can't, so it uses spec.
template <typename CHAR>
bool DoCanonicalizeFileSystemUrl(std::basic_string_view<CHAR> spec,
                                 const Replacements<CHAR>& source,
                                 CharsetConverter* charset_converter,
                                 CanonOutput* output,
                                 Parsed* new_parsed) {
  // filesystem only uses {scheme, path, query, ref} -- clear the rest.
  new_parsed->username.reset();
  new_parsed->password.reset();
  new_parsed->host.reset();
  new_parsed->port.reset();

  const Parsed* inner_parsed = source.components().inner_parsed();
  Parsed new_inner_parsed;

  // Scheme (known, so we don't bother running it through the more
  // complicated scheme canonicalizer).
  new_parsed->scheme.begin = output->length();
  output->Append("filesystem:");
  new_parsed->scheme.len = 10;

  if (!inner_parsed || !inner_parsed->scheme.is_valid())
    return false;

  bool success = true;
  SchemeType inner_scheme_type = SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION;
  if (CompareSchemeComponent(spec, inner_parsed->scheme, url::kFileScheme)) {
    new_inner_parsed.scheme.begin = output->length();
    output->Append("file://");
    new_inner_parsed.scheme.len = 4;
    success &= CanonicalizePath(inner_parsed->path.MaybeAsViewOn(spec), output,
                                &new_inner_parsed.path);
  } else if (GetStandardSchemeType(inner_parsed->scheme.AsViewOn(spec),
                                   &inner_scheme_type)) {
    if (inner_scheme_type == SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION) {
      // Strip out the user information from the inner URL, if any.
      inner_scheme_type = SCHEME_WITH_HOST_AND_PORT;
    }
    success =
        CanonicalizeStandardUrl(spec, *inner_parsed, inner_scheme_type,
                                charset_converter, output, &new_inner_parsed);
  } else {
    // TODO(ericu): The URL is wrong, but should we try to output more of what
    // we were given?  Echoing back filesystem:mailto etc. doesn't seem all that
    // useful.
    return false;
  }
  // The filesystem type must be more than just a leading slash for validity.
  success &= new_inner_parsed.path.len > 1;

  success &= CanonicalizePath(source.MaybePath(), output, &new_parsed->path);

  // Ignore failures for query/ref since the URL can probably still be loaded.
  CanonicalizeQuery(source.MaybeQuery(), charset_converter, output,
                    &new_parsed->query);
  CanonicalizeRef(source.MaybeRef(), output, &new_parsed->ref);
  if (success)
    new_parsed->set_inner_parsed(new_inner_parsed);

  return success;
}

}  // namespace

bool CanonicalizeFileSystemUrl(std::string_view spec,
                               const Parsed& parsed,
                               CharsetConverter* charset_converter,
                               CanonOutput* output,
                               Parsed* new_parsed) {
  return DoCanonicalizeFileSystemUrl(spec, Replacements<char>(spec, parsed),
                                     charset_converter, output, new_parsed);
}

bool CanonicalizeFileSystemUrl(std::u16string_view spec,
                               const Parsed& parsed,
                               CharsetConverter* charset_converter,
                               CanonOutput* output,
                               Parsed* new_parsed) {
  return DoCanonicalizeFileSystemUrl(spec, Replacements<char16_t>(spec, parsed),
                                     charset_converter, output, new_parsed);
}

bool ReplaceFileSystemUrl(std::string_view base,
                          const Parsed& base_parsed,
                          const Replacements<char>& replacements,
                          CharsetConverter* charset_converter,
                          CanonOutput* output,
                          Parsed* new_parsed) {
  Replacements<char> overridden(base, base_parsed);
  SetupOverrideComponents(replacements, overridden);
  return DoCanonicalizeFileSystemUrl(base, overridden, charset_converter,
                                     output, new_parsed);
}

bool ReplaceFileSystemUrl(std::string_view base,
                          const Parsed& base_parsed,
                          const Replacements<char16_t>& replacements,
                          CharsetConverter* charset_converter,
                          CanonOutput* output,
                          Parsed* new_parsed) {
  RawCanonOutput<1024> utf8;
  Replacements<char> overridden(base, base_parsed);
  SetupUtf16OverrideComponents(replacements, utf8, overridden);
  return DoCanonicalizeFileSystemUrl(base, overridden, charset_converter,
                                     output, new_parsed);
}

}  // namespace url
