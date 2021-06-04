// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

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

class MojoGURLStructTraitsTest : public ::testing::Test {
 public:
  MojoGURLStructTraitsTest()
      : url_test_impl_(url_test_remote_.BindNewPipeAndPassReceiver()) {}

  GURL BounceUrl(const GURL& input) {
    GURL output;
    EXPECT_TRUE(url_test_remote_->BounceUrl(input, &output));
    return output;
  }

  void ExpectSerializationRoundtrips(const GURL& input) {
    SCOPED_TRACE(testing::Message()
                 << "Input GURL: " << input.possibly_invalid_spec());
    GURL output = BounceUrl(input);

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

  Origin BounceOrigin(const Origin& input) {
    Origin output;
    EXPECT_TRUE(url_test_remote_->BounceOrigin(input, &output));
    return output;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment;
  mojo::Remote<mojom::UrlTest> url_test_remote_;
  UrlTestImpl url_test_impl_;
};

// Mojo version of chrome IPC test in url/ipc/url_param_traits_unittest.cc.
TEST_F(MojoGURLStructTraitsTest, Basic) {
  const char* serialize_cases[] = {
      "http://www.google.com/",
      "http://user:pass@host.com:888/foo;bar?baz#nop",
  };

  for (const char* test_input : serialize_cases) {
    SCOPED_TRACE(testing::Message() << "Test input: " << test_input);
    GURL input(test_input);
    ExpectSerializationRoundtrips(input);
  }
}

// Test of an excessively long GURL.
TEST_F(MojoGURLStructTraitsTest, ExcessivelyLongUrl) {
  const std::string url =
      std::string("http://example.org/").append(kMaxURLChars + 1, 'a');
  GURL input(url.c_str());
  GURL output = BounceUrl(input);
  EXPECT_TRUE(output.is_empty());
}

// Test for the GURL testcase based on https://crbug.com/1214098 (which in turn
// was based on ContentSecurityPolicyBrowserTest.FileURLs).
TEST_F(MojoGURLStructTraitsTest, WindowsDriveInPathReplacement) {
  GURL url1("file://hostname/");
  ExpectSerializationRoundtrips(url1);
  EXPECT_EQ("/", url1.path());
  EXPECT_EQ("hostname", url1.host());

  // Use GURL::Replacement to create a GURL with 1) a path that starts with a C:
  // drive letter and 2) has a non-empty hostname (inherited from `url1` above).
  // Without GURL::Replacement we would just get `url2` below, with an empty
  // hostname, because of how DoParseUNC resets the hostname on Win32 (for more
  // details see https://crbug.com/1214098#c4).
  GURL::Replacements repl;
  const std::string kNewPath = "/C:/dir/file.txt";
  repl.SetPath(kNewPath.c_str(), url::Component(0, kNewPath.length()));
  GURL url1_with_replaced_path = url1.ReplaceComponents(repl);
  EXPECT_EQ(kNewPath, url1_with_replaced_path.path());
  EXPECT_EQ("hostname", url1_with_replaced_path.host());

#ifdef WIN32
  // TODO(https://crbug.com/1214098): All GURLs should round-trip when bounced
  // through IPC, but this doesn't work for `url1_with_replaced_path` on
  // Windows.
  GURL roundtrip = BounceUrl(url1_with_replaced_path);
  EXPECT_NE(roundtrip.host(), url1_with_replaced_path.host());
#else
  // This is the MAIN VERIFICATION in this test.  The fact that this
  // verification fails on Windows is the bug tracked in
  // https://crbug.com/1214098.
  ExpectSerializationRoundtrips(url1_with_replaced_path);
#endif

  // On Windows, IPC will serialize/deserialze `url1_with_replaced_path` as
  // `url2` (i.e. it won't round-trip the URL spec).  The test assertions below
  // help illustrate why we can't assert ExpectSerializationRoundtrips above (on
  // Windows).
  EXPECT_EQ("file://hostname/C:/dir/file.txt", url1_with_replaced_path.spec());
  GURL url2(url1_with_replaced_path.spec());
#ifdef WIN32
  EXPECT_NE(url2.spec(), url1_with_replaced_path.spec());
  EXPECT_EQ("", url2.host());
#else
  EXPECT_EQ(url2.spec(), url1_with_replaced_path.spec());
  EXPECT_EQ("hostname", url2.host());
#endif
  EXPECT_EQ(url2.path(), url1_with_replaced_path.path());
  ExpectSerializationRoundtrips(url2);
}

// Test of basic Origin serialization.
TEST_F(MojoGURLStructTraitsTest, OriginSerialization) {
  Origin non_unique = Origin::UnsafelyCreateTupleOriginWithoutNormalization(
                          "http", "www.google.com", 80)
                          .value();
  Origin output = BounceOrigin(non_unique);
  EXPECT_EQ(non_unique, output);
  EXPECT_FALSE(output.opaque());

  Origin unique1;
  Origin unique2 = non_unique.DeriveNewOpaqueOrigin();
  EXPECT_NE(unique1, unique2);
  EXPECT_NE(unique2, unique1);
  EXPECT_NE(unique2, non_unique);
  output = BounceOrigin(unique1);
  EXPECT_TRUE(output.opaque());
  EXPECT_EQ(unique1, output);
  Origin output2 = BounceOrigin(unique2);
  EXPECT_EQ(unique2, output2);
  EXPECT_NE(unique2, output);
  EXPECT_NE(unique1, output2);

  Origin normalized =
      Origin::CreateFromNormalizedTuple("http", "www.google.com", 80);
  EXPECT_EQ(normalized, non_unique);
  output = BounceOrigin(normalized);
  EXPECT_EQ(normalized, output);
  EXPECT_EQ(non_unique, output);
  EXPECT_FALSE(output.opaque());
}

}  // namespace url
