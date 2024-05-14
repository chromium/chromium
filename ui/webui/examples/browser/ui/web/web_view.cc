// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/examples/browser/ui/web/web_view.h"

#include "base/notreached.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"

namespace webui_examples {

WebView::~WebView() = default;

// static
std::unique_ptr<guest_view::GuestViewBase> WebView::Create(
    content::RenderFrameHost* owner_render_frame_host) {
  return std::make_unique<WebView>(PassKey(), owner_render_frame_host);
}

WebView::WebView(PassKey pass_key,
                 content::RenderFrameHost* owner_render_frame_host)
    : guest_view::GuestView<WebView>(owner_render_frame_host) {}

const char* WebView::GetAPINamespace() const {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

int WebView::GetTaskPrefix() const {
  NOTREACHED_IN_MIGRATION();
  return 0;
}

void WebView::CreateWebContents(std::unique_ptr<GuestViewBase> owned_this,
                                const base::Value::Dict& create_params,
                                WebContentsCreatedCallback callback) {
  content::StoragePartitionConfig partition_config =
      content::StoragePartitionConfig::Create(
          browser_context(), owner_rfh()->GetLastCommittedURL().host(), "",
          true);

  scoped_refptr<content::SiteInstance> guest_site_instance =
      content::SiteInstance::CreateForGuest(browser_context(),
                                            partition_config);
  content::WebContents::CreateParams params(browser_context(),
                                            std::move(guest_site_instance));
  params.guest_delegate = this;
  SetCreateParams(create_params, params);
  std::unique_ptr<content::WebContents> new_contents =
      content::WebContents::Create(params);
  std::move(callback).Run(std::move(owned_this), std::move(new_contents));
}

void WebView::MaybeRecreateGuestContents(
    content::RenderFrameHost* outer_contents_frame) {
  NOTREACHED_IN_MIGRATION();
}

bool WebView::HandleContextMenu(content::RenderFrameHost& render_frame_host,
                                const content::ContextMenuParams& params) {
  return true;
}

}  // namespace webui_examples
