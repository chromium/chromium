// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/nsmenuitem_additions.h"
#include "base/apple/foundation_util.h"

#include <Carbon/Carbon.h>

#include "base/apple/bridging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/check.h"
#include "ui/events/keycodes/keyboard_code_conversion_mac.h"

namespace ui::cocoa {

namespace {
bool g_is_input_source_command_qwerty = false;
bool g_is_input_source_dvorak_right_or_left = false;
bool g_is_input_source_command_hebrew = false;
bool g_is_input_source_command_arabic = false;
}  // namespace

void SetIsInputSourceCommandQwertyForTesting(bool is_command_qwerty) {
  g_is_input_source_command_qwerty = is_command_qwerty;
}

void SetIsInputSourceDvorakRightOrLeftForTesting(bool is_dvorak_right_or_left) {
  g_is_input_source_dvorak_right_or_left = is_dvorak_right_or_left;
}

void SetIsInputSourceCommandHebrewForTesting(bool is_command_hebrew) {
  g_is_input_source_command_hebrew = is_command_hebrew;
}

void SetIsInputSourceCommandArabicForTesting(bool is_command_arabic) {
  g_is_input_source_command_arabic = is_command_arabic;
}

bool IsKeyboardLayoutCommandQwerty(NSString* layout_id) {
  return [layout_id isEqualToString:@"com.apple.keylayout.DVORAK-QWERTYCMD"] ||
         [layout_id isEqualToString:@"com.apple.keylayout.Dhivehi-QWERTY"] ||
         [layout_id isEqualToString:@"com.apple.keylayout.Inuktitut-QWERTY"] ||
         [layout_id isEqualToString:@"com.apple.keylayout.Cherokee-QWERTY"];
}

bool IsKeyboardLayoutDvorakRightOrLeft(NSString* layout_id) {
  return [layout_id isEqualToString:@"com.apple.keylayout.Dvorak-Right"] ||
         [layout_id isEqualToString:@"com.apple.keylayout.Dvorak-Left"];
}

bool IsKeyboardLayoutCommandHebrew(NSString* layout_id) {
  // com.apple.keylayout.Hebrew, com.apple.keylayout.Hebrew-PC,
  // com.apple.keylayout.Hebrew-QWERTY.
  return [layout_id hasPrefix:@"com.apple.keylayout.Hebrew"];
}

bool IsKeyboardLayoutCommandArabic(NSString* layout_id) {
  return [layout_id hasPrefix:@"com.apple.keylayout.ArabicPC"] ||
         [layout_id hasPrefix:@"com.apple.keylayout.Arabic-AZERTY"];
}

NSUInteger ModifierMaskForKeyEvent(NSEvent* event) {
  NSUInteger eventModifierMask =
      NSEventModifierFlagCommand | NSEventModifierFlagControl |
      NSEventModifierFlagOption | NSEventModifierFlagShift;

  // If `event` isn't a function key press or it's not a character key press
  // (e.g. it's a flags change), we can simply return the mask.
  if ((event.modifierFlags & NSEventModifierFlagFunction) == 0 ||
      event.type != NSEventTypeKeyDown) {
    return eventModifierMask;
  }

  NSString* eventString = event.charactersIgnoringModifiers;
  if (eventString.length == 0) {
    return eventModifierMask;
  }

  // "Up arrow", home, and other "function" key events include
  // NSEventModifierFlagFunction in their flags even though the user isn't
  // holding down the keyboard's function / world key. Add
  // NSEventModifierFlagFunction to the returned modifier mask only if the
  // event isn't for a function key.
  unichar firstCharacter = [eventString characterAtIndex:0];
  if (firstCharacter < NSUpArrowFunctionKey ||
      firstCharacter > NSModeSwitchFunctionKey)
    eventModifierMask |= NSEventModifierFlagFunction;

  return eventModifierMask;
}

}  // namespace ui::cocoa

@interface KeyboardInputSourceListener : NSObject
@end

@implementation KeyboardInputSourceListener

- (instancetype)init {
  if (self = [super init]) {
    [NSNotificationCenter.defaultCenter
        addObserver:self
           selector:@selector(inputSourceDidChange:)
               name:NSTextInputContextKeyboardSelectionDidChangeNotification
             object:nil];
    [self updateInputSource];
  }
  return self;
}

- (void)dealloc {
  [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)updateInputSource {
  base::apple::ScopedCFTypeRef<TISInputSourceRef> inputSource(
      TISCopyCurrentKeyboardInputSource());
  NSString* layoutId = base::apple::CFToNSPtrCast(
      base::apple::CFCast<CFStringRef>(TISGetInputSourceProperty(
          inputSource.get(), kTISPropertyInputSourceID)));
  ui::cocoa::g_is_input_source_command_qwerty =
      ui::cocoa::IsKeyboardLayoutCommandQwerty(layoutId);
  ui::cocoa::g_is_input_source_dvorak_right_or_left =
      ui::cocoa::IsKeyboardLayoutDvorakRightOrLeft(layoutId);
  ui::cocoa::g_is_input_source_command_hebrew =
      ui::cocoa::IsKeyboardLayoutCommandHebrew(layoutId);
  ui::cocoa::g_is_input_source_command_arabic =
      ui::cocoa::IsKeyboardLayoutCommandArabic(layoutId);
}

- (void)inputSourceDidChange:(NSNotification*)notification {
  [self updateInputSource];
}

@end

@implementation NSMenuItem (ChromeAdditions)

- (BOOL)cr_firesForKeyEquivalentEvent:(NSEvent*)event {
  if (![self isEnabled])
    return NO;

  DCHECK(event.type == NSEventTypeKeyDown);
  // In System Preferences->Keyboard->Keyboard Shortcuts, it is possible to add
  // arbitrary keyboard shortcuts to applications. It is not documented how this
  // works in detail, but |NSMenuItem| has a method |userKeyEquivalent| that
  // sounds related.
  // However, it looks like |userKeyEquivalent| is equal to |keyEquivalent| when
  // a user shortcut is set in system preferences, i.e. Cocoa automatically
  // sets/overwrites |keyEquivalent| as well. Hence, this method can ignore
  // |userKeyEquivalent| and check |keyEquivalent| only.

  // Menu item key equivalents are nearly all stored without modifiers. The
  // exception is shift, which is included in the key and not in the modifiers
  // for printable characters (but not for stuff like arrow keys etc).
  NSString* eventString = event.charactersIgnoringModifiers;
  NSUInteger eventModifiers =
      event.modifierFlags & NSEventModifierFlagDeviceIndependentFlagsMask;

  // cmd-opt-a gives some weird char as characters and "a" as
  // charactersWithoutModifiers with an US layout, but an "a" as characters and
  // a weird char as "charactersWithoutModifiers" with a cyrillic layout. Oh,
  // Cocoa! Instead of getting the current layout from Text Input Services,
  // and then requesting the kTISPropertyUnicodeKeyLayoutData and looking in
  // there, let's go with a pragmatic hack.
  bool useEventCharacters = eventString.length == 0;
  NSString* eventCharacters = event.characters;
  if (eventString.length > 0 && eventCharacters.length > 0) {
    if ([eventString characterAtIndex:0] > 0x7f &&
        [eventCharacters characterAtIndex:0] <= 0x7f) {
      useEventCharacters = true;
    } else if (ui::cocoa::g_is_input_source_command_hebrew &&
               [eventString isEqualToString:@"/"] &&
               [eventCharacters isEqualToString:@"q"]) {
      // Our pragmatic hack works very well except for the "q" key in Hebrew
      // layouts. In this case, the first char of eventString ("/") is
      // not < 0x7f, so the hack doesn't choose eventCharacters (which is
      // "q"). This causes Cmd-q to not take the normal processing path which
      // includes a warning to hold "Cmd q" to quit (if that option is set).
      // Instead, the Cmd-q likely travels to the renderer and upon its return
      // triggers -[NSApplication terminate:], the selector associated with
      // Chrome -> Quit. We handle this special case here.
      useEventCharacters = true;
    } else if (ui::cocoa::g_is_input_source_command_arabic &&
               [eventString isEqualToString:@"{"] &&
               [eventCharacters isEqualToString:@"V"]) {
      // Similar problem of our hack not working for the "V" key in certain
      // Arabic layouts. In this case, the first char of eventString ("{") is
      // not < 0x7f, so the hack doesn't choose eventCharacters (which is
      // "V"). This causes ⇧⌘V not to match Paste and Match Style.
      useEventCharacters = true;
    }
  }
  if (useEventCharacters) {
    eventString = eventCharacters;

    // If the user is pressing the Shift key, force the shortcut string to
    // uppercase. Otherwise, if only Caps Lock is down, ensure the shortcut
    // string is lowercase.
    if (eventModifiers & NSEventModifierFlagShift) {
      eventString = eventString.uppercaseString;
    } else if (eventModifiers & NSEventModifierFlagCapsLock) {
      eventString = eventString.lowercaseString;
    }
  }

  if (eventString.length == 0 || self.keyEquivalent.length == 0) {
    return NO;
  }

  // Turns out esc never fires unless cmd or ctrl is down.
  if (event.keyCode == kVK_Escape &&
      (eventModifiers &
       (NSEventModifierFlagControl | NSEventModifierFlagCommand)) == 0) {
    return NO;
  }

  // From the |NSMenuItem setKeyEquivalent:| documentation:
  //
  // If you want to specify the Backspace key as the key equivalent for a menu
  // item, use a single character string with NSBackspaceCharacter (defined in
  // NSText.h as 0x08) and for the Forward Delete key, use NSDeleteCharacter
  // (defined in NSText.h as 0x7F). Note that these are not the same characters
  // you get from an NSEvent key-down event when pressing those keys.
  if ([self.keyEquivalent characterAtIndex:0] == NSBackspaceCharacter &&
      [eventString characterAtIndex:0] == NSDeleteCharacter) {
    unichar chr = NSBackspaceCharacter;
    eventString = [NSString stringWithCharacters:&chr length:1];

    // Make sure "shift" is not removed from modifiers below.
    eventModifiers |= NSEventModifierFlagFunction;
  }
  if ([self.keyEquivalent characterAtIndex:0] == NSDeleteCharacter &&
      [eventString characterAtIndex:0] == NSDeleteFunctionKey) {
    unichar chr = NSDeleteCharacter;
    eventString = [NSString stringWithCharacters:&chr length:1];

    // Make sure "shift" is not removed from modifiers below.
    eventModifiers |= NSEventModifierFlagFunction;
  }

  // We intentionally leak this object.
  [[maybe_unused]] static KeyboardInputSourceListener* listener =
      [[KeyboardInputSourceListener alloc] init];

  // We typically want to compare [NSMenuItem keyEquivalent] against [NSEvent
  // charactersIgnoringModifiers]. There are special command-qwerty layouts
  // (such as DVORAK-QWERTY) which use QWERTY-style shortcuts when the Command
  // key is held down. In this case, we want to use the keycode of the event
  // rather than looking at the characters.
  if (ui::cocoa::g_is_input_source_command_qwerty) {
    ui::KeyboardCode windows_keycode =
        ui::KeyboardCodeFromKeyCode(event.keyCode);
    unichar shifted_character, character;
    ui::MacKeyCodeForWindowsKeyCode(windows_keycode, event.modifierFlags,
                                    &shifted_character, &character);
    eventString = [NSString stringWithFormat:@"%C", shifted_character];
  }

  // On all keyboards except Dvorak-Right/Left, treat cmd + <number key> as the
  // equivalent numerical key. This is technically incorrect, since the actual
  // character produced may not be a number key, but this causes Chrome to match
  // platform behavior. For example, on the Czech keyboard, we want to interpret
  // cmd + '+' as cmd + '1', even though the '1' character normally requires
  // cmd + shift + '+'.
  if (!ui::cocoa::g_is_input_source_dvorak_right_or_left &&
      eventModifiers == NSEventModifierFlagCommand) {
    ui::KeyboardCode windows_keycode =
        ui::KeyboardCodeFromKeyCode(event.keyCode);
    if (windows_keycode >= ui::VKEY_0 && windows_keycode <= ui::VKEY_9) {
      eventString =
          [NSString stringWithFormat:@"%d", windows_keycode - ui::VKEY_0];
    }
  }

  // [ctr + shift + tab] generates the "End of Medium" keyEquivalent rather than
  // "Horizontal Tab". We still use "Horizontal Tab" in the main menu to match
  // the behavior of Safari and Terminal. Thus, we need to explicitly check for
  // this case.
  if ((eventModifiers & NSEventModifierFlagShift) &&
      [eventString isEqualToString:@"\x19"]) {
    eventString = @"\x9";
  } else {
    // Clear shift key for printable characters, excluding tab.
    if ((eventModifiers &
         (NSEventModifierFlagNumericPad | NSEventModifierFlagFunction)) == 0 &&
        [self.keyEquivalent characterAtIndex:0] != '\r' &&
        [self.keyEquivalent characterAtIndex:0] != '\x9') {
      eventModifiers &= ~NSEventModifierFlagShift;
    }
  }

  // Clear all non-interesting modifiers
  eventModifiers &= ui::cocoa::ModifierMaskForKeyEvent(event);

  return [eventString isEqualToString:self.keyEquivalent] &&
         eventModifiers == self.keyEquivalentModifierMask;
}

- (void)cr_setKeyEquivalent:(NSString*)aString
               modifierMask:(NSEventModifierFlags)mask {
  DCHECK(aString);
  self.keyEquivalent = aString;
  self.keyEquivalentModifierMask = mask;
}

- (void)cr_clearKeyEquivalent {
  self.keyEquivalent = @"";
  self.keyEquivalentModifierMask = 0;
}

@end
