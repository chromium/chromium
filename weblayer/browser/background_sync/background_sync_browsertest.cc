// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/background_sync/background_sync_controller_impl.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/background_sync_parameters.h"
#include "weblayer/browser/background_sync/background_sync_controller_factory.h"
#include "weblayer/browser/background_sync/background_sync_delegate_impl.h"
#include "weblayer/browser/host_content_settings_map_factory.h"
#include "weblayer/test/weblayer_browser_test.h"

namespace {
const char kExampleUrl[] = "https://www.example.com/";
const char kTag[] = "test_tag";
}  // namespace
namespace weblayer {

class BackgroundSyncBrowserTest : public WebLayerBrowserTest {};

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest, GetBackgroundSyncController) {
  EXPECT_TRUE(BackgroundSyncControllerFactory::GetForBrowserContext(
      GetBrowserContext()));
}

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest, ZeroSiteEngagementPenalty) {
  // TODO(crbug.com/1091211): Update when we add support for Periodic Background
  // Sync.
  auto* controller = BackgroundSyncControllerFactory::GetForBrowserContext(
      GetBrowserContext());
  ASSERT_TRUE(controller);

  url::Origin origin = url::Origin::Create(GURL(kExampleUrl));
  content::BackgroundSyncRegistration registration;
  registration.set_origin(origin);

  // min interval >=0 implies Periodic Background Sync.
  blink::mojom::SyncRegistrationOptions options(
      kTag,
      /* min_interval= */ base::TimeDelta::FromHours(12).InMilliseconds());
  *registration.options() = std::move(options);
  // First attempt.
  registration.set_num_attempts(0);

  content::BackgroundSyncParameters parameters;

  base::TimeDelta delay = controller->GetNextEventDelay(
      registration, &parameters,
      /* time_till_soonest_scheduled_event_for_origin= */
      base::TimeDelta::Max());
  EXPECT_EQ(delay, base::TimeDelta::Max());
}

#if defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest, BackgroundSyncNotDisabled) {
  auto* controller = BackgroundSyncControllerFactory::GetForBrowserContext(
      GetBrowserContext());
  ASSERT_TRUE(controller);

  // TODO(crbug.com/1087486, 1091211): Update logic here if we need to support
  // Android L when we add browser wakeup logic.
  content::BackgroundSyncParameters parameters;
  controller->GetParameterOverrides(&parameters);
  EXPECT_FALSE(parameters.disable);
}
#endif  // defined (OS_ANDROID)

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest, ContentSettings) {
  auto* browser_context = GetBrowserContext();
  auto* controller =
      BackgroundSyncControllerFactory::GetForBrowserContext(browser_context);
  ASSERT_TRUE(controller);

  url::Origin origin = url::Origin::Create(GURL(kExampleUrl));
  controller->AddToTrackedOrigins(origin);
  ASSERT_TRUE(controller->IsOriginTracked(origin));

  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForBrowserContext(browser_context);
  ASSERT_TRUE(host_content_settings_map);

  host_content_settings_map->SetContentSettingDefaultScope(
      /* primary_url= */ GURL(kExampleUrl),
      /* secondary_url= */ GURL(kExampleUrl),
      ContentSettingsType::BACKGROUND_SYNC,
      /* resource_identifier= */ std::string(), CONTENT_SETTING_BLOCK);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(controller->IsOriginTracked(origin));
}

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest, NormalProfile) {
  // TODO(crbug.com/1087486, 1091211): Make this use
  // BackgroundSyncController::ScheduleBrowserWakeup() once we support waking
  // the browser up.
  auto delegate =
      std::make_unique<BackgroundSyncDelegateImpl>(GetBrowserContext());
  ASSERT_TRUE(delegate);
  EXPECT_FALSE(delegate->IsProfileOffTheRecord());
}

class IncognitoBackgroundSyncBrowserTest : public BackgroundSyncBrowserTest {
 public:
  IncognitoBackgroundSyncBrowserTest() { SetShellStartsInIncognitoMode(); }
};

IN_PROC_BROWSER_TEST_F(IncognitoBackgroundSyncBrowserTest,
                       OffTheRecordProfile) {
  // TODO(crbug.com/1087486, 1091211): Make this use
  // BackgroundSyncController::ScheduleBrowserWakeup() once we support waking
  // the browser up.
  auto delegate =
      std::make_unique<BackgroundSyncDelegateImpl>(GetBrowserContext());
  EXPECT_TRUE(delegate->IsProfileOffTheRecord());
}

}  // namespace weblayer