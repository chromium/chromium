// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/background_sync/background_sync_controller_impl.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/background_sync_parameters.h"
#include "content/public/test/background_sync_test_util.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "weblayer/browser/background_sync/background_sync_controller_factory.h"
#include "weblayer/browser/background_sync/background_sync_delegate_impl.h"
#include "weblayer/browser/host_content_settings_map_factory.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_state_observer.h"

namespace {
class TestKeepAliveStateObserver : public KeepAliveStateObserver {
 public:
  TestKeepAliveStateObserver() {
    KeepAliveRegistry::GetInstance()->AddObserver(this);
  }

  ~TestKeepAliveStateObserver() override {
    KeepAliveRegistry::GetInstance()->RemoveObserver(this);
  }

  void OnKeepAliveStateChanged(bool is_keeping_alive) override {
    if (is_keeping_alive_loop_ && !is_keeping_alive)
      is_keeping_alive_loop_->Quit();
  }

  void OnKeepAliveRestartStateChanged(bool can_restart) override {}

  void WaitUntilNoKeepAlives() {
    if (!KeepAliveRegistry::GetInstance()->IsKeepingAlive())
      return;
    is_keeping_alive_loop_ = std::make_unique<base::RunLoop>();
    is_keeping_alive_loop_->Run();
    is_keeping_alive_loop_ = nullptr;
    CHECK(!KeepAliveRegistry::GetInstance()->IsKeepingAlive());
  }

 private:
  std::unique_ptr<base::RunLoop> is_keeping_alive_loop_;
};
}  // namespace
#endif  // !BUILDFLAG(IS_ANDROID)

namespace {
const char kExampleUrl[] = "https://www.example.com/";
const char kTag[] = "test_tag";
}  // namespace

namespace weblayer {

class BackgroundSyncBrowserTest : public WebLayerBrowserTest {
 public:
  BackgroundSyncBrowserTest() = default;
  ~BackgroundSyncBrowserTest() override = default;

  void SetUpOnMainThread() override {
    sync_event_received_ = std::make_unique<base::RunLoop>();
    content::background_sync_test_util::SetIgnoreNetworkChanges(
        /* ignore= */ true);

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->RegisterRequestHandler(base::BindRepeating(
        &BackgroundSyncBrowserTest::HandleRequest, base::Unretained(this)));
    https_server_->AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("weblayer/test/data")));
    ASSERT_TRUE(https_server_->Start());
  }

  // Intercepts all requests.
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().query() == "syncreceived") {
      if (sync_event_received_)
        sync_event_received_->Quit();
    }

    // The default handlers will take care of this request.
    return nullptr;
  }

#if !BUILDFLAG(IS_ANDROID)
  void PostRunTestOnMainThread() override {
    keep_alive_observer_.WaitUntilNoKeepAlives();
    WebLayerBrowserTest::PostRunTestOnMainThread();
  }
#endif  // !BUILDFLAG(IS_ANDROID)

 protected:
  content::WebContents* web_contents() {
    return static_cast<TabImpl*>(shell()->tab())->web_contents();
  }

  std::unique_ptr<base::RunLoop> sync_event_received_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
#if !BUILDFLAG(IS_ANDROID)
  TestKeepAliveStateObserver keep_alive_observer_;
#endif  // !BUILDFLAG(IS_ANDROID)
};

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
      /* min_interval= */ base::Hours(12).InMilliseconds());
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

#if BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/1154332): Fix flaky test.
IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest,
                       DISABLED_BackgroundSyncNotDisabled) {
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
      ContentSettingsType::BACKGROUND_SYNC, CONTENT_SETTING_BLOCK);
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

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest, SyncEventFired) {
  content::background_sync_test_util::SetOnline(web_contents(), false);
  NavigateAndWaitForCompletion(
      https_server_->GetURL("/background_sync_browsertest.html"), shell());
  content::background_sync_test_util::SetOnline(web_contents(), true);
  sync_event_received_->Run();
}

class IncognitoBackgroundSyncBrowserTest : public BackgroundSyncBrowserTest {
 public:
  IncognitoBackgroundSyncBrowserTest() { SetShellStartsInIncognitoMode(); }
};

IN_PROC_BROWSER_TEST_F(IncognitoBackgroundSyncBrowserTest,
                       DISABLED_OffTheRecordProfile) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // TODO(crbug.com/1087486, 1091211): Make this use
  // BackgroundSyncController::ScheduleBrowserWakeup() once we support waking
  // the browser up.
  auto delegate =
      std::make_unique<BackgroundSyncDelegateImpl>(GetBrowserContext());
  EXPECT_TRUE(delegate->IsProfileOffTheRecord());
}

}  // namespace weblayer
