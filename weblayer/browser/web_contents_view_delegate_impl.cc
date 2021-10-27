// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/web_contents_view_delegate_impl.h"

#include "weblayer/browser/tab_impl.h"

namespace weblayer {

WebContentsViewDelegateImpl::WebContentsViewDelegateImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

WebContentsViewDelegateImpl::~WebContentsViewDelegateImpl() = default;

void WebContentsViewDelegateImpl::ShowContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  TabImpl* tab = TabImpl::FromWebContents(web_contents_);
  if (tab)
    tab->ShowContextMenu(params);
}

content::WebContentsViewDelegate* CreateWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return new WebContentsViewDelegateImpl(web_contents);
}

}  // namespace weblayer
