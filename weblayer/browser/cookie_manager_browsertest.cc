// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "weblayer/browser/cookie_manager_impl.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/public/cookie_manager.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

class CookieManagerBrowserTest : public WebLayerBrowserTest {
 public:
  const std::vector<net::CookieChangeInfo>& WaitForChanges(size_t num) {
    if (change_infos_.size() >= num)
      return change_infos_;

    num_to_wait_for_ = num;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    return change_infos_;
  }

  void OnCookieChanged(const net::CookieChangeInfo& info) {
    change_infos_.push_back(info);
    if (run_loop_ && num_to_wait_for_ == change_infos_.size())
      run_loop_->Quit();
  }

  void Reset() { change_infos_.clear(); }

  bool SetCookie(const std::string& cookie) {
    GURL base_url = embedded_test_server()->base_url();
    base::RunLoop run_loop;
    bool final_result = false;
    GetProfile()->GetCookieManager()->SetCookie(
        base_url, cookie,
        base::BindLambdaForTesting([&run_loop, &final_result](bool result) {
          final_result = result;
          run_loop.Quit();
        }));
    run_loop.Run();
    return final_result;
  }

  base::Time GetCookieDbModifiedTime() {
    base::FilePath cookie_path =
        GetBrowserContext()->GetPath().Append(FILE_PATH_LITERAL("Cookies"));

    base::ScopedAllowBlockingForTesting scoped_allow_blocking;
    base::File::Info info;
    EXPECT_TRUE(base::GetFileInfo(cookie_path, &info));
    return info.last_modified;
  }

 private:
  size_t num_to_wait_for_ = 0;
  std::vector<net::CookieChangeInfo> change_infos_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

IN_PROC_BROWSER_TEST_F(CookieManagerBrowserTest, CookieChanged) {
  EXPECT_TRUE(embedded_test_server()->Start());
  NavigateAndWaitForCompletion(
      embedded_test_server()->GetURL("/simple_page.html"), shell());

  GURL base_url = embedded_test_server()->base_url();
  base::CallbackListSubscription subscription =
      GetProfile()->GetCookieManager()->AddCookieChangedCallback(
          base_url, nullptr,
          base::BindRepeating(&CookieManagerBrowserTest::OnCookieChanged,
                              base::Unretained(this)));

  ASSERT_TRUE(SetCookie("foo=bar"));
  const auto& changes = WaitForChanges(1);
  ASSERT_EQ(changes.size(), 1u);
  const auto& cookie = changes[0].cookie;
  EXPECT_EQ(cookie.Name(), "foo");
  EXPECT_EQ(cookie.Value(), "bar");
}

IN_PROC_BROWSER_TEST_F(CookieManagerBrowserTest,
                       CookieChangedRemoveSubscription) {
  EXPECT_TRUE(embedded_test_server()->Start());
  NavigateAndWaitForCompletion(
      embedded_test_server()->GetURL("/simple_page.html"), shell());

  GURL base_url = embedded_test_server()->base_url();
  std::string cookie1 = "cookie1";
  base::CallbackListSubscription subscription1 =
      GetProfile()->GetCookieManager()->AddCookieChangedCallback(
          base_url, &cookie1,
          base::BindRepeating(&CookieManagerBrowserTest::OnCookieChanged,
                              base::Unretained(this)));
  std::string cookie2 = "cookie2";
  base::CallbackListSubscription subscription2 =
      GetProfile()->GetCookieManager()->AddCookieChangedCallback(
          base_url, &cookie2,
          base::BindRepeating(&CookieManagerBrowserTest::OnCookieChanged,
                              base::Unretained(this)));

  ASSERT_TRUE(SetCookie("cookie1=something"));
  {
    const auto& changes = WaitForChanges(1);
    ASSERT_EQ(changes.size(), 1u);
    EXPECT_EQ(changes[0].cookie.Name(), cookie1);
  }

  Reset();
  subscription1 = {};

  // Set cookie1 first and then cookie2. We should only receive a cookie change
  // event for cookie2.
  ASSERT_TRUE(SetCookie("cookie1=other"));
  ASSERT_TRUE(SetCookie("cookie2=something"));
  {
    const auto& changes = WaitForChanges(1);
    ASSERT_EQ(changes.size(), 1u);
    EXPECT_EQ(changes[0].cookie.Name(), cookie2);
  }
}

#if BUILDFLAG(IS_WIN)
// TODO(crbug.com/1204901): Disabled due to flakiness on Windows.
#define MAYBE_FlushCookiesAfterSet DISABLED_FlushCookiesAfterSet
#else
#define MAYBE_FlushCookiesAfterSet FlushCookiesAfterSet
#endif
IN_PROC_BROWSER_TEST_F(CookieManagerBrowserTest, MAYBE_FlushCookiesAfterSet) {
  EXPECT_TRUE(embedded_test_server()->Start());
  NavigateAndWaitForCompletion(
      embedded_test_server()->GetURL("/simple_page.html"), shell());

  base::Time original_modified_time = GetCookieDbModifiedTime();

  ASSERT_TRUE(SetCookie("a=b; expires=Fri, 01 Jan 2038 00:00:00 GMT"));
  EXPECT_EQ(GetCookieDbModifiedTime(), original_modified_time);

  EXPECT_TRUE(static_cast<CookieManagerImpl*>(GetProfile()->GetCookieManager())
                  ->FireFlushTimerForTesting());
  EXPECT_GT(GetCookieDbModifiedTime(), original_modified_time);
}

#if BUILDFLAG(IS_WIN)
// TODO(crbug.com/1204901): Disabled due to flakiness on Windows.
#define MAYBE_FlushCookiesAfterSetMultiple DISABLED_FlushCookiesAfterSetMultiple
#else
#define MAYBE_FlushCookiesAfterSetMultiple FlushCookiesAfterSetMultiple
#endif
IN_PROC_BROWSER_TEST_F(CookieManagerBrowserTest,
                       MAYBE_FlushCookiesAfterSetMultiple) {
  EXPECT_TRUE(embedded_test_server()->Start());
  NavigateAndWaitForCompletion(
      embedded_test_server()->GetURL("/simple_page.html"), shell());

  base::Time original_modified_time = GetCookieDbModifiedTime();

  ASSERT_TRUE(SetCookie("a=b; expires=Fri, 01 Jan 2038 00:00:00 GMT"));
  EXPECT_EQ(GetCookieDbModifiedTime(), original_modified_time);
  ASSERT_TRUE(SetCookie("c=d; expires=Fri, 01 Jan 2038 00:00:00 GMT"));
  EXPECT_EQ(GetCookieDbModifiedTime(), original_modified_time);

  CookieManagerImpl* cookie_manager =
      static_cast<CookieManagerImpl*>(GetProfile()->GetCookieManager());
  EXPECT_TRUE(cookie_manager->FireFlushTimerForTesting());
  EXPECT_GT(GetCookieDbModifiedTime(), original_modified_time);

  // Flush timer should be gone now.
  EXPECT_FALSE(cookie_manager->FireFlushTimerForTesting());

  // Try again to make sure it works a second time.
  original_modified_time = GetCookieDbModifiedTime();
  ASSERT_TRUE(SetCookie("d=f; expires=Fri, 01 Jan 2038 00:00:00 GMT"));
  EXPECT_EQ(GetCookieDbModifiedTime(), original_modified_time);

  EXPECT_TRUE(cookie_manager->FireFlushTimerForTesting());
  EXPECT_GT(GetCookieDbModifiedTime(), original_modified_time);
}

}  // namespace weblayer
