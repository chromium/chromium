// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_ORIGIN_ABSTRACT_TESTS_H_
#define URL_ORIGIN_ABSTRACT_TESTS_H_

#include <initializer_list>
#include <string>
#include <string_view>
#include <type_traits>

#include "base/containers/contains.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"
#include "url/url_features.h"
#include "url/url_util.h"

namespace url {

void ExpectParsedUrlsEqual(const GURL& a, const GURL& b);

// AbstractOriginTest below abstracts away differences between url::Origin and
// blink::SecurityOrigin by parametrizing the tests with a class that has to
// expose the same public members as UrlOriginTestTraits below.
class UrlOriginTestTraits {
 public:
  using OriginType = Origin;

  // Constructing an origin.
  static OriginType CreateOriginFromString(std::string_view s);
  static OriginType CreateUniqueOpaqueOrigin();
  static OriginType CreateWithReferenceOrigin(
      std::string_view url,
      const OriginType& reference_origin);
  static OriginType DeriveNewOpaqueOrigin(const OriginType& reference_origin);

  // Accessors for origin properties.
  static bool IsOpaque(const OriginType& origin);
  static std::string GetScheme(const OriginType& origin);
  static std::string GetHost(const OriginType& origin);
  static uint16_t GetPort(const OriginType& origin);
  static SchemeHostPort GetTupleOrPrecursorTupleIfOpaque(
      const OriginType& origin);

  // Wrappers for other instance methods of OriginType.
  static bool IsSameOrigin(const OriginType& a, const OriginType& b);
  static std::string Serialize(const OriginType& origin);

  // "Accessors" of URL properties.
  //
  // TODO(lukasza): Consider merging together OriginTraitsBase here and
  // UrlTraitsBase in //url/gurl_abstract_tests.h.
  static bool IsValidUrl(std::string_view str);

  // Only static members = no constructors are needed.
  UrlOriginTestTraits() = delete;
};

// Test suite for tests that cover both url::Origin and blink::SecurityOrigin.
template <typename TOriginTraits>
class AbstractOriginTest : public testing::Test {
 public:
  void SetUp() override {
    const char* kSchemesToRegister[] = {
        "noaccess",
        "std-with-host",
        "noaccess-std-with-host",
        "local",
        "local-noaccess",
        "local-std-with-host",
        "local-noaccess-std-with-host",
        "also-local",
        "sec",
        "sec-std-with-host",
        "sec-noaccess",
    };
    for (const char* kScheme : kSchemesToRegister) {
      std::string scheme(kScheme);
      if (base::Contains(scheme, "noaccess"))
        AddNoAccessScheme(kScheme);
      if (base::Contains(scheme, "std-with-host"))
        AddStandardScheme(kScheme, SchemeType::SCHEME_WITH_HOST);
      if (base::Contains(scheme, "local"))
        AddLocalScheme(kScheme);
      if (base::Contains(scheme, "sec"))
        AddSecureScheme(kScheme);
    }
  }

 protected:
  // Wrappers that help ellide away TOriginTraits.
  //
  // Note that calling the wrappers needs to be prefixed with `this->...` to
  // avoid hitting: explicit qualification required to use member 'IsOpaque'
  // from dependent base class.
  using OriginType = typename TOriginTraits::OriginType;
  OriginType CreateOriginFromString(std::string_view s) {
    return TOriginTraits::CreateOriginFromString(s);
  }
  OriginType CreateUniqueOpaqueOrigin() {
    return TOriginTraits::CreateUniqueOpaqueOrigin();
  }
  OriginType CreateWithReferenceOrigin(std::string_view url,
                                       const OriginType& reference_origin) {
    return TOriginTraits::CreateWithReferenceOrigin(url, reference_origin);
  }
  OriginType DeriveNewOpaqueOrigin(const OriginType& reference_origin) {
    return TOriginTraits::DeriveNewOpaqueOrigin(reference_origin);
  }
  bool IsOpaque(const OriginType& origin) {
    return TOriginTraits::IsOpaque(origin);
  }
  std::string GetScheme(const OriginType& origin) {
    return TOriginTraits::GetScheme(origin);
  }
  std::string GetHost(const OriginType& origin) {
    return TOriginTraits::GetHost(origin);
  }
  uint16_t GetPort(const OriginType& origin) {
    return TOriginTraits::GetPort(origin);
  }
  SchemeHostPort GetTupleOrPrecursorTupleIfOpaque(const OriginType& origin) {
    return TOriginTraits::GetTupleOrPrecursorTupleIfOpaque(origin);
  }
  bool IsSameOrigin(const OriginType& a, const OriginType& b) {
    bool is_a_same_with_b = TOriginTraits::IsSameOrigin(a, b);
    bool is_b_same_with_a = TOriginTraits::IsSameOrigin(b, a);
    EXPECT_EQ(is_a_same_with_b, is_b_same_with_a);
    return is_a_same_with_b;
  }
  std::string Serialize(const OriginType& origin) {
    return TOriginTraits::Serialize(origin);
  }
  bool IsValidUrl(std::string_view str) {
    return TOriginTraits::IsValidUrl(str);
  }

#define EXPECT_SAME_ORIGIN(a, b)                                 \
  EXPECT_TRUE(this->IsSameOrigin((a), (b)))                      \
      << "When checking if \"" << this->Serialize(a) << "\" is " \
      << "same-origin with \"" << this->Serialize(b) << "\""

#define EXPECT_CROSS_ORIGIN(a, b)                                \
  EXPECT_FALSE(this->IsSameOrigin((a), (b)))                     \
      << "When checking if \"" << this->Serialize(a) << "\" is " \
      << "cross-origin from \"" << this->Serialize(b) << "\""

  void VerifyOriginInvariants(const OriginType& origin) {
    // An origin is always same-origin with itself.
    EXPECT_SAME_ORIGIN(origin, origin);

    // A copy of |origin| should be same-origin as well.
    auto origin_copy = origin;
    EXPECT_EQ(this->GetScheme(origin), this->GetScheme(origin_copy));
    EXPECT_EQ(this->GetHost(origin), this->GetHost(origin_copy));
    EXPECT_EQ(this->GetPort(origin), this->GetPort(origin_copy));
    EXPECT_EQ(this->IsOpaque(origin), this->IsOpaque(origin_copy));
    EXPECT_SAME_ORIGIN(origin, origin_copy);

    // An origin is always cross-origin from another, unique, opaque origin.
    EXPECT_CROSS_ORIGIN(origin, this->CreateUniqueOpaqueOrigin());

    // An origin is always cross-origin from another tuple origin.
    auto different_tuple_origin =
        this->CreateOriginFromString("https://not-in-the-list.test/");
    EXPECT_CROSS_ORIGIN(origin, different_tuple_origin);

    // Deriving an origin for "about:blank".
    auto about_blank_origin1 =
        this->CreateWithReferenceOrigin("about:blank", origin);
    auto about_blank_origin2 =
        this->CreateWithReferenceOrigin("about:blank?bar#foo", origin);
    EXPECT_SAME_ORIGIN(origin, about_blank_origin1);
    EXPECT_SAME_ORIGIN(origin, about_blank_origin2);

    // Derived opaque origins.
    std::vector<OriginType> derived_origins = {
        this->DeriveNewOpaqueOrigin(origin),
        this->CreateWithReferenceOrigin("data:text/html,baz", origin),
        this->DeriveNewOpaqueOrigin(about_blank_origin1),
    };
    for (size_t i = 0; i < derived_origins.size(); i++) {
      SCOPED_TRACE(testing::Message() << "Derived origin #" << i);
      const OriginType& derived_origin = derived_origins[i];
      EXPECT_TRUE(this->IsOpaque(derived_origin));
      EXPECT_SAME_ORIGIN(derived_origin, derived_origin);
      EXPECT_CROSS_ORIGIN(origin, derived_origin);
      EXPECT_EQ(this->GetTupleOrPrecursorTupleIfOpaque(origin),
                this->GetTupleOrPrecursorTupleIfOpaque(derived_origin));
    }
  }

  void VerifyUniqueOpaqueOriginInvariants(const OriginType& origin) {
    if (!this->IsOpaque(origin)) {
      ADD_FAILURE() << "Got unexpectedly non-opaque origin: "
                    << this->Serialize(origin);
      return;  // Skip other test assertions.
    }

    // Opaque origins should have an "empty" scheme, host and port.
    EXPECT_EQ("", this->GetScheme(origin));
    EXPECT_EQ("", this->GetHost(origin));
    EXPECT_EQ(0, this->GetPort(origin));

    // Unique opaque origins should have an empty precursor tuple.
    EXPECT_EQ(SchemeHostPort(), this->GetTupleOrPrecursorTupleIfOpaque(origin));

    // Serialization test.
    EXPECT_EQ("null", this->Serialize(origin));

    // Invariants that should hold for any origin.
    VerifyOriginInvariants(origin);
  }

  void TestUniqueOpaqueOrigin(std::string_view test_input) {
    auto origin = this->CreateOriginFromString(test_input);
    this->VerifyUniqueOpaqueOriginInvariants(origin);

    // Re-creating from the URL should be cross-origin.
    auto origin_recreated_from_same_input =
        this->CreateOriginFromString(test_input);
    EXPECT_CROSS_ORIGIN(origin, origin_recreated_from_same_input);
  }

  void VerifyTupleOriginInvariants(const OriginType& origin,
                                   const SchemeHostPort& expected_tuple) {
    if (this->IsOpaque(origin)) {
      ADD_FAILURE() << "Got unexpectedly opaque origin";
      return;  // Skip other test assertions.
    }
    SCOPED_TRACE(testing::Message()
                 << "Actual origin: " << this->Serialize(origin));

    // Compare `origin` against the `expected_tuple`.
    EXPECT_EQ(expected_tuple.scheme(), this->GetScheme(origin));
    EXPECT_EQ(expected_tuple.host(), this->GetHost(origin));
    EXPECT_EQ(expected_tuple.port(), this->GetPort(origin));
    EXPECT_EQ(expected_tuple, this->GetTupleOrPrecursorTupleIfOpaque(origin));

    // Serialization test.
    //
    // TODO(lukasza): Consider preserving the hostname when serializing file:
    // URLs.  Dropping the hostname seems incompatible with section 6 of
    // rfc6454.  Even though section 4 says that "the implementation MAY
    // return an implementation-defined value", it seems that Chromium
    // implementation *does* include the hostname in the origin SchemeHostPort
    // tuple.
    if (expected_tuple.scheme() != kFileScheme || expected_tuple.host() == "") {
      EXPECT_SAME_ORIGIN(origin,
                         this->CreateOriginFromString(this->Serialize(origin)));
    }

    // Invariants that should hold for any origin.
    VerifyOriginInvariants(origin);
  }

 private:
  ScopedSchemeRegistryForTests scoped_scheme_registry_;
};

TYPED_TEST_SUITE_P(AbstractOriginTest);

TYPED_TEST_P(AbstractOriginTest, NonStandardSchemeWithAndroidWebViewHack) {
  EnableNonStandardSchemesForAndroidWebView();

  // Regression test for https://crbug.com/896059.
  auto origin = this->CreateOriginFromString("unknown-scheme://");
  EXPECT_FALSE(this->IsOpaque(origin));
  EXPECT_EQ("unknown-scheme", this->GetScheme(origin));
  EXPECT_EQ("", this->GetHost(origin));
  EXPECT_EQ(0, this->GetPort(origin));

  // about:blank translates into an opaque origin, even in presence of
  // EnableNonStandardSchemesForAndroidWebView.
  origin = this->CreateOriginFromString("about:blank");
  EXPECT_TRUE(this->IsOpaque(origin));
}

TYPED_TEST_P(
    AbstractOriginTest,
    AndroidWebViewHackWithStandardCompliantNonSpecialSchemeURLParsing) {
  EnableNonStandardSchemesForAndroidWebView();

  // Manual flag-dependent tests to ensure that the behavior doesn't change
  // whether the flag is enabled or not.
  for (bool flag : {false, true}) {
    base::test::ScopedFeatureList scoped_feature_list;
    if (flag) {
      scoped_feature_list.InitAndEnableFeature(
          kStandardCompliantNonSpecialSchemeURLParsing);
    } else {
      scoped_feature_list.InitAndDisableFeature(
          kStandardCompliantNonSpecialSchemeURLParsing);
    }

    // Non-Standard scheme cases.
    {
      auto origin_a = this->CreateOriginFromString("non-standard://a.com:80");
      // Ensure that a host and a port are discarded.
      EXPECT_EQ(this->GetHost(origin_a), "");
      EXPECT_EQ(this->GetPort(origin_a), 0);
      EXPECT_EQ(this->Serialize(origin_a), "non-standard://");
      EXPECT_FALSE(this->IsOpaque(origin_a));

      // URLs are considered same-origin if their schemes match, even if
      // their host and port are different.
      auto origin_b = this->CreateOriginFromString("non-standard://b.com:90");
      EXPECT_TRUE(this->IsSameOrigin(origin_a, origin_b));

      // URLs are not considered same-origin if their schemes don't match,
      // even if their host and port are same.
      auto another_origin_a =
          this->CreateOriginFromString("another-non-standard://a.com:80");
      EXPECT_FALSE(this->IsSameOrigin(origin_a, another_origin_a));
    }

    // Standard scheme cases.
    {
      // Ensure that the behavior of a standard URL is preserved.
      auto origin_a = this->CreateOriginFromString("https://a.com:80");
      EXPECT_EQ(this->GetHost(origin_a), "a.com");
      EXPECT_EQ(this->GetPort(origin_a), 80);
      EXPECT_EQ(this->Serialize(origin_a), "https://a.com:80");
      EXPECT_FALSE(this->IsOpaque(origin_a));

      auto origin_b = this->CreateOriginFromString("https://b.com:80");
      EXPECT_FALSE(this->IsSameOrigin(origin_a, origin_b));
    }
  }
}

TYPED_TEST_P(AbstractOriginTest, OpaqueOriginsFromValidUrls) {
  const char* kTestCases[] = {
      // Built-in noaccess schemes.
      "data:text/html,Hello!",
      "javascript:alert(1)",
      "about:blank",

      // Opaque blob URLs.
      "blob:null/foo",        // blob:null (actually a valid URL)
      "blob:data:foo",        // blob + data (which is nonstandard)
      "blob:about://blank/",  // blob + about (which is nonstandard)
      "blob:about:blank/",    // blob + about (which is nonstandard)
      "blob:blob:http://www.example.com/guid-goes-here",
      "blob:filesystem:ws:b/.",
      "blob:filesystem:ftp://a/b",
      "blob:blob:file://localhost/foo/bar",
  };

  for (const char* test_input : kTestCases) {
    SCOPED_TRACE(testing::Message() << "Test input: " << test_input);

    // Verify that `origin` is opaque not just because `test_input` results is
    // an invalid URL (because of a typo in the scheme name, or because of a
    // technicality like having no host in a noaccess-std-with-host: scheme).
    EXPECT_TRUE(this->IsValidUrl(test_input));

    this->TestUniqueOpaqueOrigin(test_input);
  }
}

TYPED_TEST_P(AbstractOriginTest, OpaqueOriginsFromInvalidUrls) {
  // TODO(lukasza): Consider moving those to GURL/KURL tests that verify what
  // inputs are parsed as an invalid URL.

  const char* kTestCases[] = {
      // Invalid file: URLs.
      "file://example.com:443/etc/passwd",  // No port expected.

      // Invalid HTTP URLs.
      "http",
      "http:",
      "http:/",
      "http://",
      "http://:",
      "http://:1",
      "http::///invalid.example.com/",
      "http://example.com:65536/",                    // Port out of range.
      "http://example.com:-1/",                       // Port out of range.
      "http://example.com:18446744073709551616/",     // Port = 2^64.
      "http://example.com:18446744073709551616999/",  // Lots of port digits.

      // Invalid filesystem URLs.
      "filesystem:http://example.com/",  // Missing /type/.
      "filesystem:local:baz./type/",
      "filesystem:local://hostname/type/",
      "filesystem:unknown-scheme://hostname/type/",
      "filesystem:filesystem:http://example.org:88/foo/bar",

      // Invalid IP addresses
      "http://[]/",
      "http://[2001:0db8:0000:0000:0000:0000:0000:0000:0001]/",  // 9 groups.

      // Unknown scheme without a colon character (":") gives an invalid URL.
      "unknown-scheme",

      // Standard schemes require a hostname (and result in an opaque origin if
      // the hostname is missing).
      "local-std-with-host:",
      "noaccess-std-with-host:",
  };

  for (const char* test_input : kTestCases) {
    SCOPED_TRACE(testing::Message() << "Test input: " << test_input);

    // All testcases here are expected to represent invalid URLs.
    // an invalid URL (because of a type in scheme name, or because of a
    // technicality like having no host in a noaccess-std-with-host: scheme).
    EXPECT_FALSE(this->IsValidUrl(test_input));

    // Invalid URLs should always result in an opaque origin.
    this->TestUniqueOpaqueOrigin(test_input);
  }
}

TYPED_TEST_P(AbstractOriginTest, TupleOrigins) {
  struct TestCase {
    const char* input;
    SchemeHostPort expected_tuple;
  } kTestCases[] = {
      // file: URLs
      {"file:///etc/passwd", {"file", "", 0}},
      {"file://example.com/etc/passwd", {"file", "example.com", 0}},
      {"file:///", {"file", "", 0}},
      {"file://hostname/C:/dir/file.txt", {"file", "hostname", 0}},

      // HTTP URLs
      {"http://example.com/", {"http", "example.com", 80}},
      {"http://example.com:80/", {"http", "example.com", 80}},
      {"http://example.com:123/", {"http", "example.com", 123}},
      {"http://example.com:0/", {"http", "example.com", 0}},
      {"http://example.com:65535/", {"http", "example.com", 65535}},
      {"https://example.com/", {"https", "example.com", 443}},
      {"https://example.com:443/", {"https", "example.com", 443}},
      {"https://example.com:123/", {"https", "example.com", 123}},
      {"https://example.com:0/", {"https", "example.com", 0}},
      {"https://example.com:65535/", {"https", "example.com", 65535}},
      {"http://user:pass@example.com/", {"http", "example.com", 80}},
      {"http://example.com:123/?query", {"http", "example.com", 123}},
      {"https://example.com/#1234", {"https", "example.com", 443}},
      {"https://u:p@example.com:123/?query#1234",
       {"https", "example.com", 123}},
      {"http://example/", {"http", "example", 80}},

      // Blob URLs.
      {"blob:http://example.com/guid-goes-here", {"http", "example.com", 80}},
      {"blob:http://example.com:123/guid-goes-here",
       {"http", "example.com", 123}},
      {"blob:https://example.com/guid-goes-here",
       {"https", "example.com", 443}},
      {"blob:http://u:p@example.com/guid-goes-here",
       {"http", "example.com", 80}},

      // Filesystem URLs.
      {"filesystem:http://example.com/type/", {"http", "example.com", 80}},
      {"filesystem:http://example.com:123/type/", {"http", "example.com", 123}},
      {"filesystem:https://example.com/type/", {"https", "example.com", 443}},
      {"filesystem:https://example.com:123/type/",
       {"https", "example.com", 123}},
      {"filesystem:local-std-with-host:baz./type/",
       {"local-std-with-host", "baz.", 0}},

      // IP Addresses
      {"http://192.168.9.1/", {"http", "192.168.9.1", 80}},
      {"http://[2001:db8::1]/", {"http", "[2001:db8::1]", 80}},
      {"http://[2001:0db8:0000:0000:0000:0000:0000:0001]/",
       {"http", "[2001:db8::1]", 80}},
      {"http://1/", {"http", "0.0.0.1", 80}},
      {"http://1:1/", {"http", "0.0.0.1", 1}},
      {"http://3232237825/", {"http", "192.168.9.1", 80}},

      // Punycode
      {"http://☃.net/", {"http", "xn--n3h.net", 80}},
      {"blob:http://☃.net/", {"http", "xn--n3h.net", 80}},
      {"local-std-with-host:↑↑↓↓←→←→ba.↑↑↓↓←→←→ba.0.bg",
       {"local-std-with-host", "xn--ba-rzuadaibfa.xn--ba-rzuadaibfa.0.bg", 0}},

      // Registered URLs
      {"ftp://example.com/", {"ftp", "example.com", 21}},
      {"ws://example.com/", {"ws", "example.com", 80}},
      {"wss://example.com/", {"wss", "example.com", 443}},
      {"wss://user:pass@example.com/", {"wss", "example.com", 443}},
  };

  for (const TestCase& test : kTestCases) {
    SCOPED_TRACE(testing::Message() << "Test input: " << test.input);

    // Only valid URLs should translate into valid, non-opaque origins.
    EXPECT_TRUE(this->IsValidUrl(test.input));

    auto origin = this->CreateOriginFromString(test.input);
    this->VerifyTupleOriginInvariants(origin, test.expected_tuple);
  }
}

TYPED_TEST_P(AbstractOriginTest, CustomSchemes_OpaqueOrigins) {
  const char* kTestCases[] = {
      // Unknown scheme
      "unknown-scheme:foo",
      "unknown-scheme://bar",

      // Unknown scheme that is a prefix or suffix of a registered scheme.
      "loca:foo",
      "ocal:foo",
      "local-suffix:foo",
      "prefix-local:foo",

      // Custom no-access schemes translate into an opaque origin (just like the
      // built-in no-access schemes such as about:blank or data:).
      "noaccess-std-with-host:foo",
      "noaccess-std-with-host://bar",
      "noaccess://host",
      "local-noaccess://host",
      "local-noaccess-std-with-host://host",
  };

  for (const char* test_input : kTestCases) {
    SCOPED_TRACE(testing::Message() << "Test input: " << test_input);

    // Verify that `origin` is opaque not just because `test_input` results is
    // an invalid URL (because of a typo in the scheme name, or because of a
    // technicality like having no host in a noaccess-std-with-host: scheme).
    EXPECT_TRUE(this->IsValidUrl(test_input));

    this->TestUniqueOpaqueOrigin(test_input);
  }
}

TYPED_TEST_P(AbstractOriginTest, CustomSchemes_TupleOrigins) {
  struct TestCase {
    const char* input;
    SchemeHostPort expected_tuple;
  } kTestCases[] = {
      // Scheme (registered in SetUp()) that's both local and standard.
      // TODO: Is it really appropriate to do network-host canonicalization of
      // schemes without ports?
      {"local-std-with-host:20", {"local-std-with-host", "0.0.0.20", 0}},
      {"local-std-with-host:20.", {"local-std-with-host", "0.0.0.20", 0}},
      {"local-std-with-host:foo", {"local-std-with-host", "foo", 0}},
      {"local-std-with-host://bar:20", {"local-std-with-host", "bar", 0}},
      {"local-std-with-host:baz.", {"local-std-with-host", "baz.", 0}},
      {"local-std-with-host:baz..", {"local-std-with-host", "baz..", 0}},
      {"local-std-with-host:baz..bar", {"local-std-with-host", "baz..bar", 0}},
      {"local-std-with-host:baz...", {"local-std-with-host", "baz...", 0}},

      // Scheme (registered in SetUp()) that's local but nonstandard. These
      // always have empty hostnames, but are allowed to be url::Origins.
      {"local:", {"local", "", 0}},
      {"local:foo", {"local", "", 0}},

      {"std-with-host://host", {"std-with-host", "host", 0}},
      {"local-std-with-host://host", {"local-std-with-host", "host", 0}},
  };

  for (const TestCase& test : kTestCases) {
    SCOPED_TRACE(testing::Message() << "Test input: " << test.input);

    // Only valid URLs should translate into valid, non-opaque origins.
    EXPECT_TRUE(this->IsValidUrl(test.input));

    auto origin = this->CreateOriginFromString(test.input);
    this->VerifyTupleOriginInvariants(origin, test.expected_tuple);
  }
}

TYPED_TEST_P(AbstractOriginTest,
             CustomSchemes_TupleOrigins_StandardCompliantNonSpecialSchemeFlag) {
  // Manual flag-dependent tests.
  //
  // See AbstractOriginTest/CustomSchemes_TupleOrigins, which covers common
  // test cases.
  for (bool flag : {false, true}) {
    // Note: The feature must be set before we construct test cases because
    // SchemeHostPort's constructor changes its behavior.
    base::test::ScopedFeatureList scoped_feature_list;
    if (flag) {
      scoped_feature_list.InitAndEnableFeature(
          kStandardCompliantNonSpecialSchemeURLParsing);
    } else {
      scoped_feature_list.InitAndDisableFeature(
          kStandardCompliantNonSpecialSchemeURLParsing);
    }

    struct TestCase {
      std::string_view input;
      SchemeHostPort expected_tuple_when_standard_compliant_flag_off;
      SchemeHostPort expected_tuple_when_standard_compliant_flag_on;
    } test_cases[] = {
        {"local://bar", {"local", "", 0}, {"local", "bar", 0}},
        {"also-local://bar", {"also-local", "", 0}, {"also-local", "bar", 0}},
    };
    for (const TestCase& test : test_cases) {
      SCOPED_TRACE(testing::Message() << "Test input: " << test.input);
      EXPECT_TRUE(this->IsValidUrl(test.input));
      auto origin = this->CreateOriginFromString(test.input);
      this->VerifyTupleOriginInvariants(
          origin, flag ? test.expected_tuple_when_standard_compliant_flag_on
                       : test.expected_tuple_when_standard_compliant_flag_off);
    }
  }
}

REGISTER_TYPED_TEST_SUITE_P(
    AbstractOriginTest,
    NonStandardSchemeWithAndroidWebViewHack,
    AndroidWebViewHackWithStandardCompliantNonSpecialSchemeURLParsing,
    OpaqueOriginsFromValidUrls,
    OpaqueOriginsFromInvalidUrls,
    TupleOrigins,
    CustomSchemes_OpaqueOrigins,
    CustomSchemes_TupleOrigins,
    CustomSchemes_TupleOrigins_StandardCompliantNonSpecialSchemeFlag);

}  // namespace url

#endif  // URL_ORIGIN_ABSTRACT_TESTS_H_
