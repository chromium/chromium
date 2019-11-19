// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_WEBVIEW_TEST_HELPER_H_
#define UI_VIEWS_TEST_WEBVIEW_TEST_HELPER_H_

#include <memory>

#include "base/macros.h"

namespace content {
class TestContentClientInitializer;
}  // namespace content

namespace views {

class WebViewTestHelper {
 public:
  WebViewTestHelper();
  virtual ~WebViewTestHelper();

 private:
  std::unique_ptr<content::TestContentClientInitializer>
      test_content_client_initializer_;

  DISALLOW_COPY_AND_ASSIGN(WebViewTestHelper);
};

}  // namespace views

#endif  // UI_VIEWS_TEST_WEBVIEW_TEST_HELPER_H_
