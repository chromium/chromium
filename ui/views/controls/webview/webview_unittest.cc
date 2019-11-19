// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/webview/webview.h"

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "content/test/test_content_browser_client.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/test/widget_test.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace views {

namespace {

// Provides functionality to observe events on a WebContents like
// OnVisibilityChanged/WebContentsDestroyed.
class WebViewTestWebContentsObserver : public content::WebContentsObserver {
 public:
  explicit WebViewTestWebContentsObserver(content::WebContents* web_contents)
      : web_contents_(web_contents),
        was_shown_(false),
        shown_count_(0),
        hidden_count_(0),
        valid_root_while_shown_(true) {
    content::WebContentsObserver::Observe(web_contents);
  }

  ~WebViewTestWebContentsObserver() override {
    if (web_contents_)
      content::WebContentsObserver::Observe(nullptr);
  }

  void WebContentsDestroyed() override {
    DCHECK(web_contents_);
    content::WebContentsObserver::Observe(nullptr);
    web_contents_ = nullptr;
  }

  void OnVisibilityChanged(content::Visibility visibility) override {
    switch (visibility) {
      case content::Visibility::VISIBLE: {
#if defined(USE_AURA)
        valid_root_while_shown_ =
            web_contents()->GetNativeView()->GetRootWindow() != nullptr;
#endif
        was_shown_ = true;
        ++shown_count_;
        break;
      }
      case content::Visibility::HIDDEN: {
        was_shown_ = false;
        ++hidden_count_;
        break;
      }
      default: {
        ADD_FAILURE() << "Unexpected call to OnVisibilityChanged.";
        break;
      }
    }
  }

  bool was_shown() const { return was_shown_; }

  int shown_count() const { return shown_count_; }

  int hidden_count() const { return hidden_count_; }

  bool valid_root_while_shown() const { return valid_root_while_shown_; }

 private:
  content::WebContents* web_contents_;
  bool was_shown_;
  int32_t shown_count_;
  int32_t hidden_count_;
  // Set to true if the view containing the webcontents has a valid root window.
  bool valid_root_while_shown_;

  DISALLOW_COPY_AND_ASSIGN(WebViewTestWebContentsObserver);
};

// Fakes the fullscreen browser state reported to WebContents and WebView.
class WebViewTestWebContentsDelegate : public content::WebContentsDelegate {
 public:
  WebViewTestWebContentsDelegate() = default;
  ~WebViewTestWebContentsDelegate() override = default;

  void set_is_fullscreened(bool fs) { is_fullscreened_ = fs; }

  // content::WebContentsDelegate overrides.
  bool IsFullscreenForTabOrPending(
      const content::WebContents* ignored) override {
    return is_fullscreened_;
  }

 private:
  bool is_fullscreened_ = false;

  DISALLOW_COPY_AND_ASSIGN(WebViewTestWebContentsDelegate);
};

}  // namespace

// Provides functionality to test a WebView.
class WebViewUnitTest : public views::test::WidgetTest {
 public:
  WebViewUnitTest()
      : views::test::WidgetTest(
            views::ViewsTestBase::SubclassManagesTaskEnvironment()) {}
  ~WebViewUnitTest() override = default;

  std::unique_ptr<content::WebContents> CreateWebContentsForWebView(
      content::BrowserContext* browser_context) {
    return content::WebContentsTester::CreateTestWebContents(browser_context,
                                                             nullptr);
  }

  void SetUp() override {
    rvh_enabler_ = std::make_unique<content::RenderViewHostTestEnabler>();

    views::WebView::WebContentsCreator creator = base::BindRepeating(
        &WebViewUnitTest::CreateWebContentsForWebView, base::Unretained(this));
    scoped_web_contents_creator_ =
        std::make_unique<views::WebView::ScopedWebContentsCreatorForTesting>(
            creator);
    set_views_delegate(base::WrapUnique(new views::TestViewsDelegate));
    browser_context_ = std::make_unique<content::TestBrowserContext>();
    WidgetTest::SetUp();
    // Set the test content browser client to avoid pulling in needless
    // dependencies from content.
    SetBrowserClientForTesting(&test_browser_client_);

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableBackgroundingOccludedWindowsForTesting);

    // Create a top level widget and add a child, and give it a WebView as a
    // child.
    top_level_widget_ = CreateTopLevelFramelessPlatformWidget();
    top_level_widget_->SetBounds(gfx::Rect(0, 10, 100, 100));
    View* const contents_view = new View();
    top_level_widget_->SetContentsView(contents_view);
    web_view_ = new WebView(browser_context_.get());
    web_view_->SetBoundsRect(gfx::Rect(contents_view->size()));
    contents_view->AddChildView(web_view_);
    top_level_widget_->Show();
    ASSERT_EQ(gfx::Rect(0, 0, 100, 100), web_view_->bounds());
  }

  void TearDown() override {
    scoped_web_contents_creator_.reset();
    top_level_widget_->Close();  // Deletes all children and itself.
    RunPendingMessages();

    browser_context_.reset(nullptr);
    // Flush the message loop to execute pending relase tasks as this would
    // upset ASAN and Valgrind.
    RunPendingMessages();
    WidgetTest::TearDown();
  }

 protected:
  Widget* top_level_widget() const { return top_level_widget_; }
  WebView* web_view() const { return web_view_; }
  NativeViewHost* holder() const { return web_view_->holder_; }

  std::unique_ptr<content::WebContents> CreateWebContents() const {
    return content::WebContents::Create(
        content::WebContents::CreateParams(browser_context_.get()));
  }

  void SetFullscreenNativeView(WebView* web_view, gfx::NativeView native_view) {
    web_view->fullscreen_native_view_for_testing_ = native_view;
  }

  content::BrowserTaskEnvironment task_environment_;

 private:
  std::unique_ptr<content::RenderViewHostTestEnabler> rvh_enabler_;
  std::unique_ptr<content::TestBrowserContext> browser_context_;
  content::TestContentBrowserClient test_browser_client_;
  std::unique_ptr<views::WebView::ScopedWebContentsCreatorForTesting>
      scoped_web_contents_creator_;

  Widget* top_level_widget_ = nullptr;
  WebView* web_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WebViewUnitTest);
};

// Tests that attaching and detaching a WebContents to a WebView makes the
// WebContents visible and hidden respectively.
TEST_F(WebViewUnitTest, TestWebViewAttachDetachWebContents) {
  // Case 1: Create a new WebContents and set it in the webview via
  // SetWebContents. This should make the WebContents visible.
  const std::unique_ptr<content::WebContents> web_contents1(
      CreateWebContents());
  WebViewTestWebContentsObserver observer1(web_contents1.get());
  EXPECT_FALSE(observer1.was_shown());

  web_view()->SetWebContents(web_contents1.get());
  // Layout() is normally async, call it now to ensure visibility is updated.
  web_view()->Layout();
  EXPECT_TRUE(observer1.was_shown());
#if defined(USE_AURA)
  EXPECT_TRUE(web_contents1->GetNativeView()->IsVisible());
#endif
  EXPECT_EQ(observer1.shown_count(), 1);
  EXPECT_EQ(observer1.hidden_count(), 0);
  EXPECT_TRUE(observer1.valid_root_while_shown());

  // Case 2: Create another WebContents and replace the current WebContents
  // via SetWebContents(). This should hide the current WebContents and show
  // the new one.
  const std::unique_ptr<content::WebContents> web_contents2(
      CreateWebContents());
  WebViewTestWebContentsObserver observer2(web_contents2.get());
  EXPECT_FALSE(observer2.was_shown());

  // Setting the new WebContents should hide the existing one.
  web_view()->SetWebContents(web_contents2.get());
  // Layout() is normally async, call it now to ensure visibility is updated.
  web_view()->Layout();
  EXPECT_FALSE(observer1.was_shown());
  EXPECT_TRUE(observer2.was_shown());
  EXPECT_TRUE(observer2.valid_root_while_shown());

  // WebContents1 should not get stray show calls when WebContents2 is set.
  EXPECT_EQ(observer1.shown_count(), 1);
  EXPECT_EQ(observer1.hidden_count(), 1);
  EXPECT_EQ(observer2.shown_count(), 1);
  EXPECT_EQ(observer2.hidden_count(), 0);

  // Case 3: Test that attaching to a hidden webview does not show the web
  // contents.
  web_view()->SetVisible(false);
  EXPECT_EQ(1, observer2.hidden_count());  // Now hidden.

  EXPECT_EQ(1, observer1.shown_count());
  web_view()->SetWebContents(web_contents1.get());
  // Layout() is normally async, call it now to ensure visibility is updated.
  web_view()->Layout();
  EXPECT_EQ(1, observer1.shown_count());

  // Nothing else should change.
  EXPECT_EQ(1, observer1.hidden_count());
  EXPECT_EQ(1, observer2.shown_count());
  EXPECT_EQ(1, observer2.hidden_count());

#if defined(USE_AURA)
  // Case 4: Test that making the webview visible when a window has an invisible
  // parent does not make the web contents visible.
  top_level_widget()->Hide();
  web_view()->SetVisible(true);
  EXPECT_EQ(1, observer1.shown_count());
  top_level_widget()->Show();
  EXPECT_EQ(2, observer1.shown_count());
  top_level_widget()->Hide();
  EXPECT_EQ(2, observer1.hidden_count());
#else
  // On Mac, changes to window visibility do not trigger calls to WebContents::
  // WasShown() or WasHidden(), since the OS does not provide good signals for
  // window visibility. However, we can still test that moving a visible WebView
  // whose WebContents is not currently showing to a new, visible window will
  // show the WebContents. Simulate the "hide window with visible WebView" step
  // simply by detaching the WebContents.
  web_view()->SetVisible(true);
  EXPECT_EQ(2, observer1.shown_count());
  web_view()->holder()->Detach();
  EXPECT_EQ(2, observer1.hidden_count());
#endif
  // Case 5: Test that moving from a hidden parent to a visible parent makes the
  // web contents visible.
  Widget* parent2 = CreateTopLevelFramelessPlatformWidget();
  parent2->SetBounds(gfx::Rect(0, 10, 100, 100));
  parent2->Show();
  EXPECT_EQ(2, observer1.shown_count());
  // Note: that reparenting the windows directly, after the windows have been
  // created, e.g., Widget::ReparentNativeView(widget, parent2), is not a
  // supported use case. Instead, move the WebView over.
  web_view()->parent()->RemoveChildView(web_view());
  parent2->SetContentsView(web_view());
  EXPECT_EQ(3, observer1.shown_count());
  parent2->Close();
}

// Tests that the layout of the NativeViewHost within WebView behaves as
// expected when embedding a fullscreen widget during WebContents screen
// capture.
TEST_F(WebViewUnitTest, EmbeddedFullscreenDuringScreenCapture_Layout) {
  web_view()->SetEmbedFullscreenWidgetMode(true);
  ASSERT_EQ(1u, web_view()->children().size());

  const std::unique_ptr<content::WebContents> web_contents(CreateWebContents());
  WebViewTestWebContentsDelegate delegate;
  web_contents->SetDelegate(&delegate);
  web_view()->SetWebContents(web_contents.get());

  // Initially, the holder should fill the entire WebView.
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), holder()->bounds());

  // Simulate a transition into fullscreen mode, but without screen capture
  // active on the WebContents, the holder should still fill the entire
  // WebView like before.
  delegate.set_is_fullscreened(true);
  static_cast<content::WebContentsObserver*>(web_view())->
      DidToggleFullscreenModeForTab(true, false);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), holder()->bounds());

  // ...and transition back out of fullscreen mode.
  delegate.set_is_fullscreened(false);
  static_cast<content::WebContentsObserver*>(web_view())->
      DidToggleFullscreenModeForTab(false, false);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), holder()->bounds());

  // Now, begin screen capture of the WebContents and then enter fullscreen
  // mode.  This time, the holder should be centered within WebView and
  // sized to match the capture size.
  const gfx::Size capture_size(64, 48);
  web_contents->IncrementCapturerCount(capture_size, /* stay_hidden */ false);
  delegate.set_is_fullscreened(true);
  static_cast<content::WebContentsObserver*>(web_view())->
      DidToggleFullscreenModeForTab(true, false);

  // The expected size should be scaled to whichever dimension matches the
  // holder first, with the other scaled from the capture size to match the
  // holder.  So 100, 100 holder size and 64, 48 capture size gives:
  // 100 / 64 * 48 = 75
  // The positioning centers the unmatched holder/capture dimension, giving:
  // (100 - 75 = 25) / 2 = 12
  EXPECT_EQ(gfx::Rect(0, 12, 100, 75), holder()->bounds());

  // Resize the WebView so that its width is smaller than the capture width.
  // Expect the holder to be scaled-down, letterboxed style.
  web_view()->SetBoundsRect(gfx::Rect(0, 0, 32, 100));
  EXPECT_EQ(gfx::Rect(0, 38, 32, 24), holder()->bounds());

  // Resize the WebView so that its height is smaller than the capture height.
  // Expect the holder to be scaled-down, pillarboxed style.
  web_view()->SetBoundsRect(gfx::Rect(0, 0, 100, 24));
  EXPECT_EQ(gfx::Rect(34, 0, 32, 24), holder()->bounds());

  // Transition back out of fullscreen mode a final time and confirm the bounds
  // of the holder fill the entire WebView once again.
  delegate.set_is_fullscreened(false);
  static_cast<content::WebContentsObserver*>(web_view())->
      DidToggleFullscreenModeForTab(false, false);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 24), holder()->bounds());
}

// Tests that a WebView correctly switches between WebContentses when one of
// them is embedding a fullscreen widget during WebContents screen capture.
TEST_F(WebViewUnitTest, EmbeddedFullscreenDuringScreenCapture_Switching) {
  web_view()->SetEmbedFullscreenWidgetMode(true);
  ASSERT_EQ(1u, web_view()->children().size());
  const gfx::NativeView unset_native_view = holder()->native_view();

  // Create two WebContentses to switch between.
  const std::unique_ptr<content::WebContents> web_contents1(
      CreateWebContents());
  WebViewTestWebContentsDelegate delegate1;
  web_contents1->SetDelegate(&delegate1);
  const std::unique_ptr<content::WebContents> web_contents2(
      CreateWebContents());
  WebViewTestWebContentsDelegate delegate2;
  web_contents2->SetDelegate(&delegate2);

  EXPECT_NE(web_contents1->GetNativeView(), holder()->native_view());
  web_view()->SetWebContents(web_contents1.get());
  EXPECT_EQ(web_contents1->GetNativeView(), holder()->native_view());
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), holder()->bounds());

  // Begin screen capture of the WebContents and then enter fullscreen mode.
  // The native view should not have changed, but the layout of its holder will
  // have (indicates WebView has responded).
  const gfx::Size capture_size(64, 48);
  web_contents1->IncrementCapturerCount(capture_size, /* stay_hidden */ false);
  delegate1.set_is_fullscreened(true);
  static_cast<content::WebContentsObserver*>(web_view())->
      DidToggleFullscreenModeForTab(true, false);
  EXPECT_EQ(web_contents1->GetNativeView(), holder()->native_view());
  EXPECT_EQ(gfx::Rect(0, 12, 100, 75), holder()->bounds());

  // When setting the WebContents to nullptr, the native view should become
  // unset.
  web_view()->SetWebContents(nullptr);
  EXPECT_EQ(unset_native_view, holder()->native_view());

  // ...and when setting the WebContents back to the currently-fullscreened
  // instance, expect the native view and layout to reflect that.
  web_view()->SetWebContents(web_contents1.get());
  EXPECT_EQ(web_contents1->GetNativeView(), holder()->native_view());
  EXPECT_EQ(gfx::Rect(0, 12, 100, 75), holder()->bounds());

  // Now, switch to a different, non-null WebContents instance and check that
  // the native view has changed and the holder is filling WebView again.
  web_view()->SetWebContents(web_contents2.get());
  EXPECT_EQ(web_contents2->GetNativeView(), holder()->native_view());
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), holder()->bounds());

  // Finally, switch back to the first WebContents (still fullscreened).
  web_view()->SetWebContents(web_contents1.get());
  EXPECT_EQ(web_contents1->GetNativeView(), holder()->native_view());
  EXPECT_EQ(gfx::Rect(0, 12, 100, 75), holder()->bounds());
}

// Tests that clicking anywhere within the bounds of WebView, and either outside
// or inside the bounds of its child NativeViewHost, causes WebView to gain
// focus.
TEST_F(WebViewUnitTest, EmbeddedFullscreenDuringScreenCapture_ClickToFocus) {
  // For this test, add another View that can take focus away from WebView.
  web_view()->SetBoundsRect(gfx::Rect(0, 0, 100, 90));
  views::View* const something_to_focus = new views::View();
  something_to_focus->SetBoundsRect(gfx::Rect(0, 90, 100, 10));
  something_to_focus->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  top_level_widget()->GetContentsView()->AddChildView(something_to_focus);

  web_view()->SetEmbedFullscreenWidgetMode(true);
  ASSERT_EQ(1u, web_view()->children().size());

  const std::unique_ptr<content::WebContents> web_contents(CreateWebContents());
  WebViewTestWebContentsDelegate delegate;
  web_contents->SetDelegate(&delegate);
  web_view()->SetWebContents(web_contents.get());

  // Begin screen capture of the WebContents and then enter fullscreen mode.
  // The holder should be centered within WebView and sized to match the capture
  // size.
  const gfx::Size capture_size(64, 48);
  web_contents->IncrementCapturerCount(capture_size, /* stay_hidden */ false);
  delegate.set_is_fullscreened(true);
  static_cast<content::WebContentsObserver*>(web_view())->
      DidToggleFullscreenModeForTab(true, false);
  EXPECT_EQ(gfx::Rect(0, 7, 100, 75), holder()->bounds());

  // Focus the other widget.
  something_to_focus->RequestFocus();
  EXPECT_FALSE(web_view()->HasFocus());
  EXPECT_FALSE(holder()->HasFocus());
  EXPECT_TRUE(something_to_focus->HasFocus());

  // Send mouse press event to WebView outside the bounds of the holder, and
  // confirm WebView took focus.
  const ui::MouseEvent click_outside_holder(
      ui::ET_MOUSE_PRESSED, gfx::Point(1, 1),
      gfx::Point(),  // Immaterial.
      ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);
  EXPECT_TRUE(static_cast<views::View*>(web_view())->
                  OnMousePressed(click_outside_holder));
  EXPECT_TRUE(web_view()->HasFocus());
  EXPECT_FALSE(holder()->HasFocus());
  EXPECT_FALSE(something_to_focus->HasFocus());

  // Focus the other widget again.
  something_to_focus->RequestFocus();
  EXPECT_FALSE(web_view()->HasFocus());
  EXPECT_FALSE(holder()->HasFocus());
  EXPECT_TRUE(something_to_focus->HasFocus());

  // Send a mouse press event within the bounds of the holder and expect no
  // focus change.  The reason is that WebView is not supposed to handle mouse
  // events within the bounds of the holder, and it would be up to the
  // WebContents native view to grab the focus instead.  In this test
  // environment, the WebContents native view doesn't include the implementation
  // needed to grab focus, so no focus change will occur.
  const ui::MouseEvent click_inside_holder(
      ui::ET_MOUSE_PRESSED, web_view()->bounds().CenterPoint(),
      gfx::Point(),  // Immaterial.
      ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);
  EXPECT_FALSE(static_cast<views::View*>(web_view())->
                  OnMousePressed(click_inside_holder));
  EXPECT_FALSE(web_view()->HasFocus());
  EXPECT_FALSE(holder()->HasFocus());
  EXPECT_TRUE(something_to_focus->HasFocus());
}

// Verifies that there is no crash in WebView destructor
// if WebView is already removed from Widget.
TEST_F(WebViewUnitTest, DetachedWebViewDestructor) {
  // Init WebView with attached NativeView.
  const std::unique_ptr<content::WebContents> web_contents(CreateWebContents());
  std::unique_ptr<WebView> webview(
      new WebView(web_contents->GetBrowserContext()));
  View* contents_view = top_level_widget()->GetContentsView();
  contents_view->AddChildView(webview.get());
  webview->SetWebContents(web_contents.get());

  // Remove WebView from views hierarchy. NativeView should be detached
  // from Widget.
  contents_view->RemoveChildView(webview.get());
  // Destroy WebView. NativeView should be detached secondary.
  // There should be no crash.
  webview.reset();
}

// Test that the specified crashed overlay view is shown when a WebContents
// is in a crashed state.
TEST_F(WebViewUnitTest, CrashedOverlayView) {
  const std::unique_ptr<content::WebContents> web_contents(CreateWebContents());
  std::unique_ptr<WebView> web_view(
      new WebView(web_contents->GetBrowserContext()));
  View* contents_view = top_level_widget()->GetContentsView();
  contents_view->AddChildView(web_view.get());
  web_view->SetWebContents(web_contents.get());

  View* crashed_overlay_view = new View();
  web_view->SetCrashedOverlayView(crashed_overlay_view);
  EXPECT_FALSE(crashed_overlay_view->IsDrawn());

  // Normally when a renderer crashes, the WebView will learn about it
  // automatically via WebContentsObserver. Since this is a test
  // WebContents, simulate that by calling SetIsCrashed and then
  // explicitly calling RenderViewDeleted on the WebView to trigger it
  // to swap in the crashed overlay view.
  web_contents->SetIsCrashed(base::TERMINATION_STATUS_PROCESS_CRASHED, -1);
  EXPECT_TRUE(web_contents->IsCrashed());
  static_cast<content::WebContentsObserver*>(web_view.get())
      ->RenderViewDeleted(nullptr);
  EXPECT_TRUE(crashed_overlay_view->IsDrawn());
}

// Test that a crashed overlay view isn't deleted if it's owned by client.
TEST_F(WebViewUnitTest, CrashedOverlayViewOwnedbyClient) {
  const std::unique_ptr<content::WebContents> web_contents(CreateWebContents());
  std::unique_ptr<WebView> web_view(
      new WebView(web_contents->GetBrowserContext()));
  View* contents_view = top_level_widget()->GetContentsView();
  contents_view->AddChildView(web_view.get());
  web_view->SetWebContents(web_contents.get());

  View* crashed_overlay_view = new View();
  crashed_overlay_view->set_owned_by_client();
  web_view->SetCrashedOverlayView(crashed_overlay_view);
  EXPECT_FALSE(crashed_overlay_view->IsDrawn());

  // Simulate a renderer crash (see above).
  web_contents->SetIsCrashed(base::TERMINATION_STATUS_PROCESS_CRASHED, -1);
  EXPECT_TRUE(web_contents->IsCrashed());
  static_cast<content::WebContentsObserver*>(web_view.get())
      ->RenderViewDeleted(nullptr);
  EXPECT_TRUE(crashed_overlay_view->IsDrawn());

  web_view->SetCrashedOverlayView(nullptr);
  web_view.reset();

  // This shouldn't crash, we still own this.
  delete crashed_overlay_view;
}

#if defined(USE_AURA)
namespace {

// TODO(sky): factor this for mac.
gfx::Rect GetNativeViewBounds(gfx::NativeView native_view) {
  return native_view->bounds();
}

}  // namespace

TEST_F(WebViewUnitTest, LayoutFullscreenNativeView) {
  web_view()->SetEmbedFullscreenWidgetMode(true);
  // WebView lazily creates WebContents. Force creation.
  web_view()->GetWebContents();
  // Layout is async, force a layout now to ensure bounds are set.
  web_view()->Layout();
  const gfx::Rect initial_bounds =
      GetNativeViewBounds(web_view()->GetWebContents()->GetNativeView());
  EXPECT_NE(gfx::Rect(), initial_bounds);

  // Create another WebContents for a separate gfx::NativeView. The WebContent's
  // gfx::NativeView is used as the fullscreen widget for web_view().
  const std::unique_ptr<content::WebContents> fullscreen_web_contents(
      CreateWebContents());
  EXPECT_NE(initial_bounds,
            GetNativeViewBounds(fullscreen_web_contents->GetNativeView()));
  SetFullscreenNativeView(web_view(), fullscreen_web_contents->GetNativeView());

  // Trigger going fullscreen. Once fullscreen, the fullscreen gfx::NativeView
  // should be immediately resized.
  static_cast<content::WebContentsObserver*>(web_view())
      ->DidShowFullscreenWidget();
  EXPECT_EQ(initial_bounds,
            GetNativeViewBounds(fullscreen_web_contents->GetNativeView()));

  static_cast<content::WebContentsObserver*>(web_view())
      ->DidDestroyFullscreenWidget();
}
#endif

}  // namespace views
