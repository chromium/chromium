// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_util.h"

namespace {

class SchemeHostPortTest : public testing::Test {
 public:
  SchemeHostPortTest() = default;
  ~SchemeHostPortTest() override {
    // Reset any added schemes.
    url::ResetForTests();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SchemeHostPortTest);
};

void ExpectParsedUrlsEqual(const GURL& a, const GURL& b) {
  EXPECT_EQ(a, b);
  const url::Parsed& a_parsed = a.parsed_for_possibly_invalid_spec();
  const url::Parsed& b_parsed = b.parsed_for_possibly_invalid_spec();
  EXPECT_EQ(a_parsed.scheme.begin, b_parsed.scheme.begin);
  EXPECT_EQ(a_parsed.scheme.len, b_parsed.scheme.len);
  EXPECT_EQ(a_parsed.username.begin, b_parsed.username.begin);
  EXPECT_EQ(a_parsed.username.len, b_parsed.username.len);
  EXPECT_EQ(a_parsed.password.begin, b_parsed.password.begin);
  EXPECT_EQ(a_parsed.password.len, b_parsed.password.len);
  EXPECT_EQ(a_parsed.host.begin, b_parsed.host.begin);
  EXPECT_EQ(a_parsed.host.len, b_parsed.host.len);
  EXPECT_EQ(a_parsed.port.begin, b_parsed.port.begin);
  EXPECT_EQ(a_parsed.port.len, b_parsed.port.len);
  EXPECT_EQ(a_parsed.path.begin, b_parsed.path.begin);
  EXPECT_EQ(a_parsed.path.len, b_parsed.path.len);
  EXPECT_EQ(a_parsed.query.begin, b_parsed.query.begin);
  EXPECT_EQ(a_parsed.query.len, b_parsed.query.len);
  EXPECT_EQ(a_parsed.ref.begin, b_parsed.ref.begin);
  EXPECT_EQ(a_parsed.ref.len, b_parsed.ref.len);
}

TEST_F(SchemeHostPortTest, Invalid) {
  url::SchemeHostPort invalid;
  EXPECT_EQ("", invalid.scheme());
  EXPECT_EQ("", invalid.host());
  EXPECT_EQ(0, invalid.port());
  EXPECT_TRUE(invalid.IsInvalid());
  EXPECT_EQ(invalid, invalid);

  const char* urls[] = {
      "data:text/html,Hello!", "javascript:alert(1)",
      "file://example.com:443/etc/passwd",

      // These schemes do not follow the generic URL syntax, so make sure we
      // treat them as invalid (scheme, host, port) tuples (even though such
      // URLs' _Origin_ might have a (scheme, host, port) tuple, they themselves
      // do not). This is only *implicitly* checked in the code, by means of
      // blob schemes not being standard, and filesystem schemes having type
      // SCHEME_WITHOUT_AUTHORITY. If conditions change such that the implicit
      // checks no longer hold, this policy should be made explicit.
      "blob:https://example.com/uuid-goes-here",
      "filesystem:https://example.com/temporary/yay.png"};

  for (auto* test : urls) {
    SCOPED_TRACE(test);
    GURL url(test);
    url::SchemeHostPort tuple(url);
    EXPECT_EQ("", tuple.scheme());
    EXPECT_EQ("", tuple.host());
    EXPECT_EQ(0, tuple.port());
    EXPECT_TRUE(tuple.IsInvalid());
    EXPECT_EQ(tuple, tuple);
    EXPECT_EQ(tuple, invalid);
    EXPECT_EQ(invalid, tuple);
    ExpectParsedUrlsEqual(GURL(tuple.Serialize()), tuple.GetURL());
  }
}

TEST_F(SchemeHostPortTest, ExplicitConstruction) {
  struct TestCases {
    const char* scheme;
    const char* host;
    uint16_t port;
  } cases[] = {
      {"http", "example.com", 80},
      {"http", "example.com", 123},
      {"https", "example.com", 443},
      {"https", "example.com", 123},
      {"file", "", 0},
      {"file", "example.com", 0},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << test.scheme << "://" << test.host << ":"
                                    << test.port);
    url::SchemeHostPort tuple(test.scheme, test.host, test.port);
    EXPECT_EQ(test.scheme, tuple.scheme());
    EXPECT_EQ(test.host, tuple.host());
    EXPECT_EQ(test.port, tuple.port());
    EXPECT_FALSE(tuple.IsInvalid());
    EXPECT_EQ(tuple, tuple);
    ExpectParsedUrlsEqual(GURL(tuple.Serialize()), tuple.GetURL());
  }
}

TEST_F(SchemeHostPortTest, InvalidConstruction) {
  struct TestCases {
    const char* scheme;
    const char* host;
    uint16_t port;
  } cases[] = {{"", "", 0},
               {"data", "", 0},
               {"blob", "", 0},
               {"filesystem", "", 0},
               {"http", "", 80},
               {"data", "example.com", 80},
               {"http", "☃.net", 80},
               {"http\nmore", "example.com", 80},
               {"http\rmore", "example.com", 80},
               {"http\n", "example.com", 80},
               {"http\r", "example.com", 80},
               {"http", "example.com\nnot-example.com", 80},
               {"http", "example.com\rnot-example.com", 80},
               {"http", "example.com\n", 80},
               {"http", "example.com\r", 80},
               {"http", "example.com", 0},
               {"file", "", 80}};

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << test.scheme << "://" << test.host << ":"
                                    << test.port);
    url::SchemeHostPort tuple(test.scheme, test.host, test.port);
    EXPECT_EQ("", tuple.scheme());
    EXPECT_EQ("", tuple.host());
    EXPECT_EQ(0, tuple.port());
    EXPECT_TRUE(tuple.IsInvalid());
    EXPECT_EQ(tuple, tuple);
    ExpectParsedUrlsEqual(GURL(tuple.Serialize()), tuple.GetURL());
  }
}

TEST_F(SchemeHostPortTest, InvalidConstructionWithEmbeddedNulls) {
  struct TestCases {
    const char* scheme;
    size_t scheme_length;
    const char* host;
    size_t host_length;
    uint16_t port;
  } cases[] = {{"http\0more", 9, "example.com", 11, 80},
               {"http\0", 5, "example.com", 11, 80},
               {"\0http", 5, "example.com", 11, 80},
               {"http", 4, "example.com\0not-example.com", 27, 80},
               {"http", 4, "example.com\0", 12, 80},
               {"http", 4, "\0example.com", 12, 80}};

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << test.scheme << "://" << test.host << ":"
                                    << test.port);
    url::SchemeHostPort tuple(std::string(test.scheme, test.scheme_length),
                              std::string(test.host, test.host_length),
                              test.port);
    EXPECT_EQ("", tuple.scheme());
    EXPECT_EQ("", tuple.host());
    EXPECT_EQ(0, tuple.port());
    EXPECT_TRUE(tuple.IsInvalid());
    ExpectParsedUrlsEqual(GURL(tuple.Serialize()), tuple.GetURL());
  }
}

TEST_F(SchemeHostPortTest, GURLConstruction) {
  struct TestCases {
    const char* url;
    const char* scheme;
    const char* host;
    uint16_t port;
  } cases[] = {
      {"http://192.168.9.1/", "http", "192.168.9.1", 80},
      {"http://[2001:db8::1]/", "http", "[2001:db8::1]", 80},
      {"http://☃.net/", "http", "xn--n3h.net", 80},
      {"http://example.com/", "http", "example.com", 80},
      {"http://example.com:123/", "http", "example.com", 123},
      {"https://example.com/", "https", "example.com", 443},
      {"https://example.com:123/", "https", "example.com", 123},
      {"file:///etc/passwd", "file", "", 0},
      {"file://example.com/etc/passwd", "file", "example.com", 0},
      {"http://u:p@example.com/", "http", "example.com", 80},
      {"http://u:p@example.com/path", "http", "example.com", 80},
      {"http://u:p@example.com/path?123", "http", "example.com", 80},
      {"http://u:p@example.com/path?123#hash", "http", "example.com", 80},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(test.url);
    GURL url(test.url);
    EXPECT_TRUE(url.is_valid());
    url::SchemeHostPort tuple(url);
    EXPECT_EQ(test.scheme, tuple.scheme());
    EXPECT_EQ(test.host, tuple.host());
    EXPECT_EQ(test.port, tuple.port());
    EXPECT_FALSE(tuple.IsInvalid());
    EXPECT_EQ(tuple, tuple);
    ExpectParsedUrlsEqual(GURL(tuple.Serialize()), tuple.GetURL());
  }
}

TEST_F(SchemeHostPortTest, Serialization) {
  struct TestCases {
    const char* url;
    const char* expected;
  } cases[] = {
      {"http://192.168.9.1/", "http://192.168.9.1"},
      {"http://[2001:db8::1]/", "http://[2001:db8::1]"},
      {"http://☃.net/", "http://xn--n3h.net"},
      {"http://example.com/", "http://example.com"},
      {"http://example.com:123/", "http://example.com:123"},
      {"https://example.com/", "https://example.com"},
      {"https://example.com:123/", "https://example.com:123"},
      {"file:///etc/passwd", "file://"},
      {"file://example.com/etc/passwd", "file://example.com"},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(test.url);
    GURL url(test.url);
    url::SchemeHostPort tuple(url);
    EXPECT_EQ(test.expected, tuple.Serialize());
    ExpectParsedUrlsEqual(GURL(tuple.Serialize()), tuple.GetURL());
  }
}

TEST_F(SchemeHostPortTest, Comparison) {
  // These tuples are arranged in increasing order:
  struct SchemeHostPorts {
    const char* scheme;
    const char* host;
    uint16_t port;
  } tuples[] = {
      {"http", "a", 80},
      {"http", "b", 80},
      {"https", "a", 80},
      {"https", "b", 80},
      {"http", "a", 81},
      {"http", "b", 81},
      {"https", "a", 81},
      {"https", "b", 81},
  };

  for (size_t i = 0; i < base::size(tuples); i++) {
    url::SchemeHostPort current(tuples[i].scheme, tuples[i].host,
                                tuples[i].port);
    for (size_t j = i; j < base::size(tuples); j++) {
      url::SchemeHostPort to_compare(tuples[j].scheme, tuples[j].host,
                                     tuples[j].port);
      EXPECT_EQ(i < j, current < to_compare) << i << " < " << j;
      EXPECT_EQ(j < i, to_compare < current) << j << " < " << i;
    }
  }
}

// Some schemes have optional authority. Make sure that GURL conversion from
// SchemeHostPort is not opinionated in that regard. For more info, See
// crbug.com/820194, where we considered all SchemeHostPorts with
// SCHEME_WITH_HOST (i.e., without ports) as valid with empty hosts, even though
// most are not (e.g. chrome URLs).
TEST_F(SchemeHostPortTest, EmptyHostGurlConversion) {
  url::AddStandardScheme("chrome", url::SCHEME_WITH_HOST);

  GURL chrome_url("chrome:");
  EXPECT_FALSE(chrome_url.is_valid());

  url::SchemeHostPort chrome_tuple("chrome", "", 0);
  EXPECT_FALSE(chrome_tuple.GetURL().is_valid());
  ExpectParsedUrlsEqual(GURL(chrome_tuple.Serialize()), chrome_tuple.GetURL());
  ExpectParsedUrlsEqual(chrome_url, chrome_tuple.GetURL());
}

}  // namespace url
