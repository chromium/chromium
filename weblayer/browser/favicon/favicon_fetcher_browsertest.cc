// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/favicon/favicon_fetcher_impl.h"

#include "base/run_loop.h"
#include "build/build_config.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "ui/gfx/image/image.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/favicon/favicon_fetcher_impl.h"
#include "weblayer/browser/favicon/favicon_service_impl.h"
#include "weblayer/browser/favicon/favicon_service_impl_factory.h"
#include "weblayer/browser/favicon/favicon_service_impl_observer.h"
#include "weblayer/browser/favicon/test_favicon_fetcher_delegate.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/browser.h"
#include "weblayer/public/favicon_fetcher_delegate.h"
#include "weblayer/public/navigation_controller.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/test_navigation_observer.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {
namespace {

// FaviconServiceImplObserver used to wait for download to fail.
class TestFaviconServiceImplObserver : public FaviconServiceImplObserver {
 public:
  void Wait() {
    ASSERT_EQ(nullptr, run_loop_.get());
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
  }

  // FaviconServiceImplObserver:
  void OnUnableToDownloadFavicon() override {
    if (run_loop_)
      run_loop_->Quit();
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace

using FaviconFetcherBrowserTest = WebLayerBrowserTest;

IN_PROC_BROWSER_TEST_F(FaviconFetcherBrowserTest, Basic) {
  ASSERT_TRUE(embedded_test_server()->Start());
  TestFaviconFetcherDelegate fetcher_delegate;
  auto fetcher = shell()->tab()->CreateFaviconFetcher(&fetcher_delegate);
  NavigateAndWaitForCompletion(
      embedded_test_server()->GetURL(
          "/simple_page_with_favicon_and_before_unload.html"),
      shell());
  fetcher_delegate.WaitForFavicon();
  EXPECT_FALSE(fetcher_delegate.last_image().IsEmpty());
  EXPECT_EQ(fetcher_delegate.last_image(), fetcher->GetFavicon());
  EXPECT_EQ(1, fetcher_delegate.on_favicon_changed_call_count());
  fetcher_delegate.ClearLastImage();

  const GURL url2 =
      embedded_test_server()->GetURL("/simple_page_with_favicon.html");
  shell()->tab()->GetNavigationController()->Navigate(url2);
  // Favicon doesn't change immediately on navigation.
  EXPECT_FALSE(fetcher->GetFavicon().IsEmpty());
  // Favicon does change once start is received.
  TestNavigationObserver test_observer(
      url2, TestNavigationObserver::NavigationEvent::kStart, shell());
  test_observer.Wait();
  EXPECT_TRUE(fetcher_delegate.last_image().IsEmpty());

  fetcher_delegate.WaitForNonemptyFavicon();
  EXPECT_FALSE(fetcher_delegate.last_image().IsEmpty());
  EXPECT_EQ(fetcher_delegate.last_image(), fetcher->GetFavicon());
  // OnFaviconChanged() is called twice, once with an empty image (because of
  // the navigation), the second with the real image.
  EXPECT_EQ(2, fetcher_delegate.on_favicon_changed_call_count());
}

IN_PROC_BROWSER_TEST_F(FaviconFetcherBrowserTest, NavigateToPageWithNoFavicon) {
  ASSERT_TRUE(embedded_test_server()->Start());
  TestFaviconFetcherDelegate fetcher_delegate;
  auto fetcher = shell()->tab()->CreateFaviconFetcher(&fetcher_delegate);
  NavigateAndWaitForCompletion(
      embedded_test_server()->GetURL("/simple_page_with_favicon.html"),
      shell());
  fetcher_delegate.WaitForFavicon();
  fetcher_delegate.ClearLastImage();

  TestFaviconServiceImplObserver test_observer;
  FaviconServiceImplFactory::GetForBrowserContext(
      static_cast<TabImpl*>(shell()->tab())->profile()->GetBrowserContext())
      ->set_observer(&test_observer);

  const GURL url2 = embedded_test_server()->GetURL("/simple_page.html");
  shell()->tab()->GetNavigationController()->Navigate(url2);
  EXPECT_TRUE(fetcher_delegate.last_image().IsEmpty());
  // The delegate should be notified of the empty image once.
  test_observer.Wait();
  EXPECT_TRUE(fetcher_delegate.last_image().IsEmpty());
  EXPECT_EQ(1, fetcher_delegate.on_favicon_changed_call_count());
}

IN_PROC_BROWSER_TEST_F(FaviconFetcherBrowserTest,
                       ContentFaviconDriverLifetime) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      static_cast<TabImpl*>(shell()->tab())->web_contents();

  // Drivers are immediately created for every tab.
  auto* favicon_driver =
      favicon::ContentFaviconDriver::FromWebContents(web_contents);
  EXPECT_NE(nullptr, favicon_driver);

  // Request a fetcher, which should trigger creating ContentFaviconDriver.
  TestFaviconFetcherDelegate fetcher_delegate;
  auto fetcher = shell()->tab()->CreateFaviconFetcher(&fetcher_delegate);
  // Check that the driver has not changed.
  EXPECT_EQ(favicon_driver,
            favicon::ContentFaviconDriver::FromWebContents(web_contents));

  NavigateAndWaitForCompletion(
      embedded_test_server()->GetURL("/simple_page_with_favicon.html"),
      shell());
  fetcher_delegate.WaitForFavicon();
  EXPECT_FALSE(fetcher_delegate.last_image().IsEmpty());
}

// This test creates a Browser and Tab, which doesn't work well with Java when
// driven from native code.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(FaviconFetcherBrowserTest, OffTheRecord) {
  auto otr_profile = Profile::Create(std::string(), true);
  ProfileImpl* otr_profile_impl = static_cast<ProfileImpl*>(otr_profile.get());
  EXPECT_TRUE(otr_profile_impl->GetBrowserContext()->IsOffTheRecord());
  auto otr_browser = Browser::Create(otr_profile.get(), nullptr);
  Tab* tab = otr_browser->CreateTab();

  // There is no FaviconService for off the record profiles. FaviconService
  // writes to disk, which is not appropriate for off the record mode.
  EXPECT_EQ(nullptr, FaviconServiceImplFactory::GetForBrowserContext(
                         otr_profile_impl->GetBrowserContext()));
  ASSERT_TRUE(embedded_test_server()->Start());
  TestFaviconFetcherDelegate fetcher_delegate;
  auto fetcher = tab->CreateFaviconFetcher(&fetcher_delegate);
  NavigateAndWaitForCompletion(
      embedded_test_server()->GetURL("/simple_page_with_favicon.html"), tab);
  fetcher_delegate.WaitForFavicon();
  EXPECT_FALSE(fetcher_delegate.last_image().IsEmpty());
  EXPECT_EQ(fetcher_delegate.last_image(), fetcher->GetFavicon());
  EXPECT_EQ(1, fetcher_delegate.on_favicon_changed_call_count());
}
#endif

}  // namespace weblayer
