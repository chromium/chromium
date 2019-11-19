// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_WEBVIEW_WEBVIEW_H_
#define UI_VIEWS_CONTROLS_WEBVIEW_WEBVIEW_H_

#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/webview/webview_export.h"
#include "ui/views/view.h"

namespace views {

// Provides a view of a WebContents instance.  WebView can be used standalone,
// creating and displaying an internally-owned WebContents; or within a full
// browser where the browser swaps its own WebContents instances in/out (e.g.,
// for browser tabs).
//
// WebView creates and owns a single child view, a NativeViewHost, which will
// hold and display the native view provided by a WebContents.
//
// EmbedFullscreenWidgetMode: When enabled, WebView will observe for WebContents
// fullscreen changes and automatically swap the normal native view with the
// fullscreen native view (if different).  In addition, if the WebContents is
// being screen-captured, the view will be centered within WebView, sized to
// the aspect ratio of the capture video resolution, and scaling will be avoided
// whenever possible.
class WEBVIEW_EXPORT WebView : public View,
                               public content::WebContentsDelegate,
                               public content::WebContentsObserver {
 public:
  METADATA_HEADER(WebView);

  explicit WebView(content::BrowserContext* browser_context);
  ~WebView() override;

  // This creates a WebContents if none is yet associated with this WebView. The
  // WebView owns this implicitly created WebContents.
  content::WebContents* GetWebContents();

  // WebView does not assume ownership of WebContents set via this method, only
  // those it implicitly creates via GetWebContents() above.
  void SetWebContents(content::WebContents* web_contents);

  // If |mode| is true, WebView will register itself with WebContents as a
  // WebContentsObserver, monitor for the showing/destruction of fullscreen
  // render widgets, and alter its child view hierarchy to embed the fullscreen
  // widget or restore the normal WebContentsView.
  void SetEmbedFullscreenWidgetMode(bool mode);

  content::BrowserContext* browser_context() { return browser_context_; }

  // Loads the initial URL to display in the attached WebContents. Creates the
  // WebContents if none is attached yet. Note that this is intended as a
  // convenience for loading the initial URL, and so URLs are navigated with
  // PAGE_TRANSITION_AUTO_TOPLEVEL, so this is not intended as a general purpose
  // navigation method - use WebContents' API directly.
  void LoadInitialURL(const GURL& url);

  // Controls how the attached WebContents is resized.
  // false = WebContents' views' bounds are updated continuously as the
  //         WebView's bounds change (default).
  // true  = WebContents' views' position is updated continuously but its size
  //         is not (which may result in some clipping or under-painting) until
  //         a continuous size operation completes. This allows for smoother
  //         resizing performance during interactive resizes and animations.
  void SetFastResize(bool fast_resize);

  // If enabled, this will make the WebView's preferred size dependent on the
  // WebContents' size.
  void EnableSizingFromWebContents(const gfx::Size& min_size,
                                   const gfx::Size& max_size);

  // If provided, this View will be shown in place of the web contents
  // when the web contents is in a crashed state. This is cleared automatically
  // if the web contents is changed.
  void SetCrashedOverlayView(View* crashed_overlay_view);

  // When used to host UI, we need to explicitly allow accelerators to be
  // processed. Default is false.
  void set_allow_accelerators(bool allow_accelerators) {
    allow_accelerators_ = allow_accelerators;
  }

  // Overridden from content::WebContentsDelegate:
  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override;

  NativeViewHost* holder() { return holder_; }
  using WebContentsCreator =
      base::RepeatingCallback<std::unique_ptr<content::WebContents>(
          content::BrowserContext*)>;

  // An instance of this class registers a WebContentsCreator on construction
  // and deregisters the WebContentsCreator on destruction.
  class WEBVIEW_EXPORT ScopedWebContentsCreatorForTesting {
   public:
    explicit ScopedWebContentsCreatorForTesting(WebContentsCreator creator);
    ~ScopedWebContentsCreatorForTesting();

   private:
    DISALLOW_COPY_AND_ASSIGN(ScopedWebContentsCreatorForTesting);
  };

 protected:
  // Called when the web contents is successfully attached.
  virtual void OnWebContentsAttached() {}
  // Called when letterboxing (scaling the native view to preserve aspect
  // ratio) is enabled or disabled.
  virtual void OnLetterboxingChanged() {}
  bool is_letterboxing() const { return is_letterboxing_; }

  const gfx::Size& min_size() const { return min_size_; }
  const gfx::Size& max_size() const { return max_size_; }

  // Overridden from View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnFocus() override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;

  // Overridden from content::WebContentsDelegate:
  bool EmbedsFullscreenWidget() override;

  // Overridden from content::WebContentsObserver:
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;
  void RenderViewReady() override;
  void RenderViewDeleted(content::RenderViewHost* render_view_host) override;
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;
  void WebContentsDestroyed() override;
  void DidShowFullscreenWidget() override;
  void DidDestroyFullscreenWidget() override;
  void DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                     bool will_cause_resize) override;
  void DidAttachInterstitialPage() override;
  void DidDetachInterstitialPage() override;
  // Workaround for MSVC++ linker bug/feature that requires
  // instantiation of the inline IPC::Listener methods in all translation units.
  void OnChannelConnected(int32_t peer_id) override {}
  void OnChannelError() override {}
  void OnBadMessageReceived(const IPC::Message& message) override {}
  void OnWebContentsFocused(
      content::RenderWidgetHost* render_widget_host) override;
  void RenderProcessGone(base::TerminationStatus status) override;

 private:
  friend class WebViewUnitTest;

  void AttachWebContentsNativeView();
  void DetachWebContentsNativeView();
  void ReattachForFullscreenChange(bool enter_fullscreen);
  void UpdateCrashedOverlayView();
  void NotifyAccessibilityWebContentsChanged();

  // Registers for ResizeDueToAutoResize() notifications from the
  // RenderWidgetHostView whenever it is created or changes, if
  // EnableSizingFromWebContents() has been called.
  void MaybeEnableAutoResize();

  // Create a regular or test web contents (based on whether we're running
  // in a unit test or not).
  std::unique_ptr<content::WebContents> CreateWebContents(
      content::BrowserContext* browser_context);

  NativeViewHost* const holder_ =
      AddChildView(std::make_unique<NativeViewHost>());
  // Non-NULL if |web_contents()| was created and is owned by this WebView.
  std::unique_ptr<content::WebContents> wc_owner_;
  // When true, WebView auto-embeds fullscreen widgets as a child view.
  bool embed_fullscreen_widget_mode_enabled_ = false;
  // Set to true while WebView is embedding a fullscreen widget view as a child
  // view instead of the normal WebContentsView render view. Note: This will be
  // false in the case of non-Flash fullscreen.
  bool is_embedding_fullscreen_widget_ = false;
  // Set to true when |holder_| is letterboxed (scaled to be smaller than this
  // view, to preserve its aspect ratio).
  bool is_letterboxing_ = false;
  content::BrowserContext* browser_context_;
  bool allow_accelerators_ = false;
  View* crashed_overlay_view_ = nullptr;

  // Minimum and maximum sizes to determine WebView bounds for auto-resizing.
  // Empty if auto resize is not enabled.
  gfx::Size min_size_;
  gfx::Size max_size_;

  // Tracks the child accessibility tree id which is associated with the
  // WebContents's main RenderFrameHost.
  ui::AXTreeID child_ax_tree_id_;

  // Used as the fullscreen NativeView if
  // |embed_fullscreen_widget_mode_enabled_| is enabled. This is only set in
  // tests as injecting a different value for
  // WebContents::GetFullscreenRenderWidgetHostView() is rather tricky in
  // unit-tests.
  gfx::NativeView fullscreen_native_view_for_testing_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WebView);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_WEBVIEW_WEBVIEW_H_
