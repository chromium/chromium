// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_COMMAND_DISPATCHER_H_
#define UI_BASE_COCOA_COMMAND_DISPATCHER_H_

#import <Cocoa/Cocoa.h>

#include "base/component_export.h"

@protocol CommandDispatcherDelegate;
@protocol CommandDispatchingWindow;
@protocol UserInterfaceItemCommandHandler;

// CommandDispatcher guides the processing of key events to ensure key commands
// are executed in the appropriate order. In particular, it allows a first
// responder  to handle an event asynchronously and return unhandled events
// via -redispatchKeyEvent:. An NSWindow can use CommandDispatcher by
// implementing CommandDispatchingWindow and overriding
// -[NSWindow performKeyEquivalent:] and -[NSWindow sendEvent:] to call the
// respective CommandDispatcher methods.
COMPONENT_EXPORT(UI_BASE)
@interface CommandDispatcher : NSObject

@property(weak) id<CommandDispatcherDelegate> delegate;

- (instancetype)initWithOwner:(NSWindow<CommandDispatchingWindow>*)owner;

// The main entry point for key events. The CommandDispatchingWindow should
// override -[NSResponder performKeyEquivalent:] and call this instead. Returns
// YES if the event is handled.
- (BOOL)performKeyEquivalent:(NSEvent*)event;

// Validate a user interface item (e.g. an NSMenuItem), consulting |handler| for
// -commandDispatch: item actions.
- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item
                       forHandler:(id<UserInterfaceItemCommandHandler>)handler;

// Sends a key event to -[NSApp sendEvent:]. This is used to allow default
// AppKit handling of an event that comes back from CommandDispatcherTarget,
// e.g. key equivalents in the menu, or window manager commands like Cmd+`. Once
// the event returns to the window at -preSendEvent:, handling will stop. The
// event must be of type |NSEventTypeKeyDown|, |NSEventTypeKeyUp|, or
// |NSEventTypeFlagsChanged|. Returns YES if the event is handled.
- (BOOL)redispatchKeyEvent:(NSEvent*)event;

// The CommandDispatchingWindow should override -[NSWindow sendEvent:] and call
// this before a native -sendEvent:. Ensures that a redispatched event is not
// reposted infinitely. Returns YES if the event is handled.
- (BOOL)preSendEvent:(NSEvent*)event;

// Dispatch a -commandDispatch: action either to |handler| or a parent window's
// handler.
- (void)dispatch:(id)sender
      forHandler:(id<UserInterfaceItemCommandHandler>)handler;
- (void)dispatchUsingKeyModifiers:(id)sender
                       forHandler:(id<UserInterfaceItemCommandHandler>)handler;

@end

// If the NSWindow's firstResponder implements CommandDispatcherTarget, then
// it is allowed to grant itself exclusive access to certain keyEquivalents,
// preempting the usual consumer order.
@protocol CommandDispatcherTarget

// The System Keyboard Lock API <https://w3c.github.io/keyboard-lock/>
// allows web contents to override keyEquivalents normally reserved by the
// browser. If the firstResponder returns |YES| from this method, then
// keyEquivalents shortcuts should be skipped.
- (BOOL)isKeyLocked:(NSEvent*)event;

@end

namespace ui {

enum class PerformKeyEquivalentResult {
  // The CommandDispatcherDelegate did not handle the key equivalent.
  kUnhandled,

  // The CommandDispatcherDelegate handled the key equivalent.
  kHandled,

  // The CommandDispatcherDelegate did not handle the key equivalent, but wants
  // the event to be passed to the MainMenu, which will handle the key
  // equivalent.
  kPassToMainMenu,

  // The CommandDispatcherDelegate determined the event should not be handled.
  // This can occur when an event has been sent via key repeat that we've
  // determined should not be triggered via repeat.
  kDrop,
};

}  // namespace ui

// Provides CommandDispatcher with the means to redirect key equivalents at
// different stages of event handling.
@protocol CommandDispatcherDelegate

// Gives the delegate a chance to process the keyEquivalent before the first
// responder. See https://crbug.com/846893#c5 for more details on
// keyEquivalent consumer ordering. |window| is the CommandDispatchingWindow
// that owns CommandDispatcher, not the window of the event.
- (ui::PerformKeyEquivalentResult)prePerformKeyEquivalent:(NSEvent*)event
                                                   window:(NSWindow*)window;

// Gives the delegate a chance to process the keyEquivalent after the first
// responder has declined to process the event. See https://crbug.com/846893#c5
// for more details on keyEquivalent consumer ordering.
// |window| is the CommandDispatchingWindow that owns CommandDispatcher, not the
// window of the event.
- (ui::PerformKeyEquivalentResult)postPerformKeyEquivalent:(NSEvent*)event
                                                    window:(NSWindow*)window
                                              isRedispatch:(BOOL)isRedispatch;

@end

// The set of methods an NSWindow subclass needs to implement to use
// CommandDispatcher.
@protocol CommandDispatchingWindow

// Parent window to defer command dispatching to.
- (NSWindow<CommandDispatchingWindow>*)commandDispatchParent;

// If set, NSUserInterfaceItemValidations for -commandDispatch: and
// -commandDispatchUsingKeyModifiers: will be redirected to the command handler.
// Retains |commandHandler|.
-(void)setCommandHandler:(id<UserInterfaceItemCommandHandler>) commandHandler;

// Returns the associated CommandDispatcher.
- (CommandDispatcher*)commandDispatcher;

// Short-circuit to the default -[NSResponder performKeyEquivalent:] which
// CommandDispatcher calls as part of its -performKeyEquivalent: flow.
- (BOOL)defaultPerformKeyEquivalent:(NSEvent*)event;

// Short-circuit to the default -validateUserInterfaceItem: implementation.
- (BOOL)defaultValidateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item;

// AppKit will call -[NSUserInterfaceValidations validateUserInterfaceItem:] to
// validate UI items. Any item whose target is FirstResponder, or nil, will
// traverse the responder chain looking for a responder that implements the
// item's selector. Thus NSWindow is usually the last to be checked and will
// handle any items that are not validated elsewhere in the chain. Implement the
// following so that menu items with these selectors are validated by
// CommandDispatchingWindow.
- (void)commandDispatch:(id)sender;
- (void)commandDispatchUsingKeyModifiers:(id)sender;

@end

#endif  // UI_BASE_COCOA_COMMAND_DISPATCHER_H_
