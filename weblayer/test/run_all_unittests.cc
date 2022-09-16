// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "content/public/test/content_test_suite_base.h"
#include "content/public/test/unittest_test_suite.h"
#include "weblayer/browser/content_browser_client_impl.h"
#include "weblayer/common/content_client_impl.h"

namespace weblayer {
namespace {

std::unique_ptr<content::UnitTestTestSuite::ContentClients>
CreateContentClients() {
  auto clients = std::make_unique<content::UnitTestTestSuite::ContentClients>();
  clients->content_client = std::make_unique<ContentClientImpl>();
  clients->content_browser_client =
      std::make_unique<ContentBrowserClientImpl>(nullptr);
  return clients;
}

}  // namespace

class WebLayerTestSuite : public content::ContentTestSuiteBase {
 public:
  WebLayerTestSuite(int argc, char** argv) : ContentTestSuiteBase(argc, argv) {}
  ~WebLayerTestSuite() override = default;

  WebLayerTestSuite(const WebLayerTestSuite&) = delete;
  WebLayerTestSuite& operator=(const WebLayerTestSuite&) = delete;

  void Initialize() override {
    InitializeResourceBundle();
    ContentTestSuiteBase::Initialize();
  }
};

}  // namespace weblayer

int main(int argc, char** argv) {
  content::UnitTestTestSuite test_suite(
      new weblayer::WebLayerTestSuite(argc, argv),
      base::BindRepeating(weblayer::CreateContentClients));
  return base::LaunchUnitTests(argc, argv,
                               base::BindOnce(&content::UnitTestTestSuite::Run,
                                              base::Unretained(&test_suite)));
}
