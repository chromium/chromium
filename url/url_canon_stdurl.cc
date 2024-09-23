// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/350788890): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

// Functions to canonicalize "standard" URLs, which are ones that have an
// authority section including a host name.

#include "url/url_canon.h"
#include "url/url_canon_internal.h"
#include "url/url_constants.h"

namespace url {

namespace {

template <typename CHAR>
bool DoCanonicalizeStandardURL(const URLComponentSource<CHAR>& source,
                               const Parsed& parsed,
                               SchemeType scheme_type,
                               CharsetConverter* query_converter,
                               CanonOutput* output,
                               Parsed* new_parsed) {
  DCHECK(!parsed.has_opaque_path);

  // Scheme: this will append the colon.
  bool success = CanonicalizeScheme(source.scheme, parsed.scheme,
                                    output, &new_parsed->scheme);

  bool scheme_supports_user_info =
      (scheme_type == SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION);
  bool scheme_supports_ports =
      (scheme_type == SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION ||
       scheme_type == SCHEME_WITH_HOST_AND_PORT);

  // Authority (username, password, host, port)
  bool have_authority;
  if ((scheme_supports_user_info &&
       (parsed.username.is_valid() || parsed.password.is_valid())) ||
      parsed.host.is_nonempty() ||
      (scheme_supports_ports && parsed.port.is_valid())) {
    have_authority = true;

    // Only write the authority separators when we have a scheme.
    if (parsed.scheme.is_valid()) {
      output->push_back('/');
      output->push_back('/');
    }

    // User info: the canonicalizer will handle the : and @.
    if (scheme_supports_user_info) {
      success &= CanonicalizeUserInfo(
          source.username, parsed.username, source.password, parsed.password,
          output, &new_parsed->username, &new_parsed->password);
    } else {
      new_parsed->username.reset();
      new_parsed->password.reset();
    }

    success &= CanonicalizeHost(source.host, parsed.host,
                                output, &new_parsed->host);

    // Host must not be empty for standard URLs.
    if (parsed.host.is_empty())
      success = false;

    // Port: the port canonicalizer will handle the colon.
    if (scheme_supports_ports) {
      int default_port = DefaultPortForScheme(std::string_view(
          &output->data()[new_parsed->scheme.begin], new_parsed->scheme.len));
      success &= CanonicalizePort(source.port, parsed.port, default_port,
                                  output, &new_parsed->port);
    } else {
      new_parsed->port.reset();
    }
  } else {
    // No authority, clear the components.
    have_authority = false;
    new_parsed->host.reset();
    new_parsed->username.reset();
    new_parsed->password.reset();
    new_parsed->port.reset();
    success = false;  // Standard URLs must have an authority.
  }

  // Path
  if (parsed.path.is_valid()) {
    success &= CanonicalizePath(source.path, parsed.path,
                                output, &new_parsed->path);
  } else if (have_authority ||
             parsed.query.is_valid() || parsed.ref.is_valid()) {
    // When we have an empty path, make up a path when we have an authority
    // or something following the path. The only time we allow an empty
    // output path is when there is nothing else.
    new_parsed->path = Component(output->length(), 1);
    output->push_back('/');
  } else {
    // No path at all
    new_parsed->path.reset();
  }

  // Query
  CanonicalizeQuery(source.query, parsed.query, query_converter,
                    output, &new_parsed->query);

  // Ref: ignore failure for this, since the page can probably still be loaded.
  CanonicalizeRef(source.ref, parsed.ref, output, &new_parsed->ref);

  // Carry over the flag for potentially dangling markup:
  if (parsed.potentially_dangling_markup)
    new_parsed->potentially_dangling_markup = true;

  return success;
}

}  // namespace

// Returns the default port for the given canonical scheme, or PORT_UNSPECIFIED
// if the scheme is unknown.
//
// Please keep blink::DefaultPortForProtocol and url::DefaultPortForProtocol in
// sync.
int DefaultPortForScheme(std::string_view scheme) {
  switch (scheme.length()) {
    case 4:
      if (scheme == kHttpScheme) {
        return 80;
      }
      break;
    case 5:
      if (scheme == kHttpsScheme) {
        return 443;
      }
      break;
    case 3:
      if (scheme == kFtpScheme) {
        return 21;
      } else if (scheme == kWssScheme) {
        return 443;
      }
      break;
    case 2:
      if (scheme == kWsScheme) {
        return 80;
      }
      break;
  }
  return PORT_UNSPECIFIED;
}

bool CanonicalizeStandardURL(const char* spec,
                             const Parsed& parsed,
                             SchemeType scheme_type,
                             CharsetConverter* query_converter,
                             CanonOutput* output,
                             Parsed* new_parsed) {
  return DoCanonicalizeStandardURL(URLComponentSource(spec), parsed,
                                   scheme_type, query_converter, output,
                                   new_parsed);
}

bool CanonicalizeStandardURL(const char16_t* spec,
                             const Parsed& parsed,
                             SchemeType scheme_type,
                             CharsetConverter* query_converter,
                             CanonOutput* output,
                             Parsed* new_parsed) {
  return DoCanonicalizeStandardURL(URLComponentSource(spec), parsed,
                                   scheme_type, query_converter, output,
                                   new_parsed);
}

// It might be nice in the future to optimize this so unchanged components don't
// need to be recanonicalized. This is especially true since the common case for
// ReplaceComponents is removing things we don't want, like reference fragments
// and usernames. These cases can become more efficient if we can assume the
// rest of the URL is OK with these removed (or only the modified parts
// recanonicalized). This would be much more complex to implement, however.
//
// You would also need to update DoReplaceComponents in url_util.cc which
// relies on this re-checking everything (see the comment there for why).
bool ReplaceStandardURL(const char* base,
                        const Parsed& base_parsed,
                        const Replacements<char>& replacements,
                        SchemeType scheme_type,
                        CharsetConverter* query_converter,
                        CanonOutput* output,
                        Parsed* new_parsed) {
  URLComponentSource<char> source(base);
  Parsed parsed(base_parsed);
  SetupOverrideComponents(base, replacements, &source, &parsed);
  return DoCanonicalizeStandardURL(source, parsed, scheme_type, query_converter,
                                   output, new_parsed);
}

// For 16-bit replacements, we turn all the replacements into UTF-8 so the
// regular code path can be used.
bool ReplaceStandardURL(const char* base,
                        const Parsed& base_parsed,
                        const Replacements<char16_t>& replacements,
                        SchemeType scheme_type,
                        CharsetConverter* query_converter,
                        CanonOutput* output,
                        Parsed* new_parsed) {
  RawCanonOutput<1024> utf8;
  URLComponentSource<char> source(base);
  Parsed parsed(base_parsed);
  SetupUTF16OverrideComponents(base, replacements, &utf8, &source, &parsed);
  return DoCanonicalizeStandardURL(source, parsed, scheme_type, query_converter,
                                   output, new_parsed);
}

}  // namespace url
