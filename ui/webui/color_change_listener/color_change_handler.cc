// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/color_change_listener/color_change_handler.h"

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/message.h"
#include "ui/base/webui/resource_path.h"

namespace ui {

ColorChangeHandler::ColorChangeHandler(
    content::RenderFrameHost* render_frame_host)
    : WebContentsObserver(
          content::WebContents::FromRenderFrameHost(render_frame_host)),
      content::DocumentUserData<ColorChangeHandler>(render_frame_host) {}

ColorChangeHandler::~ColorChangeHandler() = default;

void ColorChangeHandler::Bind(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
        pending_receiver) {
  // Note: This binder is registered for all renderers, as its exposed via
  // RegisterWebUIControllerInterfaceBinder as well as
  // WebUIBrowserInterfaceBrokerRegistry. Once its only exposed via
  // WebUIBrowserInterfaceBrokerRegistry we can remove this check.
  // TODO(crbug.com/452983498): Remove this check once we have migrated all
  // RegisterWebUIControllerInterfaceBinder calls to registry.ForWebUI().Add()
  // calls.
  if (!render_frame_host().GetWebUI()) {
    mojo::ReportBadMessage(
        "Attempted to bind ColorChangeHandler to a non-WebUI frame");
    return;
  }
  receiver_.Bind(std::move(pending_receiver));
}

void ColorChangeHandler::SetPage(
    mojo::PendingRemote<color_change_listener::mojom::Page> pending_page) {
  page_.Bind(std::move(pending_page));
}

void ColorChangeHandler::OnColorProviderChanged() {
  if (page_.is_bound()) {
    page_->OnColorProviderChanged();
  }
}

DOCUMENT_USER_DATA_KEY_IMPL(ColorChangeHandler);

}  // namespace ui
