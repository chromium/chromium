// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/web_contents_view_delegate_impl.h"

#include <memory>

#include "weblayer/browser/tab_impl.h"

namespace weblayer {

WebContentsViewDelegateImpl::WebContentsViewDelegateImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

WebContentsViewDelegateImpl::~WebContentsViewDelegateImpl() = default;

void WebContentsViewDelegateImpl::ShowContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  TabImpl* tab = TabImpl::FromWebContents(web_contents_);
  if (tab)
    tab->ShowContextMenu(params);
}

std::unique_ptr<content::WebContentsViewDelegate> CreateWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return std::make_unique<WebContentsViewDelegateImpl>(web_contents);
}

}  // namespace weblayer
