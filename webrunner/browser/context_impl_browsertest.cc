// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/fidl/cpp/binding.h>

#include "base/macros.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"
#include "webrunner/browser/test_common.h"
#include "webrunner/browser/webrunner_browser_test.h"
#include "webrunner/service/common.h"

namespace webrunner {

using testing::_;
using testing::Field;
using testing::InvokeWithoutArgs;

// Use a shorter name for NavigationEvent, because it is
// referenced frequently in this file.
using NavigationDetails = chromium::web::NavigationEvent;

// Defines a suite of tests that exercise browser-level configuration and
// functionality.
class ContextImplTest : public WebRunnerBrowserTest {
 public:
  ContextImplTest() : navigation_observer_binding_(&navigation_observer_) {}
  ~ContextImplTest() = default;

 protected:
  // Creates a Frame with |navigation_observer_| attached.
  chromium::web::FramePtr CreateFrame() {
    return WebRunnerBrowserTest::CreateFrame(&navigation_observer_);
  }

  // Synchronously gets a list of cookies for this BrowserContext.
  net::CookieList GetCookies();

  void TearDownOnMainThread() override {
    navigation_observer_binding_.Unbind();
  }

  testing::StrictMock<MockNavigationObserver> navigation_observer_;
  fidl::Binding<chromium::web::NavigationEventObserver>
      navigation_observer_binding_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContextImplTest);
};

void OnCookiesReceived(net::CookieList* output,
                       base::OnceClosure on_received_cb,
                       const net::CookieList& cookies) {
  *output = cookies;
  std::move(on_received_cb).Run();
}

net::CookieList ContextImplTest::GetCookies() {
  net::CookieStore* cookie_store =
      content::BrowserContext::GetDefaultStoragePartition(
          context_impl()->browser_context_for_test())
          ->GetURLRequestContext()
          ->GetURLRequestContext()
          ->cookie_store();

  base::RunLoop run_loop;
  net::CookieList cookies;
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(
          &net::CookieStore::GetAllCookiesAsync, base::Unretained(cookie_store),
          base::BindOnce(&OnCookiesReceived, base::Unretained(&cookies),
                         run_loop.QuitClosure())));
  run_loop.Run();
  return cookies;
}

// Verifies that the BrowserContext has a working cookie store by setting
// cookies in the content layer and then querying the CookieStore afterward.
IN_PROC_BROWSER_TEST_F(ContextImplTest, VerifyPersistentCookieStore) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL cookie_url(embedded_test_server()->GetURL("/set-cookie?foo=bar"));
  chromium::web::FramePtr frame = CreateFrame();

  chromium::web::NavigationControllerPtr nav;
  frame->GetNavigationController(nav.NewRequest());

  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_, MockableOnNavigationStateChanged(_))
        .WillOnce(testing::InvokeWithoutArgs([&run_loop] { run_loop.Quit(); }));

    nav->LoadUrl(cookie_url.spec(), nullptr);
    run_loop.Run();
  }

  auto cookies = GetCookies();
  bool found = false;
  for (auto c : cookies) {
    if (c.Name() == "foo" && c.Value() == "bar") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);

  // Check that the cookie persists beyond the lifetime of the Frame by
  // releasing the Frame and re-querying the CookieStore.
  frame.Unbind();
  base::RunLoop().RunUntilIdle();

  found = false;
  for (auto c : cookies) {
    if (c.Name() == "foo" && c.Value() == "bar") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);
}

// Suite for tests which run the BrowserContext in incognito mode (no data
// directory).
class IncognitoContextImplTest : public ContextImplTest {
 public:
  IncognitoContextImplTest() = default;
  ~IncognitoContextImplTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(kIncognitoSwitch);
    ContextImplTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IncognitoContextImplTest);
};

// Verify that the browser can be initialized without a persistent data
// directory.
IN_PROC_BROWSER_TEST_F(IncognitoContextImplTest, NavigateFrame) {
  chromium::web::FramePtr frame = CreateFrame();

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  base::RunLoop run_loop;
  EXPECT_CALL(navigation_observer_,
              MockableOnNavigationStateChanged(
                  Field(&NavigationDetails::url, url::kAboutBlankURL)))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  controller->LoadUrl(url::kAboutBlankURL, nullptr);
  run_loop.Run();

  frame.Unbind();
}

IN_PROC_BROWSER_TEST_F(IncognitoContextImplTest, VerifyInMemoryCookieStore) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL cookie_url(embedded_test_server()->GetURL("/set-cookie?foo=bar"));
  chromium::web::FramePtr frame = CreateFrame();

  chromium::web::NavigationControllerPtr nav;
  frame->GetNavigationController(nav.NewRequest());

  base::RunLoop run_loop;
  EXPECT_CALL(navigation_observer_, MockableOnNavigationStateChanged(_))
      .WillOnce(testing::InvokeWithoutArgs([&run_loop] { run_loop.Quit(); }));

  nav->LoadUrl(cookie_url.spec(), nullptr);
  run_loop.Run();

  auto cookies = GetCookies();
  bool found = false;
  for (auto c : cookies) {
    if (c.Name() == "foo" && c.Value() == "bar") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);
}

}  // namespace webrunner
