// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/safe_browsing/weblayer_ping_manager_factory.h"
#include "weblayer/test/weblayer_browser_test.h"

namespace weblayer {

class WeblayerPingManagerTest : public WebLayerBrowserTest {};

IN_PROC_BROWSER_TEST_F(WeblayerPingManagerTest, ReportThreatDetails) {
  auto* ping_manager = WebLayerPingManagerFactory::GetForBrowserContext(
      GetProfile()->GetBrowserContext());
  std::string report_content = "testing_report_content";
  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_EQ(GetUploadData(request), report_content);
      }));
  ping_manager->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  ping_manager->ReportThreatDetails(report_content);
}

IN_PROC_BROWSER_TEST_F(WeblayerPingManagerTest, ReportSafeBrowsingHit) {
  safe_browsing::HitReport hit_report;
  hit_report.post_data = "testing_hit_report_post_data";
  // Threat type and source are arbitrary but specified so that determining the
  // URL does not does throw an error due to input validation.
  hit_report.threat_type = safe_browsing::SB_THREAT_TYPE_URL_PHISHING;
  hit_report.threat_source = safe_browsing::ThreatSource::LOCAL_PVER4;

  auto* ping_manager = WebLayerPingManagerFactory::GetForBrowserContext(
      GetProfile()->GetBrowserContext());
  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_EQ(GetUploadData(request), hit_report.post_data);
      }));
  ping_manager->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  ping_manager->ReportSafeBrowsingHit(hit_report);
}

}  // namespace weblayer
