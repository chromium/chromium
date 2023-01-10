// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libaddressinput/chromium/chrome_metadata_source.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/core/embedder/embedder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

static const char kFakeUrl[] = "https://example.com";
static const char kFakeInsecureUrl[] = "http://example.com";

class ChromeMetadataSourceTest : public testing::Test {
 public:
  ChromeMetadataSourceTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        success_(false) {
    mojo::core::Init();
  }
  virtual ~ChromeMetadataSourceTest() {}

 protected:
  // Sets the response for the download.
  void SetFakeResponse(const std::string& payload, net::HttpStatusCode code) {
    test_url_loader_factory_.AddResponse(url_.spec(), payload, code);
  }

  // Kicks off the download.
  void Get() {
    ChromeMetadataSource impl(std::string(), test_shared_loader_factory_);
    std::unique_ptr<::i18n::addressinput::Source::Callback> callback(
        ::i18n::addressinput::BuildCallback(
            this, &ChromeMetadataSourceTest::OnDownloaded));
    impl.Get(url_.spec(), *callback);
    base::RunLoop().RunUntilIdle();
  }

  void set_url(const GURL& url) { url_ = url; }
  bool success() const { return success_; }
  bool has_data() const { return !!data_; }

  const std::string& data() const {
    DCHECK(data_);
    return *data_;
  }

 private:
  // Callback for when download is finished.
  void OnDownloaded(bool success,
                    const std::string& url,
                    std::string* data) {
    ASSERT_FALSE(success && data == NULL);
    success_ = success;
    data_.reset(data);
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  GURL url_;
  std::unique_ptr<std::string> data_;
  bool success_;
};

TEST_F(ChromeMetadataSourceTest, Success) {
  const char kFakePayload[] = "ham hock";
  set_url(GURL(kFakeUrl));
  SetFakeResponse(kFakePayload, net::HTTP_OK);
  Get();
  EXPECT_TRUE(success());
  EXPECT_EQ(kFakePayload, data());
}

TEST_F(ChromeMetadataSourceTest, Failure) {
  const char kFakePayload[] = "ham hock";
  set_url(GURL(kFakeUrl));
  SetFakeResponse(kFakePayload, net::HTTP_INTERNAL_SERVER_ERROR);
  Get();
  EXPECT_FALSE(success());
  EXPECT_TRUE(!has_data() || data().empty());
}

TEST_F(ChromeMetadataSourceTest, RejectsInsecureScheme) {
  const char kFakePayload[] = "ham hock";
  set_url(GURL(kFakeInsecureUrl));
  SetFakeResponse(kFakePayload, net::HTTP_OK);
  Get();
  EXPECT_FALSE(success());
  EXPECT_TRUE(!has_data() || data().empty());
}

}  // namespace autofill
