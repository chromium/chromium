// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/webview_example.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"

namespace views {
namespace examples {

WebViewExample::WebViewExample(content::BrowserContext* browser_context)
    : ExampleBase("WebView"),
      webview_(nullptr),
      browser_context_(browser_context) {}

WebViewExample::~WebViewExample() = default;

void WebViewExample::CreateExampleView(View* container) {
  webview_ = new WebView(browser_context_);
  webview_->GetWebContents()->SetDelegate(this);
  container->SetLayoutManager(std::make_unique<FillLayout>());
  container->AddChildView(webview_);

  webview_->LoadInitialURL(GURL("http://www.google.com/"));
  webview_->GetWebContents()->Focus();
}

bool WebViewExample::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, webview_->GetFocusManager());
}

}  // namespace examples
}  // namespace views
