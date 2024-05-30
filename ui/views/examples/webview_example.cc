// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/webview_example.h"

#include <memory>

#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/fill_layout.h"

namespace views::examples {

WebViewExample::WebViewExample(content::BrowserContext* browser_context)
    : ExampleBase(l10n_util::GetStringUTF8(IDS_WEBVIEW_SELECT_LABEL).c_str()),
      webview_(nullptr),
      browser_context_(browser_context) {}

WebViewExample::~WebViewExample() = default;

void WebViewExample::CreateExampleView(View* container) {
  webview_ =
      container->AddChildView(std::make_unique<WebView>(browser_context_));
  webview_->GetWebContents()->SetDelegate(this);
  container->SetLayoutManager(std::make_unique<FillLayout>());

  webview_->LoadInitialURL(GURL("http://www.google.com/"));
  webview_->GetWebContents()->Focus();
}

bool WebViewExample::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, webview_->GetFocusManager());
}

}  // namespace views::examples
