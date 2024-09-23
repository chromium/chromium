// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/web_dialogs/test/test_web_contents_handler.h"

#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
namespace ui {
namespace test {

TestWebContentsHandler::TestWebContentsHandler() {
}

TestWebContentsHandler::~TestWebContentsHandler() {
}

content::WebContents* TestWebContentsHandler::OpenURLFromTab(
    content::BrowserContext* context,
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  return nullptr;
}

void TestWebContentsHandler::AddNewContents(
    content::BrowserContext* context,
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture) {}

void TestWebContentsHandler::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {}

}  // namespace test
}  // namespace ui
