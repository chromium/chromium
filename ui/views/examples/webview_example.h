// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_WEBVIEW_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_WEBVIEW_EXAMPLE_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/examples/example_base.h"

namespace content {
class BrowserContext;
}

namespace views {
class WebView;

namespace examples {

class WebViewExample : public ExampleBase, public content::WebContentsDelegate {
 public:
  explicit WebViewExample(content::BrowserContext* browser_context);

  WebViewExample(const WebViewExample&) = delete;
  WebViewExample& operator=(const WebViewExample&) = delete;

  ~WebViewExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

  // content::WebContentsDelegate:
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;

 private:
  raw_ptr<WebView> webview_;
  raw_ptr<content::BrowserContext> browser_context_;
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_WEBVIEW_EXAMPLE_H_
