// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "weblayer/browser/browser_list.h"
#include "weblayer/browser/browser_list_observer.h"
#include "weblayer/browser/favicon/favicon_fetcher_impl.h"
#include "weblayer/browser/favicon/test_favicon_fetcher_delegate.h"
#include "weblayer/browser/host_content_settings_map_factory.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/browser.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

using ProfileBrowserTest = WebLayerBrowserTest;

// TODO(crbug.com/654704): Android does not support PRE_ tests.
#if !BUILDFLAG(IS_ANDROID)

// UKM enabling via Profile persists across restarts.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, PRE_PersistUKM) {
  GetProfile()->SetBooleanSetting(SettingType::UKM_ENABLED, true);
}

IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, PersistUKM) {
  ASSERT_TRUE(GetProfile()->GetBooleanSetting(SettingType::UKM_ENABLED));
}

// Enabling Network Prediction via Profile persists across restarts.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, PRE_PersistNetworkPrediction) {
  GetProfile()->SetBooleanSetting(SettingType::NETWORK_PREDICTION_ENABLED,
                                  false);
}

IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, PersistNetworkPrediction) {
  ASSERT_FALSE(
      GetProfile()->GetBooleanSetting(SettingType::NETWORK_PREDICTION_ENABLED));
}

#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       DISABLED_GetCachedFaviconForPageUrl) {
  // Navigation to a page with a favicon.
  ASSERT_TRUE(embedded_test_server()->Start());
  TestFaviconFetcherDelegate fetcher_delegate;
  auto fetcher = shell()->tab()->CreateFaviconFetcher(&fetcher_delegate);
  const GURL url =
      embedded_test_server()->GetURL("/simple_page_with_favicon.html");
  NavigateAndWaitForCompletion(url, shell());
  fetcher_delegate.WaitForFavicon();
  EXPECT_FALSE(fetcher_delegate.last_image().IsEmpty());
  EXPECT_EQ(1, fetcher_delegate.on_favicon_changed_call_count());

  // Request the favicon.
  base::RunLoop run_loop;
  static_cast<TabImpl*>(shell()->tab())
      ->profile()
      ->GetCachedFaviconForPageUrl(
          url, base::BindLambdaForTesting([&](gfx::Image image) {
            // The last parameter is the max difference allowed for each color
            // component. As the image is encoded before saving to disk some
            // variance is expected.
            EXPECT_TRUE(gfx::test::AreImagesClose(
                image, fetcher_delegate.last_image(), 10));
            run_loop.Quit();
          }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       DISABLED_ClearBrowsingDataDeletesFavicons) {
  // Navigate to a page with a favicon.
  ASSERT_TRUE(embedded_test_server()->Start());
  TestFaviconFetcherDelegate fetcher_delegate;
  auto fetcher = shell()->tab()->CreateFaviconFetcher(&fetcher_delegate);
  const GURL url =
      embedded_test_server()->GetURL("/simple_page_with_favicon.html");
  NavigateAndWaitForCompletion(url, shell());
  fetcher_delegate.WaitForNonemptyFavicon();
  EXPECT_FALSE(fetcher_delegate.last_image().IsEmpty());
  EXPECT_EQ(1, fetcher_delegate.on_favicon_changed_call_count());

  // Delete the favicons.
  base::RunLoop run_loop;
  base::Time now = base::Time::Now();
  ProfileImpl* profile = static_cast<TabImpl*>(shell()->tab())->profile();
  profile->ClearBrowsingData({BrowsingDataType::COOKIES_AND_SITE_DATA},
                             now - base::Minutes(5), now,
                             run_loop.QuitClosure());
  run_loop.Run();

  // Ask for the cached favicon, there shouldn't be one.
  base::RunLoop run_loop2;
  profile->GetCachedFaviconForPageUrl(
      url, base::BindLambdaForTesting([&](gfx::Image image) {
        EXPECT_TRUE(image.IsEmpty());
        run_loop2.Quit();
      }));
  run_loop2.Run();

  // Navigate to another page, and verify favicon is downloaded.
  fetcher_delegate.ClearLastImage();
  const GURL url2 =
      embedded_test_server()->GetURL("/simple_page_with_favicon2.html");
  NavigateAndWaitForCompletion(url2, shell());
  fetcher_delegate.WaitForNonemptyFavicon();
  EXPECT_FALSE(fetcher_delegate.last_image().IsEmpty());
  EXPECT_EQ(2, fetcher_delegate.on_favicon_changed_call_count());

  // And fetch the favicon one more time.
  base::RunLoop run_loop3;
  profile->GetCachedFaviconForPageUrl(
      url2, base::BindLambdaForTesting([&](gfx::Image image) {
        EXPECT_FALSE(image.IsEmpty());
        // The last parameter is the max difference allowed for each color
        // component. As the image is encoded before saving to disk some
        // variance is expected.
        EXPECT_TRUE(gfx::test::AreImagesClose(
            image, fetcher_delegate.last_image(), 10));
        run_loop3.Quit();
      }));
  run_loop3.Run();
}

// Test default value.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, DefaultNetworkPredictionState) {
  ASSERT_TRUE(
      GetProfile()->GetBooleanSetting(SettingType::NETWORK_PREDICTION_ENABLED));
}

IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, ClearSiteSettings) {
  auto foo_origin = GURL("http://www.foo.com");

  auto* settings_map = HostContentSettingsMapFactory::GetForBrowserContext(
      static_cast<TabImpl*>(shell()->tab())
          ->web_contents()
          ->GetBrowserContext());
  EXPECT_EQ(settings_map->GetContentSetting(foo_origin, foo_origin,
                                            ContentSettingsType::GEOLOCATION),
            CONTENT_SETTING_ASK);

  settings_map->SetContentSettingDefaultScope(foo_origin, foo_origin,
                                              ContentSettingsType::GEOLOCATION,
                                              CONTENT_SETTING_ALLOW);

  // Ensure clearing things other than site data doesn't change it
  base::RunLoop run_loop;
  base::Time now = base::Time::Now();
  ProfileImpl* profile = static_cast<TabImpl*>(shell()->tab())->profile();
  profile->ClearBrowsingData(
      {BrowsingDataType::COOKIES_AND_SITE_DATA, BrowsingDataType::CACHE},
      base::Time(), now, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(settings_map->GetContentSetting(foo_origin, foo_origin,
                                            ContentSettingsType::GEOLOCATION),
            CONTENT_SETTING_ALLOW);

  // Now clear site data.
  base::RunLoop run_loop2;
  profile->ClearBrowsingData({BrowsingDataType::SITE_SETTINGS}, base::Time(),
                             now, run_loop2.QuitClosure());
  run_loop2.Run();

  EXPECT_EQ(settings_map->GetContentSetting(foo_origin, foo_origin,
                                            ContentSettingsType::GEOLOCATION),
            CONTENT_SETTING_ASK);
}

// This test creates a Browser and Tab, which doesn't work well with Java when
// driven from native code.
#if !BUILDFLAG(IS_ANDROID)

class BrowserListObserverImpl : public BrowserListObserver {
 public:
  BrowserListObserverImpl(std::unique_ptr<Profile> profile,
                          base::OnceClosure done_closure)
      : profile_(std::move(profile)), done_closure_(std::move(done_closure)) {
    BrowserList::GetInstance()->AddObserver(this);
  }
  ~BrowserListObserverImpl() override {
    BrowserList::GetInstance()->RemoveObserver(this);
  }

  // BrowserListObserver:
  void OnBrowserDestroyed(Browser* browser) override {
    Profile::DestroyAndDeleteDataFromDisk(std::move(profile_),
                                          std::move(done_closure_));
  }

 private:
  std::unique_ptr<Profile> profile_;
  base::OnceClosure done_closure_;
};

// This is a crash test to verify no memory related problems calling
// DestroyAndDeleteDataFromDisk() from OnBrowserDestroyed().
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, DestroyFromOnBrowserRemoved) {
  auto profile = Profile::Create("2", true);
  auto browser = Browser::Create(profile.get(), nullptr);

  // MarkAsDeleted() may be called multiple times.
  static_cast<ProfileImpl*>(profile.get())->MarkAsDeleted();
  static_cast<ProfileImpl*>(profile.get())->MarkAsDeleted();

  base::RunLoop run_loop;
  BrowserListObserverImpl observer(std::move(profile), run_loop.QuitClosure());
  browser.reset();
  run_loop.Run();

  // No crashes should happen.
}
#endif

}  // namespace weblayer
