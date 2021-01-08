// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_ORIGIN_ABSTRACT_TESTS_H_
#define URL_ORIGIN_ABSTRACT_TESTS_H_

#include <string>
#include <type_traits>

#include "base/strings/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_util.h"

namespace url {

// AbstractOriginTest below abstracts away differences between url::Origin and
// blink::SecurityOrigin by parametrizing the tests with a class that has to be
// derived from OriginTraitsBase below.
template <typename TConcreteOriginType>
class OriginTraitsBase {
 public:
  using OriginType = TConcreteOriginType;
  OriginTraitsBase() = default;

  // Constructing an origin.
  virtual OriginType CreateOriginFromString(base::StringPiece s) const = 0;

  // Accessors for origin properties.
  virtual bool IsOpaque(const OriginType& origin) const = 0;
  virtual std::string GetScheme(const OriginType& origin) const = 0;
  virtual std::string GetHost(const OriginType& origin) const = 0;
  virtual uint16_t GetPort(const OriginType& origin) const = 0;

  // This type is non-copyable and non-moveable.
  OriginTraitsBase(const OriginTraitsBase&) = delete;
  OriginTraitsBase& operator=(const OriginTraitsBase&) = delete;
};

// Test suite for tests that cover both url::Origin and blink::SecurityOrigin.
template <typename TOriginTraits>
class AbstractOriginTest : public testing::Test {
  static_assert(
      std::is_base_of<OriginTraitsBase<typename TOriginTraits::OriginType>,
                      TOriginTraits>::value,
      "TOriginTraits needs to expose the right members.");

 protected:
  // Wrappers that allow tests to ignore presence of `origin_traits_`.
  //
  // Note that calling the wrappers needs to be prefixed with `this->...` to
  // avoid hitting: explicit qualification required to use member 'IsOpaque'
  // from dependent base class.
  using OriginType = typename TOriginTraits::OriginType;
  OriginType CreateOriginFromString(base::StringPiece s) const {
    return origin_traits_.CreateOriginFromString(s);
  }
  bool IsOpaque(const OriginType& origin) const {
    return origin_traits_.IsOpaque(origin);
  }
  std::string GetScheme(const OriginType& origin) const {
    return origin_traits_.GetScheme(origin);
  }
  std::string GetHost(const OriginType& origin) const {
    return origin_traits_.GetHost(origin);
  }
  uint16_t GetPort(const OriginType& origin) const {
    return origin_traits_.GetPort(origin);
  }

 private:
  TOriginTraits origin_traits_;
};

TYPED_TEST_SUITE_P(AbstractOriginTest);

TYPED_TEST_P(AbstractOriginTest, NonStandardSchemeWithAndroidWebViewHack) {
  ScopedSchemeRegistryForTests scoped_registry;
  EnableNonStandardSchemesForAndroidWebView();

  // Regression test for https://crbug.com/896059.
  auto origin = this->CreateOriginFromString("cow://");
  EXPECT_FALSE(this->IsOpaque(origin));
  EXPECT_EQ("cow", this->GetScheme(origin));
  EXPECT_EQ("", this->GetHost(origin));
  EXPECT_EQ(0, this->GetPort(origin));

  // about:blank translates into an opaque origin, even in presence of
  // EnableNonStandardSchemesForAndroidWebView.
  origin = this->CreateOriginFromString("about:blank");
  EXPECT_TRUE(this->IsOpaque(origin));
}

REGISTER_TYPED_TEST_SUITE_P(AbstractOriginTest,
                            NonStandardSchemeWithAndroidWebViewHack);

}  // namespace url

#endif  // URL_ORIGIN_ABSTRACT_TESTS_H_
