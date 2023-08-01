// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_EXAMPLES_BROWSER_UI_COCOA_CONTENT_NS_WINDOW_H_
#define UI_WEBUI_EXAMPLES_BROWSER_UI_COCOA_CONTENT_NS_WINDOW_H_

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/functional/callback.h"

@class NSWindowCloseObserver;

namespace content {
class WebContents;
}

namespace webui_examples {

class ContentNSWindow {
 public:
  ContentNSWindow(std::unique_ptr<content::WebContents> web_contents);
  ~ContentNSWindow();

  void SetTitle(const std::u16string title);
  void Show();
  void SetCloseCallback(base::OnceClosure on_close);
  void OnWindowWillClose();

 private:
  std::unique_ptr<content::WebContents> web_contents_;
  NSWindow* __strong window_;
  NSWindowCloseObserver* __strong window_tracker_;
  base::OnceClosure window_will_close_;
};

}  // namespace webui_examples

#endif  // UI_WEBUI_EXAMPLES_BROWSER_UI_COCOA_CONTENT_NS_WINDOW_H_
