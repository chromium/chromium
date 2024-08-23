// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/webview/webview.h"

#include <string>
#include <utility>

#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/views_delegate.h"

namespace views {

namespace {

// This key indicates that a WebContents is used by a WebView.
const void* const kIsWebViewContentsKey = &kIsWebViewContentsKey;

// A testing stub that creates web contents.
WebView::WebContentsCreator* GetCreatorForTesting() {
  static base::NoDestructor<WebView::WebContentsCreator> creator;
  return creator.get();
}

// Updates the parent accessible object on the NativeView. As WebView overrides
// GetNativeViewAccessible() to return the accessible from the WebContents, it
// needs to ensure the accessible from the parent is set on the NativeView.
void UpdateNativeViewHostAccessibleParent(NativeViewHost* holder,
                                          View* parent) {
  if (!parent) {
    return;
  }
  holder->SetParentAccessible(parent->GetNativeViewAccessible());
}

}  // namespace

WebView::ScopedWebContentsCreatorForTesting::ScopedWebContentsCreatorForTesting(
    WebContentsCreator creator) {
  DCHECK(!*GetCreatorForTesting());
  *GetCreatorForTesting() = creator;
}

WebView::ScopedWebContentsCreatorForTesting::
    ~ScopedWebContentsCreatorForTesting() {
  *GetCreatorForTesting() = WebView::WebContentsCreator();
}

////////////////////////////////////////////////////////////////////////////////
// WebView, public:

WebView::WebView(content::BrowserContext* browser_context) {
  set_suppress_default_focus_handling();
  ax_mode_observation_.Observe(&ui::AXPlatform::GetInstance());
  SetBrowserContext(browser_context);
  GetViewAccessibility().SetRole(ax::mojom::Role::kWebView);
  // A webview does not need an accessible name as the document title is
  // provided via other means. Providing it here would be redundant.
  // Mark the name as explicitly empty so that accessibility_checks pass.
  GetViewAccessibility().SetName(
      std::string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
}

WebView::~WebView() {
  SetWebContents(nullptr);  // Make sure all necessary tear-down takes place.
  browser_context_ = nullptr;
}

// static
bool WebView::IsWebViewContents(const content::WebContents* web_contents) {
  return web_contents->GetUserData(kIsWebViewContentsKey);
}

content::WebContents* WebView::GetWebContents(base::Location creator_location) {
  if (!web_contents()) {
    if (!browser_context_) {
      return nullptr;
    }
    wc_owner_ = CreateWebContents(browser_context_, creator_location);
    wc_owner_->SetDelegate(this);
    SetWebContents(wc_owner_.get());
  }
  return web_contents();
}

void WebView::SetWebContents(content::WebContents* replacement) {
  TRACE_EVENT0("views", "WebView::SetWebContents");
  if (replacement == web_contents()) {
    return;
  }
  SetCrashedOverlayView(nullptr);
  DetachWebContentsNativeView();
  WebContentsObserver::Observe(replacement);

  // Do not remove the observation of the previously hosted WebContents to allow
  // the WebContents to continue to use the source for colors and receive update
  // notifications when in the background and not directly part of a UI
  // hierarchy. This avoids color pop-in if the WebContents is re-inserted into
  // the same hierarchy at a later point in time.
  if (replacement) {
    replacement->SetColorProviderSource(GetWidget());
    replacement->SetUserData(kIsWebViewContentsKey,
                             std::make_unique<base::SupportsUserData::Data>());
  }

  // web_contents() now returns |replacement| from here onwards.
  if (wc_owner_.get() != replacement) {
    wc_owner_.reset();
  }
  AttachWebContentsNativeView();

  if (replacement && replacement->GetPrimaryMainFrame()->IsRenderFrameLive()) {
    SetUpNewMainFrame(replacement->GetPrimaryMainFrame());
  } else {
    LostMainFrame();
  }
}

content::BrowserContext* WebView::GetBrowserContext() {
  return browser_context_;
}

void WebView::SetBrowserContext(content::BrowserContext* browser_context) {
  browser_context_ = browser_context;
}

void WebView::LoadInitialURL(const GURL& url,
                             HttpsUpgradePolicy https_upgrade_policy,
                             base::Location invoke_location) {
  content::NavigationController::LoadURLParams params(url);
  params.referrer = content::Referrer();
  params.transition_type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  params.force_no_https_upgrade =
      https_upgrade_policy == HttpsUpgradePolicy::kNoUpgrade;
  content::WebContents* web_contents = GetWebContents(invoke_location);
  DCHECK(web_contents);
  web_contents->GetController().LoadURLWithParams(params);
}

void WebView::SetFastResize(bool fast_resize) {
  holder_->set_fast_resize(fast_resize);
}

void WebView::EnableSizingFromWebContents(const gfx::Size& min_size,
                                          const gfx::Size& max_size) {
  DCHECK(!max_size.IsEmpty());
  min_size_ = min_size;
  max_size_ = max_size;
  if (web_contents() &&
      web_contents()->GetPrimaryMainFrame()->IsRenderFrameLive()) {
    MaybeEnableAutoResize(web_contents()->GetPrimaryMainFrame());
  }
}

void WebView::SetCrashedOverlayView(View* crashed_overlay_view) {
  if (crashed_overlay_view_.view() == crashed_overlay_view) {
    return;
  }

  if (crashed_overlay_view_.view()) {
    RemoveChildView(crashed_overlay_view_.view());
    // Show the hosted web contents view iff the crashed
    // overlay is NOT showing, to ensure hit testing is
    // correct on Mac. See https://crbug.com/896508
    holder_->SetVisible(true);
  }

  crashed_overlay_view_.SetView(crashed_overlay_view);

  if (crashed_overlay_view_.view()) {
    CHECK(crashed_overlay_view_.view()->owned_by_client());
    AddChildView(crashed_overlay_view_.view());
    holder_->SetVisible(false);
    crashed_overlay_view_.view()->SetBoundsRect(GetLocalBounds());
  }

  UpdateCrashedOverlayView();
}

base::CallbackListSubscription WebView::AddWebContentsAttachedCallback(
    WebContentsAttachedCallback callback) {
  return web_contents_attached_callbacks_.Add(callback);
}

////////////////////////////////////////////////////////////////////////////////
// WebView, View overrides:

void WebView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  View* overlay = crashed_overlay_view_.view();
  if (overlay) {
    overlay->SetBoundsRect(GetLocalBounds());
  }

  // In most cases, the holder is simply sized to fill this WebView's bounds.
  // Only WebContentses that are in fullscreen mode and being screen-captured
  // will engage the special layout/sizing behavior.
  gfx::Rect holder_bounds = GetContentsBounds();
  if (!web_contents() || !web_contents()->IsBeingCaptured() ||
      web_contents()->GetPreferredSize().IsEmpty() ||
      !(web_contents()->GetDelegate() &&
        web_contents()->GetDelegate()->IsFullscreenForTabOrPending(
            web_contents()))) {
    // Reset the native view size.
    holder_->SetNativeViewSize(gfx::Size());
    holder_->SetBoundsRect(holder_bounds);
    if (is_letterboxing_) {
      is_letterboxing_ = false;
      OnLetterboxingChanged();
    }
    return;
  }

  // For screen-captured fullscreened content, scale the |holder_| to fit within
  // this View and center it.
  const gfx::Size capture_size = web_contents()->GetPreferredSize();
  const int64_t x =
      static_cast<int64_t>(capture_size.width()) * holder_bounds.height();
  const int64_t y =
      static_cast<int64_t>(capture_size.height()) * holder_bounds.width();
  if (y < x) {
    holder_bounds.ClampToCenteredSize(gfx::Size(
        holder_bounds.width(), static_cast<int>(y / capture_size.width())));
  } else {
    holder_bounds.ClampToCenteredSize(gfx::Size(
        static_cast<int>(x / capture_size.height()), holder_bounds.height()));
  }

  if (!is_letterboxing_) {
    is_letterboxing_ = true;
    OnLetterboxingChanged();
  }
  holder_->SetNativeViewSize(capture_size);
  holder_->SetBoundsRect(holder_bounds);
}

void WebView::ViewHierarchyChanged(const ViewHierarchyChangedDetails& details) {
  if (details.is_add) {
    AttachWebContentsNativeView();
  }
}

bool WebView::SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) {
  if (allow_accelerators_) {
    return FocusManager::IsTabTraversalKeyEvent(event);
  }

  // Don't look-up accelerators or tab-traversal if we are showing a non-crashed
  // TabContents.
  // We'll first give the page a chance to process the key events.  If it does
  // not process them, they'll be returned to us and we'll treat them as
  // accelerators then.
  return web_contents() && !web_contents()->IsCrashed();
}

bool WebView::OnMousePressed(const ui::MouseEvent& event) {
  // A left-click within WebView is a request to focus.  The area within the
  // native view child is excluded since it will be handling mouse pressed
  // events itself (http://crbug.com/436192).
  if (event.IsOnlyLeftMouseButton() && HitTestPoint(event.location())) {
    gfx::Point location_in_holder = event.location();
    ConvertPointToTarget(this, holder_, &location_in_holder);
    if (!holder_->HitTestPoint(location_in_holder)) {
      RequestFocus();
      return true;
    }
  }
  return View::OnMousePressed(event);
}

void WebView::OnFocus() {
  if (web_contents() && !web_contents()->IsCrashed()) {
    web_contents()->Focus();
  }
}

void WebView::AboutToRequestFocusFromTabTraversal(bool reverse) {
  if (web_contents() && !web_contents()->IsCrashed()) {
    web_contents()->FocusThroughTabTraversal(reverse);
  }
}

void WebView::AddedToWidget() {
  if (!web_contents()) {
    return;
  }

  web_contents()->SetColorProviderSource(GetWidget());

  // If added to a widget hierarchy and |holder_| already has a NativeView
  // attached, update the accessible parent here to support reparenting the
  // WebView.
  if (holder_->native_view()) {
    UpdateNativeViewHostAccessibleParent(holder_, parent());
  }
}

void WebView::RemovedFromWidget() {
  // Immediately clear the accessible parent upon being removed, as it's a
  // weak reference to an object that is about to be destroyed.
  if (holder_->native_view()) {
    holder_->SetParentAccessible(nullptr);
  }
}

gfx::NativeViewAccessible WebView::GetNativeViewAccessible() {
  if (web_contents() && !web_contents()->IsCrashed()) {
    content::RenderWidgetHostView* host_view =
        web_contents()->GetRenderWidgetHostView();
    if (host_view) {
      gfx::NativeViewAccessible accessible =
          host_view->GetNativeViewAccessible();
      // |accessible| needs to know whether this is the primary WebContents.
      if (is_primary_web_contents_for_window_) {
        if (auto* ax_platform_node =
                ui::AXPlatformNode::FromNativeViewAccessible(accessible)) {
          ax_platform_node->GetDelegate()->SetIsPrimaryWebContentsForWindow();
        }
      }
      return accessible;
    }
  }
  return View::GetNativeViewAccessible();
}

void WebView::OnAXModeAdded(ui::AXMode mode) {
  if (!GetWidget() || !web_contents()) {
    return;
  }

  // Normally, it is set during AttachWebContentsNativeView when the WebView is
  // created but this may not happen on some platforms as the accessible object
  // may not have been present when this WebView was created. So, update it when
  // AX mode is added.
  UpdateNativeViewHostAccessibleParent(holder(), parent());
}

////////////////////////////////////////////////////////////////////////////////
// WebView, content::WebContentsDelegate implementation:

////////////////////////////////////////////////////////////////////////////////
// WebView, content::WebContentsObserver implementation:

void WebView::RenderFrameCreated(content::RenderFrameHost* render_frame_host) {
  // Only handle the initial main frame, not speculative ones.
  if (render_frame_host != web_contents()->GetPrimaryMainFrame()) {
    return;
  }

  SetUpNewMainFrame(render_frame_host);
}

void WebView::RenderFrameDeleted(content::RenderFrameHost* render_frame_host) {
  // Only handle the active main frame, not speculative ones.
  if (render_frame_host != web_contents()->GetPrimaryMainFrame()) {
    return;
  }

  LostMainFrame();
}

void WebView::RenderFrameHostChanged(content::RenderFrameHost* old_host,
                                     content::RenderFrameHost* new_host) {
  // Since we skipped speculative main frames in RenderFrameCreated, we must
  // watch for them being swapped in by watching for RenderFrameHostChanged().
  if (new_host != web_contents()->GetPrimaryMainFrame()) {
    return;
  }

  // Ignore the initial main frame host, as there's no renderer frame for it
  // yet. If the DCHECK fires, then we would need to handle the initial main
  // frame when it its renderer frame is created.
  if (!old_host) {
    DCHECK(!new_host->IsRenderFrameLive());
    return;
  }

  SetUpNewMainFrame(new_host);
}

void WebView::DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                            bool will_cause_resize) {
  // Notify a bounds change on fullscreen change even though it actually
  // doesn't change. Cast needs this see https://crbug.com/1144255.
  OnBoundsChanged(bounds());
  NotifyAccessibilityWebContentsChanged();
}

void WebView::OnWebContentsFocused(
    content::RenderWidgetHost* render_widget_host) {
  RequestFocus();
}

void WebView::AXTreeIDForMainFrameHasChanged() {
  NotifyAccessibilityWebContentsChanged();
}

void WebView::WebContentsDestroyed() {
  SetWebContents(nullptr);
}

void WebView::ResizeDueToAutoResize(content::WebContents* source,
                                    const gfx::Size& new_size) {
  if (source != web_contents()) {
    return;
  }

  SetPreferredSize(new_size);
}

////////////////////////////////////////////////////////////////////////////////
// WebView, private:

void WebView::AttachWebContentsNativeView() {
  TRACE_EVENT0("views", "WebView::AttachWebContentsNativeView");
  // Prevents attachment if the WebView isn't already in a Widget, or it's
  // already attached.
  if (!GetWidget() || !web_contents()) {
    return;
  }

  gfx::NativeView view_to_attach = web_contents()->GetNativeView();
  OnBoundsChanged(bounds());
  if (holder_->native_view() == view_to_attach) {
    return;
  }

  const auto* bg_color =
      WebContentsSetBackgroundColor::FromWebContents(web_contents());
  if (bg_color) {
    holder_->SetBackgroundColorWhenClipped(bg_color->color());
  } else {
    holder_->SetBackgroundColorWhenClipped(std::nullopt);
  }

  holder_->Attach(view_to_attach);

  // We set the parent accessible of the native view to be our parent.
  UpdateNativeViewHostAccessibleParent(holder(), parent());

  // The WebContents is not focused automatically when attached, so we need to
  // tell the WebContents it has focus if this has focus.
  if (HasFocus()) {
    OnFocus();
  }

  web_contents_attached_callbacks_.Notify(this);
}

void WebView::DetachWebContentsNativeView() {
  TRACE_EVENT0("views", "WebView::DetachWebContentsNativeView");
  if (web_contents()) {
    holder_->Detach();
  }
}

void WebView::UpdateCrashedOverlayView() {
  View* overlay = crashed_overlay_view_.view();
  if (web_contents() && web_contents()->IsCrashed() && overlay) {
    SetFocusBehavior(FocusBehavior::NEVER);
    overlay->SetVisible(true);
    return;
  }

  SetFocusBehavior(web_contents() ? FocusBehavior::ALWAYS
                                  : FocusBehavior::NEVER);

  if (overlay) {
    overlay->SetVisible(false);
  }
}

void WebView::NotifyAccessibilityWebContentsChanged() {
  if (!lock_child_ax_tree_id_override_) {
    content::RenderFrameHost* rfh =
        web_contents() ? web_contents()->GetPrimaryMainFrame() : nullptr;
    GetViewAccessibility().SetChildTreeID(rfh ? rfh->GetAXTreeID()
                                                   : ui::AXTreeIDUnknown());
  }
  NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged, false);
}

std::unique_ptr<content::WebContents> WebView::CreateWebContents(
    content::BrowserContext* browser_context,
    base::Location creator_location) {
  std::unique_ptr<content::WebContents> contents;
  if (*GetCreatorForTesting()) {
    contents = GetCreatorForTesting()->Run(browser_context);
  }

  if (!contents) {
    content::WebContents::CreateParams create_params(browser_context,
                                                     creator_location);
    return content::WebContents::Create(create_params);
  }

  return contents;
}

void WebView::SetUpNewMainFrame(content::RenderFrameHost* frame_host) {
  MaybeEnableAutoResize(frame_host);
  UpdateCrashedOverlayView();
  NotifyAccessibilityWebContentsChanged();
  if (HasFocus()) {
    OnFocus();
  }
}

void WebView::LostMainFrame() {
  UpdateCrashedOverlayView();
  NotifyAccessibilityWebContentsChanged();
}

void WebView::MaybeEnableAutoResize(content::RenderFrameHost* frame_host) {
  DCHECK(frame_host->IsRenderFrameLive());
  if (!max_size_.IsEmpty()) {
    frame_host->GetView()->EnableAutoResize(min_size_, max_size_);
  }
}

BEGIN_METADATA(WebView)
END_METADATA

}  // namespace views
