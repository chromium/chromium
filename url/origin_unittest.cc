// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/origin_abstract_tests.h"
#include "url/url_features.h"
#include "url/url_util.h"

namespace url {

class OriginTest : public ::testing::TestWithParam<bool> {
 public:
  OriginTest()
      : use_standard_compliant_non_special_scheme_url_parsing_(GetParam()) {
    if (use_standard_compliant_non_special_scheme_url_parsing_) {
      scoped_feature_list_.InitAndEnableFeature(
          kStandardCompliantNonSpecialSchemeURLParsing);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          kStandardCompliantNonSpecialSchemeURLParsing);
    }
  }

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

  const base::UnguessableToken* GetNonce(const Origin& origin) {
    return origin.GetNonceForSerialization();
  }

  // Wrappers around url::Origin methods to expose it to tests.

  std::optional<Origin> UnsafelyCreateOpaqueOriginWithoutNormalization(
      std::string_view precursor_scheme,
      std::string_view precursor_host,
      uint16_t precursor_port,
      const Origin::Nonce& nonce) {
    return Origin::UnsafelyCreateOpaqueOriginWithoutNormalization(
        precursor_scheme, precursor_host, precursor_port, nonce);
  }

  std::optional<std::string> SerializeWithNonce(const Origin& origin) {
    return origin.SerializeWithNonce();
  }

  std::optional<std::string> SerializeWithNonceAndInitIfNeeded(Origin& origin) {
    return origin.SerializeWithNonceAndInitIfNeeded();
  }

  std::optional<Origin> Deserialize(const std::string& value) {
    return Origin::Deserialize(value);
  }

 protected:
  struct SerializationTestCase {
    std::string_view url;
    std::string_view expected;
    std::optional<std::string_view> expected_log;
  };

  void TestSerialization(const SerializationTestCase& test_case) const {
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
    if (test_case.expected_log) {
      EXPECT_EQ(test_case.expected_log, out.str());
    } else {
      EXPECT_EQ(test_case.expected, out.str());
    }
  }

  bool use_standard_compliant_non_special_scheme_url_parsing_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  ScopedSchemeRegistryForTests scoped_registry_;
};

INSTANTIATE_TEST_SUITE_P(All, OriginTest, ::testing::Bool());

TEST_P(OriginTest, OpaqueOriginComparison) {
  // A default-constructed Origin should should be cross origin to everything
  // but itself.
  url::Origin opaque_a, opaque_b;
  EXPECT_TRUE(opaque_a.opaque());
  EXPECT_EQ("", opaque_a.scheme());
  EXPECT_EQ("", opaque_a.host());
  EXPECT_EQ(0, opaque_a.port());
  EXPECT_EQ(SchemeHostPort(), opaque_a.GetTupleOrPrecursorTupleIfOpaque());
  EXPECT_FALSE(opaque_a.GetTupleOrPrecursorTupleIfOpaque().IsValid());

  EXPECT_TRUE(opaque_b.opaque());
  EXPECT_EQ("", opaque_b.scheme());
  EXPECT_EQ("", opaque_b.host());
  EXPECT_EQ(0, opaque_b.port());
  EXPECT_EQ(SchemeHostPort(), opaque_b.GetTupleOrPrecursorTupleIfOpaque());
  EXPECT_FALSE(opaque_b.GetTupleOrPrecursorTupleIfOpaque().IsValid());

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

  EXPECT_EQ(opaque_b, url::Origin::Resolve(GURL(), opaque_b));
  EXPECT_EQ(opaque_b, url::Origin::Resolve(GURL("about:blank"), opaque_b));
  EXPECT_EQ(opaque_b, url::Origin::Resolve(GURL("about:srcdoc"), opaque_b));
  EXPECT_EQ(opaque_b,
            url::Origin::Resolve(GURL("about:blank?hello#whee"), opaque_b));
}

TEST_P(OriginTest, ConstructFromTuple) {
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

TEST_P(OriginTest, Serialization) {
  // Common test cases
  SerializationTestCase common_cases[] = {
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
      {"git:", "null", "null [internally: (nonce TBD) anonymous]"},
      {"git:/", "null", "null [internally: (nonce TBD) anonymous]"},
      {"git://host/path", "null", "null [internally: (nonce TBD) anonymous]"},
      {"local-and-standard://host/path", "local-and-standard://host"},
      // A port is omitted if the scheme doesn't have the default port.
      // See SchemeHostPort::SerializeInternal for details.
      {"local-and-standard://host:123/path", "local-and-standard://host"},
      {"standard-but-noaccess://host/path", "null",
       "null [internally: (nonce TBD) anonymous]"},
  };
  for (const auto& test_case : common_cases) {
    TestSerialization(test_case);
  }

  // Flag-dependent test cases
  if (use_standard_compliant_non_special_scheme_url_parsing_) {
    SerializationTestCase cases[] = {
        {"local-but-nonstandard://host/path", "local-but-nonstandard://host"},
        {"local-but-nonstandard://host:123/path",
         "local-but-nonstandard://host"},
    };
    for (const auto& test_case : cases) {
      TestSerialization(test_case);
    }
  } else {
    SerializationTestCase cases[] = {
        {"local-but-nonstandard://host/path", "local-but-nonstandard://"},
        {"local-but-nonstandard://host:123/path", "local-but-nonstandard://"},
    };
    for (const auto& test_case : cases) {
      TestSerialization(test_case);
    }
  }
}

TEST_P(OriginTest, SerializationWithAndroidWebViewHackEnabled) {
  EnableNonStandardSchemesForAndroidWebView();

  if (use_standard_compliant_non_special_scheme_url_parsing_) {
    SerializationTestCase cases[] = {
        {"nonstandard://host/path", "nonstandard://"},
        {"nonstandard://host:123/path", "nonstandard://"},
    };
    for (const auto& test_case : cases) {
      TestSerialization(test_case);
    }
  } else {
  }
}

TEST_P(OriginTest, Comparison) {
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

TEST_P(OriginTest, UnsafelyCreate) {
  struct TestCase {
    const char* scheme;
    const char* host;
    uint16_t port;
  } cases[] = {
      {"http", "example.com", 80},
      {"http", "example.com", 123},
      {"https", "example.com", 443},
      {"https", "example.com", 123},
      {"http", "example.com", 0},  // 0 is a valid port for http.
      {"file", "", 0},             // 0 indicates "no port" for file: scheme.
      {"file", "example.com", 0},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << test.scheme << "://" << test.host << ":" << test.port);
    std::optional<url::Origin> origin =
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
    std::optional<url::Origin> opaque_origin =
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

TEST_P(OriginTest, UnsafelyCreateUniqueOnInvalidInput) {
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
               {"unknown-scheme", "example.com"},
               {"host-only", "\r", 0},
               {"host-only", "example.com", 22},
               {"file", "", 123}};  // file: shouldn't have a port.

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
  std::optional<url::Origin> anonymous_opaque =
      UnsafelyCreateOpaqueOriginWithoutNormalization("", "", 0,
                                                     CreateNonce(token));
  ASSERT_TRUE(anonymous_opaque)
      << "An invalid tuple is a valid input to "
      << "UnsafelyCreateOpaqueOriginWithoutNormalization, so long as it is "
      << "the canonical form of the invalid tuple.";
  EXPECT_TRUE(anonymous_opaque->opaque());
  EXPECT_EQ(*GetNonce(anonymous_opaque.value()), token);
  EXPECT_EQ(anonymous_opaque->GetTupleOrPrecursorTupleIfOpaque(),
            url::SchemeHostPort());
}

TEST_P(OriginTest, UnsafelyCreateUniqueViaEmbeddedNulls) {
  struct TestCases {
    std::string_view scheme;
    std::string_view host;
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

TEST_P(OriginTest, DomainIs) {
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
    SCOPED_TRACE(testing::Message()
                 << "(url, domain): (" << test_case.url << ", "
                 << test_case.lower_ascii_domain << ")");
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

TEST_P(OriginTest, DebugAlias) {
  Origin origin1 = Origin::Create(GURL("https://foo.com/bar"));
  DEBUG_ALIAS_FOR_ORIGIN(origin1_debug_alias, origin1);
  EXPECT_STREQ("https://foo.com", origin1_debug_alias);
}

TEST_P(OriginTest, CanBeDerivedFrom) {
  AddStandardScheme("new-standard", SchemeType::SCHEME_WITH_HOST);
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
  struct TestCase {
    const char* url;
    raw_ptr<Origin> origin;
    bool expected_value;
  };

  const TestCase common_test_cases[] = {
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
      {"standard-but-noaccess://a.com/foo", &no_access_origin, true},
      {"standard-but-noaccess://a.com/foo", &no_access_opaque_precursor_origin,
       true},
      {"standard-but-noaccess://b.com/foo", &no_access_origin, true},
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

  for (const auto& test_case : common_test_cases) {
    SCOPED_TRACE(testing::Message() << "(origin, url): (" << *test_case.origin
                                    << ", " << test_case.url << ")");
    EXPECT_EQ(test_case.expected_value,
              test_case.origin->CanBeDerivedFrom(GURL(test_case.url)));
  }

  // Flag-dependent tests
  const TestCase flag_dependent_test_cases[] = {
      {"local-but-nonstandard://b.com", &local_non_standard_origin,
       !use_standard_compliant_non_special_scheme_url_parsing_},
      {"local-but-nonstandard://b.com",
       &local_non_standard_opaque_precursor_origin,
       !use_standard_compliant_non_special_scheme_url_parsing_},
  };
  for (const auto& test_case : flag_dependent_test_cases) {
    SCOPED_TRACE(testing::Message() << "(origin, url): (" << *test_case.origin
                                    << ", " << test_case.url << ")");
    EXPECT_EQ(test_case.expected_value,
              test_case.origin->CanBeDerivedFrom(GURL(test_case.url)));
  }
}

TEST_P(OriginTest, GetDebugString) {
  Origin http_origin = Origin::Create(GURL("http://192.168.9.1"));
  EXPECT_STREQ(http_origin.GetDebugString().c_str(), "http://192.168.9.1");

  Origin http_opaque_origin = http_origin.DeriveNewOpaqueOrigin();
  EXPECT_THAT(
      http_opaque_origin.GetDebugString().c_str(),
      ::testing::MatchesRegex(
          "null \\[internally: \\(\\w*\\) derived from http://192.168.9.1\\]"));
  EXPECT_THAT(
      http_opaque_origin.GetDebugString(false /* include_nonce */).c_str(),
      ::testing::MatchesRegex(
          "null \\[internally: derived from http://192.168.9.1\\]"));

  Origin data_origin = Origin::Create(GURL("data:"));
  EXPECT_STREQ(data_origin.GetDebugString().c_str(),
               "null [internally: (nonce TBD) anonymous]");

  // The nonce of the origin will be initialized if a new opaque origin is
  // derived.
  Origin data_derived_origin = data_origin.DeriveNewOpaqueOrigin();
  EXPECT_THAT(
      data_derived_origin.GetDebugString().c_str(),
      ::testing::MatchesRegex("null \\[internally: \\(\\w*\\) anonymous\\]"));
  EXPECT_THAT(
      data_derived_origin.GetDebugString(false /* include_nonce */).c_str(),
      ::testing::MatchesRegex("null \\[internally: anonymous\\]"));

  Origin file_origin = Origin::Create(GURL("file:///etc/passwd"));
  EXPECT_STREQ(file_origin.GetDebugString().c_str(),
               "file:// [internally: file://]");

  Origin file_server_origin =
      Origin::Create(GURL("file://example.com/etc/passwd"));
  EXPECT_STREQ(file_server_origin.GetDebugString().c_str(),
               "file:// [internally: file://example.com]");
}

TEST_P(OriginTest, Deserialize) {
  std::vector<GURL> valid_urls = {
      GURL("https://a.com"),         GURL("http://a"),
      GURL("http://a:80"),           GURL("file://a.com/etc/passwd"),
      GURL("file:///etc/passwd"),    GURL("http://192.168.1.1"),
      GURL("http://[2001:db8::1]/"),
  };
  for (const GURL& url : valid_urls) {
    SCOPED_TRACE(url.spec());
    Origin origin = Origin::Create(url);
    std::optional<std::string> serialized = SerializeWithNonce(origin);
    ASSERT_TRUE(serialized);

    std::optional<Origin> deserialized = Deserialize(std::move(*serialized));
    ASSERT_TRUE(deserialized.has_value());

    EXPECT_TRUE(DoEqualityComparisons(origin, deserialized.value(), true));
    EXPECT_EQ(origin.GetDebugString(), deserialized.value().GetDebugString());
  }
}

TEST_P(OriginTest, DeserializeInvalid) {
  EXPECT_EQ(std::nullopt, Deserialize(std::string()));
  EXPECT_EQ(std::nullopt, Deserialize("deadbeef"));
  EXPECT_EQ(std::nullopt, Deserialize("0123456789"));
  EXPECT_EQ(std::nullopt, Deserialize("https://a.com"));
  EXPECT_EQ(std::nullopt, Deserialize("https://192.168.1.1"));
}

TEST_P(OriginTest, SerializeTBDNonce) {
  std::vector<GURL> invalid_urls = {
      GURL("data:uniqueness"),       GURL("data:,"),
      GURL("data:text/html,Hello!"), GURL("javascript:alert(1)"),
      GURL("about:blank"),           GURL("google.com"),
  };
  for (const GURL& url : invalid_urls) {
    SCOPED_TRACE(url.spec());
    Origin origin = Origin::Create(url);
    std::optional<std::string> serialized = SerializeWithNonce(origin);
    std::optional<Origin> deserialized = Deserialize(std::move(*serialized));
    ASSERT_TRUE(deserialized.has_value());

    // Can't use DoEqualityComparisons here since empty nonces are never ==
    // unless they are the same object.
    EXPECT_EQ(origin.GetDebugString(), deserialized.value().GetDebugString());
  }

  {
    // Same basic test as above, but without a GURL to create tuple_.
    Origin opaque;
    std::optional<std::string> serialized = SerializeWithNonce(opaque);
    ASSERT_TRUE(serialized);

    std::optional<Origin> deserialized = Deserialize(std::move(*serialized));
    ASSERT_TRUE(deserialized.has_value());

    // Can't use DoEqualityComparisons here since empty nonces are never ==
    // unless they are the same object.
    EXPECT_EQ(opaque.GetDebugString(), deserialized.value().GetDebugString());
  }

  // Now force initialization of the nonce prior to serialization.
  for (const GURL& url : invalid_urls) {
    SCOPED_TRACE(url.spec());
    Origin origin = Origin::Create(url);
    std::optional<std::string> serialized =
        SerializeWithNonceAndInitIfNeeded(origin);
    std::optional<Origin> deserialized = Deserialize(std::move(*serialized));
    ASSERT_TRUE(deserialized.has_value());

    // The nonce should have been initialized prior to Serialization().
    EXPECT_EQ(origin, deserialized.value());
  }
}

TEST_P(OriginTest, DeserializeValidNonce) {
  Origin opaque;
  GetNonce(opaque);

  std::optional<std::string> serialized = SerializeWithNonce(opaque);
  ASSERT_TRUE(serialized);

  std::optional<Origin> deserialized = Deserialize(std::move(*serialized));
  ASSERT_TRUE(deserialized.has_value());

  EXPECT_TRUE(DoEqualityComparisons(opaque, deserialized.value(), true));
  EXPECT_EQ(opaque.GetDebugString(), deserialized.value().GetDebugString());
}

TEST_P(OriginTest, IsSameOriginWith) {
  url::Origin opaque_origin;
  GURL foo_url = GURL("https://foo.com/path");
  url::Origin foo_origin = url::Origin::Create(foo_url);
  GURL bar_url = GURL("https://bar.com/path");
  url::Origin bar_origin = url::Origin::Create(bar_url);

  EXPECT_FALSE(opaque_origin.IsSameOriginWith(foo_origin));
  EXPECT_FALSE(opaque_origin.IsSameOriginWith(foo_url));

  EXPECT_TRUE(foo_origin.IsSameOriginWith(foo_origin));
  EXPECT_TRUE(foo_origin.IsSameOriginWith(foo_url));

  EXPECT_FALSE(foo_origin.IsSameOriginWith(bar_origin));
  EXPECT_FALSE(foo_origin.IsSameOriginWith(bar_url));

  // Documenting legacy behavior.  This doesn't necessarily mean that the legacy
  // behavior is correct (or desirable in the long-term).
  EXPECT_FALSE(foo_origin.IsSameOriginWith(GURL("about:blank")));
  EXPECT_FALSE(foo_origin.IsSameOriginWith(GURL()));  // Invalid GURL.
  EXPECT_TRUE(foo_origin.IsSameOriginWith(GURL("blob:https://foo.com/guid")));
}

TEST_P(OriginTest, IsSameOriginLocalNonStandardScheme) {
  GURL a_url = GURL("local-but-nonstandard://a.com/");
  GURL b_url = GURL("local-but-nonstandard://b.com/");
  url::Origin a_origin = url::Origin::Create(a_url);
  url::Origin b_origin = url::Origin::Create(b_url);

  EXPECT_TRUE(a_origin.IsSameOriginWith(a_origin));
  EXPECT_TRUE(a_origin.IsSameOriginWith(a_url));

  if (use_standard_compliant_non_special_scheme_url_parsing_) {
    // If the flag is enabled, host and port are also checked.
    EXPECT_FALSE(a_origin.IsSameOriginWith(b_origin));
    EXPECT_FALSE(a_origin.IsSameOriginWith(b_url));
  } else {
    EXPECT_TRUE(a_origin.IsSameOriginWith(b_origin));
    EXPECT_TRUE(a_origin.IsSameOriginWith(b_url));
  }
}

TEST_P(OriginTest, OriginWithAndroidWebViewHackEnabled) {
  EnableNonStandardSchemesForAndroidWebView();

  GURL a_url = GURL("nonstandard://a.com/");
  GURL b_url = GURL("nonstandard://b.com/");
  url::Origin a_origin = url::Origin::Create(a_url);
  url::Origin b_origin = url::Origin::Create(b_url);

  EXPECT_TRUE(a_origin.IsSameOriginWith(a_origin));
  EXPECT_TRUE(a_origin.IsSameOriginWith(a_url));

  // When AndroidWebViewHack is enabled, only a scheme part is checked. Thus,
  // "nonstandard://a.com/" and "nonstandard://b.com/" are considered as the
  // same origin. This is not ideal, given that a host and a port are available
  // when kStandardCompliantNonSpecialSchemeURLParsing flag is enabled, but we
  // can't check a host nor a port to avoid breaking existing WebView code.
  // See https://crbug.com/40063064 for details.
  EXPECT_TRUE(a_origin.IsSameOriginWith(b_origin));
  EXPECT_TRUE(a_origin.IsSameOriginWith(b_url));
  EXPECT_TRUE(a_origin.CanBeDerivedFrom(b_url));

  GURL another_scheme_url = GURL("another-nonstandard://a.com/");
  url::Origin another_scheme_origin = url::Origin::Create(another_scheme_url);
  EXPECT_FALSE(a_origin.IsSameOriginWith(another_scheme_origin));
  EXPECT_FALSE(a_origin.IsSameOriginWith(another_scheme_url));
  EXPECT_FALSE(a_origin.CanBeDerivedFrom(another_scheme_url));
}

INSTANTIATE_TYPED_TEST_SUITE_P(UrlOrigin,
                               AbstractOriginTest,
                               UrlOriginTestTraits);

}  // namespace url
