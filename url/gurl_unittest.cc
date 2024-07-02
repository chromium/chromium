// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/350788890): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "url/gurl.h"

#include <stddef.h>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl_abstract_tests.h"
#include "url/origin.h"
#include "url/url_canon.h"
#include "url/url_features.h"
#include "url/url_test_utils.h"

namespace url {

namespace {

// Returns the canonicalized string for the given URL string for the
// GURLTest.Types test.
std::string TypesTestCase(const char* src) {
  GURL gurl(src);
  return gurl.possibly_invalid_spec();
}

}  // namespace

// Different types of URLs should be handled differently, and handed off to
// different canonicalizers.
TEST(GURLTest, Types) {
  // URLs with unknown schemes should be treated as path URLs, even when they
  // have things like "://".
  EXPECT_EQ("something:///HOSTNAME.com/",
            TypesTestCase("something:///HOSTNAME.com/"));

  // Conversely, URLs with known schemes should always trigger standard URL
  // handling.
  EXPECT_EQ("http://hostname.com/", TypesTestCase("http:HOSTNAME.com"));
  EXPECT_EQ("http://hostname.com/", TypesTestCase("http:/HOSTNAME.com"));
  EXPECT_EQ("http://hostname.com/", TypesTestCase("http://HOSTNAME.com"));
  EXPECT_EQ("http://hostname.com/", TypesTestCase("http:///HOSTNAME.com"));

#ifdef WIN32
  // URLs that look like Windows absolute path specs.
  EXPECT_EQ("file:///C:/foo.txt", TypesTestCase("c:\\foo.txt"));
  EXPECT_EQ("file:///Z:/foo.txt", TypesTestCase("Z|foo.txt"));
  EXPECT_EQ("file://server/foo.txt", TypesTestCase("\\\\server\\foo.txt"));
  EXPECT_EQ("file://server/foo.txt", TypesTestCase("//server/foo.txt"));
#endif
}

// Test the basic creation and querying of components in a GURL. We assume that
// the parser is already tested and works, so we are mostly interested if the
// object does the right thing with the results.
TEST(GURLTest, Components) {
  GURL empty_url(u"");
  EXPECT_TRUE(empty_url.is_empty());
  EXPECT_FALSE(empty_url.is_valid());

  GURL url(u"http://user:pass@google.com:99/foo;bar?q=a#ref");
  EXPECT_FALSE(url.is_empty());
  EXPECT_TRUE(url.is_valid());
  EXPECT_TRUE(url.SchemeIs("http"));
  EXPECT_FALSE(url.SchemeIsFile());

  // This is the narrow version of the URL, which should match the wide input.
  EXPECT_EQ("http://user:pass@google.com:99/foo;bar?q=a#ref", url.spec());

  EXPECT_EQ("http", url.scheme());
  EXPECT_EQ("user", url.username());
  EXPECT_EQ("pass", url.password());
  EXPECT_EQ("google.com", url.host());
  EXPECT_EQ("99", url.port());
  EXPECT_EQ(99, url.IntPort());
  EXPECT_EQ("/foo;bar", url.path());
  EXPECT_EQ("q=a", url.query());
  EXPECT_EQ("ref", url.ref());

  // Test parsing userinfo with special characters.
  GURL url_special_pass("http://user:%40!$&'()*+,;=:@google.com:12345");
  EXPECT_TRUE(url_special_pass.is_valid());
  // GURL canonicalizes some delimiters.
  EXPECT_EQ("%40!$&%27()*+,%3B%3D%3A", url_special_pass.password());
  EXPECT_EQ("google.com", url_special_pass.host());
  EXPECT_EQ("12345", url_special_pass.port());

  // Test path collapsing.
  GURL url_path_collapse("http://example.com/a/./b/c/d/../../e");
  EXPECT_EQ("/a/b/e", url_path_collapse.path());

  // Test an IDNA (Internationalizing Domain Names in Applications) host.
  GURL url_idna("http://Bücher.exAMple/");
  EXPECT_EQ("xn--bcher-kva.example", url_idna.host());

  // Test non-ASCII characters, outside of the host (IDNA).
  GURL url_non_ascii("http://example.com/foo/aβc%2Etxt?q=r🙂s");
  EXPECT_EQ("/foo/a%CE%B2c.txt", url_non_ascii.path());
  EXPECT_EQ("q=r%F0%9F%99%82s", url_non_ascii.query());

  // Test already percent-escaped strings.
  GURL url_percent_escaped("http://example.com/a/./%2e/i%2E%2F%2fj?q=r%2Es");
  EXPECT_EQ("/a/i.%2F%2fj", url_percent_escaped.path());
  EXPECT_EQ("q=r%2Es", url_percent_escaped.query());
}

TEST(GURLTest, Empty) {
  GURL url;
  EXPECT_FALSE(url.is_valid());
  EXPECT_EQ("", url.spec());

  EXPECT_EQ("", url.scheme());
  EXPECT_EQ("", url.username());
  EXPECT_EQ("", url.password());
  EXPECT_EQ("", url.host());
  EXPECT_EQ("", url.port());
  EXPECT_EQ(PORT_UNSPECIFIED, url.IntPort());
  EXPECT_EQ("", url.path());
  EXPECT_EQ("", url.query());
  EXPECT_EQ("", url.ref());
}

TEST(GURLTest, Copy) {
  GURL url(u"http://user:pass@google.com:99/foo;bar?q=a#ref");

  GURL url2(url);
  EXPECT_TRUE(url2.is_valid());

  EXPECT_EQ("http://user:pass@google.com:99/foo;bar?q=a#ref", url2.spec());
  EXPECT_EQ("http", url2.scheme());
  EXPECT_EQ("user", url2.username());
  EXPECT_EQ("pass", url2.password());
  EXPECT_EQ("google.com", url2.host());
  EXPECT_EQ("99", url2.port());
  EXPECT_EQ(99, url2.IntPort());
  EXPECT_EQ("/foo;bar", url2.path());
  EXPECT_EQ("q=a", url2.query());
  EXPECT_EQ("ref", url2.ref());

  // Copying of invalid URL should be invalid
  GURL invalid;
  GURL invalid2(invalid);
  EXPECT_FALSE(invalid2.is_valid());
  EXPECT_EQ("", invalid2.spec());
  EXPECT_EQ("", invalid2.scheme());
  EXPECT_EQ("", invalid2.username());
  EXPECT_EQ("", invalid2.password());
  EXPECT_EQ("", invalid2.host());
  EXPECT_EQ("", invalid2.port());
  EXPECT_EQ(PORT_UNSPECIFIED, invalid2.IntPort());
  EXPECT_EQ("", invalid2.path());
  EXPECT_EQ("", invalid2.query());
  EXPECT_EQ("", invalid2.ref());
}

TEST(GURLTest, Assign) {
  GURL url(u"http://user:pass@google.com:99/foo;bar?q=a#ref");

  GURL url2;
  url2 = url;
  EXPECT_TRUE(url2.is_valid());

  EXPECT_EQ("http://user:pass@google.com:99/foo;bar?q=a#ref", url2.spec());
  EXPECT_EQ("http", url2.scheme());
  EXPECT_EQ("user", url2.username());
  EXPECT_EQ("pass", url2.password());
  EXPECT_EQ("google.com", url2.host());
  EXPECT_EQ("99", url2.port());
  EXPECT_EQ(99, url2.IntPort());
  EXPECT_EQ("/foo;bar", url2.path());
  EXPECT_EQ("q=a", url2.query());
  EXPECT_EQ("ref", url2.ref());

  // Assignment of invalid URL should be invalid
  GURL invalid;
  GURL invalid2;
  invalid2 = invalid;
  EXPECT_FALSE(invalid2.is_valid());
  EXPECT_EQ("", invalid2.spec());
  EXPECT_EQ("", invalid2.scheme());
  EXPECT_EQ("", invalid2.username());
  EXPECT_EQ("", invalid2.password());
  EXPECT_EQ("", invalid2.host());
  EXPECT_EQ("", invalid2.port());
  EXPECT_EQ(PORT_UNSPECIFIED, invalid2.IntPort());
  EXPECT_EQ("", invalid2.path());
  EXPECT_EQ("", invalid2.query());
  EXPECT_EQ("", invalid2.ref());
}

// This is a regression test for http://crbug.com/309975.
TEST(GURLTest, SelfAssign) {
  GURL a("filesystem:http://example.com/temporary/");
  // This should not crash.
  a = *&a;  // The *& defeats Clang's -Wself-assign warning.
}

TEST(GURLTest, CopyFileSystem) {
  GURL url(u"filesystem:https://user:pass@google.com:99/t/foo;bar?q=a#ref");

  GURL url2(url);
  EXPECT_TRUE(url2.is_valid());

  EXPECT_EQ("filesystem:https://google.com:99/t/foo;bar?q=a#ref", url2.spec());
  EXPECT_EQ("filesystem", url2.scheme());
  EXPECT_EQ("", url2.username());
  EXPECT_EQ("", url2.password());
  EXPECT_EQ("", url2.host());
  EXPECT_EQ("", url2.port());
  EXPECT_EQ(PORT_UNSPECIFIED, url2.IntPort());
  EXPECT_EQ("/foo;bar", url2.path());
  EXPECT_EQ("q=a", url2.query());
  EXPECT_EQ("ref", url2.ref());

  const GURL* inner = url2.inner_url();
  ASSERT_TRUE(inner);
  EXPECT_EQ("https", inner->scheme());
  EXPECT_EQ("", inner->username());
  EXPECT_EQ("", inner->password());
  EXPECT_EQ("google.com", inner->host());
  EXPECT_EQ("99", inner->port());
  EXPECT_EQ(99, inner->IntPort());
  EXPECT_EQ("/t", inner->path());
  EXPECT_EQ("", inner->query());
  EXPECT_EQ("", inner->ref());
}

TEST(GURLTest, IsValid) {
  const char* valid_cases[] = {
      "http://google.com",
      "unknown://google.com",
      "http://user:pass@google.com",
      "http://google.com:12345",
      "http://google.com:0",  // 0 is a valid port
      "http://google.com/path",
      "http://google.com//path",
      "http://google.com?k=v#fragment",
      "http://user:pass@google.com:12345/path?k=v#fragment",
      "http:/path",
      "http:path",
  };
  for (size_t i = 0; i < std::size(valid_cases); i++) {
    EXPECT_TRUE(GURL(valid_cases[i]).is_valid())
        << "Case: " << valid_cases[i];
  }

  const char* invalid_cases[] = {
      "http://?k=v",
      "http:://google.com",
      "http//google.com",
      "http://google.com:12three45",
      "file://server:123",  // file: URLs cannot have a port
      "file://server:0",
      "://google.com",
      "path",
  };
  for (size_t i = 0; i < std::size(invalid_cases); i++) {
    EXPECT_FALSE(GURL(invalid_cases[i]).is_valid())
        << "Case: " << invalid_cases[i];
  }
}

TEST(GURLTest, ExtraSlashesBeforeAuthority) {
  // According to RFC3986, the hierarchical part for URI with an authority
  // must use only two slashes; GURL intentionally just ignores extra slashes
  // if there are more than 2, and parses the following part as an authority.
  GURL url("http:///host");
  EXPECT_EQ("host", url.host());
  EXPECT_EQ("/", url.path());
}

// Given invalid URLs, we should still get most of the components.
TEST(GURLTest, ComponentGettersWorkEvenForInvalidURL) {
  constexpr struct InvalidURLTestExpectations {
    const char* url;
    const char* spec;
    const char* scheme;
    const char* host;
    const char* port;
    const char* path;
    // Extend as needed...
  } expectations[] = {
      {
          "http:google.com:foo",
          "http://google.com:foo/",
          "http",
          "google.com",
          "foo",
          "/",
      },
      {
          "https:google.com:foo",
          "https://google.com:foo/",
          "https",
          "google.com",
          "foo",
          "/",
      },
  };

  for (const auto& e : expectations) {
    const GURL url(e.url);
    EXPECT_FALSE(url.is_valid());
    EXPECT_EQ(e.spec, url.possibly_invalid_spec());
    EXPECT_EQ(e.scheme, url.scheme());
    EXPECT_EQ("", url.username());
    EXPECT_EQ("", url.password());
    EXPECT_EQ(e.host, url.host());
    EXPECT_EQ(e.port, url.port());
    EXPECT_EQ(PORT_INVALID, url.IntPort());
    EXPECT_EQ(e.path, url.path());
    EXPECT_EQ("", url.query());
    EXPECT_EQ("", url.ref());
  }
}

TEST(GURLTest, Resolve) {
  // The tricky cases for relative URL resolving are tested in the
  // canonicalizer unit test. Here, we just test that the GURL integration
  // works properly.
  struct ResolveCase {
    const char* base;
    const char* relative;
    bool expected_valid;
    const char* expected;
  } resolve_cases[] = {
      {"http://www.google.com/", "foo.html", true,
       "http://www.google.com/foo.html"},
      {"http://www.google.com/foo/", "bar", true,
       "http://www.google.com/foo/bar"},
      {"http://www.google.com/foo/", "/bar", true, "http://www.google.com/bar"},
      {"http://www.google.com/foo", "bar", true, "http://www.google.com/bar"},
      {"http://www.google.com/", "http://images.google.com/foo.html", true,
       "http://images.google.com/foo.html"},
      {"http://www.google.com/", "http://images.\tgoogle.\ncom/\rfoo.html",
       true, "http://images.google.com/foo.html"},
      {"http://www.google.com/blah/bloo?c#d", "../../../hello/./world.html?a#b",
       true, "http://www.google.com/hello/world.html?a#b"},
      {"http://www.google.com/foo#bar", "#com", true,
       "http://www.google.com/foo#com"},
      {"http://www.google.com/", "Https:images.google.com", true,
       "https://images.google.com/"},
      // An opaque path URL can be replaced with a special absolute URL.
      {"data:blahblah", "http://google.com/", true, "http://google.com/"},
      {"data:blahblah", "http:google.com", true, "http://google.com/"},
      {"data:blahblah", "https:google.com", true, "https://google.com/"},
      // An opaque path URL can not be replaced with a relative URL.
      {"git:opaque", "", false, ""},
      {"git:opaque", "path", false, ""},
      // A non-special URL which doesn't have a host can be replaced with a
      // relative URL.
      {"git:/a", "b", true, "git:/b"},
      // Filesystem URLs have different paths to test.
      {"filesystem:http://www.google.com/type/", "foo.html", true,
       "filesystem:http://www.google.com/type/foo.html"},
      {"filesystem:http://www.google.com/type/", "../foo.html", true,
       "filesystem:http://www.google.com/type/foo.html"},
      // https://crbug.com/530123 - scheme validation (e.g. are "10.0.0.7:"
      // or "x1:" valid schemes) when deciding if |relative| is an absolute url.
      {"file:///some/dir/ip-relative.html", "10.0.0.7:8080/foo.html", true,
       "file:///some/dir/10.0.0.7:8080/foo.html"},
      {"file:///some/dir/", "1://host", true, "file:///some/dir/1://host"},
      {"file:///some/dir/", "x1://host", true, "x1://host"},
      {"file:///some/dir/", "X1://host", true, "x1://host"},
      {"file:///some/dir/", "x.://host", true, "x.://host"},
      {"file:///some/dir/", "x+://host", true, "x+://host"},
      {"file:///some/dir/", "x-://host", true, "x-://host"},
      {"file:///some/dir/", "x!://host", true, "file:///some/dir/x!://host"},
      {"file:///some/dir/", "://host", true, "file:///some/dir/://host"},
  };

  for (size_t i = 0; i < std::size(resolve_cases); i++) {
    // 8-bit code path.
    GURL input(resolve_cases[i].base);
    GURL output = input.Resolve(resolve_cases[i].relative);
    EXPECT_EQ(resolve_cases[i].expected_valid, output.is_valid()) << i;
    EXPECT_EQ(resolve_cases[i].expected, output.spec()) << i;
    EXPECT_EQ(output.SchemeIsFileSystem(), output.inner_url() != NULL);

    // Wide code path.
    GURL inputw(base::UTF8ToUTF16(resolve_cases[i].base));
    GURL outputw =
        input.Resolve(base::UTF8ToUTF16(resolve_cases[i].relative));
    EXPECT_EQ(resolve_cases[i].expected_valid, outputw.is_valid()) << i;
    EXPECT_EQ(resolve_cases[i].expected, outputw.spec()) << i;
    EXPECT_EQ(outputw.SchemeIsFileSystem(), outputw.inner_url() != NULL);
  }
}

class GURLTypedTest : public ::testing::TestWithParam<bool> {
 public:
  GURLTypedTest()
      : use_standard_compliant_non_special_scheme_url_parsing_(GetParam()) {
    if (use_standard_compliant_non_special_scheme_url_parsing_) {
      scoped_feature_list_.InitAndEnableFeature(
          kStandardCompliantNonSpecialSchemeURLParsing);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          kStandardCompliantNonSpecialSchemeURLParsing);
    }
  }

 protected:
  struct ResolveCase {
    std::string_view base;
    std::string_view relative;
    std::optional<std::string_view> expected;
  };

  using ApplyReplacementsFunc = GURL(const GURL&);

  struct ReplaceCase {
    std::string_view base;
    ApplyReplacementsFunc* apply_replacements;
    std::string_view expected;
  };

  struct ReplaceHostCase {
    std::string_view base;
    std::string_view replacement_host;
    std::string_view expected;
  };

  struct ReplacePathCase {
    std::string_view base;
    std::string_view replacement_path;
    std::string_view expected;
  };

  void TestResolve(const ResolveCase& resolve_case) {
    SCOPED_TRACE(testing::Message() << "base: " << resolve_case.base
                                    << ", relative: " << resolve_case.relative);
    GURL input(resolve_case.base);
    GURL output = input.Resolve(resolve_case.relative);
    if (resolve_case.expected) {
      ASSERT_TRUE(output.is_valid());
      EXPECT_EQ(output.spec(), *resolve_case.expected);
    } else {
      EXPECT_FALSE(output.is_valid());
    }
  }

  void TestReplace(const ReplaceCase& replace) {
    GURL output = replace.apply_replacements(GURL(replace.base));
    EXPECT_EQ(output.spec(), replace.expected);
  }

  void TestReplaceHost(const ReplaceHostCase& replace) {
    GURL url(replace.base);
    GURL::Replacements replacements;
    replacements.SetHostStr(replace.replacement_host);
    GURL output = url.ReplaceComponents(replacements);
    EXPECT_EQ(output.spec(), replace.expected);
  }

  void TestReplacePath(const ReplacePathCase& replace) {
    GURL url(replace.base);
    GURL::Replacements replacements;
    replacements.SetPathStr(replace.replacement_path);
    GURL output = url.ReplaceComponents(replacements);
    EXPECT_EQ(output.spec(), replace.expected);
  }

  bool use_standard_compliant_non_special_scheme_url_parsing_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(GURLTypedTest, Resolve) {
  // Test flag-dependent behaviors.
  // Existing tests in GURLTest::Resolve cover common cases.
  if (use_standard_compliant_non_special_scheme_url_parsing_) {
    ResolveCase cases[] = {
        // Non-special base URLs whose paths are empty.
        {"git://host", "", "git://host"},
        {"git://host", ".", "git://host/"},
        {"git://host", "..", "git://host/"},
        {"git://host", "a", "git://host/a"},
        {"git://host", "/a", "git://host/a"},

        // Non-special base URLs whose paths are "/".
        {"git://host/", "", "git://host/"},
        {"git://host/", ".", "git://host/"},
        {"git://host/", "..", "git://host/"},
        {"git://host/", "a", "git://host/a"},
        {"git://host/", "/a", "git://host/a"},

        // Non-special base URLs whose hosts and paths are non-empty.
        {"git://host/b", "a", "git://host/a"},
        {"git://host/b/c", "a", "git://host/b/a"},
        {"git://host/b/c", "../a", "git://host/a"},

        // An opaque path can be specified.
        {"git://host", "git:opaque", "git:opaque"},
        {"git://host/path#ref", "git:opaque", "git:opaque"},
        {"git:/path", "git:opaque", "git:opaque"},
        {"https://host/path", "git:opaque", "git:opaque"},

        // Path-only base URLs should remain path-only URLs unless a host is
        // specified.
        {"git:/", "", "git:/"},
        {"git:/", ".", "git:/"},
        {"git:/", "..", "git:/"},
        {"git:/", "a", "git:/a"},
        {"git:/", "/a", "git:/a"},
        {"git:/#ref", "", "git:/"},
        {"git:/#ref", "a", "git:/a"},

        // Non-special base URLs whose hosts and path are both empty. The
        // result's host should remain empty unless a relative URL specify a
        // host.
        {"git://", "", "git://"},
        {"git://", ".", "git:///"},
        {"git://", "..", "git:///"},
        {"git://", "a", "git:///a"},
        {"git://", "/a", "git:///a"},

        // Non-special base URLs whose hosts are empty, but with non-empty path.
        {"git:///", "", "git:///"},
        {"git:///", ".", "git:///"},
        {"git:///", "..", "git:///"},
        {"git:///", "a", "git:///a"},
        {"git:///", "/a", "git:///a"},
        {"git:///#ref", "", "git:///"},
        {"git:///#ref", "a", "git:///a"},

        // Relative URLs can specify empty hosts for non-special base URLs.
        // e.g. "///path"
        {"git://host/", "//", "git://"},
        {"git://host/", "//a", "git://a"},
        {"git://host/", "///", "git:///"},
        {"git://host/", "////", "git:////"},
        {"git://host/", "////..", "git:///"},
        {"git://host/", "////../..", "git:///"},
        {"git://host/", "////../../..", "git:///"},
    };
    for (const auto& i : cases) {
      TestResolve(i);
    }
  } else {
    ResolveCase cases[] = {
        {"git:/", "", "git:/"},
        {"git:/", "a", "git:/a"},
        {"git:/path", "a", "git:/a"},
        // All non-special base URLs which don't start with a *single* slash
        // can not be resolved with a relative URL.
        {"git:", "", std::nullopt},
        {"git://host", "", std::nullopt},
        {"git://host", "a", std::nullopt},
        {"git://", "", std::nullopt},
        {"git:///", "", std::nullopt},
    };
    for (const auto& i : cases) {
      TestResolve(i);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(All, GURLTypedTest, ::testing::Bool());

TEST(GURLTest, GetOrigin) {
  struct TestCase {
    const char* input;
    const char* expected;
  } cases[] = {
      {"http://www.google.com", "http://www.google.com/"},
      {"javascript:window.alert(\"hello,world\");", ""},
      {"http://user:pass@www.google.com:21/blah#baz",
       "http://www.google.com:21/"},
      {"http://user@www.google.com", "http://www.google.com/"},
      {"http://:pass@www.google.com", "http://www.google.com/"},
      {"http://:@www.google.com", "http://www.google.com/"},
      {"filesystem:http://www.google.com/temp/foo?q#b",
       "http://www.google.com/"},
      {"filesystem:http://user:pass@google.com:21/blah#baz",
       "http://google.com:21/"},
      {"blob:null/guid-goes-here", ""},
      {"blob:http://origin/guid-goes-here", "" /* should be http://origin/ */},
  };
  for (size_t i = 0; i < std::size(cases); i++) {
    GURL url(cases[i].input);
    GURL origin = url.DeprecatedGetOriginAsURL();
    EXPECT_EQ(cases[i].expected, origin.spec());
  }
}

TEST(GURLTest, GetAsReferrer) {
  struct TestCase {
    const char* input;
    const char* expected;
  } cases[] = {
    {"http://www.google.com", "http://www.google.com/"},
    {"http://user:pass@www.google.com:21/blah#baz", "http://www.google.com:21/blah"},
    {"http://user@www.google.com", "http://www.google.com/"},
    {"http://:pass@www.google.com", "http://www.google.com/"},
    {"http://:@www.google.com", "http://www.google.com/"},
    {"http://www.google.com/temp/foo?q#b", "http://www.google.com/temp/foo?q"},
    {"not a url", ""},
    {"unknown-scheme://foo.html", ""},
    {"file:///tmp/test.html", ""},
    {"https://www.google.com", "https://www.google.com/"},
  };
  for (size_t i = 0; i < std::size(cases); i++) {
    GURL url(cases[i].input);
    GURL origin = url.GetAsReferrer();
    EXPECT_EQ(cases[i].expected, origin.spec());
  }
}

TEST(GURLTest, GetWithEmptyPath) {
  struct TestCase {
    const char* input;
    const char* expected;
  } cases[] = {
    {"http://www.google.com", "http://www.google.com/"},
    {"javascript:window.alert(\"hello, world\");", ""},
    {"http://www.google.com/foo/bar.html?baz=22", "http://www.google.com/"},
    {"filesystem:http://www.google.com/temporary/bar.html?baz=22", "filesystem:http://www.google.com/temporary/"},
    {"filesystem:file:///temporary/bar.html?baz=22", "filesystem:file:///temporary/"},
  };

  for (size_t i = 0; i < std::size(cases); i++) {
    GURL url(cases[i].input);
    GURL empty_path = url.GetWithEmptyPath();
    EXPECT_EQ(cases[i].expected, empty_path.spec());
  }
}

TEST(GURLTest, GetWithoutFilename) {
  struct TestCase {
    const char* input;
    const char* expected;
  } cases[] = {
    // Common Standard URLs.
    {"https://www.google.com",                    "https://www.google.com/"},
    {"https://www.google.com/",                   "https://www.google.com/"},
    {"https://www.google.com/maps.htm",           "https://www.google.com/"},
    {"https://www.google.com/maps/",              "https://www.google.com/maps/"},
    {"https://www.google.com/index.html",         "https://www.google.com/"},
    {"https://www.google.com/index.html?q=maps",  "https://www.google.com/"},
    {"https://www.google.com/index.html#maps/",   "https://www.google.com/"},
    {"https://foo:bar@www.google.com/maps.htm",   "https://foo:bar@www.google.com/"},
    {"https://www.google.com/maps/au/index.html", "https://www.google.com/maps/au/"},
    {"https://www.google.com/maps/au/north",      "https://www.google.com/maps/au/"},
    {"https://www.google.com/maps/au/north/",     "https://www.google.com/maps/au/north/"},
    {"https://www.google.com/maps/au/index.html?q=maps#fragment/",     "https://www.google.com/maps/au/"},
    {"http://www.google.com:8000/maps/au/index.html?q=maps#fragment/", "http://www.google.com:8000/maps/au/"},
    {"https://www.google.com/maps/au/north/?q=maps#fragment",          "https://www.google.com/maps/au/north/"},
    {"https://www.google.com/maps/au/north?q=maps#fragment",           "https://www.google.com/maps/au/"},
    // Less common standard URLs.
    {"filesystem:http://www.google.com/temporary/bar.html?baz=22", "filesystem:http://www.google.com/temporary/"},
    {"file:///temporary/bar.html?baz=22","file:///temporary/"},
    {"ftp://foo/test/index.html",        "ftp://foo/test/"},
    {"gopher://foo/test/index.html",     "gopher://foo/test/"},
    {"ws://foo/test/index.html",         "ws://foo/test/"},
    // Non-standard, hierarchical URLs.
    {"chrome://foo/bar.html", "chrome://foo/"},
    {"httpa://foo/test/index.html", "httpa://foo/test/"},
    // Non-standard, non-hierarchical URLs.
    {"blob:https://foo.bar/test/index.html", ""},
    {"about:blank", ""},
    {"data:foobar", ""},
    {"scheme:opaque_data", ""},
    // Invalid URLs.
    {"foobar", ""},
  };

  for (size_t i = 0; i < std::size(cases); i++) {
    GURL url(cases[i].input);
    GURL without_filename = url.GetWithoutFilename();
    EXPECT_EQ(cases[i].expected, without_filename.spec()) << i;
  }
}

TEST(GURLTest, GetWithoutRef) {
  struct TestCase {
    const char* input;
    const char* expected;
  } cases[] = {
      // Common Standard URLs.
      {"https://www.google.com/index.html",
       "https://www.google.com/index.html"},
      {"https://www.google.com/index.html#maps/",
       "https://www.google.com/index.html"},

      {"https://foo:bar@www.google.com/maps.htm",
       "https://foo:bar@www.google.com/maps.htm"},
      {"https://foo:bar@www.google.com/maps.htm#fragment",
       "https://foo:bar@www.google.com/maps.htm"},

      {"https://www.google.com/maps/au/index.html?q=maps",
       "https://www.google.com/maps/au/index.html?q=maps"},
      {"https://www.google.com/maps/au/index.html?q=maps#fragment/",
       "https://www.google.com/maps/au/index.html?q=maps"},

      {"http://www.google.com:8000/maps/au/index.html?q=maps",
       "http://www.google.com:8000/maps/au/index.html?q=maps"},
      {"http://www.google.com:8000/maps/au/index.html?q=maps#fragment/",
       "http://www.google.com:8000/maps/au/index.html?q=maps"},

      {"https://www.google.com/maps/au/north/?q=maps",
       "https://www.google.com/maps/au/north/?q=maps"},
      {"https://www.google.com/maps/au/north?q=maps#fragment",
       "https://www.google.com/maps/au/north?q=maps"},

      // Less common standard URLs.
      {"filesystem:http://www.google.com/temporary/bar.html?baz=22",
       "filesystem:http://www.google.com/temporary/bar.html?baz=22"},
      {"file:///temporary/bar.html?baz=22#fragment",
       "file:///temporary/bar.html?baz=22"},

      {"ftp://foo/test/index.html", "ftp://foo/test/index.html"},
      {"ftp://foo/test/index.html#fragment", "ftp://foo/test/index.html"},

      {"gopher://foo/test/index.html", "gopher://foo/test/index.html"},
      {"gopher://foo/test/index.html#fragment", "gopher://foo/test/index.html"},

      {"ws://foo/test/index.html", "ws://foo/test/index.html"},
      {"ws://foo/test/index.html#fragment", "ws://foo/test/index.html"},

      // Non-standard, hierarchical URLs.
      {"chrome://foo/bar.html", "chrome://foo/bar.html"},
      {"chrome://foo/bar.html#fragment", "chrome://foo/bar.html"},

      {"httpa://foo/test/index.html", "httpa://foo/test/index.html"},
      {"httpa://foo/test/index.html#fragment", "httpa://foo/test/index.html"},

      // Non-standard, non-hierarchical URLs.
      {"blob:https://foo.bar/test/index.html",
       "blob:https://foo.bar/test/index.html"},
      {"blob:https://foo.bar/test/index.html#fragment",
       "blob:https://foo.bar/test/index.html"},

      {"about:blank", "about:blank"},
      {"about:blank#ref", "about:blank"},

      {"data:foobar", "data:foobar"},
      {"scheme:opaque_data", "scheme:opaque_data"},
      // Invalid URLs.
      {"foobar", ""},
  };

  for (size_t i = 0; i < std::size(cases); i++) {
    GURL url(cases[i].input);
    GURL without_ref = url.GetWithoutRef();
    EXPECT_EQ(cases[i].expected, without_ref.spec());
  }
}

TEST(GURLTest, Replacements) {
  // The URL canonicalizer replacement test will handle most of these case.
  // The most important thing to do here is to check that the proper
  // canonicalizer gets called based on the scheme of the input.
  struct ReplaceCase {
    using ApplyReplacementsFunc = GURL(const GURL&);

    const char* base;
    ApplyReplacementsFunc* apply_replacements;
    const char* expected;
  } replace_cases[] = {
      {.base = "http://www.google.com/foo/bar.html?foo#bar",
       .apply_replacements =
           +[](const GURL& url) {
             GURL::Replacements replacements;
             replacements.SetPathStr("/");
             replacements.ClearQuery();
             replacements.ClearRef();
             return url.ReplaceComponents(replacements);
           },
       .expected = "http://www.google.com/"},
      {.base = "file:///C:/foo/bar.txt",
       .apply_replacements =
           +[](const GURL& url) {
             GURL::Replacements replacements;
             replacements.SetSchemeStr("http");
             replacements.SetHostStr("www.google.com");
             replacements.SetPortStr("99");
             replacements.SetPathStr("/foo");
             replacements.SetQueryStr("search");
             replacements.SetRefStr("ref");
             return url.ReplaceComponents(replacements);
           },
       .expected = "http://www.google.com:99/foo?search#ref"},
#ifdef WIN32
      {.base = "http://www.google.com/foo/bar.html?foo#bar",
       .apply_replacements =
           +[](const GURL& url) {
             GURL::Replacements replacements;
             replacements.SetSchemeStr("file");
             replacements.ClearUsername();
             replacements.ClearPassword();
             replacements.ClearHost();
             replacements.ClearPort();
             replacements.SetPathStr("c:\\");
             replacements.ClearQuery();
             replacements.ClearRef();
             return url.ReplaceComponents(replacements);
           },
       .expected = "file:///C:/"},
#endif
      {.base = "filesystem:http://www.google.com/foo/bar.html?foo#bar",
       .apply_replacements =
           +[](const GURL& url) {
             GURL::Replacements replacements;
             replacements.SetPathStr("/");
             replacements.ClearQuery();
             replacements.ClearRef();
             return url.ReplaceComponents(replacements);
           },
       .expected = "filesystem:http://www.google.com/foo/"},
      // Lengthen the URL instead of shortening it, to test creation of
      // inner_url.
      {.base = "filesystem:http://www.google.com/foo/",
       .apply_replacements =
           +[](const GURL& url) {
             GURL::Replacements replacements;
             replacements.SetPathStr("bar.html");
             replacements.SetQueryStr("foo");
             replacements.SetRefStr("bar");
             return url.ReplaceComponents(replacements);
           },
       .expected = "filesystem:http://www.google.com/foo/bar.html?foo#bar"},
  };

  for (const ReplaceCase& c : replace_cases) {
    GURL output = c.apply_replacements(GURL(c.base));

    EXPECT_EQ(c.expected, output.spec());

    EXPECT_EQ(output.SchemeIsFileSystem(), output.inner_url() != NULL);
    if (output.SchemeIsFileSystem()) {
      // TODO(mmenke): inner_url()->spec() is currently the same as the spec()
      // for the GURL itself.  This should be fixed.
      // See https://crbug.com/619596
      EXPECT_EQ(c.expected, output.inner_url()->spec());
    }
  }
}

TEST_P(GURLTypedTest, Replacements) {
  // Test flag-dependent behavior.
  // Existing tests in GURLTest::Replacements cover common cases.

  if (use_standard_compliant_non_special_scheme_url_parsing_) {
    ReplaceCase replace_cases[] = {
        {.base = "git://a1/a2?a3=a4#a5",
         .apply_replacements =
             +[](const GURL& url) {
               GURL::Replacements replacements;
               replacements.SetHostStr("b1");
               replacements.SetPortStr("99");
               replacements.SetPathStr("b2");
               replacements.SetQueryStr("b3=b4");
               replacements.SetRefStr("b5");
               return url.ReplaceComponents(replacements);
             },
         .expected = "git://b1:99/b2?b3=b4#b5"},
        // URL Standard: https://url.spec.whatwg.org/#dom-url-username
        // > 1. If this’s URL cannot have a username/password/port, then return.
        {.base = "git:///",
         .apply_replacements =
             +[](const GURL& url) {
               GURL::Replacements replacements;
               replacements.SetUsernameStr("x");
               return url.ReplaceComponents(replacements);
             },
         .expected = "git:///"},
        // URL Standard: https://url.spec.whatwg.org/#dom-url-password
        // > 1. If this’s URL cannot have a username/password/port, then return.
        {.base = "git:///",
         .apply_replacements =
             +[](const GURL& url) {
               GURL::Replacements replacements;
               replacements.SetPasswordStr("x");
               return url.ReplaceComponents(replacements);
             },
         .expected = "git:///"},
        // URL Standard: https://url.spec.whatwg.org/#dom-url-port
        // > 1. If this’s URL cannot have a username/password/port, then return.
        {.base = "git:///",
         .apply_replacements =
             +[](const GURL& url) {
               GURL::Replacements replacements;
               replacements.SetPortStr("80");
               return url.ReplaceComponents(replacements);
             },
         .expected = "git:///"}};

    for (const ReplaceCase& c : replace_cases) {
      TestReplace(c);
    }

    ReplaceHostCase replace_host_cases[] = {
        {"git:/", "host", "git://host/"},
        {"git:/a", "host", "git://host/a"},
        {"git://", "host", "git://host"},
        {"git:///", "host", "git://host/"},
        {"git://h/a", "host", "git://host/a"},
        // The following behavior is different from Web-facing URL APIs
        // because DOMURLUtils::setHostname disallows setting an empty host.
        //
        // Web-facing URL API behavior is:
        // > const url = new URL("git://u:p@h:80/");
        // > url.hostname = "";
        // > assertEquals(url.href, "git://u:p@h:80/");
        {"git://u:p@h:80/", "", "git:///"}};
    for (const ReplaceHostCase& c : replace_host_cases) {
      TestReplaceHost(c);
    }

    ReplacePathCase replace_path_cases[] = {
        {"git:/", "a", "git:/a"},
        {"git:/", "", "git:/"},
        {"git:/", "//a", "git:/.//a"},
        {"git:/", "/.//a", "git:/.//a"},
        {"git://", "a", "git:///a"},
        {"git:///", "a", "git:///a"},
        {"git://host", "a", "git://host/a"},
        {"git://host/b", "a", "git://host/a"}};
    for (const ReplacePathCase& c : replace_path_cases) {
      TestReplacePath(c);
    }
  } else {
    // Non-compliant behaviors.
    ReplaceHostCase replace_host_cases[] = {
        {"git://host", "h2", "git://host"},
    };
    for (const ReplaceHostCase& c : replace_host_cases) {
      TestReplaceHost(c);
    }

    // Non-compliant behaviors.
    ReplacePathCase replace_path_cases[] = {{"git://host", "path", "git:path"}};
    for (const ReplacePathCase& c : replace_path_cases) {
      TestReplacePath(c);
    }
  }
}

TEST(GURLTypedTest, ClearFragmentOnDataUrl) {
  // http://crbug.com/291747 - a data URL may legitimately have trailing
  // whitespace in the spec after the ref is cleared. Test this does not trigger
  // the Parsed importing validation DCHECK in GURL.
  GURL url(" data: one # two ");
  EXPECT_TRUE(url.is_valid());

  // By default the trailing whitespace will have been stripped.
  EXPECT_EQ("data: one #%20two", url.spec());

  // Clear the URL's ref and observe the trailing whitespace.
  GURL::Replacements repl;
  repl.ClearRef();
  GURL url_no_ref = url.ReplaceComponents(repl);
  EXPECT_TRUE(url_no_ref.is_valid());
  EXPECT_EQ("data: one ", url_no_ref.spec());

  // Importing a parsed URL via this constructor overload will retain trailing
  // whitespace.
  GURL import_url(url_no_ref.spec(),
                  url_no_ref.parsed_for_possibly_invalid_spec(),
                  url_no_ref.is_valid());
  EXPECT_TRUE(import_url.is_valid());
  EXPECT_EQ(url_no_ref, import_url);
  EXPECT_EQ("data: one ", import_url.spec());
  EXPECT_EQ(" one ", import_url.path());

  // For completeness, test that re-parsing the same URL rather than importing
  // it trims the trailing whitespace.
  GURL reparsed_url(url_no_ref.spec());
  EXPECT_TRUE(reparsed_url.is_valid());
  EXPECT_EQ("data: one", reparsed_url.spec());
}

TEST(GURLTest, PathForRequest) {
  struct TestCase {
    const char* input;
    const char* expected;
    const char* inner_expected;
  } cases[] = {
      {"http://www.google.com", "/", nullptr},
      {"http://www.google.com/", "/", nullptr},
      {"http://www.google.com/foo/bar.html?baz=22", "/foo/bar.html?baz=22",
       nullptr},
      {"http://www.google.com/foo/bar.html#ref", "/foo/bar.html", nullptr},
      {"http://www.google.com/foo/bar.html?query#ref", "/foo/bar.html?query",
       nullptr},
      {"filesystem:http://www.google.com/temporary/foo/bar.html?query#ref",
       "/foo/bar.html?query", "/temporary"},
      {"filesystem:http://www.google.com/temporary/foo/bar.html?query",
       "/foo/bar.html?query", "/temporary"},
  };

  for (size_t i = 0; i < std::size(cases); i++) {
    GURL url(cases[i].input);
    EXPECT_EQ(cases[i].expected, url.PathForRequest());
    EXPECT_EQ(cases[i].expected, url.PathForRequestPiece());
    EXPECT_EQ(cases[i].inner_expected == NULL, url.inner_url() == NULL);
    if (url.inner_url() && cases[i].inner_expected) {
      EXPECT_EQ(cases[i].inner_expected, url.inner_url()->PathForRequest());
      EXPECT_EQ(cases[i].inner_expected,
                url.inner_url()->PathForRequestPiece());
    }
  }
}

TEST(GURLTest, EffectiveIntPort) {
  struct PortTest {
    const char* spec;
    int expected_int_port;
  } port_tests[] = {
    // http
    {"http://www.google.com/", 80},
    {"http://www.google.com:80/", 80},
    {"http://www.google.com:443/", 443},

    // https
    {"https://www.google.com/", 443},
    {"https://www.google.com:443/", 443},
    {"https://www.google.com:80/", 80},

    // ftp
    {"ftp://www.google.com/", 21},
    {"ftp://www.google.com:21/", 21},
    {"ftp://www.google.com:80/", 80},

    // file - no port
    {"file://www.google.com/", PORT_UNSPECIFIED},
    {"file://www.google.com:443/", PORT_UNSPECIFIED},

    // data - no port
    {"data:www.google.com:90", PORT_UNSPECIFIED},
    {"data:www.google.com", PORT_UNSPECIFIED},

    // filesystem - no port
    {"filesystem:http://www.google.com:90/t/foo", PORT_UNSPECIFIED},
    {"filesystem:file:///t/foo", PORT_UNSPECIFIED},
  };

  for (size_t i = 0; i < std::size(port_tests); i++) {
    GURL url(port_tests[i].spec);
    EXPECT_EQ(port_tests[i].expected_int_port, url.EffectiveIntPort());
  }
}

TEST(GURLTest, IPAddress) {
  struct IPTest {
    const char* spec;
    bool expected_ip;
  } ip_tests[] = {
    {"http://www.google.com/", false},
    {"http://192.168.9.1/", true},
    {"http://192.168.9.1.2/", false},
    {"http://192.168.m.1/", false},
    {"http://2001:db8::1/", false},
    {"http://[2001:db8::1]/", true},
    {"", false},
    {"some random input!", false},
  };

  for (size_t i = 0; i < std::size(ip_tests); i++) {
    GURL url(ip_tests[i].spec);
    EXPECT_EQ(ip_tests[i].expected_ip, url.HostIsIPAddress());
  }
}

TEST(GURLTest, HostNoBrackets) {
  struct TestCase {
    const char* input;
    const char* expected_host;
    const char* expected_plainhost;
  } cases[] = {
    {"http://www.google.com", "www.google.com", "www.google.com"},
    {"http://[2001:db8::1]/", "[2001:db8::1]", "2001:db8::1"},
    {"http://[::]/", "[::]", "::"},

    // Don't require a valid URL, but don't crash either.
    {"http://[]/", "[]", ""},
    {"http://[x]/", "[x]", "x"},
    {"http://[x/", "[x", "[x"},
    {"http://x]/", "x]", "x]"},
    {"http://[/", "[", "["},
    {"http://]/", "]", "]"},
    {"", "", ""},
  };
  for (size_t i = 0; i < std::size(cases); i++) {
    GURL url(cases[i].input);
    EXPECT_EQ(cases[i].expected_host, url.host());
    EXPECT_EQ(cases[i].expected_plainhost, url.HostNoBrackets());
    EXPECT_EQ(cases[i].expected_plainhost, url.HostNoBracketsPiece());
  }
}

TEST(GURLTest, DomainIs) {
  GURL url_1("http://google.com/foo");
  EXPECT_TRUE(url_1.DomainIs("google.com"));

  // Subdomain and port are ignored.
  GURL url_2("http://www.google.com:99/foo");
  EXPECT_TRUE(url_2.DomainIs("google.com"));

  // Different top-level domain.
  GURL url_3("http://www.google.com.cn/foo");
  EXPECT_FALSE(url_3.DomainIs("google.com"));

  // Different host name.
  GURL url_4("http://www.iamnotgoogle.com/foo");
  EXPECT_FALSE(url_4.DomainIs("google.com"));

  // The input must be lower-cased otherwise DomainIs returns false.
  GURL url_5("http://www.google.com/foo");
  EXPECT_FALSE(url_5.DomainIs("Google.com"));

  // If the URL is invalid, DomainIs returns false.
  GURL invalid_url("google.com");
  EXPECT_FALSE(invalid_url.is_valid());
  EXPECT_FALSE(invalid_url.DomainIs("google.com"));

  GURL url_with_escape_chars("https://www.,.test");
  EXPECT_TRUE(url_with_escape_chars.is_valid());
  EXPECT_EQ(url_with_escape_chars.host(), "www.,.test");
  EXPECT_TRUE(url_with_escape_chars.DomainIs(",.test"));
}

TEST(GURLTest, DomainIsTerminatingDotBehavior) {
  // If the host part ends with a dot, it matches input domains
  // with or without a dot.
  GURL url_with_dot("http://www.google.com./foo");
  EXPECT_TRUE(url_with_dot.DomainIs("google.com"));
  EXPECT_TRUE(url_with_dot.DomainIs("google.com."));
  EXPECT_TRUE(url_with_dot.DomainIs(".com"));
  EXPECT_TRUE(url_with_dot.DomainIs(".com."));

  // But, if the host name doesn't end with a dot and the input
  // domain does, then it's considered to not match.
  GURL url_without_dot("http://google.com/foo");
  EXPECT_FALSE(url_without_dot.DomainIs("google.com."));

  // If the URL ends with two dots, it doesn't match.
  GURL url_with_two_dots("http://www.google.com../foo");
  EXPECT_FALSE(url_with_two_dots.DomainIs("google.com"));
}

TEST(GURLTest, DomainIsWithFilesystemScheme) {
  GURL url_1("filesystem:http://www.google.com:99/foo/");
  EXPECT_TRUE(url_1.DomainIs("google.com"));

  GURL url_2("filesystem:http://www.iamnotgoogle.com/foo/");
  EXPECT_FALSE(url_2.DomainIs("google.com"));
}

// Newlines should be stripped from inputs.
TEST(GURLTest, Newlines) {
  // Constructor.
  GURL url_1(" \t ht\ntp://\twww.goo\rgle.com/as\ndf \n ");
  EXPECT_EQ("http://www.google.com/asdf", url_1.spec());
  EXPECT_FALSE(
      url_1.parsed_for_possibly_invalid_spec().potentially_dangling_markup);

  // Relative path resolver.
  GURL url_2 = url_1.Resolve(" \n /fo\to\r ");
  EXPECT_EQ("http://www.google.com/foo", url_2.spec());
  EXPECT_FALSE(
      url_2.parsed_for_possibly_invalid_spec().potentially_dangling_markup);

  // Constructor.
  GURL url_3(" \t ht\ntp://\twww.goo\rgle.com/as\ndf< \n ");
  EXPECT_EQ("http://www.google.com/asdf%3C", url_3.spec());
  EXPECT_TRUE(
      url_3.parsed_for_possibly_invalid_spec().potentially_dangling_markup);

  // Relative path resolver.
  GURL url_4 = url_1.Resolve(" \n /fo\to<\r ");
  EXPECT_EQ("http://www.google.com/foo%3C", url_4.spec());
  EXPECT_TRUE(
      url_4.parsed_for_possibly_invalid_spec().potentially_dangling_markup);

  // Note that newlines are NOT stripped from ReplaceComponents.
}

TEST(GURLTest, IsStandard) {
  GURL a("http:foo/bar");
  EXPECT_TRUE(a.IsStandard());

  GURL b("foo:bar/baz");
  EXPECT_FALSE(b.IsStandard());

  GURL c("foo://bar/baz");
  EXPECT_FALSE(c.IsStandard());

  GURL d("cid:bar@baz");
  EXPECT_FALSE(d.IsStandard());
}

TEST(GURLTest, SchemeIsHTTPOrHTTPS) {
  EXPECT_TRUE(GURL("http://bar/").SchemeIsHTTPOrHTTPS());
  EXPECT_TRUE(GURL("HTTPS://BAR").SchemeIsHTTPOrHTTPS());
  EXPECT_FALSE(GURL("ftp://bar/").SchemeIsHTTPOrHTTPS());
}

TEST(GURLTest, SchemeIsWSOrWSS) {
  EXPECT_TRUE(GURL("WS://BAR/").SchemeIsWSOrWSS());
  EXPECT_TRUE(GURL("wss://bar/").SchemeIsWSOrWSS());
  EXPECT_FALSE(GURL("http://bar/").SchemeIsWSOrWSS());
}

TEST(GURLTest, SchemeIsCryptographic) {
  EXPECT_TRUE(GURL("https://foo.bar.com/").SchemeIsCryptographic());
  EXPECT_TRUE(GURL("HTTPS://foo.bar.com/").SchemeIsCryptographic());
  EXPECT_TRUE(GURL("HtTpS://foo.bar.com/").SchemeIsCryptographic());

  EXPECT_TRUE(GURL("wss://foo.bar.com/").SchemeIsCryptographic());
  EXPECT_TRUE(GURL("WSS://foo.bar.com/").SchemeIsCryptographic());
  EXPECT_TRUE(GURL("WsS://foo.bar.com/").SchemeIsCryptographic());

  EXPECT_FALSE(GURL("http://foo.bar.com/").SchemeIsCryptographic());
  EXPECT_FALSE(GURL("ws://foo.bar.com/").SchemeIsCryptographic());
}

TEST(GURLTest, SchemeIsCryptographicStatic) {
  EXPECT_TRUE(GURL::SchemeIsCryptographic("https"));
  EXPECT_TRUE(GURL::SchemeIsCryptographic("wss"));
  EXPECT_FALSE(GURL::SchemeIsCryptographic("http"));
  EXPECT_FALSE(GURL::SchemeIsCryptographic("ws"));
  EXPECT_FALSE(GURL::SchemeIsCryptographic("ftp"));
}

TEST(GURLTest, SchemeIsBlob) {
  EXPECT_TRUE(GURL("BLOB://BAR/").SchemeIsBlob());
  EXPECT_TRUE(GURL("blob://bar/").SchemeIsBlob());
  EXPECT_FALSE(GURL("http://bar/").SchemeIsBlob());
}

TEST(GURLTest, SchemeIsLocal) {
  EXPECT_TRUE(GURL("BLOB://BAR/").SchemeIsLocal());
  EXPECT_TRUE(GURL("blob://bar/").SchemeIsLocal());
  EXPECT_TRUE(GURL("DATA:TEXT/HTML,BAR").SchemeIsLocal());
  EXPECT_TRUE(GURL("data:text/html,bar").SchemeIsLocal());
  EXPECT_TRUE(GURL("ABOUT:BAR").SchemeIsLocal());
  EXPECT_TRUE(GURL("about:bar").SchemeIsLocal());
  EXPECT_TRUE(GURL("FILESYSTEM:HTTP://FOO.EXAMPLE/BAR").SchemeIsLocal());
  EXPECT_TRUE(GURL("filesystem:http://foo.example/bar").SchemeIsLocal());

  EXPECT_FALSE(GURL("http://bar/").SchemeIsLocal());
  EXPECT_FALSE(GURL("file:///bar").SchemeIsLocal());
}

// Tests that the 'content' of the URL is properly extracted. This can be
// complex in cases such as multiple schemes (view-source:http:) or for
// javascript URLs. See GURL::GetContent for more details.
TEST(GURLTest, ContentForNonStandardURLs) {
  struct TestCase {
    const char* url;
    const char* expected;
  } cases[] = {
      {"null", ""},
      {"not-a-standard-scheme:this is arbitrary content",
       "this is arbitrary content"},

      // When there are multiple schemes, only the first is excluded from the
      // content. Note also that for e.g. 'http://', the '//' is part of the
      // content not the scheme.
      {"view-source:http://example.com/path", "http://example.com/path"},
      {"blob:http://example.com/GUID", "http://example.com/GUID"},
      {"blob:http://user:password@example.com/GUID",
       "http://user:password@example.com/GUID"},

      // The octothorpe character ('#') marks the end of the URL content, and
      // the start of the fragment. It should not be included in the content.
      {"http://www.example.com/GUID#ref", "www.example.com/GUID"},
      {"http://me:secret@example.com/GUID/#ref", "me:secret@example.com/GUID/"},
      {"data:text/html,Question?<div style=\"color: #bad\">idea</div>",
       "text/html,Question?%3Cdiv%20style=%22color:%20"},

      // TODO(mkwst): This seems like a bug. https://crbug.com/513600
      {"filesystem:http://example.com/path", "/"},

      // Javascript URLs include '#' symbols in their content.
      {"javascript:#", "#"},
      {"javascript:alert('#');", "alert('#');"},
  };

  for (const auto& test : cases) {
    GURL url(test.url);
    EXPECT_EQ(test.expected, url.GetContent()) << test.url;
    EXPECT_EQ(test.expected, url.GetContentPiece()) << test.url;
  }
}

TEST_P(GURLTypedTest, ContentForNonStandardURLs) {
  struct TestCase {
    const std::string_view url;
    const std::string_view expected;
  };

  if (use_standard_compliant_non_special_scheme_url_parsing_) {
    TestCase cases[] = {
        {"blob://http://example.com/GUID", "http//example.com/GUID"},
        {"git://host/path#fragment", "host/path"},
    };
    for (const auto& test : cases) {
      GURL url(test.url);
      EXPECT_EQ(url.GetContent(), test.expected) << test.url;
      EXPECT_EQ(url.GetContentPiece(), test.expected) << test.url;
    }
  } else {
    TestCase cases[] = {
        {"blob://http://example.com/GUID", "//http://example.com/GUID"},
        {"git://host/path#fragment", "//host/path"},
    };
    for (const auto& test : cases) {
      GURL url(test.url);
      EXPECT_EQ(url.GetContent(), test.expected) << test.url;
      EXPECT_EQ(url.GetContentPiece(), test.expected) << test.url;
    }
  }
}

// Tests that the URL path is properly extracted for unusual URLs. This can be
// complex in cases such as multiple schemes (view-source:http:) or when
// octothorpes ('#') are involved.
TEST(GURLTest, PathForNonStandardURLs) {
  struct TestCase {
    const char* url;
    const char* expected;
  } cases[] = {
      {"null", ""},
      {"not-a-standard-scheme:this is arbitrary content",
       "this is arbitrary content"},
      {"view-source:http://example.com/path", "http://example.com/path"},
      {"blob:http://example.com/GUID", "http://example.com/GUID"},
      {"blob:http://user:password@example.com/GUID",
       "http://user:password@example.com/GUID"},

      {"http://www.example.com/GUID#ref", "/GUID"},
      {"http://me:secret@example.com/GUID/#ref", "/GUID/"},
      {"data:text/html,Question?<div style=\"color: #bad\">idea</div>",
       "text/html,Question"},

      // TODO(mkwst): This seems like a bug. https://crbug.com/513600
      {"filesystem:http://example.com/path", "/"},
  };

  for (const auto& test : cases) {
    GURL url(test.url);
    EXPECT_EQ(test.expected, url.path()) << test.url;
  }
}

TEST_P(GURLTypedTest, PathForNonStandardURLs) {
  struct TestCase {
    const std::string_view url;
    const std::string_view expected;
  };

  if (use_standard_compliant_non_special_scheme_url_parsing_) {
    TestCase cases[] = {
        {"blob://http://example.com/GUID", "//example.com/GUID"},
        {"git://host/path#fragment", "/path"},
    };
    for (const auto& test : cases) {
      GURL url(test.url);
      EXPECT_EQ(url.path(), test.expected) << test.url;
    }
  } else {
    TestCase cases[] = {
        {"blob://http://example.com/GUID", "//http://example.com/GUID"},
        {"git://host/path#fragment", "//host/path"},
    };
    for (const auto& test : cases) {
      GURL url(test.url);
      EXPECT_EQ(url.path(), test.expected) << test.url;
    }
  }
}

TEST(GURLTest, EqualsIgnoringRef) {
  const struct {
    const char* url_a;
    const char* url_b;
    bool are_equals;
  } kTestCases[] = {
      // No ref.
      {"http://a.com", "http://a.com", true},
      {"http://a.com", "http://b.com", false},

      // Same Ref.
      {"http://a.com#foo", "http://a.com#foo", true},
      {"http://a.com#foo", "http://b.com#foo", false},

      // Different Refs.
      {"http://a.com#foo", "http://a.com#bar", true},
      {"http://a.com#foo", "http://b.com#bar", false},

      // One has a ref, the other doesn't.
      {"http://a.com#foo", "http://a.com", true},
      {"http://a.com#foo", "http://b.com", false},

      // Empty refs.
      {"http://a.com#", "http://a.com#", true},
      {"http://a.com#", "http://a.com", true},

      // URLs that differ only by their last character.
      {"http://aaa", "http://aab", false},
      {"http://aaa#foo", "http://aab#foo", false},

      // Different size of the part before the ref.
      {"http://123#a", "http://123456#a", false},

      // Blob URLs
      {"blob:http://a.com#foo", "blob:http://a.com#foo", true},
      {"blob:http://a.com#foo", "blob:http://a.com#bar", true},
      {"blob:http://a.com#foo", "blob:http://b.com#bar", false},

      // Filesystem URLs
      {"filesystem:http://a.com#foo", "filesystem:http://a.com#foo", true},
      {"filesystem:http://a.com#foo", "filesystem:http://a.com#bar", true},
      {"filesystem:http://a.com#foo", "filesystem:http://b.com#bar", false},

      // Data URLs
      {"data:text/html,a#foo", "data:text/html,a#bar", true},
      {"data:text/html,a#foo", "data:text/html,a#foo", true},
      {"data:text/html,a#foo", "data:text/html,b#foo", false},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << std::endl
                 << "url_a = " << test_case.url_a << std::endl
                 << "url_b = " << test_case.url_b << std::endl);
    // A versus B.
    EXPECT_EQ(test_case.are_equals,
              GURL(test_case.url_a).EqualsIgnoringRef(GURL(test_case.url_b)));
    // B versus A.
    EXPECT_EQ(test_case.are_equals,
              GURL(test_case.url_b).EqualsIgnoringRef(GURL(test_case.url_a)));
  }
}

TEST(GURLTest, DebugAlias) {
  GURL url("https://foo.com/bar");
  DEBUG_ALIAS_FOR_GURL(url_debug_alias, url);
  EXPECT_STREQ("https://foo.com/bar", url_debug_alias);
}

TEST(GURLTest, InvalidHost) {
  // This contains an invalid percent escape (%T%) and also a valid
  // percent escape that's not 7-bit ascii (%ae), so that the unescaped
  // host contains both an invalid percent escape and invalid UTF-8.
  GURL url("http://%T%Ae");

  EXPECT_FALSE(url.is_valid());
  EXPECT_TRUE(url.SchemeIs(url::kHttpScheme));

  // The invalid percent escape becomes an escaped percent sign (%25), and the
  // invalid UTF-8 character becomes REPLACEMENT CHARACTER' (U+FFFD) encoded as
  // UTF-8.
  EXPECT_EQ(url.host_piece(), "%25t%EF%BF%BD");
}

TEST(GURLTest, PortZero) {
  GURL port_zero_url("http://127.0.0.1:0/blah");

  // https://url.spec.whatwg.org/#port-state says that the port 1) consists of
  // ASCII digits (this excludes negative numbers) and 2) cannot be greater than
  // 2^16-1.  This means that port=0 should be valid.
  EXPECT_TRUE(port_zero_url.is_valid());
  EXPECT_EQ("0", port_zero_url.port());
  EXPECT_EQ("127.0.0.1", port_zero_url.host());
  EXPECT_EQ("http", port_zero_url.scheme());

  // https://crbug.com/1065532: SchemeHostPort would previously incorrectly
  // consider port=0 to be invalid.
  SchemeHostPort scheme_host_port(port_zero_url);
  EXPECT_TRUE(scheme_host_port.IsValid());
  EXPECT_EQ(port_zero_url.scheme(), scheme_host_port.scheme());
  EXPECT_EQ(port_zero_url.host(), scheme_host_port.host());
  EXPECT_EQ(port_zero_url.port(),
            base::NumberToString(scheme_host_port.port()));

  // https://crbug.com/1065532: The SchemeHostPort problem above would lead to
  // bizarre results below - resolved origin would incorrectly be returned as an
  // opaque origin derived from |another_origin|.
  url::Origin another_origin = url::Origin::Create(GURL("http://other.com"));
  url::Origin resolved_origin =
      url::Origin::Resolve(port_zero_url, another_origin);
  EXPECT_FALSE(resolved_origin.opaque());
  EXPECT_EQ(port_zero_url.scheme(), resolved_origin.scheme());
  EXPECT_EQ(port_zero_url.host(), resolved_origin.host());
  EXPECT_EQ(port_zero_url.port(), base::NumberToString(resolved_origin.port()));

  // port=0 and default HTTP port are different.
  GURL default_port("http://127.0.0.1/foo");
  EXPECT_EQ(0, SchemeHostPort(port_zero_url).port());
  EXPECT_EQ(80, SchemeHostPort(default_port).port());
  url::Origin default_port_origin = url::Origin::Create(default_port);
  EXPECT_FALSE(default_port_origin.IsSameOriginWith(resolved_origin));
}

class GURLTestTraits {
 public:
  using UrlType = GURL;

  static UrlType CreateUrlFromString(std::string_view s) { return GURL(s); }
  static bool IsAboutBlank(const UrlType& url) { return url.IsAboutBlank(); }
  static bool IsAboutSrcdoc(const UrlType& url) { return url.IsAboutSrcdoc(); }

  // Only static members.
  GURLTestTraits() = delete;
};

INSTANTIATE_TYPED_TEST_SUITE_P(GURL, AbstractUrlTest, GURLTestTraits);

}  // namespace url
