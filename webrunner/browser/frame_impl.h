// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBRUNNER_BROWSER_FRAME_IMPL_H_
#define WEBRUNNER_BROWSER_FRAME_IMPL_H_

#include <lib/fidl/cpp/binding_set.h>
#include <lib/zx/channel.h>
#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/wm/core/focus_controller.h"
#include "url/gurl.h"
#include "webrunner/fidl/chromium/web/cpp/fidl.h"

namespace aura {
class WindowTreeHost;
}  // namespace aura

namespace content {
class WebContents;
}  // namespace content

namespace webrunner {

class ContextImpl;

// Implementation of Frame from //webrunner/fidl/frame.fidl.
// Implements a Frame service, which is a wrapper for a WebContents instance.
class FrameImpl : public chromium::web::Frame,
                  public chromium::web::NavigationController,
                  public content::WebContentsObserver,
                  public content::WebContentsDelegate {
 public:
  FrameImpl(std::unique_ptr<content::WebContents> web_contents,
            ContextImpl* context,
            fidl::InterfaceRequest<chromium::web::Frame> frame_request);
  ~FrameImpl() override;

  zx::unowned_channel GetBindingChannelForTest() const;

  content::WebContents* web_contents_for_test() { return web_contents_.get(); }

  // chromium::web::Frame implementation.
  void CreateView(
      fidl::InterfaceRequest<fuchsia::ui::viewsv1token::ViewOwner> view_owner,
      fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> services) override;
  void GetNavigationController(
      fidl::InterfaceRequest<chromium::web::NavigationController> controller)
      override;

  // chromium::web::NavigationController implementation.
  void LoadUrl(fidl::StringPtr url,
               std::unique_ptr<chromium::web::LoadUrlParams> params) override;
  void GoBack() override;
  void GoForward() override;
  void Stop() override;
  void Reload(chromium::web::ReloadType type) override;
  void GetVisibleEntry(GetVisibleEntryCallback callback) override;
  void SetNavigationEventObserver(
      fidl::InterfaceHandle<chromium::web::NavigationEventObserver> observer)
      override;

 private:
  FRIEND_TEST_ALL_PREFIXES(FrameImplTest, DelayedNavigationEventAck);
  FRIEND_TEST_ALL_PREFIXES(FrameImplTest, NavigationObserverDisconnected);
  FRIEND_TEST_ALL_PREFIXES(FrameImplTest, NoNavigationObserverAttached);
  FRIEND_TEST_ALL_PREFIXES(FrameImplTest, ReloadFrame);
  FRIEND_TEST_ALL_PREFIXES(FrameImplTest, Stop);

  aura::Window* root_window() const { return window_tree_host_->window(); }

  // Sends |pending_navigation_event_| to the observer if there are any changes
  // to be reported.
  void MaybeSendNavigationEvent();

  // content::WebContentsDelegate implementation.
  bool ShouldCreateWebContents(
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
      content::SessionStorageNamespace* session_storage_namespace) override;

  // content::WebContentsObserver implementation.
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  std::unique_ptr<aura::WindowTreeHost> window_tree_host_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<wm::FocusController> focus_controller_;

  chromium::web::NavigationEventObserverPtr navigation_observer_;
  chromium::web::NavigationEntry cached_navigation_state_;
  chromium::web::NavigationEvent pending_navigation_event_;
  bool waiting_for_navigation_event_ack_;
  bool pending_navigation_event_is_dirty_;

  ContextImpl* context_ = nullptr;
  fidl::Binding<chromium::web::Frame> binding_;
  fidl::BindingSet<chromium::web::NavigationController> controller_bindings_;

  DISALLOW_COPY_AND_ASSIGN(FrameImpl);
};

}  // namespace webrunner

#endif  // WEBRUNNER_BROWSER_FRAME_IMPL_H_
