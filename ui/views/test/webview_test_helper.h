// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_WEBVIEW_TEST_HELPER_H_
#define UI_VIEWS_TEST_WEBVIEW_TEST_HELPER_H_

#include <memory>

namespace content {
class TestContentClientInitializer;
}  // namespace content

namespace views {

class WebViewTestHelper {
 public:
  WebViewTestHelper();

  WebViewTestHelper(const WebViewTestHelper&) = delete;
  WebViewTestHelper& operator=(const WebViewTestHelper&) = delete;

  virtual ~WebViewTestHelper();

 private:
  std::unique_ptr<content::TestContentClientInitializer>
      test_content_client_initializer_;
};

}  // namespace views

#endif  // UI_VIEWS_TEST_WEBVIEW_TEST_HELPER_H_
