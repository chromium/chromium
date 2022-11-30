// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_USER_INTERFACE_ITEM_COMMAND_HANDLER_H_
#define UI_BASE_COCOA_USER_INTERFACE_ITEM_COMMAND_HANDLER_H_

#import <Cocoa/Cocoa.h>

// Used by CommandDispatchingWindow to implement NSUserInterfaceItemValidations
// for items with -commandDispatch: and -commandDispatchUsingKeyModifiers:.
@protocol UserInterfaceItemCommandHandler<NSObject>

// Called by CommandDispatchingWindow to validate menu and toolbar items. All
// the items we care about have been set with the -commandDispatch or
// -commandDispatchUsingKeyModifiers selectors and a target of FirstResponder in
// IB. If it's not one of those, it should be handled elsewhere in the responder
// chain.
- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item
                           window:(NSWindow*)window;

// Called by CommandDispatchingWindow to execute commands. This assumes that the
// command is supported and doesn't check, otherwise it would have been disabled
// in the UI in validateUserInterfaceItem:.
- (void)commandDispatch:(id)sender window:(NSWindow*)window;

// Same as |-commandDispatch:|, but executes commands using a disposition
// determined by the key flags. If the window is in the background and the
// command key is down, ignore the command key, but process any other modifiers.
- (void)commandDispatchUsingKeyModifiers:(id)sender window:(NSWindow*)window;

@end

#endif  // UI_BASE_COCOA_USER_INTERFACE_ITEM_COMMAND_HANDLER_H_
