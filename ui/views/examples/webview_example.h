// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_WEBVIEW_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_WEBVIEW_EXAMPLE_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/examples/example_base.h"

namespace content {
class BrowserContext;
}

namespace views {
class WebView;

namespace examples {

class WebViewExample : public ExampleBase {
 public:
  explicit WebViewExample(content::BrowserContext* browser_context);

  WebViewExample(const WebViewExample&) = delete;
  WebViewExample& operator=(const WebViewExample&) = delete;

  ~WebViewExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  raw_ptr<WebView> webview_;
  raw_ptr<content::BrowserContext> browser_context_;
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_WEBVIEW_EXAMPLE_H_
