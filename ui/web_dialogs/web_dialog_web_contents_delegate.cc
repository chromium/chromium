// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/web_dialogs/web_dialog_web_contents_delegate.h"

#include <utility>

#include "base/check.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"

using content::BrowserContext;
using content::OpenURLParams;
using content::WebContents;

namespace ui {

// Incognito profiles are not long-lived, so we always want to store a
// non-incognito profile.
//
// TODO(akalin): Should we make it so that we have a default incognito
// profile that's long-lived?  Of course, we'd still have to clear it out
// when all incognito browsers close.
WebDialogWebContentsDelegate::WebDialogWebContentsDelegate(
    content::BrowserContext* browser_context,
    std::unique_ptr<WebContentsHandler> handler)
    : browser_context_(browser_context), handler_(std::move(handler)) {
  DCHECK(handler_);
}

WebDialogWebContentsDelegate::~WebDialogWebContentsDelegate() {
}

void WebDialogWebContentsDelegate::Detach() {
  browser_context_ = nullptr;
}

WebContents* WebDialogWebContentsDelegate::OpenURLFromTab(
    WebContents* source,
    const OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  return handler_->OpenURLFromTab(browser_context_, source, params,
                                  std::move(navigation_handle_callback));
}

WebContents* WebDialogWebContentsDelegate::AddNewContents(
    WebContents* source,
    std::unique_ptr<WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  // TODO(erikchen): Refactor AddNewContents to take strong ownership semantics.
  // https://crbug.com/832879.
  handler_->AddNewContents(browser_context_, source, std::move(new_contents),
                           target_url, disposition, window_features,
                           user_gesture);
  return nullptr;
}

bool WebDialogWebContentsDelegate::PreHandleGestureEvent(
    WebContents* source,
    const blink::WebGestureEvent& event) {
  // Disable pinch zooming.
  return blink::WebInputEvent::IsPinchGestureEventType(event.GetType());
}

void WebDialogWebContentsDelegate::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  handler_->RunFileChooser(render_frame_host, listener, params);
}
}  // namespace ui
