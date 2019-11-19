// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/mojom/url_test.mojom.h"

namespace url {

class UrlTestImpl : public mojom::UrlTest {
 public:
  explicit UrlTestImpl(mojo::PendingReceiver<mojom::UrlTest> receiver)
      : receiver_(this, std::move(receiver)) {}

  // UrlTest:
  void BounceUrl(const GURL& in, BounceUrlCallback callback) override {
    std::move(callback).Run(in);
  }

  void BounceOrigin(const Origin& in, BounceOriginCallback callback) override {
    std::move(callback).Run(in);
  }

 private:
  mojo::Receiver<UrlTest> receiver_;
};

// Mojo version of chrome IPC test in url/ipc/url_param_traits_unittest.cc.
TEST(MojoGURLStructTraitsTest, Basic) {
  base::test::SingleThreadTaskEnvironment task_environment;

  mojo::Remote<mojom::UrlTest> remote;
  UrlTestImpl impl(remote.BindNewPipeAndPassReceiver());

  const char* serialize_cases[] = {
      "http://www.google.com/", "http://user:pass@host.com:888/foo;bar?baz#nop",
  };

  for (size_t i = 0; i < base::size(serialize_cases); i++) {
    GURL input(serialize_cases[i]);
    GURL output;
    EXPECT_TRUE(remote->BounceUrl(input, &output));

    // We want to test each component individually to make sure its range was
    // correctly serialized and deserialized, not just the spec.
    EXPECT_EQ(input.possibly_invalid_spec(), output.possibly_invalid_spec());
    EXPECT_EQ(input.is_valid(), output.is_valid());
    EXPECT_EQ(input.scheme(), output.scheme());
    EXPECT_EQ(input.username(), output.username());
    EXPECT_EQ(input.password(), output.password());
    EXPECT_EQ(input.host(), output.host());
    EXPECT_EQ(input.port(), output.port());
    EXPECT_EQ(input.path(), output.path());
    EXPECT_EQ(input.query(), output.query());
    EXPECT_EQ(input.ref(), output.ref());
  }

  // Test an excessively long GURL.
  {
    const std::string url =
        std::string("http://example.org/").append(kMaxURLChars + 1, 'a');
    GURL input(url.c_str());
    GURL output;
    EXPECT_TRUE(remote->BounceUrl(input, &output));
    EXPECT_TRUE(output.is_empty());
  }

  // Test basic Origin serialization.
  Origin non_unique = Origin::UnsafelyCreateTupleOriginWithoutNormalization(
                          "http", "www.google.com", 80)
                          .value();
  Origin output;
  EXPECT_TRUE(remote->BounceOrigin(non_unique, &output));
  EXPECT_EQ(non_unique, output);
  EXPECT_FALSE(output.opaque());

  Origin unique1;
  Origin unique2 = non_unique.DeriveNewOpaqueOrigin();
  EXPECT_NE(unique1, unique2);
  EXPECT_NE(unique2, unique1);
  EXPECT_NE(unique2, non_unique);
  EXPECT_TRUE(remote->BounceOrigin(unique1, &output));
  EXPECT_TRUE(output.opaque());
  EXPECT_EQ(unique1, output);
  Origin output2;
  EXPECT_TRUE(remote->BounceOrigin(unique2, &output2));
  EXPECT_EQ(unique2, output2);
  EXPECT_NE(unique2, output);
  EXPECT_NE(unique1, output2);

  Origin normalized =
      Origin::CreateFromNormalizedTuple("http", "www.google.com", 80);
  EXPECT_EQ(normalized, non_unique);
  EXPECT_TRUE(remote->BounceOrigin(normalized, &output));
  EXPECT_EQ(normalized, output);
  EXPECT_EQ(non_unique, output);
  EXPECT_FALSE(output.opaque());
}

}  // namespace url
