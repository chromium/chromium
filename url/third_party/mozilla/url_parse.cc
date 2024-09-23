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

namespace {

// Returns true if the given character is a valid digit to use in a port.
inline bool IsPortDigit(char16_t ch) {
  return ch >= '0' && ch <= '9';
}

// Returns the offset of the next authority terminator in the input starting
// from start_offset. If no terminator is found, the return value will be equal
// to spec_len.
template <typename CHAR>
int FindNextAuthorityTerminator(const CHAR* spec,
                                int start_offset,
                                int spec_len,
                                ParserMode parser_mode) {
  for (int i = start_offset; i < spec_len; i++) {
    if (IsAuthorityTerminator(spec[i], parser_mode)) {
      return i;
    }
  }
  return spec_len;  // Not found.
}

template <typename CHAR>
void ParseUserInfo(const CHAR* spec,
                   const Component& user,
                   Component* username,
                   Component* password) {
  // Find the first colon in the user section, which separates the username and
  // password.
  int colon_offset = 0;
  while (colon_offset < user.len && spec[user.begin + colon_offset] != ':')
    colon_offset++;

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
void ParseServerInfo(const CHAR* spec,
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
  for (int i = serverinfo.begin; i < serverinfo.end(); i++) {
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
void DoParseAuthority(const CHAR* spec,
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

template <typename CHAR>
inline void FindQueryAndRefParts(const CHAR* spec,
                                 const Component& path,
                                 int* query_separator,
                                 int* ref_separator) {
  if constexpr (sizeof(*spec) == 1) {
    // memchr is much faster than any scalar code we can write.
    const CHAR* ptr = spec + path.begin;
    const CHAR* first_hash =
        reinterpret_cast<const CHAR*>(memchr(ptr, '#', path.len));
    size_t len_before_fragment =
        first_hash == nullptr ? path.len : first_hash - ptr;
    const CHAR* first_question =
        reinterpret_cast<const CHAR*>(memchr(ptr, '?', len_before_fragment));
    if (first_hash != nullptr) {
      *ref_separator = first_hash - spec;
    }
    if (first_question != nullptr) {
      *query_separator = first_question - spec;
    }
  } else {
    int path_end = path.begin + path.len;
    for (int i = path.begin; i < path_end; i++) {
      switch (spec[i]) {
        case '?':
          // Only match the query string if it precedes the reference fragment
          // and when we haven't found one already.
          if (*query_separator < 0)
            *query_separator = i;
          break;
        case '#':
          // Record the first # sign only.
          if (*ref_separator < 0) {
            *ref_separator = i;
            return;
          }
          break;
      }
    }
  }
}

template <typename CHAR>
void ParsePath(const CHAR* spec,
               const Component& path,
               Component* filepath,
               Component* query,
               Component* ref) {
  // path = [/]<segment1>/<segment2>/<...>/<segmentN>;<param>?<query>#<ref>
  DCHECK(path.is_valid());

  // Search for first occurrence of either ? or #.
  int query_separator = -1;  // Index of the '?'
  int ref_separator = -1;    // Index of the '#'
  FindQueryAndRefParts(spec, path, &query_separator, &ref_separator);

  // Markers pointing to the character after each of these corresponding
  // components. The code below words from the end back to the beginning,
  // and will update these indices as it finds components that exist.
  int file_end, query_end;

  // Ref fragment: from the # to the end of the path.
  int path_end = path.begin + path.len;
  if (ref_separator >= 0) {
    file_end = query_end = ref_separator;
    *ref = MakeRange(ref_separator + 1, path_end);
  } else {
    file_end = query_end = path_end;
    ref->reset();
  }

  // Query fragment: everything from the ? to the next boundary (either the end
  // of the path or the ref fragment).
  if (query_separator >= 0) {
    file_end = query_separator;
    *query = MakeRange(query_separator + 1, query_end);
  } else {
    query->reset();
  }

  if (file_end != path.begin) {
    *filepath = MakeRange(path.begin, file_end);
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
      *scheme = MakeRange(begin, base::checked_cast<int>(i));
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
void DoParseAfterSpecialScheme(const CHAR* spec,
                               int spec_len,
                               int after_scheme,
                               Parsed* parsed) {
  int num_slashes = CountConsecutiveSlashes(spec, after_scheme, spec_len);
  int after_slashes = after_scheme + num_slashes;

  // First split into two main parts, the authority (username, password, host,
  // and port) and the full path (path, query, and reference).
  //
  // Treat everything from `after_slashes` to the next slash (or end of spec) to
  // be the authority. Note that we ignore the number of slashes and treat it as
  // the authority.
  int end_auth = FindNextAuthorityTerminator(spec, after_slashes, spec_len,
                                             ParserMode::kSpecialURL);

  Component authority(after_slashes, end_auth - after_slashes);
  // Everything starting from the slash to the end is the path.
  Component full_path(end_auth, spec_len - end_auth);

  // Now parse those two sub-parts.
  DoParseAuthority(spec, authority, ParserMode::kSpecialURL, &parsed->username,
                   &parsed->password, &parsed->host, &parsed->port);
  ParsePath(spec, full_path, &parsed->path, &parsed->query, &parsed->ref);
}

// The main parsing function for standard URLs. Standard URLs have a scheme,
// host, path, etc.
template <typename CharT>
Parsed DoParseStandardURL(std::basic_string_view<CharT> url) {
  // Strip leading & trailing spaces and control characters.
  int begin = 0;
  int url_len = base::checked_cast<int>(url.size());
  TrimURL(url.data(), &begin, &url_len);

  int after_scheme;
  Parsed parsed;
  if (DoExtractScheme(url.substr(0, url_len), &parsed.scheme)) {
    after_scheme = parsed.scheme.end() + 1;  // Skip past the colon.
  } else {
    // Say there's no scheme when there is no colon. We could also say that
    // everything is the scheme. Both would produce an invalid URL, but this way
    // seems less wrong in more cases.
    parsed.scheme.reset();
    after_scheme = begin;
  }
  DoParseAfterSpecialScheme(url.data(), url_len, after_scheme, &parsed);
  return parsed;
}

template <typename CHAR>
void DoParseAfterNonSpecialScheme(const CHAR* spec,
                                  int spec_len,
                                  int after_scheme,
                                  Parsed* parsed) {
  // The implementation is similar to `DoParseAfterSpecialScheme()`, but there
  // are many subtle differences. So we have a different function for parsing
  // non-special URLs.

  int num_slashes = CountConsecutiveSlashes(spec, after_scheme, spec_len);

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

    int after_slashes = after_scheme + 2;

    // First split into two main parts, the authority (username, password, host,
    // and port) and the full path (path, query, and reference).
    //
    // Treat everything from there to the next slash (or end of spec) to be the
    // authority. Note that we ignore the number of slashes and treat it as the
    // authority.
    int end_auth = FindNextAuthorityTerminator(spec, after_slashes, spec_len,
                                               ParserMode::kNonSpecialURL);
    Component authority(after_slashes, end_auth - after_slashes);

    // Now parse those two sub-parts.
    DoParseAuthority(spec, authority, ParserMode::kNonSpecialURL,
                     &parsed->username, &parsed->password, &parsed->host,
                     &parsed->port);

    // Everything starting from the slash to the end is the path.
    Component full_path(end_auth, spec_len - end_auth);
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
Parsed DoParseNonSpecialURL(std::basic_string_view<CharT> url,
                            bool trim_path_end) {
  // Strip leading & trailing spaces and control characters.
  int begin = 0;
  int url_len = base::checked_cast<int>(url.size());
  TrimURL(url.data(), &begin, &url_len, trim_path_end);

  int after_scheme;
  Parsed parsed;
  if (DoExtractScheme(url.substr(0, url_len), &parsed.scheme)) {
    after_scheme = parsed.scheme.end() + 1;  // Skip past the colon.
  } else {
    // Say there's no scheme when there is no colon. We could also say that
    // everything is the scheme. Both would produce an invalid URL, but this way
    // seems less wrong in more cases.
    parsed.scheme.reset();
    after_scheme = 0;
  }
  DoParseAfterNonSpecialScheme(url.data(), url_len, after_scheme, &parsed);
  return parsed;
}

template <typename CharT>
Parsed DoParseFileSystemURL(std::basic_string_view<CharT> url) {
  // Strip leading & trailing spaces and control characters.
  int begin = 0;
  int url_len = base::checked_cast<int>(url.size());
  TrimURL(url.data(), &begin, &url_len);

  // Handle empty specs or ones that contain only whitespace or control chars.
  if (begin == url_len) {
    return {};
  }

  int inner_start = -1;
  // Extract the scheme.  We also handle the case where there is no scheme.
  Parsed parsed;
  if (DoExtractScheme(url.substr(begin, url_len - begin), &parsed.scheme)) {
    // Offset the results since we gave ExtractScheme a substring.
    parsed.scheme.begin += begin;

    if (parsed.scheme.end() == url_len - 1) {
      return parsed;
    }

    inner_start = parsed.scheme.end() + 1;
  } else {
    // No scheme found; that's not valid for filesystem URLs.
    return {};
  }

  Component inner_scheme;
  std::basic_string_view inner_url =
      url.substr(inner_start, url_len - inner_start);
  if (DoExtractScheme(inner_url, &inner_scheme)) {
    // Offset the results since we gave ExtractScheme a substring.
    inner_scheme.begin += inner_start;

    if (inner_scheme.end() == url_len - 1) {
      return parsed;
    }
  } else {
    // No scheme found; that's not valid for filesystem URLs.
    // The best we can do is return "filesystem://".
    return parsed;
  }

  Parsed inner_parsed;
  if (CompareSchemeComponent(url.data(), inner_scheme, kFileScheme)) {
    // File URLs are special. The static cast is safe because we calculated the
    // size above as the difference of two ints.
    inner_parsed = ParseFileURL(inner_url);
  } else if (CompareSchemeComponent(url.data(), inner_scheme,
                                    kFileSystemScheme)) {
    // Filesystem URLs don't nest.
    return parsed;
  } else if (IsStandard(url.data(), inner_scheme)) {
    // All "normal" URLs.
    inner_parsed = DoParseStandardURL(inner_url);
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
  while (inner_path_end < url_len && !IsSlashOrBackslash(url[inner_path_end])) {
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
Parsed DoParsePathURL(std::basic_string_view<CharT> url, bool trim_path_end) {
  // Strip leading & trailing spaces and control characters.
  int scheme_begin = 0;
  int url_len = base::checked_cast<int>(url.size());
  TrimURL(url.data(), &scheme_begin, &url_len, trim_path_end);

  // Handle empty specs or ones that contain only whitespace or control chars.
  if (scheme_begin == url_len) {
    return {};
  }

  int path_begin;
  Parsed parsed;
  // Extract the scheme, with the path being everything following. We also
  // handle the case where there is no scheme.
  if (ExtractScheme(&url[scheme_begin], url_len - scheme_begin,
                    &parsed.scheme)) {
    // Offset the results since we gave ExtractScheme a substring.
    parsed.scheme.begin += scheme_begin;
    path_begin = parsed.scheme.end() + 1;
  } else {
    // No scheme case.
    parsed.scheme.reset();
    path_begin = scheme_begin;
  }

  if (path_begin == url_len) {
    return parsed;
  }
  DCHECK_LT(path_begin, url_len);

  ParsePath(url.data(), MakeRange(path_begin, url_len), &parsed.path,
            &parsed.query, &parsed.ref);
  return parsed;
}

template <typename CharT>
Parsed DoParseMailtoURL(std::basic_string_view<CharT> url) {
  // Strip leading & trailing spaces and control characters.
  int begin = 0;
  // TODO(crbug.com/325408566): Transition to size_t and avoid the checked_cast
  // once Component's members are no longer integers.
  int url_len = base::checked_cast<int>(url.size());
  TrimURL(url.data(), &begin, &url_len);

  // Handle empty specs or ones that contain only whitespace or control chars.
  if (begin == url_len) {
    return {};
  }

  int path_begin = -1;
  int path_end = -1;

  // Extract the scheme, with the path being everything following. We also
  // handle the case where there is no scheme.
  Parsed parsed;
  if (ExtractScheme(url.substr(begin, url_len - begin), &parsed.scheme)) {
    // Offset the results since we gave ExtractScheme a substring.
    parsed.scheme.begin += begin;

    if (parsed.scheme.end() != url_len - 1) {
      path_begin = parsed.scheme.end() + 1;
      path_end = url_len;
    }
  } else {
    // No scheme found, just path.
    parsed.scheme.reset();
    path_begin = begin;
    path_end = url_len;
  }

  // Split [path_begin, path_end) into a path + query.
  for (int i = path_begin; i < path_end; ++i) {
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

// Converts a port number in a string to an integer. We'd like to just call
// sscanf but our input is not NULL-terminated, which sscanf requires. Instead,
// we copy the digits to a small stack buffer (since we know the maximum number
// of digits in a valid port number) that we can NULL terminate.
template <typename CHAR>
int DoParsePort(const CHAR* spec, const Component& component) {
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
    digits[i] = static_cast<char>(ch);
  }

  // Null-terminate the string and convert to integer. Since we guarantee
  // only digits, atoi's lack of error handling is OK.
  digits[digits_comp.len] = 0;
  int port = atoi(digits);
  if (port > 65535)
    return PORT_INVALID;  // Out of range.
  return port;
}

template <typename CHAR>
void DoExtractFileName(const CHAR* spec,
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

bool ExtractScheme(const char16_t* url, int url_len, Component* scheme) {
  return DoExtractScheme(std::u16string_view(url, url_len), scheme);
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

void ExtractFileName(const char* url,
                     const Component& path,
                     Component* file_name) {
  DoExtractFileName(url, path, file_name);
}

void ExtractFileName(const char16_t* url,
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
  DoParseAuthority(spec, auth, ParserMode::kSpecialURL, username, password,
                   hostname, port_num);
}

void ParseAuthority(const char16_t* spec,
                    const Component& auth,
                    Component* username,
                    Component* password,
                    Component* hostname,
                    Component* port_num) {
  DoParseAuthority(spec, auth, ParserMode::kSpecialURL, username, password,
                   hostname, port_num);
}

void ParseAuthority(const char* spec,
                    const Component& auth,
                    ParserMode parser_mode,
                    Component* username,
                    Component* password,
                    Component* hostname,
                    Component* port_num) {
  DoParseAuthority(spec, auth, parser_mode, username, password, hostname,
                   port_num);
}

void ParseAuthority(const char16_t* spec,
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
  return DoParsePort(url, port);
}

int ParsePort(const char16_t* url, const Component& port) {
  return DoParsePort(url, port);
}

Parsed ParseStandardURL(std::string_view url) {
  return DoParseStandardURL(url);
}

Parsed ParseStandardURL(std::u16string_view url) {
  return DoParseStandardURL(url);
}

void ParseStandardURL(const char* url, int url_len, Parsed* parsed) {
  CHECK_GE(url_len, 0);
  *parsed = DoParseStandardURL(std::basic_string_view(url, url_len));
}

Parsed ParseNonSpecialURL(std::string_view url) {
  return DoParseNonSpecialURL(url, /*trim_path_end=*/true);
}

Parsed ParseNonSpecialURL(std::u16string_view url) {
  return DoParseNonSpecialURL(url, /*trim_path_end=*/true);
}

Parsed ParseNonSpecialURLInternal(std::string_view url, bool trim_path_end) {
  return DoParseNonSpecialURL(url, trim_path_end);
}

Parsed ParseNonSpecialURLInternal(std::u16string_view url, bool trim_path_end) {
  return DoParseNonSpecialURL(url, trim_path_end);
}

Parsed ParsePathURL(std::string_view url, bool trim_path_end) {
  return DoParsePathURL(url, trim_path_end);
}

Parsed ParsePathURL(std::u16string_view url, bool trim_path_end) {
  return DoParsePathURL(url, trim_path_end);
}

void ParsePathURL(const char* url,
                  int url_len,
                  bool trim_path_end,
                  Parsed* parsed) {
  CHECK_GE(url_len, 0);
  *parsed = ParsePathURL(std::string_view(url, url_len), trim_path_end);
}

Parsed ParseFileSystemURL(std::string_view url) {
  return DoParseFileSystemURL(url);
}

Parsed ParseFileSystemURL(std::u16string_view url) {
  return DoParseFileSystemURL(url);
}

Parsed ParseMailtoURL(std::string_view url) {
  return DoParseMailtoURL(url);
}

Parsed ParseMailtoURL(std::u16string_view url) {
  return DoParseMailtoURL(url);
}

void ParsePathInternal(const char* spec,
                       const Component& path,
                       Component* filepath,
                       Component* query,
                       Component* ref) {
  ParsePath(spec, path, filepath, query, ref);
}

void ParsePathInternal(const char16_t* spec,
                       const Component& path,
                       Component* filepath,
                       Component* query,
                       Component* ref) {
  ParsePath(spec, path, filepath, query, ref);
}

void ParseAfterSpecialScheme(const char* spec,
                             int spec_len,
                             int after_scheme,
                             Parsed* parsed) {
  DoParseAfterSpecialScheme(spec, spec_len, after_scheme, parsed);
}

void ParseAfterSpecialScheme(const char16_t* spec,
                             int spec_len,
                             int after_scheme,
                             Parsed* parsed) {
  DoParseAfterSpecialScheme(spec, spec_len, after_scheme, parsed);
}

void ParseAfterNonSpecialScheme(const char* spec,
                                int spec_len,
                                int after_scheme,
                                Parsed* parsed) {
  DoParseAfterNonSpecialScheme(spec, spec_len, after_scheme, parsed);
}

void ParseAfterNonSpecialScheme(const char16_t* spec,
                                int spec_len,
                                int after_scheme,
                                Parsed* parsed) {
  DoParseAfterNonSpecialScheme(spec, spec_len, after_scheme, parsed);
}

}  // namespace url
