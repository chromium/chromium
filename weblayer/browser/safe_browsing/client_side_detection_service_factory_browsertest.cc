// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/client_side_detection_service_factory.h"

#include "base/test/scoped_feature_list.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/common/features.h"
#include "weblayer/test/weblayer_browser_test.h"

namespace weblayer {

class ClientSideDetectionServiceFactoryBrowserTest
    : public WebLayerBrowserTest {
 public:
  ClientSideDetectionServiceFactoryBrowserTest() {
    feature_list_.InitAndDisableFeature(
        features::kWebLayerClientSidePhishingDetection);
  }

 private:
  void SetUpOnMainThread() override {}
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ClientSideDetectionServiceFactoryBrowserTest,
                       ClientDetectionServiceNullWhenDisabled) {
  EXPECT_EQ(nullptr, ClientSideDetectionServiceFactory::GetForBrowserContext(
                         GetProfile()->GetBrowserContext()));
}

}  // namespace weblayer
