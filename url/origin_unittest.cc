// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace url {

void ExpectParsedUrlsEqual(const GURL& a, const GURL& b) {
  EXPECT_EQ(a, b);
  const Parsed& a_parsed = a.parsed_for_possibly_invalid_spec();
  const Parsed& b_parsed = b.parsed_for_possibly_invalid_spec();
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

class OriginTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Add two schemes which are local but nonstandard.
    AddLocalScheme("local-but-nonstandard");
    AddLocalScheme("also-local-but-nonstandard");

    // Add a scheme that's both local and standard.
    AddStandardScheme("local-and-standard", SchemeType::SCHEME_WITH_HOST);
    AddLocalScheme("local-and-standard");

    // Add a scheme that's standard but no-access. We still want these to
    // form valid SchemeHostPorts, even though they always commit as opaque
    // origins, so that they can represent the source of the resource even if
    // it's not committable as a non-opaque origin.
    AddStandardScheme("standard-but-noaccess", SchemeType::SCHEME_WITH_HOST);
    AddNoAccessScheme("standard-but-noaccess");
  }
  void TearDown() override { url::ResetForTests(); }

  ::testing::AssertionResult DoEqualityComparisons(const url::Origin& a,
                                                   const url::Origin& b,
                                                   bool should_compare_equal) {
    ::testing::AssertionResult failure = ::testing::AssertionFailure();
    failure << "DoEqualityComparisons failure. Expecting "
            << (should_compare_equal ? "equality" : "inequality")
            << " between:\n  a\n    Which is: " << a
            << "\n  b\n    Which is: " << b << "\nThe following check failed: ";
    if (a.IsSameOriginWith(b) != should_compare_equal)
      return failure << "a.IsSameOriginWith(b)";
    if (b.IsSameOriginWith(a) != should_compare_equal)
      return failure << "b.IsSameOriginWith(a)";
    if ((a == b) != should_compare_equal)
      return failure << "(a == b)";
    if ((b == a) != should_compare_equal)
      return failure << "(b == a)";
    if ((b != a) != !should_compare_equal)
      return failure << "(b != a)";
    if ((a != b) != !should_compare_equal)
      return failure << "(a != b)";
    return ::testing::AssertionSuccess();
  }

  bool HasNonceTokenBeenInitialized(const url::Origin& origin) {
    EXPECT_TRUE(origin.opaque());
    // Avoid calling nonce_.token() here, to not trigger lazy initialization.
    return !origin.nonce_->token_.is_empty();
  }

  Origin::Nonce CreateNonce() { return Origin::Nonce(); }

  Origin::Nonce CreateNonce(base::UnguessableToken nonce) {
    return Origin::Nonce(nonce);
  }

  base::Optional<base::UnguessableToken> GetNonce(const Origin& origin) {
    return origin.GetNonceForSerialization();
  }

  // Wrapper around url::Origin method to expose it to tests.
  base::Optional<Origin> UnsafelyCreateOpaqueOriginWithoutNormalization(
      base::StringPiece precursor_scheme,
      base::StringPiece precursor_host,
      uint16_t precursor_port,
      const Origin::Nonce& nonce) {
    return Origin::UnsafelyCreateOpaqueOriginWithoutNormalization(
        precursor_scheme, precursor_host, precursor_port, nonce);
  }
};

TEST_F(OriginTest, OpaqueOriginComparison) {
  // A default-constructed Origin should should be cross origin to everything
  // but itself.
  url::Origin opaque_a, opaque_b;
  EXPECT_TRUE(opaque_a.opaque());
  EXPECT_EQ("", opaque_a.scheme());
  EXPECT_EQ("", opaque_a.host());
  EXPECT_EQ(0, opaque_a.port());
  EXPECT_EQ(SchemeHostPort(), opaque_a.GetTupleOrPrecursorTupleIfOpaque());
  EXPECT_TRUE(opaque_a.GetTupleOrPrecursorTupleIfOpaque().IsInvalid());

  EXPECT_TRUE(opaque_b.opaque());
  EXPECT_EQ("", opaque_b.scheme());
  EXPECT_EQ("", opaque_b.host());
  EXPECT_EQ(0, opaque_b.port());
  EXPECT_EQ(SchemeHostPort(), opaque_b.GetTupleOrPrecursorTupleIfOpaque());
  EXPECT_TRUE(opaque_b.GetTupleOrPrecursorTupleIfOpaque().IsInvalid());

  // Two default-constructed Origins should always be cross origin to each
  // other.
  EXPECT_TRUE(DoEqualityComparisons(opaque_a, opaque_b, false));
  EXPECT_TRUE(DoEqualityComparisons(opaque_b, opaque_b, true));
  EXPECT_TRUE(DoEqualityComparisons(opaque_a, opaque_a, true));

  // The streaming operator should not trigger lazy initialization to the token.
  std::ostringstream stream;
  stream << opaque_a;
  EXPECT_STREQ("null [internally: (nonce TBD) anonymous]",
               stream.str().c_str());
  EXPECT_FALSE(HasNonceTokenBeenInitialized(opaque_a));

  // None of the operations thus far should have triggered lazy-generation of
  // the UnguessableToken. Copying an origin, however, should trigger this.
  EXPECT_FALSE(HasNonceTokenBeenInitialized(opaque_a));
  EXPECT_FALSE(HasNonceTokenBeenInitialized(opaque_b));
  opaque_b = opaque_a;

  EXPECT_TRUE(HasNonceTokenBeenInitialized(opaque_a));
  EXPECT_TRUE(HasNonceTokenBeenInitialized(opaque_b));
  EXPECT_TRUE(DoEqualityComparisons(opaque_a, opaque_b, true));
  EXPECT_TRUE(DoEqualityComparisons(opaque_b, opaque_b, true));
  EXPECT_TRUE(DoEqualityComparisons(opaque_a, opaque_a, true));

  // Move-initializing to a fresh Origin should restore the lazy initialization.
  opaque_a = url::Origin();
  EXPECT_FALSE(HasNonceTokenBeenInitialized(opaque_a));
  EXPECT_TRUE(HasNonceTokenBeenInitialized(opaque_b));
  EXPECT_TRUE(DoEqualityComparisons(opaque_a, opaque_b, false));
  EXPECT_TRUE(DoEqualityComparisons(opaque_b, opaque_b, true));
  EXPECT_TRUE(DoEqualityComparisons(opaque_a, opaque_a, true));

  // Comparing two opaque Origins with matching SchemeHostPorts should trigger
  // lazy initialization.
  EXPECT_FALSE(HasNonceTokenBeenInitialized(opaque_a));
  EXPECT_TRUE(HasNonceTokenBeenInitialized(opaque_b));
  bool should_swap = opaque_b < opaque_a;
  EXPECT_TRUE(HasNonceTokenBeenInitialized(opaque_a));
  EXPECT_TRUE(HasNonceTokenBeenInitialized(opaque_b));

  if (should_swap)
    std::swap(opaque_a, opaque_b);
  EXPECT_LT(opaque_a, opaque_b);
  EXPECT_FALSE(opaque_b < opaque_a);

  EXPECT_TRUE(DoEqualityComparisons(opaque_a, opaque_b, false));
  EXPECT_TRUE(DoEqualityComparisons(opaque_b, opaque_b, true));
  EXPECT_TRUE(DoEqualityComparisons(opaque_a, opaque_a, true));

  EXPECT_LT(opaque_a, url::Origin::Create(GURL("http://www.google.com")));
  EXPECT_LT(opaque_b, url::Origin::Create(GURL("http://www.google.com")));

  EXPECT_EQ(opaque_b, url::Origin::Resolve(GURL("about:blank"), opaque_b));
  EXPECT_EQ(opaque_b, url::Origin::Resolve(GURL("about:srcdoc"), opaque_b));
  EXPECT_EQ(opaque_b,
            url::Origin::Resolve(GURL("about:blank?hello#whee"), opaque_b));

  const char* const urls[] = {
      "data:text/html,Hello!",
      "javascript:alert(1)",
      "about:blank",
      "file://example.com:443/etc/passwd",
      "unknown-scheme:foo",
      "unknown-scheme://bar",
      "http",
      "http:",
      "http:/",
      "http://",
      "http://:",
      "http://:1",
      "yay",
      "http::///invalid.example.com/",
      "blob:null/foo",                   // blob:null (actually a valid URL)
      "blob:data:foo",                   // blob + data (which is nonstandard)
      "blob:about://blank/",             // blob + about (which is nonstandard)
      "blob:about:blank/",               // blob + about (which is nonstandard)
      "filesystem:http://example.com/",  // Invalid (missing /type/)
      "filesystem:local-but-nonstandard:baz./type/",  // fs requires standard
      "filesystem:local-but-nonstandard://hostname/type/",
      "filesystem:unknown-scheme://hostname/type/",
      "local-but-nonstandar:foo",  // Prefix of registered scheme.
      "but-nonstandard:foo",       // Suffix of registered scheme.
      "local-and-standard:",       // Standard scheme needs a hostname.
      "standard-but-noaccess:",    // Standard scheme needs a hostname.
      "blob:blob:http://www.example.com/guid-goes-here",  // Double blob.
  };

  for (auto* test_url : urls) {
    SCOPED_TRACE(test_url);
    GURL url(test_url);
    const url::Origin opaque_origin;

    // Opaque origins returned by Origin::Create().
    {
      Origin origin = Origin::Create(url);
      EXPECT_EQ("", origin.scheme());
      EXPECT_EQ("", origin.host());
      EXPECT_EQ(0, origin.port());
      EXPECT_TRUE(origin.opaque());
      // An origin is always same-origin with itself.
      EXPECT_EQ(origin, origin);
      EXPECT_NE(origin, url::Origin());
      EXPECT_EQ(SchemeHostPort(), origin.GetTupleOrPrecursorTupleIfOpaque());
      // A copy of |origin| should be same-origin as well.
      Origin origin_copy = origin;
      EXPECT_EQ("", origin_copy.scheme());
      EXPECT_EQ("", origin_copy.host());
      EXPECT_EQ(0, origin_copy.port());
      EXPECT_TRUE(origin_copy.opaque());
      EXPECT_EQ(origin, origin_copy);
      // And it should always be cross-origin to another opaque Origin.
      EXPECT_NE(origin, opaque_origin);
      // Re-creating from the URL should also be cross-origin.
      EXPECT_NE(origin, Origin::Create(url));

      ExpectParsedUrlsEqual(GURL(origin.Serialize()), origin.GetURL());
    }
  }
}

TEST_F(OriginTest, ConstructFromTuple) {
  struct TestCases {
    const char* const scheme;
    const char* const host;
    const uint16_t port;
  } cases[] = {
      {"http", "example.com", 80},
      {"http", "example.com", 123},
      {"https", "example.com", 443},
  };

  for (const auto& test_case : cases) {
    testing::Message scope_message;
    scope_message << test_case.scheme << "://" << test_case.host << ":"
                  << test_case.port;
    SCOPED_TRACE(scope_message);
    Origin origin = Origin::CreateFromNormalizedTuple(
        test_case.scheme, test_case.host, test_case.port);

    EXPECT_EQ(test_case.scheme, origin.scheme());
    EXPECT_EQ(test_case.host, origin.host());
    EXPECT_EQ(test_case.port, origin.port());
  }
}

TEST_F(OriginTest, ConstructFromGURL) {
  Origin different_origin =
      Origin::Create(GURL("https://not-in-the-list.test/"));

  struct TestCases {
    const char* const url;
    const char* const expected_scheme;
    const char* const expected_host;
    const uint16_t expected_port;
  } cases[] = {
      // IP Addresses
      {"http://192.168.9.1/", "http", "192.168.9.1", 80},
      {"http://[2001:db8::1]/", "http", "[2001:db8::1]", 80},
      {"http://1/", "http", "0.0.0.1", 80},
      {"http://1:1/", "http", "0.0.0.1", 1},
      {"http://3232237825/", "http", "192.168.9.1", 80},

      // Punycode
      {"http://☃.net/", "http", "xn--n3h.net", 80},
      {"blob:http://☃.net/", "http", "xn--n3h.net", 80},

      // Generic URLs
      {"http://example.com/", "http", "example.com", 80},
      {"http://example.com:123/", "http", "example.com", 123},
      {"https://example.com/", "https", "example.com", 443},
      {"https://example.com:123/", "https", "example.com", 123},
      {"http://user:pass@example.com/", "http", "example.com", 80},
      {"http://example.com:123/?query", "http", "example.com", 123},
      {"https://example.com/#1234", "https", "example.com", 443},
      {"https://u:p@example.com:123/?query#1234", "https", "example.com", 123},

      // Registered URLs
      {"ftp://example.com/", "ftp", "example.com", 21},
      {"ws://example.com/", "ws", "example.com", 80},
      {"wss://example.com/", "wss", "example.com", 443},
      {"wss://user:pass@example.com/", "wss", "example.com", 443},

      // Scheme (registered in SetUp()) that's both local and standard.
      // TODO: Is it really appropriate to do network-host canonicalization of
      // schemes without ports?
      {"local-and-standard:20", "local-and-standard", "0.0.0.20", 0},
      {"local-and-standard:20.", "local-and-standard", "0.0.0.20", 0},
      {"local-and-standard:↑↑↓↓←→←→ba.↑↑↓↓←→←→ba.0.bg", "local-and-standard",
       "xn--ba-rzuadaibfa.xn--ba-rzuadaibfa.0.bg", 0},
      {"local-and-standard:foo", "local-and-standard", "foo", 0},
      {"local-and-standard://bar:20", "local-and-standard", "bar", 0},
      {"local-and-standard:baz.", "local-and-standard", "baz.", 0},
      {"local-and-standard:baz..", "local-and-standard", "baz..", 0},
      {"local-and-standard:baz..bar", "local-and-standard", "baz..bar", 0},
      {"local-and-standard:baz...", "local-and-standard", "baz...", 0},

      // Scheme (registered in SetUp()) that's local but nonstandard. These
      // always have empty hostnames, but are allowed to be url::Origins.
      {"local-but-nonstandard:", "local-but-nonstandard", "", 0},
      {"local-but-nonstandard:foo", "local-but-nonstandard", "", 0},
      {"local-but-nonstandard://bar", "local-but-nonstandard", "", 0},
      {"also-local-but-nonstandard://bar", "also-local-but-nonstandard", "", 0},

      // Scheme (registered in SetUp()) that's standard but marked as noaccess.
      // url::Origin doesn't currently take the noaccess property into account,
      // so these aren't expected to result in opaque origins.
      {"standard-but-noaccess:foo", "standard-but-noaccess", "foo", 0},
      {"standard-but-noaccess://bar", "standard-but-noaccess", "bar", 0},

      // file: URLs
      {"file:///etc/passwd", "file", "", 0},
      {"file://example.com/etc/passwd", "file", "example.com", 0},

      // Filesystem:
      {"filesystem:http://example.com/type/", "http", "example.com", 80},
      {"filesystem:http://example.com:123/type/", "http", "example.com", 123},
      {"filesystem:https://example.com/type/", "https", "example.com", 443},
      {"filesystem:https://example.com:123/type/", "https", "example.com", 123},
      {"filesystem:local-and-standard:baz./type/", "local-and-standard", "baz.",
       0},

      // Blob:
      {"blob:http://example.com/guid-goes-here", "http", "example.com", 80},
      {"blob:http://example.com:123/guid-goes-here", "http", "example.com",
       123},
      {"blob:https://example.com/guid-goes-here", "https", "example.com", 443},
      {"blob:http://u:p@example.com/guid-goes-here", "http", "example.com", 80},
  };

  for (const auto& test_case : cases) {
    SCOPED_TRACE(test_case.url);
    GURL url(test_case.url);
    EXPECT_TRUE(url.is_valid());
    Origin origin = Origin::Create(url);
    EXPECT_EQ(test_case.expected_scheme, origin.scheme());
    EXPECT_EQ(test_case.expected_host, origin.host());
    EXPECT_EQ(test_case.expected_port, origin.port());
    EXPECT_FALSE(origin.opaque());
    EXPECT_EQ(origin, origin);
    EXPECT_NE(different_origin, origin);
    EXPECT_NE(origin, different_origin);
    EXPECT_EQ(origin, Origin::Resolve(GURL("about:blank"), origin));
    EXPECT_EQ(origin, Origin::Resolve(GURL("about:blank?bar#foo"), origin));

    ExpectParsedUrlsEqual(GURL(origin.Serialize()), origin.GetURL());

    url::Origin derived_opaque =
        Origin::Resolve(GURL("about:blank?bar#foo"), origin)
            .DeriveNewOpaqueOrigin();
    EXPECT_TRUE(derived_opaque.opaque());
    EXPECT_NE(origin, derived_opaque);
    EXPECT_FALSE(derived_opaque.GetTupleOrPrecursorTupleIfOpaque().IsInvalid());
    EXPECT_EQ(origin.GetTupleOrPrecursorTupleIfOpaque(),
              derived_opaque.GetTupleOrPrecursorTupleIfOpaque());
    EXPECT_EQ(derived_opaque, derived_opaque);

    url::Origin derived_opaque_via_data_url =
        Origin::Resolve(GURL("data:text/html,baz"), origin);
    EXPECT_TRUE(derived_opaque_via_data_url.opaque());
    EXPECT_NE(origin, derived_opaque_via_data_url);
    EXPECT_FALSE(derived_opaque_via_data_url.GetTupleOrPrecursorTupleIfOpaque()
                     .IsInvalid());
    EXPECT_EQ(origin.GetTupleOrPrecursorTupleIfOpaque(),
              derived_opaque_via_data_url.GetTupleOrPrecursorTupleIfOpaque());
    EXPECT_NE(derived_opaque, derived_opaque_via_data_url);
    EXPECT_NE(derived_opaque_via_data_url, derived_opaque);
    EXPECT_NE(derived_opaque.DeriveNewOpaqueOrigin(), derived_opaque);
    EXPECT_EQ(derived_opaque_via_data_url, derived_opaque_via_data_url);
  }
}

TEST_F(OriginTest, Serialization) {
  struct TestCases {
    const char* const url;
    const char* const expected;
    const char* const expected_log;
  } cases[] = {
      {"http://192.168.9.1/", "http://192.168.9.1"},
      {"http://[2001:db8::1]/", "http://[2001:db8::1]"},
      {"http://☃.net/", "http://xn--n3h.net"},
      {"http://example.com/", "http://example.com"},
      {"http://example.com:123/", "http://example.com:123"},
      {"https://example.com/", "https://example.com"},
      {"https://example.com:123/", "https://example.com:123"},
      {"file:///etc/passwd", "file://", "file:// [internally: file://]"},
      {"file://example.com/etc/passwd", "file://",
       "file:// [internally: file://example.com]"},
      {"data:,", "null", "null [internally: (nonce TBD) anonymous]"},
  };

  for (const auto& test_case : cases) {
    SCOPED_TRACE(test_case.url);
    GURL url(test_case.url);
    EXPECT_TRUE(url.is_valid());
    Origin origin = Origin::Create(url);
    std::string serialized = origin.Serialize();
    ExpectParsedUrlsEqual(GURL(serialized), origin.GetURL());

    EXPECT_EQ(test_case.expected, serialized);

    // The '<<' operator sometimes produces additional information.
    std::stringstream out;
    out << origin;
    if (test_case.expected_log)
      EXPECT_EQ(test_case.expected_log, out.str());
    else
      EXPECT_EQ(test_case.expected, out.str());
  }
}

TEST_F(OriginTest, Comparison) {
  // These URLs are arranged in increasing order:
  const char* const urls[] = {
      "data:uniqueness", "http://a:80",  "http://b:80",
      "https://a:80",    "https://b:80", "http://a:81",
      "http://b:81",     "https://a:81", "https://b:81",
  };
  // Validate the comparison logic still works when creating a canonical origin,
  // when any created opaque origins contain a nonce.
  {
    // Pre-create the origins, as the internal nonce for unique origins changes
    // with each freshly-constructed Origin (that's not copied).
    std::vector<Origin> origins;
    for (const auto* test_url : urls)
      origins.push_back(Origin::Create(GURL(test_url)));
    for (size_t i = 0; i < origins.size(); i++) {
      const Origin& current = origins[i];
      for (size_t j = i; j < origins.size(); j++) {
        const Origin& to_compare = origins[j];
        EXPECT_EQ(i < j, current < to_compare) << i << " < " << j;
        EXPECT_EQ(j < i, to_compare < current) << j << " < " << i;
      }
    }
  }
}

TEST_F(OriginTest, UnsafelyCreate) {
  struct TestCase {
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
    SCOPED_TRACE(testing::Message()
                 << test.scheme << "://" << test.host << ":" << test.port);
    base::Optional<url::Origin> origin =
        url::Origin::UnsafelyCreateTupleOriginWithoutNormalization(
            test.scheme, test.host, test.port);
    ASSERT_TRUE(origin);
    EXPECT_EQ(test.scheme, origin->scheme());
    EXPECT_EQ(test.host, origin->host());
    EXPECT_EQ(test.port, origin->port());
    EXPECT_FALSE(origin->opaque());
    EXPECT_TRUE(origin->IsSameOriginWith(*origin));

    ExpectParsedUrlsEqual(GURL(origin->Serialize()), origin->GetURL());

    base::UnguessableToken nonce = base::UnguessableToken::Create();
    base::Optional<url::Origin> opaque_origin =
        UnsafelyCreateOpaqueOriginWithoutNormalization(
            test.scheme, test.host, test.port, CreateNonce(nonce));
    ASSERT_TRUE(opaque_origin);
    EXPECT_TRUE(opaque_origin->opaque());
    EXPECT_FALSE(*opaque_origin == origin);
    EXPECT_EQ(opaque_origin->GetTupleOrPrecursorTupleIfOpaque(),
              origin->GetTupleOrPrecursorTupleIfOpaque());
    EXPECT_EQ(opaque_origin,
              UnsafelyCreateOpaqueOriginWithoutNormalization(
                  test.scheme, test.host, test.port, CreateNonce(nonce)));
    EXPECT_FALSE(*opaque_origin == origin->DeriveNewOpaqueOrigin());
  }
}

TEST_F(OriginTest, UnsafelyCreateUniqueOnInvalidInput) {
  url::AddStandardScheme("host-only", url::SCHEME_WITH_HOST);
  url::AddStandardScheme("host-port-only", url::SCHEME_WITH_HOST_AND_PORT);
  struct TestCases {
    const char* scheme;
    const char* host;
    uint16_t port = 80;
  } cases[] = {{"", "", 33},
               {"data", "", 0},
               {"blob", "", 0},
               {"filesystem", "", 0},
               {"data", "example.com"},
               {"http", "☃.net"},
               {"http\nmore", "example.com"},
               {"http\rmore", "example.com"},
               {"http\n", "example.com"},
               {"http\r", "example.com"},
               {"http", "example.com\nnot-example.com"},
               {"http", "example.com\rnot-example.com"},
               {"http", "example.com\n"},
               {"http", "example.com\r"},
               {"http", "example.com", 0},
               {"unknown-scheme", "example.com"},
               {"host-only", "\r", 0},
               {"host-only", "example.com", 22},
               {"host-port-only", "example.com", 0},
               {"file", ""}};

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << test.scheme << "://" << test.host << ":" << test.port);
    EXPECT_FALSE(UnsafelyCreateOpaqueOriginWithoutNormalization(
        test.scheme, test.host, test.port, CreateNonce()));
    EXPECT_FALSE(url::Origin::UnsafelyCreateTupleOriginWithoutNormalization(
        test.scheme, test.host, test.port));
  }

  // An empty scheme/host/port tuple is not a valid tuple origin.
  EXPECT_FALSE(
      url::Origin::UnsafelyCreateTupleOriginWithoutNormalization("", "", 0));

  // Opaque origins with unknown precursors are allowed.
  base::UnguessableToken token = base::UnguessableToken::Create();
  base::Optional<url::Origin> anonymous_opaque =
      UnsafelyCreateOpaqueOriginWithoutNormalization("", "", 0,
                                                     CreateNonce(token));
  ASSERT_TRUE(anonymous_opaque)
      << "An invalid tuple is a valid input to "
      << "UnsafelyCreateOpaqueOriginWithoutNormalization, so long as it is "
      << "the canonical form of the invalid tuple.";
  EXPECT_TRUE(anonymous_opaque->opaque());
  EXPECT_EQ(GetNonce(anonymous_opaque.value()), token);
  EXPECT_EQ(anonymous_opaque->GetTupleOrPrecursorTupleIfOpaque(),
            url::SchemeHostPort());
}

TEST_F(OriginTest, UnsafelyCreateUniqueViaEmbeddedNulls) {
  struct TestCases {
    base::StringPiece scheme;
    base::StringPiece host;
    uint16_t port = 80;
  } cases[] = {{{"http\0more", 9}, {"example.com", 11}},
               {{"http\0", 5}, {"example.com", 11}},
               {{"\0http", 5}, {"example.com", 11}},
               {{"http"}, {"example.com\0not-example.com", 27}},
               {{"http"}, {"example.com\0", 12}},
               {{"http"}, {"\0example.com", 12}},
               {{""}, {"\0", 1}, 0},
               {{"\0", 1}, {""}, 0}};

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << test.scheme << "://" << test.host << ":" << test.port);
    EXPECT_FALSE(url::Origin::UnsafelyCreateTupleOriginWithoutNormalization(
        test.scheme, test.host, test.port));
    EXPECT_FALSE(UnsafelyCreateOpaqueOriginWithoutNormalization(
        test.scheme, test.host, test.port, CreateNonce()));
  }
}

TEST_F(OriginTest, DomainIs) {
  const struct {
    const char* url;
    const char* lower_ascii_domain;
    bool expected_domain_is;
  } kTestCases[] = {
      {"http://google.com/foo", "google.com", true},
      {"http://www.google.com:99/foo", "google.com", true},
      {"http://www.google.com.cn/foo", "google.com", false},
      {"http://www.google.comm", "google.com", false},
      {"http://www.iamnotgoogle.com/foo", "google.com", false},
      {"http://www.google.com/foo", "Google.com", false},

      // If the host ends with a dot, it matches domains with or without a dot.
      {"http://www.google.com./foo", "google.com", true},
      {"http://www.google.com./foo", "google.com.", true},
      {"http://www.google.com./foo", ".com", true},
      {"http://www.google.com./foo", ".com.", true},

      // But, if the host doesn't end with a dot and the input domain does, then
      // it's considered to not match.
      {"http://google.com/foo", "google.com.", false},

      // If the host ends with two dots, it doesn't match.
      {"http://www.google.com../foo", "google.com", false},

      // Filesystem scheme.
      {"filesystem:http://www.google.com:99/foo/", "google.com", true},
      {"filesystem:http://www.iamnotgoogle.com/foo/", "google.com", false},

      // File scheme.
      {"file:///home/user/text.txt", "", false},
      {"file:///home/user/text.txt", "txt", false},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "(url, domain): (" << test_case.url
                                    << ", " << test_case.lower_ascii_domain
                                    << ")");
    GURL url(test_case.url);
    ASSERT_TRUE(url.is_valid());
    Origin origin = Origin::Create(url);

    EXPECT_EQ(test_case.expected_domain_is,
              origin.DomainIs(test_case.lower_ascii_domain));
    EXPECT_FALSE(
        origin.DeriveNewOpaqueOrigin().DomainIs(test_case.lower_ascii_domain));
  }

  // If the URL is invalid, DomainIs returns false.
  GURL invalid_url("google.com");
  ASSERT_FALSE(invalid_url.is_valid());
  EXPECT_FALSE(Origin::Create(invalid_url).DomainIs("google.com"));

  // Unique origins.
  EXPECT_FALSE(Origin().DomainIs(""));
  EXPECT_FALSE(Origin().DomainIs("com"));
}

TEST_F(OriginTest, DebugAlias) {
  Origin origin1 = Origin::Create(GURL("https://foo.com/bar"));
  DEBUG_ALIAS_FOR_ORIGIN(origin1_debug_alias, origin1);
  EXPECT_STREQ("https://foo.com", origin1_debug_alias);
}

TEST_F(OriginTest, NonStandardScheme) {
  Origin origin = Origin::Create(GURL("cow://"));
  EXPECT_TRUE(origin.opaque());
}
TEST_F(OriginTest, NonStandardSchemeWithAndroidWebViewHack) {
  EnableNonStandardSchemesForAndroidWebView();
  Origin origin = Origin::Create(GURL("cow://"));
  EXPECT_FALSE(origin.opaque());
  EXPECT_EQ("cow", origin.scheme());
  EXPECT_EQ("", origin.host());
  EXPECT_EQ(0, origin.port());
  ResetForTests();
}

TEST_F(OriginTest, CanBeDerivedFrom) {
  Origin opaque_unique_origin = Origin();

  Origin regular_origin = Origin::Create(GURL("https://a.com/"));
  Origin opaque_precursor_origin = regular_origin.DeriveNewOpaqueOrigin();

  Origin file_origin = Origin::Create(GURL("file:///foo/bar"));
  Origin file_opaque_precursor_origin = file_origin.DeriveNewOpaqueOrigin();
  Origin file_host_origin = Origin::Create(GURL("file://a.com/foo/bar"));
  Origin file_host_opaque_precursor_origin =
      file_host_origin.DeriveNewOpaqueOrigin();

  Origin non_standard_scheme_origin =
      Origin::Create(GURL("non-standard-scheme:foo"));
  Origin non_standard_opaque_precursor_origin =
      non_standard_scheme_origin.DeriveNewOpaqueOrigin();

  // Also, add new standard scheme that is local to the test.
  AddStandardScheme("new-standard", SchemeType::SCHEME_WITH_HOST);
  Origin new_standard_origin = Origin::Create(GURL("new-standard://host/"));
  Origin new_standard_opaque_precursor_origin =
      new_standard_origin.DeriveNewOpaqueOrigin();

  // No access schemes always get unique opaque origins.
  Origin no_access_origin =
      Origin::Create(GURL("standard-but-noaccess://b.com"));
  Origin no_access_opaque_precursor_origin =
      no_access_origin.DeriveNewOpaqueOrigin();

  Origin local_non_standard_origin =
      Origin::Create(GURL("local-but-nonstandard://a.com"));
  Origin local_non_standard_opaque_precursor_origin =
      local_non_standard_origin.DeriveNewOpaqueOrigin();

  // Call origin.CanBeDerivedFrom(url) for each of the following test cases
  // and ensure that it returns |expected_value|
  const struct {
    const char* url;
    Origin* origin;
    bool expected_value;
  } kTestCases[] = {
      {"https://a.com", &regular_origin, true},
      // Web URL can commit in an opaque origin with precursor information.
      // Example: iframe sandbox navigated to a.com.
      {"https://a.com", &opaque_precursor_origin, true},
      // URL that comes from the web can never commit in an opaque unique
      // origin. It must have precursor information.
      {"https://a.com", &opaque_unique_origin, false},

      // Cross-origin URLs should never work.
      {"https://b.com", &regular_origin, false},
      {"https://b.com", &opaque_precursor_origin, false},

      // data: URL can never commit in a regular, non-opaque origin.
      {"data:text/html,foo", &regular_origin, false},
      // This is the default case: data: URLs commit in opaque origin carrying
      // precursor information for the origin that created them.
      {"data:text/html,foo", &opaque_precursor_origin, true},
      // Browser-initiated navigations can result in data: URL committing in
      // opaque unique origin.
      {"data:text/html,foo", &opaque_unique_origin, true},

      // about:blank can commit in regular origin (default case for iframes).
      {"about:blank", &regular_origin, true},
      // This can happen if data: URL that originated at a.com creates an
      // about:blank iframe.
      {"about:blank", &opaque_precursor_origin, true},
      // Browser-initiated navigations can result in about:blank URL committing
      // in opaque unique origin.
      {"about:blank", &opaque_unique_origin, true},

      // Default behavior of srcdoc is to inherit the origin of the parent
      // document.
      {"about:srcdoc", &regular_origin, true},
      // This happens for sandboxed srcdoc iframe.
      {"about:srcdoc", &opaque_precursor_origin, true},
      // This can happen with browser-initiated navigation to about:blank or
      // data: URL, which in turn add srcdoc iframe.
      {"about:srcdoc", &opaque_unique_origin, true},

      // Just like srcdoc, blob: URLs can be created in all the cases.
      {"blob:https://a.com/foo", &regular_origin, true},
      {"blob:https://a.com/foo", &opaque_precursor_origin, true},
      {"blob:https://a.com/foo", &opaque_unique_origin, true},

      {"filesystem:https://a.com/foo", &regular_origin, true},
      {"filesystem:https://a.com/foo", &opaque_precursor_origin, true},
      // Unlike blob: URLs, filesystem: ones cannot be created in an unique
      // opaque origin.
      {"filesystem:https://a.com/foo", &opaque_unique_origin, false},

      // file: URLs cannot result in regular web origins, regardless of
      // opaqueness.
      {"file:///etc/passwd", &regular_origin, false},
      {"file:///etc/passwd", &opaque_precursor_origin, false},
      // However, they can result in regular file: origin and an opaque one
      // containing another file: origin as precursor.
      {"file:///etc/passwd", &file_origin, true},
      {"file:///etc/passwd", &file_opaque_precursor_origin, true},
      // It should not be possible to get an opaque unique origin for file:
      // as it is a standard scheme and will always result in a tuple origin
      // or will always be derived by other origin.
      // Note: file:// URLs should become unique opaque origins at some point.
      {"file:///etc/passwd", &opaque_unique_origin, false},

      // The same set as above, but including a host.
      {"file://a.com/etc/passwd", &regular_origin, false},
      {"file://a.com/etc/passwd", &opaque_precursor_origin, false},
      {"file://a.com/etc/passwd", &file_host_origin, true},
      {"file://a.com/etc/passwd", &file_host_opaque_precursor_origin, true},
      {"file://a.com/etc/passwd", &opaque_unique_origin, false},

      // Locally registered standard scheme should behave the same way
      // as built-in standard schemes.
      {"new-standard://host/foo", &new_standard_origin, true},
      {"new-standard://host/foo", &new_standard_opaque_precursor_origin, true},
      {"new-standard://host/foo", &opaque_unique_origin, false},
      {"new-standard://host2/foo", &new_standard_origin, false},
      {"new-standard://host2/foo", &new_standard_opaque_precursor_origin,
       false},

      // A non-standard scheme should never commit in an standard origin or
      // opaque origin with standard precursor information.
      {"non-standard-scheme://a.com/foo", &regular_origin, false},
      {"non-standard-scheme://a.com/foo", &opaque_precursor_origin, false},
      // However, it should be fine to commit in unique opaque origins or in its
      // own origin.
      // Note: since non-standard scheme URLs don't parse out anything
      // but the scheme, using a random different hostname here would work.
      {"non-standard-scheme://b.com/foo2", &opaque_unique_origin, true},
      {"non-standard-scheme://b.com/foo3", &non_standard_scheme_origin, true},
      {"non-standard-scheme://b.com/foo4",
       &non_standard_opaque_precursor_origin, true},

      // No access scheme can only commit in opaque origin.
      {"standard-but-noaccess://a.com/foo", &regular_origin, false},
      {"standard-but-noaccess://a.com/foo", &opaque_precursor_origin, false},
      {"standard-but-noaccess://a.com/foo", &opaque_unique_origin, true},
      {"standard-but-noaccess://a.com/foo", &no_access_origin, false},
      {"standard-but-noaccess://a.com/foo", &no_access_opaque_precursor_origin,
       false},
      {"standard-but-noaccess://b.com/foo", &no_access_origin, false},
      {"standard-but-noaccess://b.com/foo", &no_access_opaque_precursor_origin,
       true},

      // Local schemes can be non-standard, verify they also work as expected.
      {"local-but-nonstandard://a.com", &regular_origin, false},
      {"local-but-nonstandard://a.com", &opaque_precursor_origin, false},
      {"local-but-nonstandard://a.com", &opaque_unique_origin, true},
      {"local-but-nonstandard://a.com", &local_non_standard_origin, true},
      {"local-but-nonstandard://a.com",
       &local_non_standard_opaque_precursor_origin, true},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "(origin, url): (" << *test_case.origin
                                    << ", " << test_case.url << ")");
    EXPECT_EQ(test_case.expected_value,
              test_case.origin->CanBeDerivedFrom(GURL(test_case.url)));
  }
}

TEST_F(OriginTest, GetDebugString) {
  Origin http_origin = Origin::Create(GURL("http://192.168.9.1"));
  EXPECT_STREQ(http_origin.GetDebugString().c_str(), "http://192.168.9.1");

  Origin http_opaque_origin = http_origin.DeriveNewOpaqueOrigin();
  EXPECT_THAT(
      http_opaque_origin.GetDebugString().c_str(),
      ::testing::MatchesRegex(
          "null \\[internally: \\(\\w*\\) derived from http://192.168.9.1\\]"));

  Origin data_origin = Origin::Create(GURL("data:"));
  EXPECT_STREQ(data_origin.GetDebugString().c_str(),
               "null [internally: (nonce TBD) anonymous]");

  // The nonce of the origin will be initialized if a new opaque origin is
  // derived.
  Origin data_derived_origin = data_origin.DeriveNewOpaqueOrigin();
  EXPECT_THAT(
      data_derived_origin.GetDebugString().c_str(),
      ::testing::MatchesRegex("null \\[internally: \\(\\w*\\) anonymous\\]"));

  Origin file_origin = Origin::Create(GURL("file:///etc/passwd"));
  EXPECT_STREQ(file_origin.GetDebugString().c_str(),
               "file:// [internally: file://]");

  Origin file_server_origin =
      Origin::Create(GURL("file://example.com/etc/passwd"));
  EXPECT_STREQ(file_server_origin.GetDebugString().c_str(),
               "file:// [internally: file://example.com]");
}

}  // namespace url
