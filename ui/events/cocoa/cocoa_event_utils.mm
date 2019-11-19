// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/events/cocoa/cocoa_event_utils.h"

#include "base/mac/scoped_cftyperef.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"

namespace {

bool IsLeftButtonEvent(NSEvent* event) {
  NSEventType type = [event type];
  return type == NSLeftMouseDown || type == NSLeftMouseDragged ||
         type == NSLeftMouseUp;
}

bool IsRightButtonEvent(NSEvent* event) {
  NSEventType type = [event type];
  return type == NSRightMouseDown || type == NSRightMouseDragged ||
         type == NSRightMouseUp;
}

bool IsMiddleButtonEvent(NSEvent* event) {
  if ([event buttonNumber] != 2)
    return false;

  NSEventType type = [event type];
  return type == NSOtherMouseDown || type == NSOtherMouseDragged ||
         type == NSOtherMouseUp;
}

// Return true if the target modifier key is up. OS X has an "official" flag
// to test whether either left or right versions of a modifier key are held,
// and "unofficial" flags for the left and right versions independently. This
// function verifies that |target_key_mask| and |otherKeyMask| (which should be
// the left and right versions of a modifier) are consistent with with the
// state of |eitherKeyMask| (which should be the corresponding ""official"
// flag). If they are consistent, it tests |target_key_mask|; otherwise it tests
// |either_key_mask|.
inline bool IsModifierKeyUp(unsigned int flags,
                            unsigned int target_key_mask,
                            unsigned int other_key_mask,
                            unsigned int either_key_mask) {
  bool either_key_down = (flags & either_key_mask) != 0;
  bool target_key_down = (flags & target_key_mask) != 0;
  bool other_key_down = (flags & other_key_mask) != 0;
  if (either_key_down != (target_key_down || other_key_down))
    return !either_key_down;
  return !target_key_down;
}

}  // namespace

namespace ui {

int EventFlagsFromModifiers(NSUInteger modifiers) {
  int flags = 0;
  flags |= (modifiers & NSAlphaShiftKeyMask) ? ui::EF_CAPS_LOCK_ON : 0;
  flags |= (modifiers & NSShiftKeyMask) ? ui::EF_SHIFT_DOWN : 0;
  flags |= (modifiers & NSControlKeyMask) ? ui::EF_CONTROL_DOWN : 0;
  flags |= (modifiers & NSAlternateKeyMask) ? ui::EF_ALT_DOWN : 0;
  flags |= (modifiers & NSCommandKeyMask) ? ui::EF_COMMAND_DOWN : 0;
  return flags;
}

int EventFlagsFromNSEventWithModifiers(NSEvent* event, NSUInteger modifiers) {
  int flags = EventFlagsFromModifiers(modifiers);
  if (IsLeftButtonEvent(event)) {
    // For Mac, convert Ctrl+LeftClick to a RightClick, and remove the Control
    // key modifier.
    if (modifiers & NSControlKeyMask)
      flags = (flags & ~ui::EF_CONTROL_DOWN) | ui::EF_RIGHT_MOUSE_BUTTON;
    else
      flags |= ui::EF_LEFT_MOUSE_BUTTON;
  }

  flags |= IsRightButtonEvent(event) ? ui::EF_RIGHT_MOUSE_BUTTON : 0;
  flags |= IsMiddleButtonEvent(event) ? ui::EF_MIDDLE_MOUSE_BUTTON : 0;
  return flags;
}

bool IsKeyUpEvent(NSEvent* event) {
  if ([event type] != NSFlagsChanged)
    return [event type] == NSKeyUp;

  // Unofficial bit-masks for left- and right-hand versions of modifier keys.
  // These values were determined empirically.
  const unsigned int kLeftControlKeyMask = 1 << 0;
  const unsigned int kLeftShiftKeyMask = 1 << 1;
  const unsigned int kRightShiftKeyMask = 1 << 2;
  const unsigned int kLeftCommandKeyMask = 1 << 3;
  const unsigned int kRightCommandKeyMask = 1 << 4;
  const unsigned int kLeftAlternateKeyMask = 1 << 5;
  const unsigned int kRightAlternateKeyMask = 1 << 6;
  const unsigned int kRightControlKeyMask = 1 << 13;

  switch ([event keyCode]) {
    case 54:  // Right Command
      return IsModifierKeyUp([event modifierFlags], kRightCommandKeyMask,
                             kLeftCommandKeyMask, NSCommandKeyMask);
    case 55:  // Left Command
      return IsModifierKeyUp([event modifierFlags], kLeftCommandKeyMask,
                             kRightCommandKeyMask, NSCommandKeyMask);

    case 57:  // Capslock
      return ([event modifierFlags] & NSAlphaShiftKeyMask) == 0;

    case 56:  // Left Shift
      return IsModifierKeyUp([event modifierFlags], kLeftShiftKeyMask,
                             kRightShiftKeyMask, NSShiftKeyMask);
    case 60:  // Right Shift
      return IsModifierKeyUp([event modifierFlags], kRightShiftKeyMask,
                             kLeftShiftKeyMask, NSShiftKeyMask);

    case 58:  // Left Alt
      return IsModifierKeyUp([event modifierFlags], kLeftAlternateKeyMask,
                             kRightAlternateKeyMask, NSAlternateKeyMask);
    case 61:  // Right Alt
      return IsModifierKeyUp([event modifierFlags], kRightAlternateKeyMask,
                             kLeftAlternateKeyMask, NSAlternateKeyMask);

    case 59:  // Left Ctrl
      return IsModifierKeyUp([event modifierFlags], kLeftControlKeyMask,
                             kRightControlKeyMask, NSControlKeyMask);
    case 62:  // Right Ctrl
      return IsModifierKeyUp([event modifierFlags], kRightControlKeyMask,
                             kLeftControlKeyMask, NSControlKeyMask);

    case 63:  // Function
      return ([event modifierFlags] & NSFunctionKeyMask) == 0;
  }
  return false;
}

std::vector<uint8_t> EventToData(NSEvent* event) {
  base::ScopedCFTypeRef<CFDataRef> cf_data(
      CGEventCreateData(nullptr, [event CGEvent]));
  const uint8_t* cf_data_ptr = CFDataGetBytePtr(cf_data.get());
  size_t cf_data_size = CFDataGetLength(cf_data.get());
  return std::vector<uint8_t>(cf_data_ptr, cf_data_ptr + cf_data_size);
}

NSEvent* EventFromData(const std::vector<uint8_t>& data) {
  base::ScopedCFTypeRef<CFDataRef> cf_data(
      CFDataCreate(nullptr, data.data(), data.size()));
  base::ScopedCFTypeRef<CGEventRef> cg_event(
      CGEventCreateFromData(nullptr, cf_data.get()));
  return [NSEvent eventWithCGEvent:cg_event.get()];
}

}  // namespace ui
