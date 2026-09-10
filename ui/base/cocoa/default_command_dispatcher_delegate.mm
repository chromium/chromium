// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/default_command_dispatcher_delegate.h"

@implementation DefaultCommandDispatcherDelegate

- (ui::PerformKeyEquivalentResult)prePerformKeyEquivalent:(NSEvent*)event
                                                   window:(NSWindow*)window {
  NSResponder* responder = window.firstResponder;
  if ([responder respondsToSelector:@selector(isKeyLocked:)] &&
      [(id)responder isKeyLocked:event]) {
    return ui::PerformKeyEquivalentResult::kUnhandled;
  }

  if (![window conformsToProtocol:@protocol(CommandDispatchingWindow)]) {
    return ui::PerformKeyEquivalentResult::kUnhandled;
  }

  NSWindow<CommandDispatchingWindow>* dispatching_window =
      static_cast<NSWindow<CommandDispatchingWindow>*>(window);
  NSWindow<CommandDispatchingWindow>* parent =
      dispatching_window.commandDispatchParent;
  return [[parent commandDispatcher].delegate prePerformKeyEquivalent:event
                                                               window:parent];
}

- (ui::PerformKeyEquivalentResult)postPerformKeyEquivalent:(NSEvent*)event
                                                    window:(NSWindow*)window
                                              isRedispatch:(BOOL)isRedispatch {
  return ui::PerformKeyEquivalentResult::kUnhandled;
}

@end
