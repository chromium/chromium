// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/browser/frame_impl.h"

#include <string>

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/wm/core/base_focus_rules.h"
#include "url/gurl.h"
#include "webrunner/browser/context_impl.h"

namespace webrunner {

namespace {

// Layout manager that allows only one child window and stretches it to fill the
// parent.
class LayoutManagerImpl : public aura::LayoutManager {
 public:
  LayoutManagerImpl() = default;
  ~LayoutManagerImpl() override = default;

  // aura::LayoutManager.
  void OnWindowResized() override {
    // Resize the child to match the size of the parent
    if (child_) {
      SetChildBoundsDirect(child_,
                           gfx::Rect(child_->parent()->bounds().size()));
    }
  }
  void OnWindowAddedToLayout(aura::Window* child) override {
    DCHECK(!child_);
    child_ = child;
    SetChildBoundsDirect(child_, gfx::Rect(child_->parent()->bounds().size()));
  }

  void OnWillRemoveWindowFromLayout(aura::Window* child) override {
    DCHECK_EQ(child, child_);
    child_ = nullptr;
  }

  void OnWindowRemovedFromLayout(aura::Window* child) override {}
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override {}

 private:
  aura::Window* child_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LayoutManagerImpl);
};

chromium::web::NavigationEntry ConvertContentNavigationEntry(
    content::NavigationEntry* entry) {
  DCHECK(entry);
  chromium::web::NavigationEntry converted;
  converted.title = base::UTF16ToUTF8(entry->GetTitleForDisplay());
  converted.url = entry->GetURL().spec();
  converted.is_error =
      entry->GetPageType() == content::PageType::PAGE_TYPE_ERROR;
  return converted;
}

// Computes the observable differences between |entry_1| and |entry_2|.
// Returns true if they are different, |false| if their observable fields are
// identical.
bool ComputeNavigationEvent(const chromium::web::NavigationEntry& old_entry,
                            const chromium::web::NavigationEntry& new_entry,
                            chromium::web::NavigationEvent* computed_event) {
  DCHECK(computed_event);

  bool is_changed = false;

  if (old_entry.title != new_entry.title) {
    is_changed = true;
    computed_event->title = new_entry.title;
  }

  if (old_entry.url != new_entry.url) {
    is_changed = true;
    computed_event->url = new_entry.url;
  }

  computed_event->is_error = new_entry.is_error;
  if (old_entry.is_error != new_entry.is_error)
    is_changed = true;

  return is_changed;
}

class FrameFocusRules : public wm::BaseFocusRules {
 public:
  FrameFocusRules() = default;
  ~FrameFocusRules() override = default;

  // wm::BaseFocusRules implementation.
  bool SupportsChildActivation(aura::Window*) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameFocusRules);
};

bool FrameFocusRules::SupportsChildActivation(aura::Window*) const {
  // TODO(crbug.com/878439): Return a result based on window properties such as
  // visibility.
  return true;
}

}  // namespace

FrameImpl::FrameImpl(std::unique_ptr<content::WebContents> web_contents,
                     ContextImpl* context,
                     fidl::InterfaceRequest<chromium::web::Frame> frame_request)
    : web_contents_(std::move(web_contents)),
      focus_controller_(
          std::make_unique<wm::FocusController>(new FrameFocusRules)),
      context_(context),
      binding_(this, std::move(frame_request)) {
  web_contents_->SetDelegate(this);
  binding_.set_error_handler([this]() { context_->DestroyFrame(this); });
}

FrameImpl::~FrameImpl() {
  if (window_tree_host_) {
    aura::client::SetFocusClient(root_window(), nullptr);
    wm::SetActivationClient(root_window(), nullptr);
    web_contents_->ClosePage();
    window_tree_host_->Hide();
    window_tree_host_->compositor()->SetVisible(false);

    // Allows posted focus events to process before the FocusController
    // is torn down.
    content::BrowserThread::DeleteSoon(content::BrowserThread::UI, FROM_HERE,
                                       focus_controller_.release());
  }
}

zx::unowned_channel FrameImpl::GetBindingChannelForTest() const {
  return zx::unowned_channel(binding_.channel());
}

bool FrameImpl::ShouldCreateWebContents(
    content::WebContents* web_contents,
    content::RenderFrameHost* opener,
    content::SiteInstance* source_site_instance,
    int32_t route_id,
    int32_t main_frame_route_id,
    int32_t main_frame_widget_route_id,
    content::mojom::WindowContainerType window_container_type,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url,
    const std::string& partition_id,
    content::SessionStorageNamespace* session_storage_namespace) {
  DCHECK_EQ(web_contents, web_contents_.get());

  // Prevent any child WebContents (popup windows, tabs, etc.) from spawning.
  // TODO(crbug.com/888131): Implement support for popup windows.
  NOTIMPLEMENTED() << "Ignored popup window request for URL: "
                   << target_url.spec();

  return false;
}

void FrameImpl::CreateView(
    fidl::InterfaceRequest<fuchsia::ui::viewsv1token::ViewOwner> view_owner,
    fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> services) {
  ui::PlatformWindowInitProperties properties;
  properties.view_owner_request = std::move(view_owner);

  window_tree_host_ =
      std::make_unique<aura::WindowTreeHostPlatform>(std::move(properties));
  window_tree_host_->InitHost();

  aura::client::SetFocusClient(root_window(), focus_controller_.get());
  wm::SetActivationClient(root_window(), focus_controller_.get());

  // Add hooks which automatically set the focus state when input events are
  // received.
  root_window()->AddPreTargetHandler(focus_controller_.get());

  // Track child windows for enforcement of window management policies and
  // propagate window manager events to them (e.g. window resizing).
  root_window()->SetLayoutManager(new LayoutManagerImpl());

  root_window()->AddChild(web_contents_->GetNativeView());
  web_contents_->GetNativeView()->Show();
  window_tree_host_->Show();
}

void FrameImpl::GetNavigationController(
    fidl::InterfaceRequest<chromium::web::NavigationController> controller) {
  controller_bindings_.AddBinding(this, std::move(controller));
}

void FrameImpl::LoadUrl(fidl::StringPtr url,
                        std::unique_ptr<chromium::web::LoadUrlParams> params) {
  GURL validated_url(*url);
  if (!validated_url.is_valid()) {
    DLOG(WARNING) << "Invalid URL: " << *url;
    return;
  }

  content::NavigationController::LoadURLParams params_converted(validated_url);

  if (validated_url.scheme() == url::kDataScheme)
    params_converted.load_type = content::NavigationController::LOAD_TYPE_DATA;

  params_converted.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  web_contents_->GetController().LoadURLWithParams(params_converted);
}

void FrameImpl::GoBack() {
  if (web_contents_->GetController().CanGoBack())
    web_contents_->GetController().GoBack();
}

void FrameImpl::GoForward() {
  if (web_contents_->GetController().CanGoForward())
    web_contents_->GetController().GoForward();
}

void FrameImpl::Stop() {
  web_contents_->Stop();
}

void FrameImpl::Reload(chromium::web::ReloadType type) {
  content::ReloadType internal_reload_type;
  switch (type) {
    case chromium::web::ReloadType::PARTIAL_CACHE:
      internal_reload_type = content::ReloadType::NORMAL;
      break;
    case chromium::web::ReloadType::NO_CACHE:
      internal_reload_type = content::ReloadType::BYPASSING_CACHE;
      break;
  }
  web_contents_->GetController().Reload(internal_reload_type, false);
}

void FrameImpl::GetVisibleEntry(GetVisibleEntryCallback callback) {
  content::NavigationEntry* entry =
      web_contents_->GetController().GetVisibleEntry();
  if (!entry) {
    callback(nullptr);
    return;
  }

  chromium::web::NavigationEntry output = ConvertContentNavigationEntry(entry);
  callback(std::make_unique<chromium::web::NavigationEntry>(std::move(output)));
}

void FrameImpl::SetNavigationEventObserver(
    fidl::InterfaceHandle<chromium::web::NavigationEventObserver> observer) {
  // Reset the event buffer state.
  waiting_for_navigation_event_ack_ = false;
  cached_navigation_state_ = {};
  pending_navigation_event_ = {};
  pending_navigation_event_is_dirty_ = false;

  if (observer) {
    navigation_observer_.Bind(std::move(observer));
    navigation_observer_.set_error_handler([this]() {
      // Stop observing on Observer connection loss.
      SetNavigationEventObserver(nullptr);
    });
    Observe(web_contents_.get());
  } else {
    navigation_observer_.Unbind();
    Observe(nullptr);  // Stop receiving WebContentsObserver events.
  }
}

void FrameImpl::DidFinishLoad(content::RenderFrameHost* render_frame_host,
                              const GURL& validated_url) {
  if (web_contents_->GetMainFrame() != render_frame_host) {
    return;
  }

  chromium::web::NavigationEntry current_navigation_state =
      ConvertContentNavigationEntry(
          web_contents_->GetController().GetVisibleEntry());
  pending_navigation_event_is_dirty_ |=
      ComputeNavigationEvent(cached_navigation_state_, current_navigation_state,
                             &pending_navigation_event_);
  cached_navigation_state_ = std::move(current_navigation_state);

  if (pending_navigation_event_is_dirty_ &&
      !waiting_for_navigation_event_ack_) {
    MaybeSendNavigationEvent();
  }
}

void FrameImpl::MaybeSendNavigationEvent() {
  if (pending_navigation_event_is_dirty_) {
    pending_navigation_event_is_dirty_ = false;
    waiting_for_navigation_event_ack_ = true;

    // Send the event to the observer and, upon acknowledgement, revisit this
    // function to send another update.
    navigation_observer_->OnNavigationStateChanged(
        std::move(pending_navigation_event_),
        [this]() { MaybeSendNavigationEvent(); });
    return;
  } else {
    // No more changes to report.
    waiting_for_navigation_event_ack_ = false;
  }
}

}  // namespace webrunner
