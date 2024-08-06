// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/command_dispatcher.h"

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#import "ui/base/cocoa/user_interface_item_command_handler.h"

// Expose -[NSWindow hasKeyAppearance], which determines whether the traffic
// lights on the window are "lit". CommandDispatcher uses this property on a
// parent window to decide whether keys and commands should bubble up.
@interface NSWindow (PrivateAPI)
- (BOOL)hasKeyAppearance;
@end

namespace {

// Duplicate the given key event, but changing the associated window.
NSEvent* KeyEventForWindow(NSWindow* window, NSEvent* event) {
  NSEventType event_type = event.type;

  // Convert the event's location from the original window's coordinates into
  // our own.
  NSPoint location = event.locationInWindow;
  location = [event.window convertPointToScreen:location];
  location = [window convertPointFromScreen:location];

  // Various things *only* apply to key down/up.
  bool is_a_repeat = false;
  NSString* characters = nil;
  NSString* characters_ignoring_modifiers = nil;
  if (event_type == NSEventTypeKeyDown || event_type == NSEventTypeKeyUp) {
    is_a_repeat = event.ARepeat;
    characters = event.characters;
    characters_ignoring_modifiers = event.charactersIgnoringModifiers;
  }

  return [NSEvent keyEventWithType:event_type
                          location:location
                     modifierFlags:event.modifierFlags
                         timestamp:event.timestamp
                      windowNumber:window.windowNumber
                           context:nil
                        characters:characters
       charactersIgnoringModifiers:characters_ignoring_modifiers
                         isARepeat:is_a_repeat
                           keyCode:event.keyCode];
}

}  // namespace

@implementation CommandDispatcher {
  BOOL _eventHandled;
  BOOL _isRedispatchingKeyEvent;

  NSWindow<CommandDispatchingWindow>* __weak _owner;  // Weak, owns us.
}

@synthesize delegate = _delegate;

- (instancetype)initWithOwner:(NSWindow<CommandDispatchingWindow>*)owner {
  if ((self = [super init])) {
    _owner = owner;
  }
  return self;
}

// When an event is being redispatched, its window is rewritten to be the owner_
// of the CommandDispatcher. However, AppKit may still choose to send the event
// to the key window. To check of an event is being redispatched, we check the
// event's window.
- (BOOL)isEventBeingRedispatched:(NSEvent*)event {
  if ([event.window conformsToProtocol:@protocol(CommandDispatchingWindow)]) {
    NSObject<CommandDispatchingWindow>* window =
        static_cast<NSObject<CommandDispatchingWindow>*>(event.window);
    return [window commandDispatcher]->_isRedispatchingKeyEvent;
  }
  return NO;
}

// |_delegate| may be nil in this method. Rather than adding nil checks to every
// call, we rely on the fact that method calls to nil return nil, and that nil
// == ui::PerformKeyEquivalentResult::kUnhandled;
- (BOOL)performKeyEquivalent:(NSEvent*)event {
  // TODO(bokan): Tracing added temporarily to diagnose crbug.com/1039833.
  TRACE_EVENT2("ui", "CommandDispatcher::performKeyEquivalent", "window num",
               _owner.windowNumber, "is keyWin", NSApp.keyWindow == _owner);
  DCHECK_EQ(NSEventTypeKeyDown, event.type);

  // If the event is being redispatched, then this is the second time
  // performKeyEquivalent: is being called on the event. The first time, a
  // WebContents was firstResponder and claimed to have handled the event [but
  // instead sent the event asynchronously to the renderer process]. The
  // renderer process chose not to handle the event, and the consumer
  // redispatched the event by calling -[CommandDispatchingWindow
  // redispatchKeyEvent:].
  //
  // We skip all steps before postPerformKeyEquivalent, since those were already
  // triggered on the first pass of the event.
  if ([self isEventBeingRedispatched:event]) {
    // TODO(bokan): Tracing added temporarily to diagnose crbug.com/1039833.
    TRACE_EVENT_INSTANT0("ui", "IsRedispatch", TRACE_EVENT_SCOPE_THREAD);
    ui::PerformKeyEquivalentResult result =
        [_delegate postPerformKeyEquivalent:event
                                     window:_owner
                               isRedispatch:YES];
    TRACE_EVENT_INSTANT1("ui", "postPerformKeyEquivalent",
                         TRACE_EVENT_SCOPE_THREAD, "result", result);
    if (result == ui::PerformKeyEquivalentResult::kHandled)
      return YES;
    if (result == ui::PerformKeyEquivalentResult::kPassToMainMenu)
      return NO;
    return [_owner.commandDispatchParent performKeyEquivalent:event];
  }

  // First, give the delegate an opportunity to consume this event.
  ui::PerformKeyEquivalentResult result =
      [_delegate prePerformKeyEquivalent:event window:_owner];
  if (result == ui::PerformKeyEquivalentResult::kHandled ||
      result == ui::PerformKeyEquivalentResult::kDrop)
    return YES;
  if (result == ui::PerformKeyEquivalentResult::kPassToMainMenu)
    return NO;

  // Next, pass the event down the NSView hierarchy. Surprisingly, this doesn't
  // use the responder chain. See implementation of -[NSWindow
  // performKeyEquivalent:]. If the view hierarchy contains a
  // RenderWidgetHostViewCocoa, it may choose to return true, and to
  // asynchronously pass the event to the renderer. See
  // -[RenderWidgetHostViewCocoa performKeyEquivalent:].
  if ([_owner defaultPerformKeyEquivalent:event])
    return YES;

  // If the firstResponder [e.g. omnibox] chose not to handle the keyEquivalent,
  // then give the delegate another chance to consume it.
  result =
      [_delegate postPerformKeyEquivalent:event window:_owner isRedispatch:NO];
  if (result == ui::PerformKeyEquivalentResult::kHandled)
    return YES;
  if (result == ui::PerformKeyEquivalentResult::kPassToMainMenu)
    return NO;

  // Allow commands to "bubble up" to CommandDispatchers in parent windows, if
  // they were not handled here.
  return [_owner.commandDispatchParent performKeyEquivalent:event];
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item
                       forHandler:(id<UserInterfaceItemCommandHandler>)handler {
  // Since this class implements these selectors, |super| will always say they
  // are enabled. Only use [super] to validate other selectors. If there is no
  // command handler, defer to AppController.
  if (item.action == @selector(commandDispatch:) ||
      item.action == @selector(commandDispatchUsingKeyModifiers:)) {
    if (handler) {
      // -dispatch:.. can't later decide to bubble events because
      // -commandDispatch:.. is assumed to always succeed. So, if there is a
      // |handler|, only validate against that for -commandDispatch:.
      return [handler validateUserInterfaceItem:item window:_owner];
    }

    id appController = [NSApp delegate];
    DCHECK([appController
        respondsToSelector:@selector(validateUserInterfaceItem:)]);
    if ([appController validateUserInterfaceItem:item])
      return YES;
  }

  // -defaultValidateUserInterfaceItem: in most cases will return YES. If
  // there is a command dispatch parent give it a chance to validate the item.
  if (!_owner.commandDispatchParent) {
    // Note this may validate an action bubbled up from a child window. However,
    // if the child window also -respondsToSelector: (but validated it `NO`),
    // the action will be dispatched to the child only, which may NSBeep().
    // TODO(tapted): Fix this. E.g. bubble up validation via the
    // commandDispatchParent's CommandDispatcher rather than the
    // NSUserInterfaceValidations protocol, so that this step can be skipped.
    return [_owner defaultValidateUserInterfaceItem:item];
  }

  return [_owner.commandDispatchParent validateUserInterfaceItem:item];
}

- (BOOL)redispatchKeyEvent:(NSEvent*)event {
  // TODO(bokan): Tracing added temporarily to diagnose crbug.com/1039833.
  TRACE_EVENT2("ui", "CommandDispatcher::redispatchKeyEvent", "window num",
               _owner.windowNumber, "event window num",
               event.window.windowNumber);
  DCHECK(!_isRedispatchingKeyEvent);
  base::AutoReset<BOOL> resetter(&_isRedispatchingKeyEvent, YES);

  DCHECK(event);
  NSEventType eventType = event.type;
  CHECK(eventType == NSEventTypeKeyDown || eventType == NSEventTypeKeyUp ||
        eventType == NSEventTypeFlagsChanged);

  // Sometimes, an event will be redispatched from a child window to a parent
  // window to allow the parent window a chance to handle it. In that case, fix
  // up the native event to reference the correct window. Failure to do this can
  // cause infinite redispatch loops; see https://crbug.com/1085578 for more
  // details.
  if (event.window != _owner) {
    event = KeyEventForWindow(_owner, event);
  }

  // Redispatch the event.
  _eventHandled = YES;
  [NSApp sendEvent:event];

  // If the event was not handled by [NSApp sendEvent:], the preSendEvent:
  // method below will be called, and because the event is being redispatched,
  // |eventHandled_| will be set to NO.
  return _eventHandled;
}

- (BOOL)preSendEvent:(NSEvent*)event {
  // TODO(bokan): Tracing added temporarily to diagnose crbug.com/1039833.
  TRACE_EVENT2("ui", "CommandDispatcher::preSendEvent", "window num",
               _owner.windowNumber, "event window num",
               event.window.windowNumber);

  // AppKit does not call performKeyEquivalent: if the event only has the
  // NSEventModifierFlagOption modifier. However, Chrome wants to treat these
  // events just like keyEquivalents, since they can be consumed by extensions.
  if (event.type == NSEventTypeKeyDown &&
      (event.modifierFlags & NSEventModifierFlagOption)) {
    BOOL handled = [self performKeyEquivalent:event];
    if (handled)
      return YES;
  }

  if ([self isEventBeingRedispatched:event]) {
    // If we get here, then the event was not handled by NSApplication.
    _eventHandled = NO;
    // Return YES to stop native -sendEvent handling.
    return YES;
  }

  return NO;
}

- (void)dispatch:(id)sender
      forHandler:(id<UserInterfaceItemCommandHandler>)handler {
  if (handler)
    [handler commandDispatch:sender window:_owner];
  else
    [_owner.commandDispatchParent commandDispatch:sender];
}

- (void)dispatchUsingKeyModifiers:(id)sender
                       forHandler:(id<UserInterfaceItemCommandHandler>)handler {
  if (handler)
    [handler commandDispatchUsingKeyModifiers:sender window:_owner];
  else
    [_owner.commandDispatchParent commandDispatchUsingKeyModifiers:sender];
}

@end
