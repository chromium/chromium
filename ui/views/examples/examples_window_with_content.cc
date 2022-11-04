// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/examples_window_with_content.h"

#include <memory>
#include <utility>
#include <vector>

#include "content/public/browser/browser_context.h"
#include "ui/views/examples/create_examples.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/examples/webview_example.h"

namespace views::examples {

void ShowExamplesWindowWithContent(base::OnceClosure on_close,
                                   content::BrowserContext* browser_context,
                                   gfx::NativeWindow window_context) {
  ExampleVector examples;
  examples.push_back(std::make_unique<WebViewExample>(browser_context));
  ShowExamplesWindow(std::move(on_close), CreateExamples(std::move(examples)),
                     window_context);
}

}  // namespace views::examples
