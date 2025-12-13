/* Based on nsURLParsers.cc from Mozilla
 * -------------------------------------
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "url/third_party/mozilla/url_parse.h"

#include <stdlib.h>

#include <ostream>
#include <string_view>

#include "base/check_op.h"
#include "url/url_parse_internal.h"
#include "url/url_util.h"
#include "url/url_util_internal.h"

namespace url {

std::ostream& operator<<(std::ostream& os, const Parsed& parsed) {
  return os << "{ scheme: " << parsed.scheme
            << ", username: " << parsed.username
            << ", password: " << parsed.password << ", host: " << parsed.host
            << ", port: " << parsed.port << ", path: " << parsed.path
            << ", query: " << parsed.query << ", ref: " << parsed.ref
            << ", has_opaque_path: " << parsed.has_opaque_path << " }";
}

Component MakeRange(size_t begin, size_t end) {
  CHECK_LE(begin, end);
  return Component(base::checked_cast<int>(begin),
                   base::checked_cast<int>(end - begin));
}

namespace {

// Returns true if the given character is a valid digit to use in a port.
inline bool IsPortDigit(char16_t ch) {
  return ch >= '0' && ch <= '9';
}

// Returns the offset of the next authority terminator in the input starting
// from start_offset. If no terminator is found, the return value will be equal
// to `spec.length()`.
template <typename CHAR>
size_t FindNextAuthorityTerminator(std::basic_string_view<CHAR> spec,
                                   size_t start_offset,
                                   ParserMode parser_mode) {
  for (size_t i = start_offset; i < spec.length(); ++i) {
    if (IsAuthorityTerminator(spec[i], parser_mode)) {
      return i;
    }
  }
  return spec.length();  // Not found.
}

template <typename CHAR>
void ParseUserInfo(std::basic_string_view<CHAR> spec,
                   const Component& user,
                   Component* username,
                   Component* password) {
  // Find the first colon in the user section, which separates the username and
  // password.
  int colon_offset = 0;
  while (colon_offset < user.len && spec[user.begin + colon_offset] != ':') {
    ++colon_offset;
  }

  if (colon_offset < user.len) {
    // Found separator: <username>:<password>
    *username = Component(user.begin, colon_offset);
    *password = MakeRange(user.begin + colon_offset + 1, user.begin + user.len);
  } else {
    // No separator, treat everything as the username
    *username = user;
    *password = Component();
  }
}

template <typename CHAR>
void ParseServerInfo(std::basic_string_view<CHAR> spec,
                     const Component& serverinfo,
                     Component* hostname,
                     Component* port_num) {
  if (serverinfo.len == 0) {
    // No server info, host name is empty.
    hostname->reset();
    port_num->reset();
    return;
  }

  // If the host starts with a left-bracket, assume the entire host is an
  // IPv6 literal.  Otherwise, assume none of the host is an IPv6 literal.
  // This assumption will be overridden if we find a right-bracket.
  //
  // Our IPv6 address canonicalization code requires both brackets to exist,
  // but the ability to locate an incomplete address can still be useful.
  int ipv6_terminator = spec[serverinfo.begin] == '[' ? serverinfo.end() : -1;
  int colon = -1;

  // Find the last right-bracket, and the last colon.
  for (int i = serverinfo.begin; i < serverinfo.end(); ++i) {
    switch (spec[i]) {
      case ']':
        ipv6_terminator = i;
        break;
      case ':':
        colon = i;
        break;
    }
  }

  if (colon > ipv6_terminator) {
    // Found a port number: <hostname>:<port>
    *hostname = MakeRange(serverinfo.begin, colon);
    if (hostname->len == 0)
      hostname->reset();
    *port_num = MakeRange(colon + 1, serverinfo.end());
  } else {
    // No port: <hostname>
    *hostname = serverinfo;
    port_num->reset();
  }
}

// Given an already-identified auth section, breaks it into its consituent
// parts. The port number will be parsed and the resulting integer will be
// filled into the given *port variable, or -1 if there is no port number or it
// is invalid.
template <typename CHAR>
void DoParseAuthority(std::basic_string_view<CHAR> spec,
                      const Component& auth,
                      ParserMode parser_mode,
                      Component* username,
                      Component* password,
                      Component* hostname,
                      Component* port_num) {
  DCHECK(auth.is_valid()) << "We should always get an authority";
  if (auth.len == 0) {
    username->reset();
    password->reset();
    if (parser_mode == ParserMode::kSpecialURL) {
      hostname->reset();
    } else {
      // Non-special URLs can have an empty host. The difference between "host
      // is empty" and "host does not exist" matters in the canonicalization
      // phase.
      //
      // Examples:
      // - "git:///" => host is empty (this case).
      // - "git:/" => host does not exist.
      *hostname = Component(auth.begin, 0);
    }
    port_num->reset();
    return;
  }

  // Search backwards for @, which is the separator between the user info and
  // the server info.
  int i = auth.begin + auth.len - 1;
  while (i > auth.begin && spec[i] != '@')
    i--;

  if (spec[i] == '@') {
    // Found user info: <user-info>@<server-info>
    ParseUserInfo(spec, Component(auth.begin, i - auth.begin), username,
                  password);
    ParseServerInfo(spec, MakeRange(i + 1, auth.begin + auth.len), hostname,
                    port_num);
  } else {
    // No user info, everything is server info.
    username->reset();
    password->reset();
    ParseServerInfo(spec, auth, hostname, port_num);
  }
}

// This function returns a pair of the index of the query separator `?` and the
// index of the reference separator `#`.  They are `npos` if they are not found
// in the `spec`.
template <typename CHAR>
inline std::pair<size_t, size_t> FindQueryAndRefParts(
    std::basic_string_view<CHAR> spec,
    const Component& path) {
  size_t path_begin = static_cast<size_t>(path.begin);
  size_t path_end = path.CheckedEnd();
  size_t ref_separator =
      spec.substr(0, path_end).find_first_of('#', path_begin);
  // Only match the query string if it precedes the reference fragment
  size_t len_before_fragment =
      ref_separator == std::basic_string_view<CHAR>::npos ? path_end
                                                          : ref_separator;
  size_t query_separator =
      spec.substr(0, len_before_fragment).find_first_of('?', path_begin);
  return {query_separator, ref_separator};
}

template <typename CHAR>
void ParsePath(std::basic_string_view<CHAR> spec,
               const Component& path,
               Component* filepath,
               Component* query,
               Component* ref) {
  // path = [/]<segment1>/<segment2>/<...>/<segmentN>;<param>?<query>#<ref>
  DCHECK(path.is_valid());

  // Search for first occurrence of either ? or #.
  //  query_separator: Index of the '?'
  //  ref_separator: Index of the '#'
  auto [query_separator, ref_separator] = FindQueryAndRefParts(spec, path);

  // Markers pointing to the character after each of these corresponding
  // components. The code below words from the end back to the beginning,
  // and will update these indices as it finds components that exist.
  size_t file_end, query_end;

  // Ref fragment: from the # to the end of the path.
  size_t path_end = path.CheckedEnd();
  constexpr size_t npos = std::basic_string_view<CHAR>::npos;
  if (ref_separator != npos) {
    file_end = query_end = ref_separator;
    *ref = MakeRange(ref_separator + 1, path_end);
  } else {
    file_end = query_end = path_end;
    ref->reset();
  }

  // Query fragment: everything from the ? to the next boundary (either the end
  // of the path or the ref fragment).
  if (query_separator != npos) {
    file_end = query_separator;
    *query = MakeRange(query_separator + 1, query_end);
  } else {
    query->reset();
  }

  size_t path_begin = static_cast<size_t>(path.begin);
  if (file_end != path_begin) {
    *filepath = MakeRange(path_begin, file_end);
  } else {
    // File path: treat an empty file path as no file path.
    //
    // TODO(crbug.com/40063064): Consider to assign zero-length path component
    // for non-special URLs because a path can be empty in non-special URLs.
    // Currently, we don't have to distinguish between them. There is no visible
    // difference.
    filepath->reset();
  }
}

template <typename CharT>
bool DoExtractScheme(std::basic_string_view<CharT> url, Component* scheme) {
  // Skip leading whitespace and control characters.
  size_t begin = 0;
  while (begin < url.size() && ShouldTrimFromURL(url[begin])) {
    begin++;
  }
  if (begin == url.size()) {
    return false;  // Input is empty or all whitespace.
  }

  // Find the first colon character.
  for (size_t i = begin; i < url.size(); i++) {
    if (url[i] == ':') {
      *scheme = MakeRange(begin, i);
      return true;
    }
  }
  return false;  // No colon found: no scheme
}

// Fills in all members of the Parsed structure except for the scheme.
//
// |spec| is the full spec being parsed, of length |spec_len|.
// |after_scheme| is the character immediately following the scheme (after the
//   colon) where we'll begin parsing.
//
// Compatability data points. I list "host", "path" extracted:
// Input                IE6             Firefox                Us
// -----                --------------  --------------         --------------
// http://foo.com/      "foo.com", "/"  "foo.com", "/"         "foo.com", "/"
// http:foo.com/        "foo.com", "/"  "foo.com", "/"         "foo.com", "/"
// http:/foo.com/       fail(*)         "foo.com", "/"         "foo.com", "/"
// http:\foo.com/       fail(*)         "\foo.com", "/"(fail)  "foo.com", "/"
// http:////foo.com/    "foo.com", "/"  "foo.com", "/"         "foo.com", "/"
//
// (*) Interestingly, although IE fails to load these URLs, its history
// canonicalizer handles them, meaning if you've been to the corresponding
// "http://foo.com/" link, it will be colored.
template <typename CHAR>
void DoParseAfterSpecialScheme(std::basic_string_view<CHAR> spec,
                               int after_scheme,
                               Parsed* parsed) {
  size_t num_slashes = CountConsecutiveSlashesOrBackslashes(spec, after_scheme);
  size_t after_slashes = after_scheme + num_slashes;

  // First split into two main parts, the authority (username, password, host,
  // and port) and the full path (path, query, and reference).
  //
  // Treat everything from `after_slashes` to the next slash (or end of spec) to
  // be the authority. Note that we ignore the number of slashes and treat it as
  // the authority.
  size_t end_auth =
      FindNextAuthorityTerminator(spec, after_slashes, ParserMode::kSpecialURL);

  Component authority = MakeRange(after_slashes, end_auth);
  // Everything starting from the slash to the end is the path.
  Component full_path = MakeRange(end_auth, spec.length());

  // Now parse those two sub-parts.
  DoParseAuthority(spec, authority, ParserMode::kSpecialURL, &parsed->username,
                   &parsed->password, &parsed->host, &parsed->port);
  ParsePath(spec, full_path, &parsed->path, &parsed->query, &parsed->ref);
}

// The main parsing function for standard URLs. Standard URLs have a scheme,
// host, path, etc.
template <typename CharT>
Parsed DoParseStandardUrl(std::basic_string_view<CharT> url) {
  // Strip leading & trailing spaces and control characters.
  auto [begin, url_len] = TrimUrl(url);
  url = url.substr(0, url_len);

  int after_scheme;
  Parsed parsed;
  if (DoExtractScheme(url, &parsed.scheme)) {
    after_scheme = parsed.scheme.end() + 1;  // Skip past the colon.
  } else {
    // Say there's no scheme when there is no colon. We could also say that
    // everything is the scheme. Both would produce an invalid URL, but this way
    // seems less wrong in more cases.
    parsed.scheme.reset();
    after_scheme = base::checked_cast<int>(begin);
  }
  DoParseAfterSpecialScheme(url, after_scheme, &parsed);
  return parsed;
}

template <typename CHAR>
void DoParseAfterNonSpecialScheme(std::basic_string_view<CHAR> spec,
                                  int after_scheme,
                                  Parsed* parsed) {
  // The implementation is similar to `DoParseAfterSpecialScheme()`, but there
  // are many subtle differences. So we have a different function for parsing
  // non-special URLs.

  size_t num_slashes = CountConsecutiveSlashes(spec, after_scheme);

  size_t spec_len = spec.length();
  if (num_slashes >= 2) {
    // Found "//<some data>", looks like an authority section.
    //
    // e.g.
    //   "git://host:8000/path"
    //          ^
    //
    // The state machine transition in the URL Standard is:
    //
    // https://url.spec.whatwg.org/#scheme-state
    // => https://url.spec.whatwg.org/#path-or-authority-state
    // => https://url.spec.whatwg.org/#authority-state
    //
    parsed->has_opaque_path = false;

    size_t after_slashes = after_scheme + 2;

    // First split into two main parts, the authority (username, password, host,
    // and port) and the full path (path, query, and reference).
    //
    // Treat everything from there to the next slash (or end of spec) to be the
    // authority. Note that we ignore the number of slashes and treat it as the
    // authority.
    size_t end_auth = FindNextAuthorityTerminator(spec, after_slashes,
                                                  ParserMode::kNonSpecialURL);
    Component authority = MakeRange(after_slashes, end_auth);

    // Now parse those two sub-parts.
    DoParseAuthority(spec, authority, ParserMode::kNonSpecialURL,
                     &parsed->username, &parsed->password, &parsed->host,
                     &parsed->port);

    // Everything starting from the slash to the end is the path.
    Component full_path = MakeRange(end_auth, spec_len);
    ParsePath(spec, full_path, &parsed->path, &parsed->query, &parsed->ref);
    return;
  }

  if (num_slashes == 1) {
    // Examples:
    //   "git:/path"
    //        ^
    //
    // The state machine transition in the URL Standard is:
    //
    // https://url.spec.whatwg.org/#scheme-state
    // => https://url.spec.whatwg.org/#path-or-authority-state
    // => https://url.spec.whatwg.org/#path-state
    parsed->has_opaque_path = false;
  } else {
    // We didn't found "//" nor "/", so entering into an opaque-path-state.
    //
    // Examples:
    //   "git:opaque path"
    //        ^
    //
    // The state machine transition in the URL Standard is:
    //
    // https://url.spec.whatwg.org/#scheme-state
    // => https://url.spec.whatwg.org/#cannot-be-a-base-url-path-state
    parsed->has_opaque_path = true;
  }

  parsed->username.reset();
  parsed->password.reset();
  // It's important to reset `parsed->host` here to distinguish between "host
  // is empty" and "host doesn't exist".
  parsed->host.reset();
  parsed->port.reset();

  // Everything starting after scheme to the end is the path.
  Component full_path(after_scheme, spec_len - after_scheme);
  ParsePath(spec, full_path, &parsed->path, &parsed->query, &parsed->ref);
}

// The main parsing function for non-special scheme URLs.
template <typename CharT>
Parsed DoParseNonSpecialUrl(std::basic_string_view<CharT> url,
                            bool trim_path_end) {
  // Strip leading & trailing spaces and control characters.
  auto [begin, url_len] = TrimUrl(url, trim_path_end);
  url = url.substr(0, url_len);

  int after_scheme;
  Parsed parsed;
  if (DoExtractScheme(url, &parsed.scheme)) {
    after_scheme = parsed.scheme.end() + 1;  // Skip past the colon.
  } else {
    // Say there's no scheme when there is no colon. We could also say that
    // everything is the scheme. Both would produce an invalid URL, but this way
    // seems less wrong in more cases.
    parsed.scheme.reset();
    after_scheme = 0;
  }
  DoParseAfterNonSpecialScheme(url, after_scheme, &parsed);
  return parsed;
}

template <typename CharT>
Parsed DoParseFileSystemUrl(std::basic_string_view<CharT> url) {
  // Strip leading & trailing spaces and control characters.
  auto [begin, url_len] = TrimUrl(url);

  // Handle empty specs or ones that contain only whitespace or control chars.
  if (begin == url_len) {
    return {};
  }

  size_t inner_start = std::basic_string_view<CharT>::npos;
  // Extract the scheme.  We also handle the case where there is no scheme.
  Parsed parsed;
  if (DoExtractScheme(url.substr(begin, url_len - begin), &parsed.scheme)) {
    // Offset the results since we gave ExtractScheme a substring.
    parsed.scheme.OffsetBy(begin);

    size_t scheme_end = parsed.scheme.CheckedEnd();
    if (scheme_end == url_len - 1) {
      return parsed;
    }

    inner_start = scheme_end + 1;
  } else {
    // No scheme found; that's not valid for filesystem URLs.
    return {};
  }

  Component inner_scheme;
  std::basic_string_view inner_url =
      url.substr(inner_start, url_len - inner_start);
  if (DoExtractScheme(inner_url, &inner_scheme)) {
    // Offset the results since we gave ExtractScheme a substring.
    inner_scheme.OffsetBy(inner_start);

    if (inner_scheme.CheckedEnd() == url_len - 1) {
      return parsed;
    }
  } else {
    // No scheme found; that's not valid for filesystem URLs.
    // The best we can do is return "filesystem://".
    return parsed;
  }

  Parsed inner_parsed;
  if (CompareSchemeComponent(url, inner_scheme, kFileScheme)) {
    // File URLs are special. The static cast is safe because we calculated the
    // size above as the difference of two ints.
    inner_parsed = ParseFileUrl(inner_url);
  } else if (CompareSchemeComponent(url, inner_scheme, kFileSystemScheme)) {
    // Filesystem URLs don't nest.
    return parsed;
  } else if (IsStandard(inner_scheme.AsViewOn(url))) {
    // All "normal" URLs.
    inner_parsed = DoParseStandardUrl(inner_url);
  } else {
    return parsed;
  }

  // All members of inner_parsed need to be offset by inner_start.
  // If we had any scheme that supported nesting more than one level deep,
  // we'd have to recurse into the inner_parsed's inner_parsed when
  // adjusting by inner_start.
  inner_parsed.scheme.begin += inner_start;
  inner_parsed.username.begin += inner_start;
  inner_parsed.password.begin += inner_start;
  inner_parsed.host.begin += inner_start;
  inner_parsed.port.begin += inner_start;
  inner_parsed.query.begin += inner_start;
  inner_parsed.ref.begin += inner_start;
  inner_parsed.path.begin += inner_start;

  // Query and ref move from inner_parsed to parsed.
  parsed.query = inner_parsed.query;
  inner_parsed.query.reset();
  parsed.ref = inner_parsed.ref;
  inner_parsed.ref.reset();

  parsed.set_inner_parsed(inner_parsed);
  if (!inner_parsed.scheme.is_valid() || !inner_parsed.path.is_valid() ||
      inner_parsed.inner_parsed()) {
    return parsed;
  }

  // The path in inner_parsed should start with a slash, then have a filesystem
  // type followed by a slash.  From the first slash up to but excluding the
  // second should be what it keeps; the rest goes to parsed.  If the path ends
  // before the second slash, it's still pretty clear what the user meant, so
  // we'll let that through.
  if (!IsSlashOrBackslash(url[inner_parsed.path.begin])) {
    return parsed;
  }
  int inner_path_end = inner_parsed.path.begin + 1;  // skip the leading slash
  while (static_cast<size_t>(inner_path_end) < url_len &&
         !IsSlashOrBackslash(url[inner_path_end])) {
    ++inner_path_end;
  }
  parsed.path.begin = inner_path_end;
  int new_inner_path_length = inner_path_end - inner_parsed.path.begin;
  parsed.path.len = inner_parsed.path.len - new_inner_path_length;
  parsed.inner_parsed()->path.len = new_inner_path_length;
  return parsed;
}

// Initializes a path URL which is merely a scheme followed by a path. Examples
// include "about:foo" and "javascript:alert('bar');"
template <typename CharT>
Parsed DoParsePathUrl(std::basic_string_view<CharT> url, bool trim_path_end) {
  // Strip leading & trailing spaces and control characters.
  auto [scheme_begin, url_len] = TrimUrl(url, trim_path_end);

  // Handle empty specs or ones that contain only whitespace or control chars.
  if (scheme_begin == url_len) {
    return {};
  }

  size_t path_begin;
  Parsed parsed;
  // Extract the scheme, with the path being everything following. We also
  // handle the case where there is no scheme.
  if (ExtractScheme(url.substr(scheme_begin, url_len - scheme_begin),
                    &parsed.scheme)) {
    // Offset the results since we gave ExtractScheme a substring.
    parsed.scheme.OffsetBy(scheme_begin);
    path_begin = parsed.scheme.CheckedEnd() + 1;
  } else {
    // No scheme case.
    parsed.scheme.reset();
    path_begin = scheme_begin;
  }

  if (path_begin == url_len) {
    return parsed;
  }
  DCHECK_LT(path_begin, url_len);

  ParsePath(url, MakeRange(path_begin, url_len), &parsed.path, &parsed.query,
            &parsed.ref);
  return parsed;
}

template <typename CharT>
Parsed DoParseMailtoUrl(std::basic_string_view<CharT> url) {
  // Strip leading & trailing spaces and control characters.
  auto [begin, url_len] = TrimUrl(url);

  // Handle empty specs or ones that contain only whitespace or control chars.
  if (begin == url_len) {
    return {};
  }

  size_t path_begin = std::basic_string_view<CharT>::npos;
  size_t path_end = std::basic_string_view<CharT>::npos;

  // Extract the scheme, with the path being everything following. We also
  // handle the case where there is no scheme.
  Parsed parsed;
  if (ExtractScheme(url.substr(begin, url_len - begin), &parsed.scheme)) {
    // Offset the results since we gave ExtractScheme a substring.
    parsed.scheme.OffsetBy(begin);

    size_t scheme_end = parsed.scheme.CheckedEnd();
    if (scheme_end != url_len - 1) {
      path_begin = scheme_end + 1;
      path_end = url_len;
    }
  } else {
    // No scheme found, just path.
    parsed.scheme.reset();
    path_begin = begin;
    path_end = url_len;
  }

  // Split [path_begin, path_end) into a path + query.
  for (size_t i = path_begin; i < path_end; ++i) {
    if (url[i] == '?') {
      parsed.query = MakeRange(i + 1, path_end);
      path_end = i;
      break;
    }
  }

  // For compatability with the standard URL parser, treat no path as
  // -1, rather than having a length of 0
  if (path_begin == path_end) {
    parsed.path.reset();
  } else {
    parsed.path = MakeRange(path_begin, path_end);
  }
  return parsed;
}

// Converts a port number in a string to an integer. C++ does not have a simple
// way to convert strings to numbers that works for both `char` and `char16_t`.
// We copy the digits to a small stack buffer (since we know the maximum number
// of digits in a valid port number) that we can use atoi().
template <typename CHAR>
int DoParsePort(std::basic_string_view<CHAR> spec, const Component& component) {
  // Easy success case when there is no port.
  const int kMaxDigits = 5;
  if (component.is_empty())
    return PORT_UNSPECIFIED;

  // Skip over any leading 0s.
  Component digits_comp(component.end(), 0);
  for (int i = 0; i < component.len; i++) {
    if (spec[component.begin + i] != '0') {
      digits_comp = MakeRange(component.begin + i, component.end());
      break;
    }
  }
  if (digits_comp.len == 0)
    return 0;  // All digits were 0.

  // Verify we don't have too many digits (we'll be copying to our buffer so
  // we need to double-check).
  if (digits_comp.len > kMaxDigits)
    return PORT_INVALID;

  // Copy valid digits to the buffer.
  char digits[kMaxDigits + 1];  // +1 for null terminator
  for (int i = 0; i < digits_comp.len; i++) {
    CHAR ch = spec[digits_comp.begin + i];
    if (!IsPortDigit(ch)) {
      // Invalid port digit, fail.
      return PORT_INVALID;
    }
    UNSAFE_TODO(digits[i]) = static_cast<char>(ch);
  }

  // Null-terminate the string and convert to integer. Since we guarantee
  // only digits, atoi's lack of error handling is OK.
  UNSAFE_TODO(digits[digits_comp.len]) = 0;
  int port = atoi(digits);
  if (port > 65535)
    return PORT_INVALID;  // Out of range.
  return port;
}

template <typename CHAR>
void DoExtractFileName(std::basic_string_view<CHAR> spec,
                       const Component& path,
                       Component* file_name) {
  // Handle empty paths: they have no file names.
  if (path.is_empty()) {
    file_name->reset();
    return;
  }

  // Extract the filename range from the path which is between
  // the last slash and the following semicolon.
  int file_end = path.end();
  for (int i = path.end() - 1; i >= path.begin; i--) {
    if (spec[i] == ';') {
      file_end = i;
    } else if (IsSlashOrBackslash(spec[i])) {
      // File name is everything following this character to the end
      *file_name = MakeRange(i + 1, file_end);
      return;
    }
  }

  // No slash found, this means the input was degenerate (generally paths
  // will start with a slash). Let's call everything the file name.
  *file_name = MakeRange(path.begin, file_end);
  return;
}

template <typename CharT>
bool DoExtractQueryKeyValue(std::basic_string_view<CharT> spec,
                            Component* query,
                            Component* key,
                            Component* value) {
  if (!query->is_nonempty())
    return false;

  int start = query->begin;
  int cur = start;
  int end = query->end();

  // We assume the beginning of the input is the beginning of the "key" and we
  // skip to the end of it.
  key->begin = cur;
  while (cur < end && spec[cur] != '&' && spec[cur] != '=')
    cur++;
  key->len = cur - key->begin;

  // Skip the separator after the key (if any).
  if (cur < end && spec[cur] == '=')
    cur++;

  // Find the value part.
  value->begin = cur;
  while (cur < end && spec[cur] != '&')
    cur++;
  value->len = cur - value->begin;

  // Finally skip the next separator if any
  if (cur < end && spec[cur] == '&')
    cur++;

  // Save the new query
  *query = MakeRange(cur, end);
  return true;
}

}  // namespace

COMPONENT_EXPORT(URL)
std::ostream& operator<<(std::ostream& os, const Component& component) {
  return os << '{' << component.begin << ", " << component.len << "}";
}

Parsed::Parsed() = default;

Parsed::Parsed(const Parsed& other)
    : scheme(other.scheme),
      username(other.username),
      password(other.password),
      host(other.host),
      port(other.port),
      path(other.path),
      query(other.query),
      ref(other.ref),
      potentially_dangling_markup(other.potentially_dangling_markup),
      has_opaque_path(other.has_opaque_path) {
  if (other.inner_parsed_)
    set_inner_parsed(*other.inner_parsed_);
}

Parsed& Parsed::operator=(const Parsed& other) {
  if (this != &other) {
    scheme = other.scheme;
    username = other.username;
    password = other.password;
    host = other.host;
    port = other.port;
    path = other.path;
    query = other.query;
    ref = other.ref;
    potentially_dangling_markup = other.potentially_dangling_markup;
    has_opaque_path = other.has_opaque_path;
    if (other.inner_parsed_)
      set_inner_parsed(*other.inner_parsed_);
    else
      clear_inner_parsed();
  }
  return *this;
}

Parsed::~Parsed() {
  delete inner_parsed_;
}

int Parsed::Length() const {
  if (ref.is_valid())
    return ref.end();
  return CountCharactersBefore(REF, false);
}

int Parsed::CountCharactersBefore(ComponentType type,
                                  bool include_delimiter) const {
  if (type == SCHEME)
    return scheme.begin;

  // There will be some characters after the scheme like "://" and we don't
  // know how many. Search forwards for the next thing until we find one.
  int cur = 0;
  if (scheme.is_valid())
    cur = scheme.end() + 1;  // Advance over the ':' at the end of the scheme.

  if (username.is_valid()) {
    if (type <= USERNAME)
      return username.begin;
    cur = username.end() + 1;  // Advance over the '@' or ':' at the end.
  }

  if (password.is_valid()) {
    if (type <= PASSWORD)
      return password.begin;
    cur = password.end() + 1;  // Advance over the '@' at the end.
  }

  if (host.is_valid()) {
    if (type <= HOST)
      return host.begin;
    cur = host.end();
  }

  if (port.is_valid()) {
    if (type < PORT || (type == PORT && include_delimiter))
      return port.begin - 1;  // Back over delimiter.
    if (type == PORT)
      return port.begin;  // Don't want delimiter counted.
    cur = port.end();
  }

  if (path.is_valid()) {
    if (type <= PATH)
      return path.begin;
    cur = path.end();
  }

  if (query.is_valid()) {
    if (type < QUERY || (type == QUERY && include_delimiter))
      return query.begin - 1;  // Back over delimiter.
    if (type == QUERY)
      return query.begin;  // Don't want delimiter counted.
    cur = query.end();
  }

  if (ref.is_valid()) {
    if (type == REF && !include_delimiter)
      return ref.begin;  // Back over delimiter.

    // When there is a ref and we get here, the component we wanted was before
    // this and not found, so we always know the beginning of the ref is right.
    return ref.begin - 1;  // Don't want delimiter counted.
  }

  return cur;
}

Component Parsed::GetContent() const {
  const int begin = CountCharactersBefore(USERNAME, false);
  const int len = Length() - begin;
  // For compatability with the standard URL parser, we treat no content as
  // -1, rather than having a length of 0 (we normally wouldn't care so
  // much for these non-standard URLs).
  return len ? Component(begin, len) : Component();
}

bool ExtractScheme(std::string_view url, Component* scheme) {
  return DoExtractScheme(url, scheme);
}

bool ExtractScheme(std::u16string_view url, Component* scheme) {
  return DoExtractScheme(url, scheme);
}

bool ExtractScheme(const char* url, int url_len, Component* scheme) {
  return DoExtractScheme(std::string_view(url, url_len), scheme);
}

// This handles everything that may be an authority terminator.
//
// URL Standard:
// https://url.spec.whatwg.org/#authority-state
// >> 2. Otherwise, if one of the following is true:
// >>    - c is the EOF code point, U+002F (/), U+003F (?), or U+0023 (#)
// >>    - url is special and c is U+005C (\)
bool IsAuthorityTerminator(char16_t ch, ParserMode parser_mode) {
  if (parser_mode == ParserMode::kSpecialURL) {
    return IsSlashOrBackslash(ch) || ch == '?' || ch == '#';
  }
  return ch == '/' || ch == '?' || ch == '#';
}

void ExtractFileName(std::string_view url,
                     const Component& path,
                     Component* file_name) {
  DoExtractFileName(url, path, file_name);
}

void ExtractFileName(std::u16string_view url,
                     const Component& path,
                     Component* file_name) {
  DoExtractFileName(url, path, file_name);
}

bool ExtractQueryKeyValue(std::string_view url,
                          Component* query,
                          Component* key,
                          Component* value) {
  return DoExtractQueryKeyValue(url, query, key, value);
}

bool ExtractQueryKeyValue(std::u16string_view url,
                          Component* query,
                          Component* key,
                          Component* value) {
  return DoExtractQueryKeyValue(url, query, key, value);
}

void ParseAuthority(const char* spec,
                    const Component& auth,
                    Component* username,
                    Component* password,
                    Component* hostname,
                    Component* port_num) {
  size_t length = auth.is_valid() ? auth.end() : 0;
  DoParseAuthority(std::string_view(spec, length), auth,
                   ParserMode::kSpecialURL, username, password, hostname,
                   port_num);
}

void ParseAuthority(std::string_view spec,
                    const Component& auth,
                    ParserMode parser_mode,
                    Component* username,
                    Component* password,
                    Component* hostname,
                    Component* port_num) {
  DoParseAuthority(spec, auth, parser_mode, username, password, hostname,
                   port_num);
}

void ParseAuthority(std::u16string_view spec,
                    const Component& auth,
                    ParserMode parser_mode,
                    Component* username,
                    Component* password,
                    Component* hostname,
                    Component* port_num) {
  DoParseAuthority(spec, auth, parser_mode, username, password, hostname,
                   port_num);
}

int ParsePort(const char* url, const Component& port) {
  return port.is_empty()
             ? PORT_UNSPECIFIED
             : DoParsePort(
                   std::string_view(url, static_cast<size_t>(port.end())),
                   port);
}

int ParsePort(std::string_view url, const Component& port) {
  return DoParsePort(url, port);
}

int ParsePort(std::u16string_view url, const Component& port) {
  return DoParsePort(url, port);
}

Parsed ParseStandardUrl(std::string_view url) {
  return DoParseStandardUrl(url);
}

Parsed ParseStandardUrl(std::u16string_view url) {
  return DoParseStandardUrl(url);
}

void ParseStandardURL(const char* url, int url_len, Parsed* parsed) {
  CHECK_GE(url_len, 0);
  *parsed = DoParseStandardUrl(std::basic_string_view(url, url_len));
}

Parsed ParseNonSpecialUrl(std::string_view url) {
  return DoParseNonSpecialUrl(url, /*trim_path_end=*/true);
}

Parsed ParseNonSpecialUrl(std::u16string_view url) {
  return DoParseNonSpecialUrl(url, /*trim_path_end=*/true);
}

Parsed ParseNonSpecialUrlInternal(std::string_view url, bool trim_path_end) {
  return DoParseNonSpecialUrl(url, trim_path_end);
}

Parsed ParseNonSpecialUrlInternal(std::u16string_view url, bool trim_path_end) {
  return DoParseNonSpecialUrl(url, trim_path_end);
}

Parsed ParsePathUrl(std::string_view url, bool trim_path_end) {
  return DoParsePathUrl(url, trim_path_end);
}

Parsed ParsePathUrl(std::u16string_view url, bool trim_path_end) {
  return DoParsePathUrl(url, trim_path_end);
}

void ParsePathURL(const char* url,
                  int url_len,
                  bool trim_path_end,
                  Parsed* parsed) {
  CHECK_GE(url_len, 0);
  *parsed = ParsePathUrl(std::string_view(url, url_len), trim_path_end);
}

Parsed ParseFileSystemUrl(std::string_view url) {
  return DoParseFileSystemUrl(url);
}

Parsed ParseFileSystemUrl(std::u16string_view url) {
  return DoParseFileSystemUrl(url);
}

Parsed ParseMailtoUrl(std::string_view url) {
  return DoParseMailtoUrl(url);
}

Parsed ParseMailtoUrl(std::u16string_view url) {
  return DoParseMailtoUrl(url);
}

void ParsePathInternal(std::string_view spec,
                       const Component& path,
                       Component* filepath,
                       Component* query,
                       Component* ref) {
  ParsePath(spec, path, filepath, query, ref);
}

void ParsePathInternal(std::u16string_view spec,
                       const Component& path,
                       Component* filepath,
                       Component* query,
                       Component* ref) {
  ParsePath(spec, path, filepath, query, ref);
}

void ParseAfterSpecialScheme(std::string_view spec,
                             int after_scheme,
                             Parsed* parsed) {
  DoParseAfterSpecialScheme(spec, after_scheme, parsed);
}

void ParseAfterSpecialScheme(std::u16string_view spec,
                             int after_scheme,
                             Parsed* parsed) {
  DoParseAfterSpecialScheme(spec, after_scheme, parsed);
}

void ParseAfterNonSpecialScheme(std::string_view spec,
                                int after_scheme,
                                Parsed* parsed) {
  DoParseAfterNonSpecialScheme(spec, after_scheme, parsed);
}

void ParseAfterNonSpecialScheme(std::u16string_view spec,
                                int after_scheme,
                                Parsed* parsed) {
  DoParseAfterNonSpecialScheme(spec, after_scheme, parsed);
}

}  // namespace url
