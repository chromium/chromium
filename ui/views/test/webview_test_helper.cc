// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/webview_test_helper.h"

#include "content/public/test/test_content_client_initializer.h"
#include "ui/views/controls/webview/webview.h"

namespace views {

WebViewTestHelper::WebViewTestHelper() {
  test_content_client_initializer_ =
      std::make_unique<content::TestContentClientInitializer>();

  // Setup to register a new RenderViewHost factory which manufactures
  // mock render process hosts. This ensures that we never create a 'real'
  // render view host since support for it doesn't exist in unit tests.
  test_content_client_initializer_->CreateTestRenderViewHosts();
}

WebViewTestHelper::~WebViewTestHelper() = default;

}  // namespace views
