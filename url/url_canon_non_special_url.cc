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
  //
  // Since canonicalization is also used from url::ReplaceComponents(),
  // we have to handle an invalid URL replacement here, such as:
  //
  // > const url = "git:///";
  // > url.username = "x";
  // > url.href
  // "git:///" (this should not be "git://x@").

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

    // Username and Password
    //
    // URL Standard:
    // - https://url.spec.whatwg.org/#cannot-have-a-username-password-port
    // - https://url.spec.whatwg.org/#dom-url-username
    // - https://url.spec.whatwg.org/#dom-url-password
    if (parsed.host.is_nonempty()) {
      // User info: the canonicalizer will handle the : and @.
      success &= CanonicalizeUserInfo(
          source.username, parsed.username, source.password, parsed.password,
          &output, &new_parsed.username, &new_parsed.password);
    } else {
      new_parsed.username.reset();
      new_parsed.password.reset();
    }

    // Host
    if (parsed.host.is_valid()) {
      success &= CanonicalizeNonSpecialHost(source.host, parsed.host, output,
                                            new_parsed.host);
    } else {
      new_parsed.host.reset();
      // URL is invalid if `have_authority` is true, but `parsed.host` is
      // invalid. Example: "git://@/".
      success = false;
    }

    // Port
    //
    // URL Standard:
    // - https://url.spec.whatwg.org/#cannot-have-a-username-password-port
    // - https://url.spec.whatwg.org/#dom-url-port
    if (parsed.host.is_nonempty()) {
      success &= CanonicalizePort(source.port, parsed.port, PORT_UNSPECIFIED,
                                  &output, &new_parsed.port);
    } else {
      new_parsed.port.reset();
    }
  } else {
    // No authority, clear the components.
    new_parsed.host.reset();
    new_parsed.username.reset();
    new_parsed.password.reset();
    new_parsed.port.reset();
  }

  // Path
  if (parsed.path.is_valid()) {
    if (!parsed.host.is_valid() && parsed.path.is_empty()) {
      // Handle an edge case: Replacing non-special path-only URL's pathname
      // with an empty path.
      //
      // Path-only non-special URLs cannot have their paths erased.
      //
      // Example:
      //
      // > const url = new URL("git:/a");
      // > url.pathname = '';
      // > url.href
      // => The result should be "git:/", instead of "git:".
      // > url.pathname
      // => The result should be "/", instead of "".
      //
      // URL Standard is https://url.spec.whatwg.org/#dom-url-pathname, however,
      // it would take some time to understand why url.pathname ends up as "/"
      // in this case. Please read the URL Standard carefully to understand
      // that.
      new_parsed.path.begin = output.length();
      output.push_back('/');
      new_parsed.path.len = output.length() - new_parsed.path.begin;
    } else {
      success &=
          CanonicalizePath(source.path, parsed.path, CanonMode::kNonSpecialURL,
                           &output, &new_parsed.path);
      if (!parsed.host.is_valid() && new_parsed.path.is_valid() &&
          new_parsed.path.as_string_view_on(output.view().data())
              .starts_with("//")) {
        // To avoid path being treated as the host, prepend "/." to the path".
        //
        // Examples:
        //
        // > const url = new URL("git:/.//a");
        // > url.href
        // => The result should be "git:/.//a", instead of "git://a".
        //
        // > const url = new URL("git:/");
        // > url.pathname = "/.//a"
        // > url.href
        // => The result should be "git:/.//a", instead of "git://a".
        //
        // URL Standard: https://url.spec.whatwg.org/#concept-url-serializer
        //
        // > 3. If url’s host is null, url does not have an opaque path, url’s
        // > path’s size is greater than 1, and url’s path[0] is the empty
        // > string, then append U+002F (/) followed by U+002E (.) to output.
        //
        // Since the path length is unknown in advance, we post-process the new
        // path here. This case is likely to be infrequent, so the performance
        // impact should be minimal.
        size_t prior_output_length = output.length();
        output.Insert(new_parsed.path.begin, "/.");
        // Adjust path.
        new_parsed.path.begin += output.length() - prior_output_length;
      }
    }
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

bool ReplaceNonSpecialURL(const char* base,
                          const Parsed& base_parsed,
                          const Replacements<char>& replacements,
                          CharsetConverter* query_converter,
                          CanonOutput& output,
                          Parsed& new_parsed) {
  // Carry over the flag.
  new_parsed.has_opaque_path = base_parsed.has_opaque_path;

  if (base_parsed.has_opaque_path) {
    return ReplacePathURL(base, base_parsed, replacements, &output,
                          &new_parsed);
  }

  URLComponentSource<char> source(base);
  Parsed parsed(base_parsed);
  SetupOverrideComponents(base, replacements, &source, &parsed);
  return DoCanonicalizeNonSpecialURL(source, parsed, query_converter, output,
                                     new_parsed);
}

// For 16-bit replacements, we turn all the replacements into UTF-8 so the
// regular code path can be used.
bool ReplaceNonSpecialURL(const char* base,
                          const Parsed& base_parsed,
                          const Replacements<char16_t>& replacements,
                          CharsetConverter* query_converter,
                          CanonOutput& output,
                          Parsed& new_parsed) {
  // Carry over the flag.
  new_parsed.has_opaque_path = base_parsed.has_opaque_path;

  if (base_parsed.has_opaque_path) {
    return ReplacePathURL(base, base_parsed, replacements, &output,
                          &new_parsed);
  }

  RawCanonOutput<1024> utf8;
  URLComponentSource<char> source(base);
  Parsed parsed(base_parsed);
  SetupUTF16OverrideComponents(base, replacements, &utf8, &source, &parsed);
  return DoCanonicalizeNonSpecialURL(source, parsed, query_converter, output,
                                     new_parsed);
}

}  // namespace url
