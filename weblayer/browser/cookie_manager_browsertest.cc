// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
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
  auto subscription =
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
  auto subscription1 =
      GetProfile()->GetCookieManager()->AddCookieChangedCallback(
          base_url, &cookie1,
          base::BindRepeating(&CookieManagerBrowserTest::OnCookieChanged,
                              base::Unretained(this)));
  std::string cookie2 = "cookie2";
  auto subscription2 =
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
  subscription1 = nullptr;

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

}  // namespace weblayer
