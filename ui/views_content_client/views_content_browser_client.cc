// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views_content_client/views_content_browser_client.h"

#include <utility>

#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "ui/views_content_client/views_content_client_main_parts.h"

namespace ui {

ViewsContentBrowserClient::ViewsContentBrowserClient(
    ViewsContentClient* views_content_client)
    : views_content_client_(views_content_client) {}

ViewsContentBrowserClient::~ViewsContentBrowserClient() {
}

std::unique_ptr<content::BrowserMainParts>
ViewsContentBrowserClient::CreateBrowserMainParts(
    bool /* is_integration_test */) {
  return ViewsContentClientMainParts::Create(views_content_client_);
}

}  // namespace ui
