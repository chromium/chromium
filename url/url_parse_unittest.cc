// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/350788890): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "url/third_party/mozilla/url_parse.h"

// Interesting IE file:isms...
//
//  file:/foo/bar              file:///foo/bar
//      The result here seems totally invalid!?!? This isn't UNC.
//
//  file:/
//  file:// or any other number of slashes
//      IE6 doesn't do anything at all if you click on this link. No error:
//      nothing. IE6's history system seems to always color this link, so I'm
//      guessing that it maps internally to the empty URL.
//
//  C:\                        file:///C:/
//  /                          file:///C:/
//  /foo                       file:///C:/foo
//      Interestingly, IE treats "/" as an alias for "c:\", which makes sense,
//      but is weird to think about on Windows.
//
//  file:foo/                  file:foo/  (invalid?!?!?)
//  file:/foo/                 file:///foo/  (invalid?!?!?)
//  file://foo/                file://foo/   (UNC to server "foo")
//  file:///foo/               file:///foo/  (invalid)
//  file:////foo/              file://foo/   (UNC to server "foo")
//      Any more than four slashes is also treated as UNC.
//
//  file:C:/                   file://C:/
//  file:/C:/                  file://C:/
//      The number of slashes after "file:" don't matter if the thing following
//      it looks like an absolute drive path. Also, slashes and backslashes are
//      equally valid here.

namespace url {

namespace {

using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;

// Used for regular URL parse cases.
struct URLParseCase {
  const char* input;

  const char* scheme;
  const char* username;
  const char* password;
  const char* host;
  int port;
  const char* path;
  const char* query;
  const char* ref;
};

// Simpler version of URLParseCase for testing path URLs.
struct PathURLParseCase {
  const char* input;

  const char* scheme;
  const char* path;
};

// Simpler version of URLParseCase for testing mailto URLs.
struct MailtoURLParseCase {
  const char* input;

  const char* scheme;
  const char* path;
  const char* query;
};

// More complicated version of URLParseCase for testing filesystem URLs.
struct FileSystemURLParseCase {
  const char* input;

  const char* inner_scheme;
  const char* inner_username;
  const char* inner_password;
  const char* inner_host;
  int inner_port;
  const char* inner_path;
  const char* path;
  const char* query;
  const char* ref;
};

AssertionResult ComponentMatches(const char* input,
                                 const char* reference,
                                 const Component& component) {
  // Check that the -1 sentinel is the only allowed negative value.
  if (!component.is_valid() && component.len != -1) {
    return AssertionFailure()
           << "-1 is the only allowed negative value for len";
  }

  // Begin should be valid.
  if (component.begin < 0) {
    return AssertionFailure() << "begin must be non-negative";
  }

  // A NULL reference means the component should be nonexistent.
  if (!reference)
    return component.len == -1 ? AssertionSuccess()
                               : AssertionFailure() << "len should be -1";
  if (!component.is_valid())
    return AssertionFailure()
           << "for a non null reference, the component should be valid";

  if (strlen(reference) != static_cast<size_t>(component.len)) {
    return AssertionFailure() << "lengths do not match";
  }

  // Now check the actual characters.
  return strncmp(reference, &input[component.begin], component.len) == 0
             ? AssertionSuccess()
             : AssertionFailure() << "characters do not match";
}

void ExpectInvalidComponent(const Component& component) {
  EXPECT_EQ(0, component.begin);
  EXPECT_EQ(-1, component.len);
}

void URLParseCaseMatches(const URLParseCase& expected, const Parsed& parsed) {
  const char* url = expected.input;
  SCOPED_TRACE(testing::Message()
               << "url: \"" << url << "\", parsed: " << parsed);
  int port = ParsePort(url, parsed.port);
  EXPECT_TRUE(ComponentMatches(url, expected.scheme, parsed.scheme));
  EXPECT_TRUE(ComponentMatches(url, expected.username, parsed.username));
  EXPECT_TRUE(ComponentMatches(url, expected.password, parsed.password));
  EXPECT_TRUE(ComponentMatches(url, expected.host, parsed.host));
  EXPECT_EQ(expected.port, port);
  EXPECT_TRUE(ComponentMatches(url, expected.path, parsed.path));
  EXPECT_TRUE(ComponentMatches(url, expected.query, parsed.query));
  EXPECT_TRUE(ComponentMatches(url, expected.ref, parsed.ref));
}

// Parsed ----------------------------------------------------------------------

TEST(URLParser, Length) {
  const char* length_cases[] = {
      // One with everything in it.
    "http://user:pass@host:99/foo?bar#baz",
      // One with nothing in it.
    "",
      // Working backwards, let's start taking off stuff from the full one.
    "http://user:pass@host:99/foo?bar#",
    "http://user:pass@host:99/foo?bar",
    "http://user:pass@host:99/foo?",
    "http://user:pass@host:99/foo",
    "http://user:pass@host:99/",
    "http://user:pass@host:99",
    "http://user:pass@host:",
    "http://user:pass@host",
    "http://host",
    "http://user@",
    "http:",
  };
  for (const char* length_case : length_cases) {
    int true_length = static_cast<int>(strlen(length_case));
    Parsed parsed = ParseStandardURL(length_case);

    EXPECT_EQ(true_length, parsed.Length());
  }
}

TEST(URLParser, CountCharactersBefore) {
  struct CountCase {
    const char* url;
    Parsed::ComponentType component;
    bool include_delimiter;
    int expected_count;
  } count_cases[] = {
  // Test each possibility in the case where all components are present.
  //    0         1         2
  //    0123456789012345678901
    {"http://u:p@h:8/p?q#r", Parsed::SCHEME, true, 0},
    {"http://u:p@h:8/p?q#r", Parsed::SCHEME, false, 0},
    {"http://u:p@h:8/p?q#r", Parsed::USERNAME, true, 7},
    {"http://u:p@h:8/p?q#r", Parsed::USERNAME, false, 7},
    {"http://u:p@h:8/p?q#r", Parsed::PASSWORD, true, 9},
    {"http://u:p@h:8/p?q#r", Parsed::PASSWORD, false, 9},
    {"http://u:p@h:8/p?q#r", Parsed::HOST, true, 11},
    {"http://u:p@h:8/p?q#r", Parsed::HOST, false, 11},
    {"http://u:p@h:8/p?q#r", Parsed::PORT, true, 12},
    {"http://u:p@h:8/p?q#r", Parsed::PORT, false, 13},
    {"http://u:p@h:8/p?q#r", Parsed::PATH, false, 14},
    {"http://u:p@h:8/p?q#r", Parsed::PATH, true, 14},
    {"http://u:p@h:8/p?q#r", Parsed::QUERY, true, 16},
    {"http://u:p@h:8/p?q#r", Parsed::QUERY, false, 17},
    {"http://u:p@h:8/p?q#r", Parsed::REF, true, 18},
    {"http://u:p@h:8/p?q#r", Parsed::REF, false, 19},
      // Now test when the requested component is missing.
    {"http://u:p@h:8/p?", Parsed::REF, true, 17},
    {"http://u:p@h:8/p?q", Parsed::REF, true, 18},
    {"http://u:p@h:8/p#r", Parsed::QUERY, true, 16},
    {"http://u:p@h:8#r", Parsed::PATH, true, 14},
    {"http://u:p@h/", Parsed::PORT, true, 12},
    {"http://u:p@/", Parsed::HOST, true, 11},
      // This case is a little weird. It will report that the password would
      // start where the host begins. This is arguably correct, although you
      // could also argue that it should start at the '@' sign. Doing it
      // starting with the '@' sign is actually harder, so we don't bother.
    {"http://u@h/", Parsed::PASSWORD, true, 9},
    {"http://h/", Parsed::USERNAME, true, 7},
    {"http:", Parsed::USERNAME, true, 5},
    {"", Parsed::SCHEME, true, 0},
      // Make sure a random component still works when there's nothing there.
    {"", Parsed::REF, true, 0},
      // File URLs are special with no host, so we test those.
    {"file:///c:/foo", Parsed::USERNAME, true, 7},
    {"file:///c:/foo", Parsed::PASSWORD, true, 7},
    {"file:///c:/foo", Parsed::HOST, true, 7},
    {"file:///c:/foo", Parsed::PATH, true, 7},
  };
  for (const auto& count_case : count_cases) {
    // Simple test to distinguish file and standard URLs.
    Parsed parsed = count_case.url[0] == 'f' ? ParseFileURL(count_case.url)
                                             : ParseStandardURL(count_case.url);

    int chars_before = parsed.CountCharactersBefore(
        count_case.component, count_case.include_delimiter);
    EXPECT_EQ(count_case.expected_count, chars_before);
  }
}

// Standard --------------------------------------------------------------------

// clang-format off
// Input                               Scheme  Usrname  Passwd     Host         Port Path       Query        Ref
// ------------------------------------ ------- -------- ---------- ------------ --- ---------- ------------ -----
static URLParseCase cases[] = {
  // Regular URL with all the parts
{"http://user:pass@foo:21/bar;par?b#c", "http", "user",  "pass",    "foo",       21, "/bar;par","b",          "c"},

  // Known schemes should lean towards authority identification
{"http:foo.com",                        "http", nullptr, nullptr,   "foo.com",    -1, nullptr,   nullptr,     nullptr},

  // Spaces!
{"\t   :foo.com   \n",                  "",     nullptr, nullptr,   "foo.com",    -1, nullptr,   nullptr,     nullptr},
{" foo.com  ",                          nullptr,nullptr, nullptr,   "foo.com",    -1, nullptr,   nullptr,     nullptr},
{"a:\t foo.com",                        "a",    nullptr, nullptr,   "\t foo.com", -1, nullptr,   nullptr,     nullptr},
{"http://f:21/ b ? d # e ",             "http", nullptr, nullptr,   "f",          21, "/ b ",    " d ",       " e"},

  // Invalid port numbers should be identified and turned into -2, empty port
  // numbers should be -1. Spaces aren't allowed in port numbers
{"http://f:/c",                         "http", nullptr, nullptr,   "f",          -1, "/c",      nullptr,     nullptr},
{"http://f:0/c",                        "http", nullptr, nullptr,   "f",           0, "/c",      nullptr,     nullptr},
{"http://f:00000000000000/c",           "http", nullptr, nullptr,   "f",           0, "/c",      nullptr,     nullptr},
{"http://f:00000000000000000000080/c",  "http", nullptr, nullptr,   "f",          80, "/c",      nullptr,     nullptr},
{"http://f:b/c",                        "http", nullptr, nullptr,   "f",          -2, "/c",      nullptr,     nullptr},
{"http://f: /c",                        "http", nullptr, nullptr,   "f",          -2, "/c",      nullptr,     nullptr},
{"http://f:\n/c",                       "http", nullptr, nullptr,   "f",          -2, "/c",      nullptr,     nullptr},
{"http://f:fifty-two/c",                "http", nullptr, nullptr,   "f",          -2, "/c",      nullptr,     nullptr},
{"http://f:999999/c",                   "http", nullptr, nullptr,   "f",          -2, "/c",      nullptr,     nullptr},
{"http://f: 21 / b ? d # e ",           "http", nullptr, nullptr,   "f",          -2, "/ b ",    " d ",       " e"},

  // Creative URLs missing key elements
{"",                                    nullptr,nullptr, nullptr,   nullptr,      -1, nullptr,   nullptr,     nullptr},
{"  \t",                                nullptr,nullptr, nullptr,   nullptr,      -1, nullptr,   nullptr,     nullptr},
{":foo.com/",                           "",     nullptr, nullptr,   "foo.com",    -1, "/",       nullptr,     nullptr},
{":foo.com\\",                          "",     nullptr, nullptr,   "foo.com",    -1, "\\",      nullptr,     nullptr},
{":",                                   "",     nullptr, nullptr,   nullptr,      -1, nullptr,   nullptr,     nullptr},
{":a",                                  "",     nullptr, nullptr,   "a",          -1, nullptr,   nullptr,     nullptr},
{":/",                                  "",     nullptr, nullptr,   nullptr,      -1, nullptr,   nullptr,     nullptr},
{":\\",                                 "",     nullptr, nullptr,   nullptr,      -1, nullptr,   nullptr,     nullptr},
{":#",                                  "",     nullptr, nullptr,   nullptr,      -1, nullptr,   nullptr,     ""},
{"#",                                   nullptr,nullptr, nullptr,   nullptr,      -1, nullptr,   nullptr,     ""},
{"#/",                                  nullptr,nullptr, nullptr,   nullptr,      -1, nullptr,   nullptr,     "/"},
{"#\\",                                 nullptr,nullptr, nullptr,   nullptr,      -1, nullptr,   nullptr,     "\\"},
{"#;?",                                 nullptr,nullptr, nullptr,   nullptr,      -1, nullptr,   nullptr,     ";?"},
{"?",                                   nullptr,nullptr, nullptr,   nullptr,      -1, nullptr,   "",          nullptr},
{"/",                                   nullptr,nullptr, nullptr,   nullptr,      -1, nullptr,   nullptr,     nullptr},
{":23",                                 "",     nullptr, nullptr,   "23",         -1, nullptr,   nullptr,     nullptr},
{"/:23",                                "/",    nullptr, nullptr,   "23",         -1, nullptr,   nullptr,     nullptr},
{"//",                                  nullptr,nullptr, nullptr,   nullptr,      -1, nullptr,   nullptr,     nullptr},
{"::",                                  "",     nullptr, nullptr,   nullptr,      -1, nullptr,   nullptr,     nullptr},
{"::23",                                "",     nullptr, nullptr,   nullptr,      23, nullptr,   nullptr,     nullptr},
{"foo://",                              "foo",  nullptr, nullptr,   nullptr,      -1, nullptr,   nullptr,     nullptr},

  // Username/passwords and things that look like them
{"http://a:b@c:29/d",                   "http", "a",    "b",       "c",           29, "/d",      nullptr,     nullptr},
{"http::@c:29",                         "http", "",     "",        "c",           29, nullptr,   nullptr,     nullptr},
  // ... "]" in the password field isn't allowed, but we tolerate it here...
{"http://&a:foo(b]c@d:2/",              "http", "&a",   "foo(b]c", "d",            2, "/",       nullptr,     nullptr},
{"http://::@c@d:2",                     "http", "",     ":@c",     "d",            2, nullptr,   nullptr,     nullptr},
{"http://foo.com:b@d/",                 "http", "foo.com","b",     "d",           -1, "/",       nullptr,     nullptr},

{"http://foo.com/\\@",                  "http", nullptr, nullptr,   "foo.com",    -1, "/\\@",    nullptr,     nullptr},
{"http:\\\\foo.com\\",                  "http", nullptr, nullptr,   "foo.com",    -1, "\\",      nullptr,     nullptr},
{"http:\\\\a\\b:c\\d@foo.com\\",        "http", nullptr, nullptr,   "a",          -1, "\\b:c\\d@foo.com\\", nullptr,nullptr},

  // Tolerate different numbers of slashes.
{"foo:/",                               "foo",  nullptr, nullptr,   nullptr,      -1, nullptr,   nullptr,     nullptr},
{"foo:/bar.com/",                       "foo",  nullptr, nullptr,   "bar.com",    -1, "/",       nullptr,     nullptr},
{"foo://///////",                       "foo",  nullptr, nullptr,   nullptr,      -1, nullptr,   nullptr,     nullptr},
{"foo://///////bar.com/",               "foo",  nullptr, nullptr,   "bar.com",    -1, "/",       nullptr,     nullptr},
{"foo:////://///",                      "foo",  nullptr, nullptr,   nullptr,      -1, "/////",   nullptr,     nullptr},

  // Raw file paths on Windows aren't handled by the parser.
{"c:/foo",                              "c",    nullptr, nullptr,   "foo",        -1, nullptr,   nullptr,     nullptr},
{"//foo/bar",                           nullptr,nullptr, nullptr,   "foo",        -1, "/bar",    nullptr,     nullptr},

  // Use the first question mark for the query and the ref.
{"http://foo/path;a??e#f#g",            "http", nullptr, nullptr,   "foo",        -1, "/path;a", "?e",        "f#g"},
{"http://foo/abcd?efgh?ijkl",           "http", nullptr, nullptr,   "foo",        -1, "/abcd",   "efgh?ijkl", nullptr},
{"http://foo/abcd#foo?bar",             "http", nullptr, nullptr,   "foo",        -1, "/abcd",   nullptr,     "foo?bar"},

  // IPv6, check also interesting uses of colons.
{"[61:24:74]:98",                       "[61",  nullptr, nullptr,   "24:74]",     98, nullptr,   nullptr,     nullptr},
{"http://[61:27]:98",                   "http", nullptr, nullptr,   "[61:27]",    98, nullptr,   nullptr,     nullptr},
{"http:[61:27]/:foo",                   "http", nullptr, nullptr,   "[61:27]",    -1, "/:foo",   nullptr,     nullptr},
{"http://[1::2]:3:4",                   "http", nullptr, nullptr,   "[1::2]:3",    4, nullptr,   nullptr,     nullptr},

  // Partially-complete IPv6 literals, and related cases.
{"http://2001::1",                      "http", nullptr, nullptr,   "2001:",       1, nullptr,   nullptr,     nullptr},
{"http://[2001::1",                     "http", nullptr, nullptr,   "[2001::1",   -1, nullptr,   nullptr,     nullptr},
{"http://2001::1]",                     "http", nullptr, nullptr,   "2001::1]",   -1, nullptr,   nullptr,     nullptr},
{"http://2001::1]:80",                  "http", nullptr, nullptr,   "2001::1]",   80, nullptr,   nullptr,     nullptr},
{"http://[2001::1]",                    "http", nullptr, nullptr,   "[2001::1]",  -1, nullptr,   nullptr,     nullptr},
{"http://[2001::1]:80",                 "http", nullptr, nullptr,   "[2001::1]",  80, nullptr,   nullptr,     nullptr},
{"http://[[::]]",                       "http", nullptr, nullptr,   "[[::]]",     -1, nullptr,   nullptr,     nullptr},

};
// clang-format on

TEST(URLParser, Standard) {
  // Declared outside for loop to try to catch cases in init() where we forget
  // to reset something that is reset by the constructor.
  for (const auto& i : cases) {
    Parsed parsed = ParseStandardURL(i.input);
    URLParseCaseMatches(i, parsed);
  }
}

// PathURL --------------------------------------------------------------------

// Various incarnations of path URLs.
// clang-format off
static PathURLParseCase path_cases[] = {
{"",                                        nullptr,       nullptr},
{":",                                       "",            nullptr},
{":/",                                      "",            "/"},
{"/",                                       nullptr,       "/"},
{" This is \\interesting// \t",             nullptr,       "This is \\interesting// \t"},
{"about:",                                  "about",       nullptr},
{"about:blank",                             "about",       "blank"},
{"  about: blank ",                         "about",       " blank "},
{"javascript :alert(\"He:/l\\l#o?foo\"); ", "javascript ", "alert(\"He:/l\\l#o?foo\"); "},
};
// clang-format on

TEST(URLParser, PathURL) {
  // Declared outside for loop to try to catch cases in init() where we forget
  // to reset something that is reset by the constructor.
  for (size_t i = 0; i < std::size(path_cases); i++) {
    const char* url = path_cases[i].input;
    Parsed parsed = ParsePathURL(url, false);

    EXPECT_TRUE(ComponentMatches(url, path_cases[i].scheme, parsed.scheme))
        << i;
    EXPECT_TRUE(ComponentMatches(url, path_cases[i].path, parsed.GetContent()))
        << i;

    // The remaining components are never used for path URLs.
    ExpectInvalidComponent(parsed.username);
    ExpectInvalidComponent(parsed.password);
    ExpectInvalidComponent(parsed.host);
    ExpectInvalidComponent(parsed.port);
  }
}

// Various incarnations of file URLs.
// clang-format off
static URLParseCase file_cases[] = {
#ifdef WIN32
{"file:server",              "file", nullptr, nullptr, "server", -1, nullptr,       nullptr, nullptr},
{"  file: server  \t",       "file", nullptr, nullptr, " server",-1, nullptr,       nullptr, nullptr},
{"FiLe:c|",                  "FiLe", nullptr, nullptr, nullptr,  -1, "c|",          nullptr, nullptr},
{"FILE:/\\\\/server/file",   "FILE", nullptr, nullptr, "server", -1, "/file",       nullptr, nullptr},
{"file://server/",           "file", nullptr, nullptr, "server", -1, "/",           nullptr, nullptr},
{"file://localhost/c:/",     "file", nullptr, nullptr, "localhost", -1, "/c:/",     nullptr, nullptr},
{"file://127.0.0.1/c|\\",    "file", nullptr, nullptr, "127.0.0.1", -1, "/c|\\",    nullptr, nullptr},
{"file:/",                   "file", nullptr, nullptr, nullptr,  -1, nullptr,       nullptr, nullptr},
{"file:",                    "file", nullptr, nullptr, nullptr,  -1, nullptr,       nullptr, nullptr},
  // If there is a Windows drive letter, treat any number of slashes as the
  // path part.
{"file:c:\\fo\\b",           "file", nullptr, nullptr, nullptr,  -1, "c:\\fo\\b",   nullptr, nullptr},
{"file:/c:\\foo/bar",        "file", nullptr, nullptr, nullptr,  -1, "/c:\\foo/bar",nullptr, nullptr},
{"file://c:/f\\b",           "file", nullptr, nullptr, nullptr,  -1, "/c:/f\\b",    nullptr, nullptr},
{"file:///C:/foo",           "file", nullptr, nullptr, nullptr,  -1, "/C:/foo",     nullptr, nullptr},
{"file://///\\/\\/c:\\f\\b", "file", nullptr, nullptr, nullptr,  -1, "/c:\\f\\b",   nullptr, nullptr},
  // If there is not a drive letter, we should treat is as UNC EXCEPT for
  // three slashes, which we treat as a Unix style path.
{"file:server/file",         "file", nullptr, nullptr, "server", -1, "/file",       nullptr, nullptr},
{"file:/server/file",        "file", nullptr, nullptr, "server", -1, "/file",       nullptr, nullptr},
{"file://server/file",       "file", nullptr, nullptr, "server", -1, "/file",       nullptr, nullptr},
{"file:///server/file",      "file", nullptr, nullptr, nullptr,  -1, "/server/file",nullptr, nullptr},
{"file://\\server/file",     "file", nullptr, nullptr, nullptr,  -1, "\\server/file",nullptr, nullptr},
{"file:////server/file",     "file", nullptr, nullptr, "server", -1, "/file",       nullptr, nullptr},
  // Queries and refs are valid for file URLs as well.
{"file:///C:/foo.html?#",   "file", nullptr, nullptr,  nullptr,  -1, "/C:/foo.html",  "",   ""},
{"file:///C:/foo.html?query=yes#ref", "file", nullptr, nullptr, nullptr, -1, "/C:/foo.html", "query=yes", "ref"},
#else  // WIN32
  // No slashes.
  {"file:",                    "file", nullptr, nullptr, nullptr,   -1, nullptr,          nullptr, nullptr},
  {"file:path",                "file", nullptr, nullptr, nullptr,   -1, "path",           nullptr, nullptr},
  {"file:path/",               "file", nullptr, nullptr, nullptr,   -1, "path/",          nullptr, nullptr},
  {"file:path/f.txt",          "file", nullptr, nullptr, nullptr,   -1, "path/f.txt",     nullptr, nullptr},
  // One slash.
  {"file:/",                   "file", nullptr, nullptr, nullptr,   -1, "/",              nullptr, nullptr},
  {"file:/path",               "file", nullptr, nullptr, nullptr,   -1, "/path",          nullptr, nullptr},
  {"file:/path/",              "file", nullptr, nullptr, nullptr,   -1, "/path/",         nullptr, nullptr},
  {"file:/path/f.txt",         "file", nullptr, nullptr, nullptr,   -1, "/path/f.txt",    nullptr, nullptr},
  // Two slashes.
  {"file://",                  "file", nullptr, nullptr, nullptr,   -1, nullptr,          nullptr, nullptr},
  {"file://server",            "file", nullptr, nullptr, "server",  -1, nullptr,          nullptr, nullptr},
  {"file://server/",           "file", nullptr, nullptr, "server",  -1, "/",              nullptr, nullptr},
  {"file://server/f.txt",      "file", nullptr, nullptr, "server",  -1, "/f.txt",         nullptr, nullptr},
  // Three slashes.
  {"file:///",                 "file", nullptr, nullptr, nullptr,   -1, "/",              nullptr, nullptr},
  {"file:///path",             "file", nullptr, nullptr, nullptr,   -1, "/path",          nullptr, nullptr},
  {"file:///path/",            "file", nullptr, nullptr, nullptr,   -1, "/path/",         nullptr, nullptr},
  {"file:///path/f.txt",       "file", nullptr, nullptr, nullptr,   -1, "/path/f.txt",    nullptr, nullptr},
  // More than three slashes.
  {"file:////",                "file", nullptr, nullptr, nullptr,   -1, "/",              nullptr, nullptr},
  {"file:////path",            "file", nullptr, nullptr, nullptr,   -1, "/path",          nullptr, nullptr},
  {"file:////path/",           "file", nullptr, nullptr, nullptr,   -1, "/path/",         nullptr, nullptr},
  {"file:////path/f.txt",      "file", nullptr, nullptr, nullptr,   -1, "/path/f.txt",    nullptr, nullptr},
  // Schemeless URLs
  {"path/f.txt",               nullptr,nullptr, nullptr, nullptr,    -1, "path/f.txt",    nullptr, nullptr},
  {"path:80/f.txt",            "path", nullptr, nullptr, nullptr,    -1, "80/f.txt",      nullptr, nullptr},
  {"path/f.txt:80",            "path/f.txt",nullptr, nullptr, nullptr,-1,"80",            nullptr, nullptr}, // Wrong.
  {"/path/f.txt",              nullptr,nullptr, nullptr, nullptr,    -1, "/path/f.txt",   nullptr, nullptr},
  {"/path:80/f.txt",           nullptr,nullptr, nullptr, nullptr,    -1, "/path:80/f.txt",nullptr, nullptr},
  {"/path/f.txt:80",           nullptr,nullptr, nullptr, nullptr,    -1, "/path/f.txt:80",nullptr, nullptr},
  {"//server/f.txt",           nullptr,nullptr, nullptr, "server",   -1, "/f.txt",        nullptr, nullptr},
  {"//server:80/f.txt",        nullptr,nullptr, nullptr, "server:80",-1, "/f.txt",        nullptr, nullptr},
  {"//server/f.txt:80",        nullptr,nullptr, nullptr, "server",   -1, "/f.txt:80",     nullptr, nullptr},
  {"///path/f.txt",            nullptr,nullptr, nullptr, nullptr,    -1, "/path/f.txt",   nullptr, nullptr},
  {"///path:80/f.txt",         nullptr,nullptr, nullptr, nullptr,    -1, "/path:80/f.txt",nullptr, nullptr},
  {"///path/f.txt:80",         nullptr,nullptr, nullptr, nullptr,    -1, "/path/f.txt:80",nullptr, nullptr},
  {"////path/f.txt",           nullptr,nullptr, nullptr, nullptr,    -1, "/path/f.txt",   nullptr, nullptr},
  {"////path:80/f.txt",        nullptr,nullptr, nullptr, nullptr,    -1, "/path:80/f.txt",nullptr, nullptr},
  {"////path/f.txt:80",        nullptr,nullptr, nullptr, nullptr,    -1, "/path/f.txt:80",nullptr, nullptr},
  // Queries and refs are valid for file URLs as well.
  {"file:///foo.html?#",       "file", nullptr, nullptr, nullptr,    -1, "/foo.html",     "",   ""},
  {"file:///foo.html?q=y#ref", "file", nullptr, nullptr, nullptr,    -1, "/foo.html",    "q=y", "ref"},
#endif  // WIN32
};
// clang-format on

TEST(URLParser, ParseFileURL) {
  // Declared outside for loop to try to catch cases in init() where we forget
  // to reset something that is reset by the construtor.
  for (const auto& file_case : file_cases) {
    Parsed parsed = ParseFileURL(file_case.input);
    URLParseCaseMatches(file_case, parsed);
    EXPECT_FALSE(parsed.has_opaque_path);
  }
}

TEST(URLParser, ExtractFileName) {
  struct FileCase {
    const char* input;
    const char* expected;
  } extract_cases[] = {
      {"http://www.google.com", nullptr},
      {"http://www.google.com/", ""},
      {"http://www.google.com/search", "search"},
      {"http://www.google.com/search/", ""},
      {"http://www.google.com/foo/bar.html?baz=22", "bar.html"},
      {"http://www.google.com/foo/bar.html#ref", "bar.html"},
      {"http://www.google.com/search/;param", ""},
      {"http://www.google.com/foo/bar.html;param#ref", "bar.html"},
      {"http://www.google.com/foo/bar.html;foo;param#ref", "bar.html"},
      {"http://www.google.com/foo/bar.html?query#ref", "bar.html"},
      {"http://www.google.com/foo;/bar.html", "bar.html"},
      {"http://www.google.com/foo;/", ""},
      {"http://www.google.com/foo;", "foo"},
      {"http://www.google.com/;", ""},
      {"http://www.google.com/foo;bar;html", "foo"},
  };

  for (const auto& extract_case : extract_cases) {
    const char* url = extract_case.input;
    Parsed parsed = ParseStandardURL(url);

    Component file_name;
    ExtractFileName(url, parsed.path, &file_name);

    EXPECT_TRUE(ComponentMatches(url, extract_case.expected, file_name));
  }
}

// Returns true if the parameter with index |parameter| in the given URL's
// query string. The expected key can be NULL to indicate no such key index
// should exist. The parameter number is 1-based.
static bool NthParameterIs(const char* url,
                           int parameter,
                           const char* expected_key,
                           const char* expected_value) {
  Parsed parsed = ParseStandardURL(url);

  Component query = parsed.query;

  for (int i = 1; i <= parameter; i++) {
    Component key, value;
    if (!ExtractQueryKeyValue(url, &query, &key, &value)) {
      if (parameter >= i && !expected_key)
        return true;  // Expected nonexistent key, got one.
      return false;  // Not enough keys.
    }

    if (i == parameter) {
      if (!expected_key)
        return false;

      if (strncmp(&url[key.begin], expected_key, key.len) != 0)
        return false;
      if (strncmp(&url[value.begin], expected_value, value.len) != 0)
        return false;
      return true;
    }
  }
  return expected_key == nullptr;  // We didn't find that many parameters.
}

TEST(URLParser, ExtractQueryKeyValue) {
  EXPECT_TRUE(NthParameterIs("http://www.google.com", 1, nullptr, nullptr));

  // Basic case.
  char a[] = "http://www.google.com?arg1=1&arg2=2&bar";
  EXPECT_TRUE(NthParameterIs(a, 1, "arg1", "1"));
  EXPECT_TRUE(NthParameterIs(a, 2, "arg2", "2"));
  EXPECT_TRUE(NthParameterIs(a, 3, "bar", ""));
  EXPECT_TRUE(NthParameterIs(a, 4, nullptr, nullptr));

  // Empty param at the end.
  char b[] = "http://www.google.com?foo=bar&";
  EXPECT_TRUE(NthParameterIs(b, 1, "foo", "bar"));
  EXPECT_TRUE(NthParameterIs(b, 2, nullptr, nullptr));

  // Empty param at the beginning.
  char c[] = "http://www.google.com?&foo=bar";
  EXPECT_TRUE(NthParameterIs(c, 1, "", ""));
  EXPECT_TRUE(NthParameterIs(c, 2, "foo", "bar"));
  EXPECT_TRUE(NthParameterIs(c, 3, nullptr, nullptr));

  // Empty key with value.
  char d[] = "http://www.google.com?=foo";
  EXPECT_TRUE(NthParameterIs(d, 1, "", "foo"));
  EXPECT_TRUE(NthParameterIs(d, 2, nullptr, nullptr));

  // Empty value with key.
  char e[] = "http://www.google.com?foo=";
  EXPECT_TRUE(NthParameterIs(e, 1, "foo", ""));
  EXPECT_TRUE(NthParameterIs(e, 2, nullptr, nullptr));

  // Empty key and values.
  char f[] = "http://www.google.com?&&==&=";
  EXPECT_TRUE(NthParameterIs(f, 1, "", ""));
  EXPECT_TRUE(NthParameterIs(f, 2, "", ""));
  EXPECT_TRUE(NthParameterIs(f, 3, "", "="));
  EXPECT_TRUE(NthParameterIs(f, 4, "", ""));
  EXPECT_TRUE(NthParameterIs(f, 5, nullptr, nullptr));
}

// MailtoURL --------------------------------------------------------------------

// clang-format off
static MailtoURLParseCase mailto_cases[] = {
//|input                       |scheme   |path               |query
{"mailto:foo@gmail.com",        "mailto", "foo@gmail.com",    nullptr},
{"  mailto: to  \t",            "mailto", " to",              nullptr},
{"mailto:addr1%2C%20addr2 ",    "mailto", "addr1%2C%20addr2", nullptr},
{"Mailto:addr1, addr2 ",        "Mailto", "addr1, addr2",     nullptr},
{"mailto:addr1:addr2 ",         "mailto", "addr1:addr2",      nullptr},
{"mailto:?to=addr1,addr2",      "mailto", nullptr,            "to=addr1,addr2"},
{"mailto:?to=addr1%2C%20addr2", "mailto", nullptr,            "to=addr1%2C%20addr2"},
{"mailto:addr1?to=addr2",       "mailto", "addr1",            "to=addr2"},
{"mailto:?body=#foobar#",       "mailto", nullptr,            "body=#foobar#",},
{"mailto:#?body=#foobar#",      "mailto", "#",                "body=#foobar#"},
};
// clang-format on

TEST(URLParser, MailtoUrl) {
  // Declared outside for loop to try to catch cases in init() where we forget
  // to reset something that is reset by the constructor.
  for (const auto& mailto_case : mailto_cases) {
    const char* url = mailto_case.input;
    Parsed parsed = ParseMailtoURL(url);
    int port = ParsePort(url, parsed.port);

    EXPECT_TRUE(ComponentMatches(url, mailto_case.scheme, parsed.scheme));
    EXPECT_TRUE(ComponentMatches(url, mailto_case.path, parsed.path));
    EXPECT_TRUE(ComponentMatches(url, mailto_case.query, parsed.query));
    EXPECT_EQ(PORT_UNSPECIFIED, port);
    EXPECT_FALSE(parsed.has_opaque_path);

    // The remaining components are never used for mailto URLs.
    ExpectInvalidComponent(parsed.username);
    ExpectInvalidComponent(parsed.password);
    ExpectInvalidComponent(parsed.port);
    ExpectInvalidComponent(parsed.ref);
  }
}

// Various incarnations of filesystem URLs.
static FileSystemURLParseCase filesystem_cases[] = {
    // Regular URL with all the parts
    {"filesystem:http://user:pass@foo:21/temporary/bar;par?b#c", "http", "user",
     "pass", "foo", 21, "/temporary", "/bar;par", "b", "c"},
    {"filesystem:https://foo/persistent/bar;par/", "https", nullptr, nullptr,
     "foo", -1, "/persistent", "/bar;par/", nullptr, nullptr},
    {"filesystem:file:///persistent/bar;par/", "file", nullptr, nullptr,
     nullptr, -1, "/persistent", "/bar;par/", nullptr, nullptr},
    {"filesystem:file:///persistent/bar;par/?query#ref", "file", nullptr,
     nullptr, nullptr, -1, "/persistent", "/bar;par/", "query", "ref"},
    {"filesystem:file:///persistent", "file", nullptr, nullptr, nullptr, -1,
     "/persistent", "", nullptr, nullptr},
    {"filesystem:", nullptr, nullptr, nullptr, nullptr, -1, nullptr, nullptr,
     nullptr, nullptr},
};

TEST(URLParser, FileSystemURL) {
  // Declared outside for loop to try to catch cases in init() where we forget
  // to reset something that is reset by the constructor.
  for (const auto& filesystem_case : filesystem_cases) {
    const char* url = filesystem_case.input;
    Parsed parsed = ParseFileSystemURL(url);

    EXPECT_TRUE(ComponentMatches(url, "filesystem", parsed.scheme));
    EXPECT_EQ(!filesystem_case.inner_scheme, !parsed.inner_parsed());
    // Only check the inner_parsed if there is one.
    if (parsed.inner_parsed()) {
      EXPECT_TRUE(ComponentMatches(url, filesystem_case.inner_scheme,
          parsed.inner_parsed()->scheme));
      EXPECT_TRUE(ComponentMatches(url, filesystem_case.inner_username,
          parsed.inner_parsed()->username));
      EXPECT_TRUE(ComponentMatches(url, filesystem_case.inner_password,
          parsed.inner_parsed()->password));
      EXPECT_TRUE(ComponentMatches(url, filesystem_case.inner_host,
          parsed.inner_parsed()->host));
      int port = ParsePort(url, parsed.inner_parsed()->port);
      EXPECT_EQ(filesystem_case.inner_port, port);

      // The remaining components are never used for filesystem URLs.
      ExpectInvalidComponent(parsed.inner_parsed()->query);
      ExpectInvalidComponent(parsed.inner_parsed()->ref);
    }

    EXPECT_TRUE(ComponentMatches(url, filesystem_case.path, parsed.path));
    EXPECT_TRUE(ComponentMatches(url, filesystem_case.query, parsed.query));
    EXPECT_TRUE(ComponentMatches(url, filesystem_case.ref, parsed.ref));
    EXPECT_FALSE(parsed.has_opaque_path);

    // The remaining components are never used for filesystem URLs.
    ExpectInvalidComponent(parsed.username);
    ExpectInvalidComponent(parsed.password);
    ExpectInvalidComponent(parsed.host);
    ExpectInvalidComponent(parsed.port);
  }
}

// Non-special URLs which don't have an opaque path.
static URLParseCase non_special_cases[] = {
    {"git://user:pass@foo:21/bar;par?b#c", "git", "user", "pass", "foo", 21,
     "/bar;par", "b", "c"},
    {"git://host", "git", nullptr, nullptr, "host", -1, nullptr, nullptr,
     nullptr},
    {"git://host/a/../b", "git", nullptr, nullptr, "host", -1, "/a/../b",
     nullptr, nullptr},
    {"git://host/a b", "git", nullptr, nullptr, "host", -1, "/a b", nullptr,
     nullptr},
    {"git://ho\\st/", "git", nullptr, nullptr, "ho\\st", -1, "/", nullptr,
     nullptr},
    // Empty users
    {"git://@host", "git", "", nullptr, "host", -1, nullptr, nullptr, nullptr},
    // Empty user and invalid host. "git://@" is an invalid URL.
    {"git://@", "git", "", nullptr, nullptr, -1, nullptr, nullptr, nullptr},
    // Invalid host and non-empty port. "git://:80" is an invalid URL.
    {"git://:80", "git", nullptr, nullptr, nullptr, 80, nullptr, nullptr,
     nullptr},
    // Empty host cases
    {"git://", "git", nullptr, nullptr, "", -1, nullptr, nullptr, nullptr},
    {"git:///", "git", nullptr, nullptr, "", -1, "/", nullptr, nullptr},
    {"git:////", "git", nullptr, nullptr, "", -1, "//", nullptr, nullptr},
    // Null host cases
    {"git:/", "git", nullptr, nullptr, nullptr, -1, "/", nullptr, nullptr},
    {"git:/trailing-space ", "git", nullptr, nullptr, nullptr, -1,
     "/trailing-space", nullptr, nullptr},
};

TEST(URLParser, NonSpecial) {
  // Declared outside for loop to try to catch cases in init() where we forget
  // to reset something that is reset by the constructor.
  for (const auto& i : non_special_cases) {
    Parsed parsed = ParseNonSpecialURL(i.input);
    URLParseCaseMatches(i, parsed);
    EXPECT_FALSE(parsed.has_opaque_path) << "url: " << i.input;
  }
}

// Non-special URLs which have an opaque path.
static URLParseCase non_special_opaque_path_cases[] = {
    {"git:", "git", nullptr, nullptr, nullptr, -1, nullptr, nullptr, nullptr},
    {"git:opaque", "git", nullptr, nullptr, nullptr, -1, "opaque", nullptr,
     nullptr},
    {"git:opaque?a=b#c", "git", nullptr, nullptr, nullptr, -1, "opaque", "a=b",
     "c"},
    {"git: o p a q u e ", "git", nullptr, nullptr, nullptr, -1, " o p a q u e",
     nullptr, nullptr},
    {"git:opa\\que", "git", nullptr, nullptr, nullptr, -1, "opa\\que", nullptr,
     nullptr},
};

TEST(URLParser, NonSpecialOpaquePath) {
  // Declared outside for loop to try to catch cases in init() where we forget
  // to reset something that is reset by the constructor.
  for (const auto& i : non_special_opaque_path_cases) {
    Parsed parsed = ParseNonSpecialURL(i.input);
    URLParseCaseMatches(i, parsed);
    EXPECT_TRUE(parsed.has_opaque_path) << "url: " << i.input;
  }
}

}  // namespace
}  // namespace url
