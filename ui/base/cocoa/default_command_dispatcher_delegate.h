// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_DEFAULT_COMMAND_DISPATCHER_DELEGATE_H_
#define UI_BASE_COCOA_DEFAULT_COMMAND_DISPATCHER_DELEGATE_H_

#import <Cocoa/Cocoa.h>

#include "base/component_export.h"
#import "ui/base/cocoa/command_dispatcher.h"

// Default CommandDispatcherDelegate for windows that do not install a more
// specific one (e.g. a child widget). Forwards the pre-firstResponder stage to
// the command dispatch parent so reserved accelerators such as Cmd+T still
// preempt this window's firstResponder (e.g. RenderWidgetHostViewCocoa).
//
// postPerformKeyEquivalent: returns kUnhandled so the usual bubble up at the
// end of -performKeyEquivalent: still runs.
COMPONENT_EXPORT(UI_BASE)
@interface DefaultCommandDispatcherDelegate
    : NSObject <CommandDispatcherDelegate>
@end

#endif  // UI_BASE_COCOA_DEFAULT_COMMAND_DISPATCHER_DELEGATE_H_
