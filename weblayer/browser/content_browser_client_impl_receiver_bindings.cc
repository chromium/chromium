// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file exposes services from the browser to child processes.

#include "weblayer/browser/content_browser_client_impl.h"

#include "weblayer/browser/content_settings_manager_delegate.h"

namespace weblayer {

void ContentBrowserClientImpl::BindHostReceiverForRenderer(
    content::RenderProcessHost* render_process_host,
    mojo::GenericPendingReceiver receiver) {
  if (auto host_receiver =
          receiver.As<content_settings::mojom::ContentSettingsManager>()) {
    content_settings::ContentSettingsManagerImpl::Create(
        render_process_host, std::move(host_receiver),
        std::make_unique<ContentSettingsManagerDelegate>());
    return;
  }
}

}  // namespace weblayer
