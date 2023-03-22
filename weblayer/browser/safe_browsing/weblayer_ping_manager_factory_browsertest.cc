// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/safe_browsing/weblayer_ping_manager_factory.h"
#include "weblayer/browser/safe_browsing/weblayer_user_population_helper.h"
#include "weblayer/test/weblayer_browser_test.h"

using safe_browsing::ClientSafeBrowsingReportRequest;
using ReportThreatDetailsResult =
    safe_browsing::PingManager::ReportThreatDetailsResult;

namespace weblayer {

class WeblayerPingManagerFactoryTest : public WebLayerBrowserTest {
 protected:
  void RunShouldFetchAccessTokenForReportTest(bool is_enhanced_protection,
                                              bool is_signed_in,
                                              bool expect_should_fetch);
};
class IncognitoModeWeblayerPingManagerFactoryTest
    : public WeblayerPingManagerFactoryTest {
 public:
  IncognitoModeWeblayerPingManagerFactoryTest() {
    SetShellStartsInIncognitoMode();
  }
};

void WeblayerPingManagerFactoryTest::RunShouldFetchAccessTokenForReportTest(
    bool is_enhanced_protection,
    bool is_signed_in,
    bool expect_should_fetch) {
  SetSafeBrowsingState(
      GetProfile()->GetBrowserContext()->pref_service(),
      is_enhanced_protection
          ? safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION
          : safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
  if (is_signed_in) {
    WebLayerPingManagerFactory::GetInstance()->SignInAccountForTesting();
  }
  EXPECT_EQ(
      WebLayerPingManagerFactory::GetInstance()
          ->ShouldFetchAccessTokenForReport(GetProfile()->GetBrowserContext()),
      expect_should_fetch);
}

IN_PROC_BROWSER_TEST_F(WeblayerPingManagerFactoryTest, ReportThreatDetails) {
  auto* ping_manager = WebLayerPingManagerFactory::GetForBrowserContext(
      GetProfile()->GetBrowserContext());

  std::string input_report_content;
  std::unique_ptr<ClientSafeBrowsingReportRequest> report =
      std::make_unique<ClientSafeBrowsingReportRequest>();
  // The report must be non-empty. The selected property to set is arbitrary.
  report->set_type(ClientSafeBrowsingReportRequest::URL_PHISHING);
  EXPECT_TRUE(report->SerializeToString(&input_report_content));
  ClientSafeBrowsingReportRequest expected_report;
  expected_report.ParseFromString(input_report_content);
  *expected_report.mutable_population() =
      GetUserPopulationForBrowserContext(GetProfile()->GetBrowserContext());
  std::string expected_report_content;
  EXPECT_TRUE(expected_report.SerializeToString(&expected_report_content));

  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_EQ(GetUploadData(request), expected_report_content);
      }));
  ping_manager->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  EXPECT_EQ(ping_manager->ReportThreatDetails(std::move(report)),
            ReportThreatDetailsResult::SUCCESS);
}
IN_PROC_BROWSER_TEST_F(IncognitoModeWeblayerPingManagerFactoryTest,
                       DISABLED_NoPingManagerForIncognito) {
  EXPECT_EQ(WebLayerPingManagerFactory::GetForBrowserContext(
                GetProfile()->GetBrowserContext()),
            nullptr);
}
IN_PROC_BROWSER_TEST_F(WeblayerPingManagerFactoryTest,
                       ShouldFetchAccessTokenForReport_Yes) {
  RunShouldFetchAccessTokenForReportTest(/*is_enhanced_protection=*/true,
                                         /*is_signed_in=*/true,
                                         /*expect_should_fetch=*/true);
}
IN_PROC_BROWSER_TEST_F(WeblayerPingManagerFactoryTest,
                       ShouldFetchAccessTokenForReport_NotEnhancedProtection) {
  RunShouldFetchAccessTokenForReportTest(/*is_enhanced_protection=*/false,
                                         /*is_signed_in=*/true,
                                         /*expect_should_fetch=*/false);
}
IN_PROC_BROWSER_TEST_F(WeblayerPingManagerFactoryTest,
                       ShouldFetchAccessTokenForReport_NotSignedIn) {
  RunShouldFetchAccessTokenForReportTest(/*is_enhanced_protection=*/true,
                                         /*is_signed_in=*/false,
                                         /*expect_should_fetch=*/false);
}

}  // namespace weblayer
