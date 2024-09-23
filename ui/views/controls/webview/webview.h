// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_WEBVIEW_WEBVIEW_H_
#define UI_VIEWS_CONTROLS_WEBVIEW_WEBVIEW_H_

#include <stdint.h>

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/accessibility/ax_mode_observer.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/webview/webview_export.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"

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
                               public content::WebContentsObserver,
                               public ui::AXModeObserver {
  METADATA_HEADER(WebView, View)

 public:
  // Whether the navigation should be allowed to be automatically upgraded to
  // HTTPS. Only applies to initial loads.
  enum class HttpsUpgradePolicy {
    // Allows the navigation to be upgraded to HTTPS when possible.
    kAllowUpgrade,
    // Exempts the navigation from being upgraded to HTTPS (e.g. when loading
    // a captive portal login page).
    kNoUpgrade,
  };

  using WebContentsAttachedCallback = base::RepeatingCallback<void(WebView*)>;

  explicit WebView(content::BrowserContext* browser_context = nullptr);

  WebView(const WebView&) = delete;
  WebView& operator=(const WebView&) = delete;

  ~WebView() override;

  static bool IsWebViewContents(const content::WebContents* web_contents);

  // This creates a WebContents if |browser_context_| has been set and there is
  // not yet a WebContents associated with this WebView, otherwise it will
  // return a nullptr.
  content::WebContents* GetWebContents(
      base::Location creator_location = base::Location::Current());

  // WebView does not assume ownership of WebContents set via this method, only
  // those it implicitly creates via GetWebContents() above.
  void SetWebContents(content::WebContents* web_contents);

  content::BrowserContext* GetBrowserContext();
  void SetBrowserContext(content::BrowserContext* browser_context);

  // Loads the initial URL to display in the attached WebContents. Creates the
  // WebContents if none is attached yet. Note that this is intended as a
  // convenience for loading the initial URL, and so URLs are navigated with
  // PAGE_TRANSITION_AUTO_TOPLEVEL, so this is not intended as a general purpose
  // navigation method - use WebContents' API directly.
  void LoadInitialURL(
      const GURL& url,
      HttpsUpgradePolicy https_upgrade_policy =
          HttpsUpgradePolicy::kAllowUpgrade,
      base::Location invoke_location = base::Location::Current());

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
  // if the web contents is changed. The passed-in overlay view must be owned by
  // the client; this method never takes ownership of it.
  //
  // TODO(crbug.com/40278361): This method should take ownership of
  // `crashed_overlay_view`.
  void SetCrashedOverlayView(View* crashed_overlay_view);

  // Adds a callback for when a WebContents is attached to this WebView.
  base::CallbackListSubscription AddWebContentsAttachedCallback(
      WebContentsAttachedCallback callback);

  // Sets whether this is the primary web contents for the window.
  void set_is_primary_web_contents_for_window(bool is_primary) {
    is_primary_web_contents_for_window_ = is_primary;
  }

  // When used to host UI, we need to explicitly allow accelerators to be
  // processed. Default is false.
  void set_allow_accelerators(bool allow_accelerators) {
    allow_accelerators_ = allow_accelerators;
  }

  // When `lock = true` changes in web contents will not reset the override.
  // Default is false.
  void set_lock_child_ax_tree_id_override(bool lock) {
    lock_child_ax_tree_id_override_ = lock;
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

    ScopedWebContentsCreatorForTesting(
        const ScopedWebContentsCreatorForTesting&) = delete;
    ScopedWebContentsCreatorForTesting& operator=(
        const ScopedWebContentsCreatorForTesting&) = delete;

    ~ScopedWebContentsCreatorForTesting();
  };

 protected:
  // Called when letterboxing (scaling the native view to preserve aspect
  // ratio) is enabled or disabled.
  virtual void OnLetterboxingChanged() {}
  bool is_letterboxing() const { return is_letterboxing_; }

  const gfx::Size& min_size() const { return min_size_; }
  const gfx::Size& max_size() const { return max_size_; }

  // View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnFocus() override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  // Overridden from content::WebContentsObserver:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;
  void DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                     bool will_cause_resize) override;
  void OnWebContentsFocused(
      content::RenderWidgetHost* render_widget_host) override;
  void AXTreeIDForMainFrameHasChanged() override;
  void WebContentsDestroyed() override;

  // Override from ui::AXModeObserver
  void OnAXModeAdded(ui::AXMode mode) override;

 private:
  friend class WebViewUnitTest;

  void AttachWebContentsNativeView();
  void DetachWebContentsNativeView();
  void UpdateCrashedOverlayView();
  void NotifyAccessibilityWebContentsChanged();

  // Called when the main frame in the renderer becomes present.
  void SetUpNewMainFrame(content::RenderFrameHost* frame_host);
  // Called when the main frame in the renderer is no longer present.
  void LostMainFrame();

  // Registers for ResizeDueToAutoResize() notifications from `frame_host`'s
  // RenderWidgetHostView whenever it is created or changes, if
  // EnableSizingFromWebContents() has been called. This should only be called
  // for main frames; other frames can not have auto resize set.
  void MaybeEnableAutoResize(content::RenderFrameHost* frame_host);

  // Create a regular or test web contents (based on whether we're running
  // in a unit test or not).
  std::unique_ptr<content::WebContents> CreateWebContents(
      content::BrowserContext* browser_context,
      base::Location creator_location = base::Location::Current());

  const raw_ptr<NativeViewHost> holder_ =
      AddChildView(std::make_unique<NativeViewHost>());
  base::ScopedObservation<ui::AXPlatform, ui::AXModeObserver>
      ax_mode_observation_{this};
  // Non-NULL if |web_contents()| was created and is owned by this WebView.
  std::unique_ptr<content::WebContents> wc_owner_;
  // Set to true when |holder_| is letterboxed (scaled to be smaller than this
  // view, to preserve its aspect ratio).
  bool is_letterboxing_ = false;
  raw_ptr<content::BrowserContext> browser_context_;
  bool allow_accelerators_ = false;
  ViewTracker crashed_overlay_view_;
  bool is_primary_web_contents_for_window_ = false;

  bool lock_child_ax_tree_id_override_ = false;

  // Minimum and maximum sizes to determine WebView bounds for auto-resizing.
  // Empty if auto resize is not enabled.
  gfx::Size min_size_;
  gfx::Size max_size_;

  // List of subscriptions listening for new WebContents being attached to this
  // WebView.
  base::RepeatingCallbackList<void(WebView*)> web_contents_attached_callbacks_;
};

BEGIN_VIEW_BUILDER(WEBVIEW_EXPORT, WebView, View)
VIEW_BUILDER_PROPERTY(content::BrowserContext*, BrowserContext)
VIEW_BUILDER_PROPERTY(content::WebContents*, WebContents)
VIEW_BUILDER_PROPERTY(bool, FastResize)
VIEW_BUILDER_METHOD(EnableSizingFromWebContents,
                    const gfx::Size&,
                    const gfx::Size&)
VIEW_BUILDER_PROPERTY(View*, CrashedOverlayView)
VIEW_BUILDER_METHOD(set_is_primary_web_contents_for_window, bool)
VIEW_BUILDER_METHOD(set_allow_accelerators, bool)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(WEBVIEW_EXPORT, WebView)

#endif  // UI_VIEWS_CONTROLS_WEBVIEW_WEBVIEW_H_
