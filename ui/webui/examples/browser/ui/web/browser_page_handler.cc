// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/examples/browser/ui/web/browser_page_handler.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/webui/examples/browser/ui/web/browser.h"

namespace webui_examples {

BrowserPageHandler::~BrowserPageHandler() = default;

// static
BrowserPageHandler* BrowserPageHandler::Create(
    Browser* webui_controller,
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<webui_examples::mojom::PageHandler> receiver) {
  // The RenderFrameHost takes ownership of this object via the DocumentService.
  return new BrowserPageHandler(webui_controller, render_frame_host,
                                std::move(receiver));
}

void BrowserPageHandler::Navigate(int32_t guest_instance_id, const GURL& src) {
  content::WebContents* guest_contents = webui_controller()->guest_contents();
  content::NavigationController::LoadURLParams load_url_params(src);
  guest_contents->GetController().LoadURLWithParams(load_url_params);
}

void BrowserPageHandler::GoBack(int32_t guest_instance_id) {
  content::WebContents* guest_contents = webui_controller()->guest_contents();
  auto& navigation_controller = guest_contents->GetController();
  if (navigation_controller.CanGoBack()) {
    navigation_controller.GoBack();
  }
}

void BrowserPageHandler::GoForward(int32_t guest_instance_id) {
  content::WebContents* guest_contents = webui_controller()->guest_contents();
  auto& navigation_controller = guest_contents->GetController();
  if (navigation_controller.CanGoForward()) {
    navigation_controller.GoForward();
  }
}

void BrowserPageHandler::WebUIControllerDestroyed() {
  webui_controller_ = nullptr;
}

BrowserPageHandler::BrowserPageHandler(
    Browser* webui_controller,
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<webui_examples::mojom::PageHandler> receiver)
    : content::DocumentService<webui_examples::mojom::PageHandler>(
          render_frame_host,
          std::move(receiver)),
      webui_controller_(webui_controller) {}

}  // namespace webui_examples
