// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/examples/browser/ui/web/browser_page_handler.h"

#include "base/notreached.h"

namespace webui_examples {

BrowserPageHandler::BrowserPageHandler(
    mojo::PendingReceiver<webui_examples::mojom::PageHandler> receiver)
    : receiver_(this, std::move(receiver)) {}

BrowserPageHandler::~BrowserPageHandler() = default;

void BrowserPageHandler::Navigate(int32_t view_instance_id, const GURL& src) {
  NOTIMPLEMENTED();
}

void BrowserPageHandler::GoBack(int32_t view_instance_id) {
  NOTIMPLEMENTED();
}

void BrowserPageHandler::GoForward(int32_t view_instance_id) {
  NOTIMPLEMENTED();
}

}  // namespace webui_examples
