// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/color_change_listener/color_change_handler.h"

#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/resource_path.h"

namespace ui {

ColorChangeHandler::ColorChangeHandler(
    content::WebContents* web_contents,
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
        pending_page_handler)
    : WebContentsObserver(web_contents),
      page_handler_(this, std::move(pending_page_handler)) {}

ColorChangeHandler::~ColorChangeHandler() = default;

void ColorChangeHandler::SetPage(
    mojo::PendingRemote<color_change_listener::mojom::Page> pending_page) {
  page_.Bind(std::move(pending_page));
}

void ColorChangeHandler::OnColorProviderChanged() {
  if (page_)
    page_->OnColorProviderChanged();
}

}  // namespace ui