// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views_content_client/views_content_browser_client.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "storage/browser/quota/quota_settings.h"
#include "ui/views_content_client/views_content_client_main_parts.h"

namespace ui {

ViewsContentBrowserClient::ViewsContentBrowserClient(
    ViewsContentClient* views_content_client)
    : views_content_client_(views_content_client) {}

ViewsContentBrowserClient::~ViewsContentBrowserClient() {
}

std::unique_ptr<content::BrowserMainParts>
ViewsContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  return ViewsContentClientMainParts::Create(parameters, views_content_client_);
}

void ViewsContentBrowserClient::GetQuotaSettings(
    content::BrowserContext* context,
    content::StoragePartition* partition,
    storage::OptionalQuotaSettingsCallback callback) {
  storage::GetNominalDynamicSettings(
      partition->GetPath(), context->IsOffTheRecord(),
      storage::GetDefaultDeviceInfoHelper(), std::move(callback));
}

}  // namespace ui
