// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/fidl/cpp/binding.h>

#include "base/macros.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/url_request/url_request_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"
#include "webrunner/browser/frame_impl.h"
#include "webrunner/browser/test_common.h"
#include "webrunner/browser/webrunner_browser_test.h"
#include "webrunner/service/common.h"

namespace webrunner {

using testing::_;
using testing::AllOf;
using testing::Field;
using testing::InvokeWithoutArgs;
using testing::Mock;

// Use a shorter name for NavigationEvent, because it is
// referenced frequently in this file.
using NavigationDetails = chromium::web::NavigationEvent;

const char kPage1Path[] = "/title1.html";
const char kPage2Path[] = "/title2.html";
const char kPage1Title[] = "title 1";
const char kPage2Title[] = "title 2";
const char kDataUrl[] =
    "data:text/html;base64,PGI+SGVsbG8sIHdvcmxkLi4uPC9iPg==";

MATCHER(IsSet, "Checks if an optional field is set.") {
  return !arg.is_null();
}

// Defines a suite of tests that exercise Frame-level functionality, such as
// navigation commands and page events.
class FrameImplTest : public WebRunnerBrowserTest {
 public:
  FrameImplTest() = default;
  ~FrameImplTest() = default;

  MOCK_METHOD1(OnServeHttpRequest,
               void(const net::test_server::HttpRequest& request));

 protected:
  // Creates a Frame with |navigation_observer_| attached.
  chromium::web::FramePtr CreateFrame() {
    return WebRunnerBrowserTest::CreateFrame(&navigation_observer_);
  }

  // Navigates a |controller| to |url|, blocking until navigation is complete.
  void CheckLoadUrl(const std::string& url,
                    const std::string& expected_title,
                    chromium::web::NavigationController* controller) {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_,
                MockableOnNavigationStateChanged(testing::AllOf(
                    Field(&NavigationDetails::title, expected_title),
                    Field(&NavigationDetails::url, url))))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->LoadUrl(url, nullptr);
    run_loop.Run();
    Mock::VerifyAndClearExpectations(this);
    navigation_observer_.Acknowledge();
  }

  testing::StrictMock<MockNavigationObserver> navigation_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameImplTest);
};

class WebContentsDeletionObserver : public content::WebContentsObserver {
 public:
  explicit WebContentsDeletionObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  MOCK_METHOD1(RenderViewDeleted,
               void(content::RenderViewHost* render_view_host));
};

// Verifies that the browser will navigate and generate a navigation observer
// event when LoadUrl() is called.
IN_PROC_BROWSER_TEST_F(FrameImplTest, NavigateFrame) {
  chromium::web::FramePtr frame = CreateFrame();

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  CheckLoadUrl(url::kAboutBlankURL, url::kAboutBlankURL, controller.get());

  frame.Unbind();
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, NavigateDataFrame) {
  chromium::web::FramePtr frame = CreateFrame();

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  CheckLoadUrl(kDataUrl, kDataUrl, controller.get());

  frame.Unbind();
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, FrameDeletedBeforeContext) {
  chromium::web::FramePtr frame = CreateFrame();

  // Process the frame creation message.
  base::RunLoop().RunUntilIdle();

  FrameImpl* frame_impl = context_impl()->GetFrameImplForTest(&frame);
  WebContentsDeletionObserver deletion_observer(
      frame_impl->web_contents_for_test());
  base::RunLoop run_loop;
  EXPECT_CALL(deletion_observer, RenderViewDeleted(_))
      .WillOnce(InvokeWithoutArgs([&run_loop] { run_loop.Quit(); }));

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  controller->LoadUrl(url::kAboutBlankURL, nullptr);

  frame.Unbind();
  run_loop.Run();

  // Check that |context| remains bound after the frame is closed.
  EXPECT_TRUE(context());
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, ContextDeletedBeforeFrame) {
  chromium::web::FramePtr frame = CreateFrame();
  EXPECT_TRUE(frame);

  base::RunLoop run_loop;
  frame.set_error_handler([&run_loop]() { run_loop.Quit(); });
  context().Unbind();
  run_loop.Run();
  EXPECT_FALSE(frame);
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, GoBackAndForward) {
  chromium::web::FramePtr frame = CreateFrame();
  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL title1(embedded_test_server()->GetURL(kPage1Path));
  GURL title2(embedded_test_server()->GetURL(kPage2Path));

  CheckLoadUrl(title1.spec(), kPage1Title, controller.get());
  CheckLoadUrl(title2.spec(), kPage2Title, controller.get());

  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_,
                MockableOnNavigationStateChanged(testing::AllOf(
                    Field(&NavigationDetails::title, kPage1Title),
                    Field(&NavigationDetails::url, IsSet()))))
        .WillOnce(InvokeWithoutArgs([&run_loop] { run_loop.Quit(); }));
    controller->GoBack();
    run_loop.Run();
    navigation_observer_.Acknowledge();
  }

  // At the top of the navigation entry list; this should be a no-op.
  controller->GoBack();

  // Process the navigation request message.
  base::RunLoop().RunUntilIdle();

  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_,
                MockableOnNavigationStateChanged(testing::AllOf(
                    Field(&NavigationDetails::title, kPage2Title),
                    Field(&NavigationDetails::url, IsSet()))))
        .WillOnce(InvokeWithoutArgs([&run_loop] { run_loop.Quit(); }));
    controller->GoForward();
    run_loop.Run();
    navigation_observer_.Acknowledge();
  }

  // At the end of the navigation entry list; this should be a no-op.
  controller->GoForward();

  // Process the navigation request message.
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, ReloadFrame) {
  chromium::web::FramePtr frame = CreateFrame();
  chromium::web::NavigationControllerPtr navigation_controller;
  frame->GetNavigationController(navigation_controller.NewRequest());

  embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
      &FrameImplTest::OnServeHttpRequest, base::Unretained(this)));

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kPage1Path));

  EXPECT_CALL(*this, OnServeHttpRequest(_));
  CheckLoadUrl(url.spec(), kPage1Title, navigation_controller.get());

  navigation_observer_.Observe(
      context_impl()->GetFrameImplForTest(&frame)->web_contents_.get());

  // Reload with NO_CACHE.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnServeHttpRequest(_));
    EXPECT_CALL(navigation_observer_, DidFinishLoad(_, url))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    navigation_controller->Reload(chromium::web::ReloadType::NO_CACHE);
    run_loop.Run();
    Mock::VerifyAndClearExpectations(this);
    navigation_observer_.Acknowledge();
  }
  // Reload with PARTIAL_CACHE.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnServeHttpRequest(_));
    EXPECT_CALL(navigation_observer_, DidFinishLoad(_, url))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    navigation_controller->Reload(chromium::web::ReloadType::PARTIAL_CACHE);
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, GetVisibleEntry) {
  chromium::web::FramePtr frame = CreateFrame();

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  // Verify that a Frame returns a null NavigationEntry prior to receiving any
  // LoadUrl() calls.
  {
    base::RunLoop run_loop;
    controller->GetVisibleEntry(
        [&run_loop](std::unique_ptr<chromium::web::NavigationEntry> details) {
          EXPECT_EQ(nullptr, details.get());
          run_loop.Quit();
        });
    run_loop.Run();
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL title1(embedded_test_server()->GetURL(kPage1Path));
  GURL title2(embedded_test_server()->GetURL(kPage2Path));

  // Navigate to a page.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_,
                MockableOnNavigationStateChanged(testing::AllOf(
                    Field(&NavigationDetails::title, kPage1Title),
                    Field(&NavigationDetails::url, IsSet()))))
        .WillOnce(testing::InvokeWithoutArgs([&run_loop] { run_loop.Quit(); }));
    controller->LoadUrl(title1.spec(), nullptr);
    run_loop.Run();
    navigation_observer_.Acknowledge();
  }

  // Verify that GetVisibleEntry() reflects the new Frame navigation state.
  {
    base::RunLoop run_loop;
    controller->GetVisibleEntry(
        [&run_loop,
         &title1](std::unique_ptr<chromium::web::NavigationEntry> details) {
          EXPECT_TRUE(details);
          EXPECT_EQ(details->url, title1.spec());
          EXPECT_EQ(details->title, kPage1Title);
          run_loop.Quit();
        });
    run_loop.Run();
  }

  // Navigate to another page.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_,
                MockableOnNavigationStateChanged(testing::AllOf(
                    Field(&NavigationDetails::title, kPage2Title),
                    Field(&NavigationDetails::url, IsSet()))))
        .WillOnce(testing::InvokeWithoutArgs([&run_loop] { run_loop.Quit(); }));
    controller->LoadUrl(title2.spec(), nullptr);
    run_loop.Run();
    navigation_observer_.Acknowledge();
  }

  // Verify the navigation with GetVisibleEntry().
  {
    base::RunLoop run_loop;
    controller->GetVisibleEntry(
        [&run_loop,
         &title2](std::unique_ptr<chromium::web::NavigationEntry> details) {
          EXPECT_TRUE(details);
          EXPECT_EQ(details->url, title2.spec());
          EXPECT_EQ(details->title, kPage2Title);
          run_loop.Quit();
        });
    run_loop.Run();
  }

  // Navigate back to the first page.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_,
                MockableOnNavigationStateChanged(testing::AllOf(
                    Field(&NavigationDetails::title, kPage1Title),
                    Field(&NavigationDetails::url, IsSet()))))
        .WillOnce(testing::InvokeWithoutArgs([&run_loop] { run_loop.Quit(); }));
    controller->GoBack();
    run_loop.Run();
    navigation_observer_.Acknowledge();
  }

  // Verify the navigation with GetVisibleEntry().
  {
    base::RunLoop run_loop;
    controller->GetVisibleEntry(
        [&run_loop,
         &title1](std::unique_ptr<chromium::web::NavigationEntry> details) {
          EXPECT_TRUE(details);
          EXPECT_EQ(details->url, title1.spec());
          EXPECT_EQ(details->title, kPage1Title);
          run_loop.Quit();
        });
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, NoNavigationObserverAttached) {
  chromium::web::FramePtr frame;
  context()->CreateFrame(frame.NewRequest());
  base::RunLoop().RunUntilIdle();

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL title1(embedded_test_server()->GetURL(kPage1Path));
  GURL title2(embedded_test_server()->GetURL(kPage2Path));

  navigation_observer_.Observe(
      context_impl()->GetFrameImplForTest(&frame)->web_contents_.get());

  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_, DidFinishLoad(_, title1))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->LoadUrl(title1.spec(), nullptr);
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_, DidFinishLoad(_, title2))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->LoadUrl(title2.spec(), nullptr);
    run_loop.Run();
  }
}

// Verifies that a Frame will handle navigation observer disconnection events
// gracefully.
IN_PROC_BROWSER_TEST_F(FrameImplTest, NavigationObserverDisconnected) {
  chromium::web::FramePtr frame = CreateFrame();

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL title1(embedded_test_server()->GetURL(kPage1Path));
  GURL title2(embedded_test_server()->GetURL(kPage2Path));

  navigation_observer_.Observe(
      context_impl()->GetFrameImplForTest(&frame)->web_contents_.get());

  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_, DidFinishLoad(_, title1));
    EXPECT_CALL(navigation_observer_,
                MockableOnNavigationStateChanged(testing::AllOf(
                    Field(&NavigationDetails::title, kPage1Title),
                    Field(&NavigationDetails::url, IsSet()))))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->LoadUrl(title1.spec(), nullptr);
    run_loop.Run();
  }

  // Disconnect the observer & spin the runloop to propagate the disconnection
  // event over IPC.
  navigation_observer_bindings().CloseAll();
  base::RunLoop().RunUntilIdle();

  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_, DidFinishLoad(_, title2))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->LoadUrl(title2.spec(), nullptr);
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, DISABLED_DelayedNavigationEventAck) {
  chromium::web::FramePtr frame = CreateFrame();

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL title1(embedded_test_server()->GetURL(kPage1Path));
  GURL title2(embedded_test_server()->GetURL(kPage2Path));

  // Expect an navigation event here, but deliberately postpone acknowledgement
  // until the end of the test.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_,
                MockableOnNavigationStateChanged(testing::AllOf(
                    Field(&NavigationDetails::title, kPage1Title),
                    Field(&NavigationDetails::url, IsSet()))))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->LoadUrl(title1.spec(), nullptr);
    run_loop.Run();
    Mock::VerifyAndClearExpectations(this);
  }

  // Since we have blocked NavigationEventObserver's flow, we must observe the
  // WebContents events directly via a test-only seam.
  navigation_observer_.Observe(
      context_impl()->GetFrameImplForTest(&frame)->web_contents_.get());

  // Navigate to a second page.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_, DidFinishLoad(_, title2))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->LoadUrl(title2.spec(), nullptr);
    run_loop.Run();
    Mock::VerifyAndClearExpectations(this);
  }

  // Navigate to the first page.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_, DidFinishLoad(_, title1))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->LoadUrl(title1.spec(), nullptr);
    run_loop.Run();
    Mock::VerifyAndClearExpectations(this);
  }

  // Since there was no observable change in navigation state since the last
  // ack, there should be no more NavigationEvents generated.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_,
                MockableOnNavigationStateChanged(testing::AllOf(
                    Field(&NavigationDetails::title, kPage1Title),
                    Field(&NavigationDetails::url, IsSet()))))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    navigation_observer_.Acknowledge();
    run_loop.Run();
  }
}

// Observes events specific to the Stop() test case.
struct WebContentsObserverForStop : public content::WebContentsObserver {
  using content::WebContentsObserver::Observe;
  MOCK_METHOD1(DidStartNavigation, void(content::NavigationHandle*));
  MOCK_METHOD0(NavigationStopped, void());
};

IN_PROC_BROWSER_TEST_F(FrameImplTest, Stop) {
  chromium::web::FramePtr frame = CreateFrame();

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  ASSERT_TRUE(embedded_test_server()->Start());

  // Use a request handler that will accept the connection and stall
  // indefinitely.
  GURL hung_url(embedded_test_server()->GetURL("/hung"));

  WebContentsObserverForStop observer;
  observer.Observe(
      context_impl()->GetFrameImplForTest(&frame)->web_contents_.get());

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer, DidStartNavigation(_))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->LoadUrl(hung_url.spec(), nullptr);
    run_loop.Run();
    Mock::VerifyAndClearExpectations(this);
  }

  EXPECT_TRUE(
      context_impl()->GetFrameImplForTest(&frame)->web_contents_->IsLoading());

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer, NavigationStopped())
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->Stop();
    run_loop.Run();
    Mock::VerifyAndClearExpectations(this);
  }

  EXPECT_FALSE(
      context_impl()->GetFrameImplForTest(&frame)->web_contents_->IsLoading());
}

}  // namespace webrunner
