// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/mojom/scheme_host_port_mojom_traits.h"

#include <string>

#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/mojom/scheme_host_port.mojom.h"
#include "url/scheme_host_port.h"

namespace url {

namespace {

void TestRoundTrip(const url::SchemeHostPort& in) {
  url::SchemeHostPort result;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::SchemeHostPort>(in, result))
      << in.Serialize();
  EXPECT_EQ(in, result) << "Expected " << in.Serialize() << ", but got "
                        << result.Serialize();
}

}  // namespace

TEST(SchemeHostPortMojomTraitsTest, RoundTrip) {
  TestRoundTrip(url::SchemeHostPort());
  TestRoundTrip(url::SchemeHostPort("http", "test", 80));
  TestRoundTrip(url::SchemeHostPort("https", "foo.test", 443));
  TestRoundTrip(url::SchemeHostPort("file", "", 0));
}

}  // namespace url
