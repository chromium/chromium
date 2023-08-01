// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/webui/examples/browser/ui/cocoa/content_ns_window.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/cocoa/window_size_constants.h"

@interface NSWindowCloseObserver : NSObject
- (instancetype)initWithNSWindow:(NSWindow*)window
                 contentNSWindow:
                     (webui_examples::ContentNSWindow*)content_ns_window;
- (void)onWillClose;
@end

@implementation NSWindowCloseObserver

NSWindow* __strong _window;
raw_ptr<webui_examples::ContentNSWindow> _content_ns_window;

- (instancetype)initWithNSWindow:(NSWindow*)window
                 contentNSWindow:
                     (webui_examples::ContentNSWindow*)content_ns_window {
  _window = window;
  _content_ns_window = content_ns_window;
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(onWillClose)
             name:NSWindowWillCloseNotification
           object:_window];
  return self;
}

- (void)onWillClose {
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:NSWindowWillCloseNotification
              object:_window];
  _content_ns_window->OnWindowWillClose();
}

@end

namespace webui_examples {

ContentNSWindow::ContentNSWindow(
    std::unique_ptr<content::WebContents> web_contents)
    : web_contents_(std::move(web_contents)) {
  window_ = [[NSWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                          NSWindowStyleMaskMiniaturizable |
                          NSWindowStyleMaskResizable
                  backing:NSBackingStoreBuffered
                    defer:NO];
  // Required for ARC to avoid a double-release later during NSWindow closure.
  window_.releasedWhenClosed = NO;
  window_tracker_ = [[NSWindowCloseObserver alloc] initWithNSWindow:window_
                                                    contentNSWindow:this];
  [window_ setContentView:web_contents_->GetNativeView().GetNativeNSView()];
  NSRect frame = [window_ frame];
  frame.size = NSMakeSize(800, 600);
  [window_ setFrame:frame display:YES animate:NO];
}

ContentNSWindow::~ContentNSWindow() = default;

void ContentNSWindow::SetTitle(const std::u16string title) {
  window_.title = base::SysUTF16ToNSString(title);
}

void ContentNSWindow::Show() {
  [window_ makeKeyAndOrderFront:nil];
}

void ContentNSWindow::SetCloseCallback(base::OnceClosure on_close) {
  window_will_close_ = std::move(on_close);
}

void ContentNSWindow::OnWindowWillClose() {
  std::move(window_will_close_).Run();
}

}  // namespace webui_examples
