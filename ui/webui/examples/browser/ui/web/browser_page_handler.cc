// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/examples/browser/ui/web/browser_page_handler.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_process_host.h"
#include "ui/webui/examples/browser/ui/web/guest_view_manager_delegate.h"
#include "ui/webui/examples/browser/ui/web/web_view.h"

namespace webui_examples {

BrowserPageHandler::~BrowserPageHandler() = default;

// static
void BrowserPageHandler::CreateForRenderFrameHost(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<webui_examples::mojom::PageHandler> receiver) {
  // The RenderFrameHost takes ownership of this object via the DocumentService.
  new BrowserPageHandler(render_frame_host, std::move(receiver));
}

void BrowserPageHandler::CreateGuestView(base::Value::Dict create_params,
                                         CreateGuestViewCallback callback) {
  auto* browser_context = render_frame_host().GetBrowserContext();
  guest_view::GuestViewManager* guest_view_manager =
      guest_view::GuestViewManager::FromBrowserContext(browser_context);
  if (!guest_view_manager) {
    guest_view_manager = guest_view::GuestViewManager::CreateWithDelegate(
        browser_context, std::make_unique<GuestViewManagerDelegate>());
  }

  auto create_guest_callback = base::BindOnce(
      [](CreateGuestViewCallback on_success, guest_view::GuestViewBase* guest) {
        std::move(on_success).Run(guest->guest_instance_id());
      },
      std::move(callback));

  guest_view_manager->CreateGuest(WebView::Type, &render_frame_host(),
                                  create_params,
                                  std::move(create_guest_callback));
}

void BrowserPageHandler::Navigate(int32_t guest_instance_id, const GURL& src) {
  auto* guest_view = WebView::FromInstanceID(
      render_frame_host().GetProcess()->GetID(), guest_instance_id);
  content::NavigationController::LoadURLParams load_url_params(src);
  guest_view->GetController().LoadURLWithParams(load_url_params);
}

void BrowserPageHandler::GoBack(int32_t guest_instance_id) {
  auto* guest_view = WebView::FromInstanceID(
      render_frame_host().GetProcess()->GetID(), guest_instance_id);
  auto& navigation_controller = guest_view->GetController();
  if (navigation_controller.CanGoBack()) {
    navigation_controller.GoBack();
  }
}

void BrowserPageHandler::GoForward(int32_t guest_instance_id) {
  auto* guest_view = WebView::FromInstanceID(
      render_frame_host().GetProcess()->GetID(), guest_instance_id);
  auto& navigation_controller = guest_view->GetController();
  if (navigation_controller.CanGoForward()) {
    navigation_controller.GoForward();
  }
}

BrowserPageHandler::BrowserPageHandler(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<webui_examples::mojom::PageHandler> receiver)
    : content::DocumentService<webui_examples::mojom::PageHandler>(
          render_frame_host,
          std::move(receiver)) {}

}  // namespace webui_examples
