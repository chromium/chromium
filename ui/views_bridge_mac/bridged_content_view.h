// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BRIDGE_MAC_BRIDGED_CONTENT_VIEW_H_
#define UI_VIEWS_BRIDGE_MAC_BRIDGED_CONTENT_VIEW_H_

#import <Cocoa/Cocoa.h>

#include "base/strings/string16.h"
#import "ui/base/cocoa/tool_tip_base_view.h"
#import "ui/base/cocoa/tracking_area.h"
#include "ui/views/views_export.h"

namespace ui {
class TextInputClient;
}

namespace views {
class BridgedNativeWidgetImpl;
}

// The NSView that sits as the root contentView of the NSWindow, whilst it has
// a views::RootView present. Bridges requests from Cocoa to the hosted
// views::View.
VIEWS_EXPORT
@interface BridgedContentView : ToolTipBaseView<NSTextInputClient,
                                                NSUserInterfaceValidations,
                                                NSDraggingSource,
                                                NSServicesMenuRequestor> {
 @private
  // Weak, reset by clearView.
  views::BridgedNativeWidgetImpl* bridge_;

  // Weak. If non-null the TextInputClient of the currently focused View in the
  // hierarchy rooted at |hostedView_|. Owned by the focused View.
  // TODO(ccameron): Remove this member.
  ui::TextInputClient* textInputClient_;

  // The TextInputClient about to be set. Requests for a new -inputContext will
  // use this, but while the input is changing, |self| still needs to service
  // IME requests using the old |textInputClient_|.
  // TODO(ccameron): Remove this member.
  ui::TextInputClient* pendingTextInputClient_;

  // A tracking area installed to enable mouseMoved events.
  ui::ScopedCrTrackingArea cursorTrackingArea_;

  // The keyDown event currently being handled, nil otherwise.
  NSEvent* keyDownEvent_;

  // Whether there's an active key down event which is not handled yet.
  BOOL hasUnhandledKeyDownEvent_;

  // Whether any -insertFoo: selector (e.g. -insertNewLine:) was passed to
  // -doCommandBySelector: during the processing of this keyDown. These must
  // always be dispatched as a ui::KeyEvent in -keyDown:.
  BOOL wantsKeyHandledForInsert_;

  // The last tooltip text, used to limit updates.
  base::string16 lastTooltipText_;
}

@property(readonly, nonatomic) views::BridgedNativeWidgetImpl* bridge;
@property(assign, nonatomic) ui::TextInputClient* textInputClient;
@property(assign, nonatomic) BOOL drawMenuBackgroundForBlur;

// Initialize the NSView -> views::View bridge. |viewToHost| must be non-NULL.
- (instancetype)initWithBridge:(views::BridgedNativeWidgetImpl*)bridge
                        bounds:(gfx::Rect)rect;

// Clear the hosted view. For example, if it is about to be destroyed.
- (void)clearView;

// Process a mouse event captured while the widget had global mouse capture.
- (void)processCapturedMouseEvent:(NSEvent*)theEvent;

// Mac's version of views::corewm::TooltipController::UpdateIfRequired().
// Updates the tooltip on the ToolTipBaseView if the text needs to change.
// |locationInContent| is the position from the top left of the window's
// contentRect (also this NSView's frame), as given by a ui::LocatedEvent.
- (void)updateTooltipIfRequiredAt:(const gfx::Point&)locationInContent;

// Notifies the associated FocusManager whether full keyboard access is enabled
// or not.
- (void)updateFullKeyboardAccess;

@end

#endif  // UI_VIEWS_BRIDGE_MAC_BRIDGED_CONTENT_VIEW_H_
