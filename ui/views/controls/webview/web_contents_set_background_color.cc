// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/webview/web_contents_set_background_color.h"

#include "base/memory/ptr_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"

namespace views {

// static
void WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
    content::WebContents* web_contents,
    SkColor color) {
  if (FromWebContents(web_contents))
    return;

  // SupportsUserData::Data takes ownership over the
  // WebContentsSetBackgroundColor instance and will destroy it when the
  // WebContents instance is destroyed.
  web_contents->SetUserData(
      UserDataKey(),
      base::WrapUnique(new WebContentsSetBackgroundColor(web_contents, color)));
}

WebContentsSetBackgroundColor::WebContentsSetBackgroundColor(
    content::WebContents* web_contents,
    SkColor color)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<WebContentsSetBackgroundColor>(
          *web_contents),
      color_(color) {}

WebContentsSetBackgroundColor::~WebContentsSetBackgroundColor() = default;

void WebContentsSetBackgroundColor::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  // We set the background color just on the outermost main frame's widget.
  // Other frames that are local roots would have a widget of their own, but
  // their background colors are part of, and controlled by, the webpage.
  if (!render_frame_host->GetParentOrOuterDocument())
    render_frame_host->GetView()->SetBackgroundColor(color_);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebContentsSetBackgroundColor);

}  // namespace views
