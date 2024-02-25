// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_GURL_ABSTRACT_TESTS_H_
#define URL_GURL_ABSTRACT_TESTS_H_

// Test suite for tests that cover both url::Url and blink::SecurityUrl.
//
// AbstractUrlTest below abstracts away differences between GURL and blink::KURL
// by parametrizing the tests with a class that has to expose the following
// members:
//   using UrlType = ...;
//   static UrlType CreateUrlFromString(std::string_view s);
//   static bool IsAboutBlank(const UrlType& url);
//   static bool IsAboutSrcdoc(const UrlType& url);
template <typename TUrlTraits>
class AbstractUrlTest : public testing::Test {
 protected:
  // Wrappers that help ellide away TUrlTraits.
  //
  // Note that calling the wrappers needs to be prefixed with `this->...` to
  // avoid hitting: explicit qualification required to use member 'IsAboutBlank'
  // from dependent base class.
  using UrlType = typename TUrlTraits::UrlType;
  UrlType CreateUrlFromString(std::string_view s) {
    return TUrlTraits::CreateUrlFromString(s);
  }
  bool IsAboutBlank(const UrlType& url) {
    return TUrlTraits::IsAboutBlank(url);
  }
  bool IsAboutSrcdoc(const UrlType& url) {
    return TUrlTraits::IsAboutSrcdoc(url);
  }
};

TYPED_TEST_SUITE_P(AbstractUrlTest);

TYPED_TEST_P(AbstractUrlTest, IsAboutBlankTest) {
  // See https://tools.ietf.org/html/rfc6694 which explicitly allows
  // `about-query` and `about-fragment` parts in about: URLs.
  const std::string kAboutBlankUrls[] = {"about:blank", "about:blank?foo",
                                         "about:blank/#foo",
                                         "about:blank?foo#foo"};
  for (const auto& input : kAboutBlankUrls) {
    SCOPED_TRACE(testing::Message() << "Test input: " << input);
    auto url = this->CreateUrlFromString(input);
    EXPECT_TRUE(this->IsAboutBlank(url));
  }

  const std::string kNotAboutBlankUrls[] = {"",
                                            "about",
                                            "about:",
                                            "about:blanky",
                                            "about:blan",
                                            "about:about:blank:",
                                            "data:blank",
                                            "http:blank",
                                            "about://blank",
                                            "about:blank/foo",
                                            "about://:8000/blank",
                                            "about://foo:foo@/blank",
                                            "foo@about:blank",
                                            "foo:bar@about:blank",
                                            "about:blank:8000",
                                            "about:blANk"};
  for (const auto& input : kNotAboutBlankUrls) {
    SCOPED_TRACE(testing::Message() << "Test input: " << input);
    auto url = this->CreateUrlFromString(input);
    EXPECT_FALSE(this->IsAboutBlank(url));
  }
}

TYPED_TEST_P(AbstractUrlTest, IsAboutSrcdocTest) {
  // See https://tools.ietf.org/html/rfc6694 which explicitly allows
  // `about-query` and `about-fragment` parts in about: URLs.
  //
  // `about:srcdoc` is defined in
  // https://html.spec.whatwg.org/multipage/urls-and-fetching.html#about:srcdoc
  // which refers to rfc6694 for details.
  const std::string kAboutSrcdocUrls[] = {
      "about:srcdoc", "about:srcdoc/", "about:srcdoc?foo", "about:srcdoc/#foo",
      "about:srcdoc?foo#foo"};
  for (const auto& input : kAboutSrcdocUrls) {
    SCOPED_TRACE(testing::Message() << "Test input: " << input);
    auto url = this->CreateUrlFromString(input);
    EXPECT_TRUE(this->IsAboutSrcdoc(url));
  }

  const std::string kNotAboutSrcdocUrls[] = {"",
                                             "about",
                                             "about:",
                                             "about:srcdocx",
                                             "about:srcdo",
                                             "about:about:srcdoc:",
                                             "data:srcdoc",
                                             "http:srcdoc",
                                             "about:srcdo",
                                             "about://srcdoc",
                                             "about://srcdoc\\",
                                             "about:srcdoc/foo",
                                             "about://:8000/srcdoc",
                                             "about://foo:foo@/srcdoc",
                                             "foo@about:srcdoc",
                                             "foo:bar@about:srcdoc",
                                             "about:srcdoc:8000",
                                             "about:srCDOc"};
  for (const auto& input : kNotAboutSrcdocUrls) {
    SCOPED_TRACE(testing::Message() << "Test input: " << input);
    auto url = this->CreateUrlFromString(input);
    EXPECT_FALSE(this->IsAboutSrcdoc(url));
  }
}

REGISTER_TYPED_TEST_SUITE_P(AbstractUrlTest,
                            IsAboutBlankTest,
                            IsAboutSrcdocTest);

#endif  // URL_GURL_ABSTRACT_TESTS_H_
