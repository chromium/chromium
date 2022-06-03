// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/test/content_test_suite_base.h"
#include "content/public/test/unittest_test_suite.h"
#include "weblayer/browser/content_browser_client_impl.h"
#include "weblayer/common/content_client_impl.h"
#include "weblayer/utility/content_utility_client_impl.h"

namespace weblayer {
namespace {
class WebLayerUnitTestSuiteInitializer
    : public testing::EmptyTestEventListener {
 public:
  WebLayerUnitTestSuiteInitializer() = default;
  ~WebLayerUnitTestSuiteInitializer() override = default;

  WebLayerUnitTestSuiteInitializer(const WebLayerUnitTestSuiteInitializer&) =
      delete;
  WebLayerUnitTestSuiteInitializer& operator=(
      const WebLayerUnitTestSuiteInitializer&) = delete;

  void OnTestStart(const testing::TestInfo& test_info) override {
    content_client_ = std::make_unique<ContentClientImpl>();
    content::SetContentClient(content_client_.get());

    browser_content_client_ =
        std::make_unique<ContentBrowserClientImpl>(nullptr);
    content::SetBrowserClientForTesting(browser_content_client_.get());

    utility_content_client_ = std::make_unique<ContentUtilityClientImpl>();
    content::SetUtilityClientForTesting(utility_content_client_.get());
  }

  void OnTestEnd(const testing::TestInfo& test_info) override {
    browser_content_client_ = nullptr;
    utility_content_client_ = nullptr;
    content_client_ = nullptr;
    content::SetContentClient(nullptr);
  }

 private:
  std::unique_ptr<ContentClientImpl> content_client_;
  std::unique_ptr<ContentBrowserClientImpl> browser_content_client_;
  std::unique_ptr<ContentUtilityClientImpl> utility_content_client_;
};
}  // namespace

class WebLayerTestSuite : public content::ContentTestSuiteBase {
 public:
  WebLayerTestSuite(int argc, char** argv) : ContentTestSuiteBase(argc, argv) {}
  ~WebLayerTestSuite() override = default;

  WebLayerTestSuite(const WebLayerTestSuite&) = delete;
  WebLayerTestSuite& operator=(const WebLayerTestSuite&) = delete;

  void Initialize() override {
    testing::TestEventListeners& listeners =
        testing::UnitTest::GetInstance()->listeners();
    listeners.Append(new WebLayerUnitTestSuiteInitializer);

    ContentTestSuiteBase::Initialize();
  }
};

}  // namespace weblayer

int main(int argc, char** argv) {
  content::UnitTestTestSuite test_suite(
      new weblayer::WebLayerTestSuite(argc, argv));
  return base::LaunchUnitTests(argc, argv,
                               base::BindOnce(&content::UnitTestTestSuite::Run,
                                              base::Unretained(&test_suite)));
}
