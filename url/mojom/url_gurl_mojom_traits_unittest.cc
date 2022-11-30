// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

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
  {
    // #1: Try creating a file URL with a non-empty hostname.
    GURL url_without_windows_drive_letter("file://hostname/");
    EXPECT_EQ("/", url_without_windows_drive_letter.path());
    EXPECT_EQ("hostname", url_without_windows_drive_letter.host());
    ExpectSerializationRoundtrips(url_without_windows_drive_letter);
  }

  {
    // #2: Use GURL::Replacement to create a GURL with 1) a path that starts
    // with a Windows drive letter and 2) has a non-empty hostname (inherited
    // from `url_without_windows_drive_letter` above). This used to not go
    // through the DoParseUNC path that normally strips the hostname (for more
    // details, see https://crbug.com/1214098#c4).
    GURL::Replacements repl;
    const std::string kNewPath = "/C:/dir/file.txt";
    repl.SetPathStr(kNewPath);
    GURL url_made_with_replace_components =
        GURL("file://hostname/").ReplaceComponents(repl);

    EXPECT_EQ(kNewPath, url_made_with_replace_components.path());
    EXPECT_EQ("hostname", url_made_with_replace_components.host());
    EXPECT_EQ("file://hostname/C:/dir/file.txt",
              url_made_with_replace_components.spec());
    // This is the MAIN VERIFICATION in this test. This used to fail on Windows,
    // see https://crbug.com/1214098.
    ExpectSerializationRoundtrips(url_made_with_replace_components);
  }

  {
    // #3: Try to create a URL with a Windows drive letter and a non-empty
    // hostname directly.
    GURL url_created_directly("file://hostname/C:/dir/file.txt");
    EXPECT_EQ("/C:/dir/file.txt", url_created_directly.path());
    EXPECT_EQ("hostname", url_created_directly.host());
    EXPECT_EQ("file://hostname/C:/dir/file.txt", url_created_directly.spec());
    ExpectSerializationRoundtrips(url_created_directly);

    // The URL created directly and the URL created through ReplaceComponents
    // should be the same.
    GURL::Replacements repl;
    const std::string kNewPath = "/C:/dir/file.txt";
    repl.SetPathStr(kNewPath);
    GURL url_made_with_replace_components =
        GURL("file://hostname/").ReplaceComponents(repl);
    EXPECT_EQ(url_created_directly.spec(),
              url_made_with_replace_components.spec());
  }

  {
    // #4: Try to create a URL with a Windows drive letter and "localhost" as
    // hostname directly.
    GURL url_created_directly("file://localhost/C:/dir/file.txt");
    EXPECT_EQ("/C:/dir/file.txt", url_created_directly.path());
    EXPECT_EQ("", url_created_directly.host());
    EXPECT_EQ("file:///C:/dir/file.txt", url_created_directly.spec());
    ExpectSerializationRoundtrips(url_created_directly);

    // The URL created directly and the URL created through ReplaceComponents
    // should be the same.
    GURL::Replacements repl;
    const std::string kNewPath = "/C:/dir/file.txt";
    repl.SetPathStr(kNewPath);
    GURL url_made_with_replace_components =
        GURL("file://localhost/").ReplaceComponents(repl);
    EXPECT_EQ(url_created_directly.spec(),
              url_made_with_replace_components.spec());
  }
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

// Test that the "kMaxURLChars" values are the same in url.mojom and
// url_constants.cc.
TEST_F(MojoGURLStructTraitsTest, TestMaxURLChars) {
  EXPECT_EQ(kMaxURLChars, mojom::kMaxURLChars);
}

}  // namespace url
