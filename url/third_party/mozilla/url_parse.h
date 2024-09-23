// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_THIRD_PARTY_MOZILLA_URL_PARSE_H_
#define URL_THIRD_PARTY_MOZILLA_URL_PARSE_H_

#include <iosfwd>
#include <string_view>

#include "base/check.h"
#include "base/component_export.h"

namespace url {

// Represents the different behavior between parsing special URLs
// (https://url.spec.whatwg.org/#is-special) and parsing URLs which are not
// special.
//
// Examples:
// - Special URLs: "https://host/path", "ftp://host/path"
// - Non Special URLs: "about:blank", "data:xxx", "git://host/path"
enum class ParserMode { kSpecialURL, kNonSpecialURL };

// Component ------------------------------------------------------------------

// Represents a substring for URL parsing.
struct Component {
  Component() : begin(0), len(-1) {}

  // Normal constructor: takes an offset and a length.
  Component(int b, int l) : begin(b), len(l) {}

  int end() const {
    return begin + len;
  }

  // Returns true if this component is valid, meaning the length is given.
  // Valid components may be empty to record the fact that they exist.
  bool is_valid() const { return len >= 0; }

  // Determine if the component is empty or not. Empty means the length is
  // zero or the component is invalid.
  bool is_empty() const { return len <= 0; }
  bool is_nonempty() const { return len > 0; }

  void reset() {
    begin = 0;
    len = -1;
  }

  bool operator==(const Component& other) const {
    return begin == other.begin && len == other.len;
  }

  // Returns a string_view using `source` as a backend.
  template <typename CharT>
  std::basic_string_view<CharT> as_string_view_on(const CharT* source) const {
    DCHECK(is_valid());
    return std::basic_string_view(&source[begin], len);
  }

  int begin;  // Byte offset in the string of this component.
  int len;    // Will be -1 if the component is unspecified.
};

// Permit printing Components by CHECK macros.
COMPONENT_EXPORT(URL)
std::ostream& operator<<(std::ostream& os, const Component& component);

// Helper that returns a component created with the given begin and ending
// points. The ending point is non-inclusive.
inline Component MakeRange(int begin, int end) {
  return Component(begin, end - begin);
}

// Parsed ---------------------------------------------------------------------

// A structure that holds the identified parts of an input URL. This structure
// does NOT store the URL itself. The caller will have to store the URL text
// and its corresponding Parsed structure separately.
//
// Typical usage would be:
//
//    Parsed parsed;
//    Component scheme;
//    if (!ExtractScheme(url, url_len, &scheme))
//      return I_CAN_NOT_FIND_THE_SCHEME_DUDE;
//
//    if (IsStandardScheme(url, scheme))  // Not provided by this component
//      ParseStandardURL(url, url_len, &parsed);
//    else if (IsFileURL(url, scheme))    // Not provided by this component
//      ParseFileURL(url, url_len, &parsed);
//    else
//      ParsePathURL(url, url_len, &parsed);
//
struct COMPONENT_EXPORT(URL) Parsed {
  // Identifies different components.
  enum ComponentType {
    SCHEME,
    USERNAME,
    PASSWORD,
    HOST,
    PORT,
    PATH,
    QUERY,
    REF,
  };

  // The default constructor is sufficient for the components, but inner_parsed_
  // requires special handling.
  Parsed();
  Parsed(const Parsed&);
  Parsed& operator=(const Parsed&);
  ~Parsed();

  // Returns the length of the URL (the end of the last component).
  //
  // Note that for some invalid, non-canonical URLs, this may not be the length
  // of the string. For example "http://": the parsed structure will only
  // contain an entry for the four-character scheme, and it doesn't know about
  // the "://". For all other last-components, it will return the real length.
  int Length() const;

  // Returns the number of characters before the given component if it exists,
  // or where the component would be if it did exist. This will return the
  // string length if the component would be appended to the end.
  //
  // Note that this can get a little funny for the port, query, and ref
  // components which have a delimiter that is not counted as part of the
  // component. The |include_delimiter| flag controls if you want this counted
  // as part of the component or not when the component exists.
  //
  // This example shows the difference between the two flags for two of these
  // delimited components that is present (the port and query) and one that
  // isn't (the reference). The components that this flag affects are marked
  // with a *.
  //                 0         1         2
  //                 012345678901234567890
  // Example input:  http://foo:80/?query
  //              include_delim=true,  ...=false  ("<-" indicates different)
  //      SCHEME: 0                    0
  //    USERNAME: 5                    5
  //    PASSWORD: 5                    5
  //        HOST: 7                    7
  //       *PORT: 10                   11 <-
  //        PATH: 13                   13
  //      *QUERY: 14                   15 <-
  //        *REF: 20                   20
  //
  int CountCharactersBefore(ComponentType type, bool include_delimiter) const;

  // Scheme without the colon: "http://foo"/ would have a scheme of "http".
  // The length will be -1 if no scheme is specified ("foo.com"), or 0 if there
  // is a colon but no scheme (":foo"). Note that the scheme is not guaranteed
  // to start at the beginning of the string if there are preceeding whitespace
  // or control characters.
  Component scheme;

  // Username. Specified in URLs with an @ sign before the host. See |password|
  Component username;

  // Password. The length will be -1 if unspecified, 0 if specified but empty.
  // Not all URLs with a username have a password, as in "http://me@host/".
  // The password is separated form the username with a colon, as in
  // "http://me:secret@host/"
  Component password;

  // Host name.
  //
  // For non-special URLs, the length will be -1 unless "//" (two consecutive
  // slashes) follows the scheme part. This corresponds to "url's host is null"
  // in URL Standard (https://url.spec.whatwg.org/#concept-url-host).
  //
  // Examples:
  // - "git:/path" => The length is -1.
  //
  // The length can be 0 for non-special URLs when a host is the empty string,
  // but not null.
  //
  // Examples:
  // - "git:///path" => The length is 0.
  Component host;

  // Port number.
  Component port;

  // Path, this is everything following the host name, stopping at the query of
  // ref delimiter (if any). Length will be -1 if unspecified. This includes
  // the preceeding slash, so the path on http://www.google.com/asdf" is
  // "/asdf". As a result, it is impossible to have a 0 length path, it will
  // be -1 in cases like "http://host?foo".
  // Note that we treat backslashes the same as slashes.
  //
  // For non-special URLs which have an empty path, e.g. "git://host", or an
  // empty opaque path, e.g. "git:", path will be -1. See
  // https://crbug.com/1416006.
  Component path;

  // Stuff between the ? and the # after the path. This does not include the
  // preceeding ? character. Length will be -1 if unspecified, 0 if there is
  // a question mark but no query string.
  Component query;

  // Indicated by a #, this is everything following the hash sign (not
  // including it). If there are multiple hash signs, we'll use the last one.
  // Length will be -1 if there is no hash sign, or 0 if there is one but
  // nothing follows it.
  Component ref;

  // The URL spec from the character after the scheme: until the end of the
  // URL, regardless of the scheme. This is mostly useful for 'opaque' non-
  // hierarchical schemes like data: and javascript: as a convient way to get
  // the string with the scheme stripped off.
  Component GetContent() const;

  // True if the URL's source contained a raw `<` character, and whitespace was
  // removed from the URL during parsing
  //
  // TODO(mkwst): Link this to something in a spec if
  // https://github.com/whatwg/url/pull/284 lands.
  bool potentially_dangling_markup = false;

  // True if the URL has an opaque path. See
  // https://url.spec.whatwg.org/#url-opaque-path.
  // Only non-special URLs can have an opaque path.
  //
  // Examples: "data:xxx", "custom:opaque path"
  //
  // Note: Non-special URLs like "data:/xxx" and "custom://host/path" don't have
  // an opaque path because '/' (slash) character follows "scheme:" part.
  bool has_opaque_path = false;

  // This is used for nested URL types, currently only filesystem.  If you
  // parse a filesystem URL, the resulting Parsed will have a nested
  // inner_parsed_ to hold the parsed inner URL's component information.
  // For all other url types [including the inner URL], it will be NULL.
  Parsed* inner_parsed() const {
    return inner_parsed_;
  }

  void set_inner_parsed(const Parsed& inner_parsed) {
    if (!inner_parsed_)
      inner_parsed_ = new Parsed(inner_parsed);
    else
      *inner_parsed_ = inner_parsed;
  }

  void clear_inner_parsed() {
    if (inner_parsed_) {
      delete inner_parsed_;
      inner_parsed_ = nullptr;
    }
  }

 private:
  // This object is owned and managed by this struct.
  Parsed* inner_parsed_ = nullptr;
};

// Permits printing `Parsed` in gtest.
COMPONENT_EXPORT(URL)
std::ostream& operator<<(std::ostream& os, const Parsed& parsed);

// Initialization functions ---------------------------------------------------
//
// These functions parse the given URL, filling in all of the structure's
// components. These functions can not fail, they will always do their best
// at interpreting the input given.
//
// The string length of the URL MUST be specified, we do not check for NULLs
// at any point in the process, and will actually handle embedded NULLs.
//
// IMPORTANT: These functions do NOT hang on to the given pointer or copy it
// in any way. See the comment above the struct.
//
// The 8-bit versions require UTF-8 encoding.

// StandardURL is for when the scheme is known, such as "https:", "ftp:".
// This is defined as "special" in URL Standard.
// See https://url.spec.whatwg.org/#is-special
COMPONENT_EXPORT(URL) Parsed ParseStandardURL(std::string_view url);
COMPONENT_EXPORT(URL) Parsed ParseStandardURL(std::u16string_view url);
// TODO(crbug.com/325408566): Remove once all third-party libraries use the
// overloads above.
COMPONENT_EXPORT(URL)
void ParseStandardURL(const char* url, int url_len, Parsed* parsed);

// Non-special URL is for when the scheme is not special, such as "about:",
// "javascript:". See https://url.spec.whatwg.org/#is-not-special
COMPONENT_EXPORT(URL) Parsed ParseNonSpecialURL(std::string_view url);
COMPONENT_EXPORT(URL) Parsed ParseNonSpecialURL(std::u16string_view url);

// PathURL is for when the scheme is known not to have an authority (host)
// section but that aren't file URLs either. The scheme is parsed, and
// everything after the scheme is considered as the path. This is used for
// things like "about:" and "javascript:"
//
// Historically, this is used to parse non-special URLs, but this should be
// removed after StandardCompliantNonSpecialSchemeURLParsing is enabled by
// default.
COMPONENT_EXPORT(URL)
Parsed ParsePathURL(std::string_view url, bool trim_path_end);
COMPONENT_EXPORT(URL)
Parsed ParsePathURL(std::u16string_view url, bool trim_path_end);
// TODO(crbug.com/325408566): Remove once all third-party libraries use the
// overloads above.
COMPONENT_EXPORT(URL)
void ParsePathURL(const char* url,
                  int url_len,
                  bool trim_path_end,
                  Parsed* parsed);

// FileURL is for file URLs. There are some special rules for interpreting
// these.
COMPONENT_EXPORT(URL) Parsed ParseFileURL(std::string_view url);
COMPONENT_EXPORT(URL) Parsed ParseFileURL(std::u16string_view url);

// Filesystem URLs are structured differently than other URLs.
COMPONENT_EXPORT(URL) Parsed ParseFileSystemURL(std::string_view url);
COMPONENT_EXPORT(URL) Parsed ParseFileSystemURL(std::u16string_view url);

// MailtoURL is for mailto: urls. They are made up scheme,path,query
COMPONENT_EXPORT(URL) Parsed ParseMailtoURL(std::string_view url);
COMPONENT_EXPORT(URL) Parsed ParseMailtoURL(std::u16string_view url);

// Helper functions -----------------------------------------------------------

// Locates the scheme according to the URL  parser's rules. This function is
// designed so the caller can find the scheme and call the correct Init*
// function according to their known scheme types.
//
// It also does not perform any validation on the scheme.
//
// This function will return true if the scheme is found and will put the
// scheme's range into *scheme. False means no scheme could be found. Note
// that a URL beginning with a colon has a scheme, but it is empty, so this
// function will return true but *scheme will = (0,0).
//
// The scheme is found by skipping spaces and control characters at the
// beginning, and taking everything from there to the first colon to be the
// scheme. The character at scheme.end() will be the colon (we may enhance
// this to handle full width colons or something, so don't count on the
// actual character value). The character at scheme.end()+1 will be the
// beginning of the rest of the URL, be it the authority or the path (or the
// end of the string).
//
// The 8-bit version requires UTF-8 encoding.
COMPONENT_EXPORT(URL)
bool ExtractScheme(std::string_view url, Component* scheme);
COMPONENT_EXPORT(URL)
bool ExtractScheme(std::u16string_view url, Component* scheme);
// Deprecated (crbug.com/325408566): Prefer using the overloads above.
COMPONENT_EXPORT(URL)
bool ExtractScheme(const char* url, int url_len, Component* scheme);
COMPONENT_EXPORT(URL)
bool ExtractScheme(const char16_t* url, int url_len, Component* scheme);

// Returns true if ch is a character that terminates the authority segment
// of a URL.
COMPONENT_EXPORT(URL)
bool IsAuthorityTerminator(char16_t ch, ParserMode parser_mode);

// Deprecated. Please pass `ParserMode` explicitly.
//
// These functions are also used in net/third_party code. So removing these
// functions requires several steps.
COMPONENT_EXPORT(URL)
void ParseAuthority(const char* spec,
                    const Component& auth,
                    Component* username,
                    Component* password,
                    Component* hostname,
                    Component* port_num);
COMPONENT_EXPORT(URL)
void ParseAuthority(const char16_t* spec,
                    const Component& auth,
                    Component* username,
                    Component* password,
                    Component* hostname,
                    Component* port_num);

// Does a best effort parse of input `spec`, in range `auth`. If a particular
// component is not found, it will be set to invalid. `ParserMode` is used to
// determine the appropriate authority terminator. See `IsAuthorityTerminator`
// for details.
COMPONENT_EXPORT(URL)
void ParseAuthority(const char* spec,
                    const Component& auth,
                    ParserMode parser_mode,
                    Component* username,
                    Component* password,
                    Component* hostname,
                    Component* port_num);
COMPONENT_EXPORT(URL)
void ParseAuthority(const char16_t* spec,
                    const Component& auth,
                    ParserMode parser_mode,
                    Component* username,
                    Component* password,
                    Component* hostname,
                    Component* port_num);

// Computes the integer port value from the given port component. The port
// component should have been identified by one of the init functions on
// |Parsed| for the given input url.
//
// The return value will be a positive integer between 0 and 64K, or one of
// the two special values below.
enum SpecialPort { PORT_UNSPECIFIED = -1, PORT_INVALID = -2 };
COMPONENT_EXPORT(URL) int ParsePort(const char* url, const Component& port);
COMPONENT_EXPORT(URL)
int ParsePort(const char16_t* url, const Component& port);

// Extracts the range of the file name in the given url. The path must
// already have been computed by the parse function, and the matching URL
// and extracted path are provided to this function. The filename is
// defined as being everything from the last slash/backslash of the path
// to the end of the path.
//
// The file name will be empty if the path is empty or there is nothing
// following the last slash.
//
// The 8-bit version requires UTF-8 encoding.
COMPONENT_EXPORT(URL)
void ExtractFileName(const char* url,
                     const Component& path,
                     Component* file_name);
COMPONENT_EXPORT(URL)
void ExtractFileName(const char16_t* url,
                     const Component& path,
                     Component* file_name);

// Extract the first key/value from the range defined by |*query|. Updates
// |*query| to start at the end of the extracted key/value pair. This is
// designed for use in a loop: you can keep calling it with the same query
// object and it will iterate over all items in the query.
//
// Some key/value pairs may have the key, the value, or both be empty (for
// example, the query string "?&"). These will be returned. Note that an empty
// last parameter "foo.com?" or foo.com?a&" will not be returned, this case
// is the same as "done."
//
// The initial query component should not include the '?' (this is the default
// for parsed URLs).
//
// If no key/value are found |*key| and |*value| will be unchanged and it will
// return false.

COMPONENT_EXPORT(URL)
bool ExtractQueryKeyValue(std::string_view url,
                          Component* query,
                          Component* key,
                          Component* value);
COMPONENT_EXPORT(URL)
bool ExtractQueryKeyValue(std::u16string_view url,
                          Component* query,
                          Component* key,
                          Component* value);

}  // namespace url

#endif  // URL_THIRD_PARTY_MOZILLA_URL_PARSE_H_
