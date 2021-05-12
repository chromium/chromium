// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_WEBVIEW_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_WEBVIEW_EXAMPLE_H_

#include "base/macros.h"
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
  ~WebViewExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

  // content::WebContentsDelegate:
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;

 private:
  WebView* webview_;
  content::BrowserContext* browser_context_;
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  DISALLOW_COPY_AND_ASSIGN(WebViewExample);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_WEBVIEW_EXAMPLE_H_
