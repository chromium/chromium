// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/accelerator.h"

#include <stdint.h>

#include <tuple>

#include "base/check_op.h"
#include "base/i18n/rtl.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/strings/grit/ui_strings.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#if !BUILDFLAG(IS_WIN) && (defined(USE_AURA) || BUILDFLAG(IS_MAC))
#include "ui/events/keycodes/keyboard_code_conversion.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/base/accelerators/ash/right_alt_event_property.h"
#include "ui/base/ui_base_features.h"
#endif

namespace ui {

namespace {

const int kModifierMask = EF_SHIFT_DOWN | EF_CONTROL_DOWN | EF_ALT_DOWN |
                          EF_COMMAND_DOWN | EF_FUNCTION_DOWN | EF_ALTGR_DOWN;

const int kInterestingFlagsMask =
    kModifierMask | EF_IS_SYNTHESIZED | EF_IS_REPEAT;

std::u16string ApplyModifierToAcceleratorString(
    const std::u16string& accelerator,
    int modifier_message_id) {
  return l10n_util::GetStringFUTF16(
      IDS_APP_ACCELERATOR_WITH_MODIFIER,
      l10n_util::GetStringUTF16(modifier_message_id), accelerator);
}

}  // namespace

Accelerator::Accelerator() : Accelerator(VKEY_UNKNOWN, EF_NONE) {}

Accelerator::Accelerator(KeyboardCode key_code,
                         int modifiers,
                         KeyState key_state,
                         base::TimeTicks time_stamp)
    : key_code_(key_code),
      key_state_(key_state),
      modifiers_(modifiers & kInterestingFlagsMask),
      time_stamp_(time_stamp),
      interrupted_by_mouse_event_(false) {}

#if BUILDFLAG(IS_CHROMEOS)
Accelerator::Accelerator(KeyboardCode key_code,
                         DomCode code,
                         int modifiers,
                         KeyState key_state,
                         base::TimeTicks time_stamp)
    : key_code_(key_code),
      code_(code),
      key_state_(key_state),
      modifiers_(modifiers & kInterestingFlagsMask),
      time_stamp_(time_stamp),
      interrupted_by_mouse_event_(false) {}
#endif

Accelerator::Accelerator(const KeyEvent& key_event)
    : key_code_(key_event.key_code()),
      key_state_(key_event.type() == EventType::kKeyPressed
                     ? KeyState::PRESSED
                     : KeyState::RELEASED),
      // |modifiers_| may include the repeat flag.
      modifiers_(key_event.flags() & kInterestingFlagsMask),
      time_stamp_(key_event.time_stamp()),
      interrupted_by_mouse_event_(false),
      source_device_id_(key_event.source_device_id()) {
#if BUILDFLAG(IS_CHROMEOS)
  if (features::IsImprovedKeyboardShortcutsEnabled()) {
    code_ = key_event.code();
  }

  // Rewrite to Right Alt based on the presence of the property.
  if (key_event.key_code() == VKEY_ASSISTANT &&
      HasRightAltProperty(key_event)) {
    key_code_ = VKEY_RIGHT_ALT;
  }
#endif
}

Accelerator::Accelerator(const Accelerator& accelerator) = default;

Accelerator& Accelerator::operator=(const Accelerator& accelerator) = default;

Accelerator::~Accelerator() = default;

// static
int Accelerator::MaskOutKeyEventFlags(int flags) {
  return flags & kModifierMask;
}

KeyEvent Accelerator::ToKeyEvent() const {
  return KeyEvent(key_state() == Accelerator::KeyState::PRESSED
                      ? EventType::kKeyPressed
                      : EventType::kKeyReleased,
                  key_code(),
#if BUILDFLAG(IS_CHROMEOS)
                  code(),
#endif
                  modifiers(), time_stamp());
}

bool Accelerator::operator<(const Accelerator& rhs) const {
  const int modifiers_with_mask = MaskOutKeyEventFlags(modifiers_);
  const int rhs_modifiers_with_mask = MaskOutKeyEventFlags(rhs.modifiers_);
  return std::tie(key_code_, key_state_, modifiers_with_mask) <
         std::tie(rhs.key_code_, rhs.key_state_, rhs_modifiers_with_mask);
}

bool Accelerator::operator==(const Accelerator& rhs) const {
  return (key_code_ == rhs.key_code_) && (key_state_ == rhs.key_state_) &&
         (MaskOutKeyEventFlags(modifiers_) ==
          MaskOutKeyEventFlags(rhs.modifiers_)) &&
         interrupted_by_mouse_event_ == rhs.interrupted_by_mouse_event_;
}

bool Accelerator::operator!=(const Accelerator& rhs) const {
  return !(*this == rhs);
}

bool Accelerator::IsShiftDown() const {
  return (modifiers_ & EF_SHIFT_DOWN) != 0;
}

bool Accelerator::IsCtrlDown() const {
  return (modifiers_ & EF_CONTROL_DOWN) != 0;
}

bool Accelerator::IsAltDown() const {
  return (modifiers_ & EF_ALT_DOWN) != 0;
}

bool Accelerator::IsAltGrDown() const {
  return (modifiers_ & EF_ALTGR_DOWN) != 0;
}

bool Accelerator::IsCmdDown() const {
  return (modifiers_ & EF_COMMAND_DOWN) != 0;
}

bool Accelerator::IsFunctionDown() const {
  return (modifiers_ & EF_FUNCTION_DOWN) != 0;
}

bool Accelerator::IsRepeat() const {
  return (modifiers_ & EF_IS_REPEAT) != 0;
}

std::u16string Accelerator::GetShortcutText() const {
  std::u16string shortcut;

#if BUILDFLAG(IS_MAC)
  shortcut = KeyCodeToMacSymbol();
#else
  shortcut = KeyCodeToName();
#endif

  if (shortcut.empty()) {
#if BUILDFLAG(IS_WIN)
    // Our fallback is to try translate the key code to a regular character
    // unless it is one of digits (VK_0 to VK_9). Some keyboard
    // layouts have characters other than digits assigned in
    // an unshifted mode (e.g. French AZERY layout has 'a with grave
    // accent' for '0'). For display in the menu (e.g. Ctrl-0 for the
    // default zoom level), we leave VK_[0-9] alone without translation.
    wchar_t key;
    if (base::IsAsciiDigit(key_code_))
      key = static_cast<wchar_t>(key_code_);
    else
      key = LOWORD(::MapVirtualKeyW(key_code_, MAPVK_VK_TO_CHAR));
    // If there is no translation for the given |key_code_| (e.g.
    // VKEY_UNKNOWN), |::MapVirtualKeyW| returns 0.
    if (key != 0)
      shortcut += key;
#elif defined(USE_AURA) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
    const uint16_t c = DomCodeToUsLayoutCharacter(
        UsLayoutKeyboardCodeToDomCode(key_code_), false);
    if (c != 0)
      shortcut +=
          static_cast<std::u16string::value_type>(base::ToUpperASCII(c));
#endif
  }

#if BUILDFLAG(IS_MAC)
  shortcut = ApplyShortFormModifiers(shortcut);
#else
  // Checking whether the character used for the accelerator is alphanumeric.
  // If it is not, then we need to adjust the string later on if the locale is
  // right-to-left. See below for more information of why such adjustment is
  // required.
  std::u16string shortcut_rtl;
  bool adjust_shortcut_for_rtl = false;
  if (base::i18n::IsRTL() && shortcut.length() == 1 &&
      !base::IsAsciiAlpha(shortcut[0]) && !base::IsAsciiDigit(shortcut[0])) {
    adjust_shortcut_for_rtl = true;
    shortcut_rtl.assign(shortcut);
  }

  shortcut = ApplyLongFormModifiers(shortcut);

  // For some reason, menus in Windows ignore standard Unicode directionality
  // marks (such as LRE, PDF, etc.). On RTL locales, we use RTL menus and
  // therefore any text we draw for the menu items is drawn in an RTL context.
  // Thus, the text "Ctrl++" (which we currently use for the Zoom In option)
  // appears as "++Ctrl" in RTL because the Unicode BiDi algorithm puts
  // punctuations on the left when the context is right-to-left. Shortcuts that
  // do not end with a punctuation mark (such as "Ctrl+H" do not have this
  // problem).
  //
  // The only way to solve this problem is to adjust the string if the locale
  // is RTL so that it is drawn correctly in an RTL context. Instead of
  // returning "Ctrl++" in the above example, we return "++Ctrl". This will
  // cause the text to appear as "Ctrl++" when Windows draws the string in an
  // RTL context because the punctuation no longer appears at the end of the
  // string.
  //
  // TODO(crbug.com/40175605): This hack of doing the RTL adjustment here was
  // intended to be removed when the menu system moved to MenuItemView. That was
  // crbug.com/2822, closed in 2010. Can we finally remove all of this?
  if (adjust_shortcut_for_rtl) {
    DCHECK_GT(shortcut_rtl.length(), 0u);
    shortcut_rtl.append(u"+");

    shortcut_rtl.append(shortcut, 0, shortcut.length() - shortcut_rtl.length());
    shortcut.swap(shortcut_rtl);
  }
#endif  // BUILDFLAG(IS_MAC)

  return shortcut;
}

#if BUILDFLAG(IS_MAC)
// In macOS 10.13, the glyphs used for page up, page down, home, and end were
// changed from the arrows below to new, skinny arrows. The tricky bit is that
// the underlying Unicode characters weren't changed, just the font used. Maybe
// the keyboard font, CTFontCreateUIFontForLanguage, with key
// kCTFontUIFontMenuItemCmdKey, can be used everywhere this symbol is used. (If
// so, then the RTL stuff will need to be removed.)
std::u16string Accelerator::KeyCodeToMacSymbol() const {
  switch (key_code_) {
    case VKEY_CAPITAL:
      return u"⇪";  // U+21EA, UPWARDS WHITE ARROW FROM BAR
    case VKEY_RETURN:
      return u"⌤";  // U+2324, UP ARROWHEAD BETWEEN TWO HORIZONTAL BARS
    case VKEY_BACK:
      return u"⌫";  // U+232B, ERASE TO THE LEFT
    case VKEY_ESCAPE:
      return u"⎋";  // U+238B, BROKEN CIRCLE WITH NORTHWEST ARROW
    case VKEY_RIGHT:
      return u"→";  // U+2192, RIGHTWARDS ARROW
    case VKEY_LEFT:
      return u"←";  // U+2190, LEFTWARDS ARROW
    case VKEY_UP:
      return u"↑";  // U+2191, UPWARDS ARROW
    case VKEY_DOWN:
      return u"↓";  // U+2193, DOWNWARDS ARROW
    case VKEY_PRIOR:
      return u"⇞";  // U+21DE, UPWARDS ARROW WITH DOUBLE STROKE
    case VKEY_NEXT:
      return u"⇟";  // U+21DF, DOWNWARDS ARROW WITH DOUBLE STROKE
    case VKEY_HOME:
      return base::i18n::IsRTL() ? u"↗"   // U+2197, NORTH EAST ARROW
                                 : u"↖";  // U+2196, NORTH WEST ARROW
    case VKEY_END:
      return base::i18n::IsRTL() ? u"↙"   // U+2199, SOUTH WEST ARROW
                                 : u"↘";  // U+2198, SOUTH EAST ARROW
    case VKEY_TAB:
      return u"⇥";  // U+21E5, RIGHTWARDS ARROW TO BAR
    // Mac has a shift-tab icon ("⇤", U+21E4, LEFTWARDS ARROW TO BAR) but we
    // don't use it. "Space" and some other keys are written out; fall back to
    // KeyCodeToName() for those (and any other unhandled keys).
    default:
      return KeyCodeToName();
  }
}
#endif  // BUILDFLAG(IS_MAC)

std::u16string Accelerator::KeyCodeToName() const {
  int string_id = 0;
  switch (key_code_) {
    case VKEY_TAB:
      string_id = IDS_APP_TAB_KEY;
      break;
    case VKEY_RETURN:
      string_id = IDS_APP_ENTER_KEY;
      break;
    case VKEY_SPACE:
      string_id = IDS_APP_SPACE_KEY;
      break;
    case VKEY_PRIOR:
      string_id = IDS_APP_PAGEUP_KEY;
      break;
    case VKEY_NEXT:
      string_id = IDS_APP_PAGEDOWN_KEY;
      break;
    case VKEY_END:
      string_id = IDS_APP_END_KEY;
      break;
    case VKEY_HOME:
      string_id = IDS_APP_HOME_KEY;
      break;
    case VKEY_INSERT:
      string_id = IDS_APP_INSERT_KEY;
      break;
    case VKEY_DELETE:
      string_id = IDS_APP_DELETE_KEY;
      break;
    case VKEY_LEFT:
      string_id = IDS_APP_LEFT_ARROW_KEY;
      break;
    case VKEY_RIGHT:
      string_id = IDS_APP_RIGHT_ARROW_KEY;
      break;
    case VKEY_UP:
      string_id = IDS_APP_UP_ARROW_KEY;
      break;
    case VKEY_DOWN:
      string_id = IDS_APP_DOWN_ARROW_KEY;
      break;
    case VKEY_ESCAPE:
      string_id = IDS_APP_ESC_KEY;
      break;
    case VKEY_BACK:
      string_id = IDS_APP_BACKSPACE_KEY;
      break;
    case VKEY_F1:
      string_id = IDS_APP_F1_KEY;
      break;
    case VKEY_F6:
      string_id = IDS_APP_F6_KEY;
      break;
    case VKEY_F11:
      string_id = IDS_APP_F11_KEY;
      break;
#if !BUILDFLAG(IS_MAC)
    // On Mac, commas and periods are used literally in accelerator text.
    case VKEY_OEM_COMMA:
      string_id = IDS_APP_COMMA_KEY;
      break;
    case VKEY_OEM_PERIOD:
      string_id = IDS_APP_PERIOD_KEY;
      break;
#endif
    case VKEY_MEDIA_NEXT_TRACK:
      string_id = IDS_APP_MEDIA_NEXT_TRACK_KEY;
      break;
    case VKEY_MEDIA_PLAY_PAUSE:
      string_id = IDS_APP_MEDIA_PLAY_PAUSE_KEY;
      break;
    case VKEY_MEDIA_PREV_TRACK:
      string_id = IDS_APP_MEDIA_PREV_TRACK_KEY;
      break;
    case VKEY_MEDIA_STOP:
      string_id = IDS_APP_MEDIA_STOP_KEY;
      break;
    default:
      break;
  }
  return string_id ? l10n_util::GetStringUTF16(string_id) : std::u16string();
}

std::u16string Accelerator::ApplyLongFormModifiers(
    const std::u16string& shortcut) const {
  std::u16string result = shortcut;

  if (IsShiftDown())
    result = ApplyModifierToAcceleratorString(result, IDS_APP_SHIFT_KEY);

  // Note that we use 'else-if' in order to avoid using Ctrl+Alt as a shortcut.
  // See https://devblogs.microsoft.com/oldnewthing/20040329-00/?p=40003 for
  // more information.
  if (IsCtrlDown())
    result = ApplyModifierToAcceleratorString(result, IDS_APP_CTRL_KEY);
  else if (IsAltDown())
    result = ApplyModifierToAcceleratorString(result, IDS_APP_ALT_KEY);

  if (IsCmdDown()) {
#if BUILDFLAG(IS_MAC)
    result = ApplyModifierToAcceleratorString(result, IDS_APP_COMMAND_KEY);
#elif BUILDFLAG(IS_CHROMEOS)
    result = ApplyModifierToAcceleratorString(result, IDS_APP_SEARCH_KEY);
#elif BUILDFLAG(IS_WIN)
    result = ApplyModifierToAcceleratorString(result, IDS_APP_WINDOWS_KEY);
#else
    NOTREACHED();
#endif
  }

  return result;
}

std::u16string Accelerator::ApplyShortFormModifiers(
    const std::u16string& shortcut) const {
  std::u16string result;
  result.reserve(6);

  // Add modifiers in the order that matches how they are displayed in native
  // menus.
  if (IsCtrlDown()) {
    result.push_back(u'⌃');  // U+2303, UP ARROWHEAD
  }
  if (IsAltDown()) {
    result.push_back(u'⌥');  // U+2325, OPTION KEY
  }
  if (IsShiftDown()) {
    result.push_back(u'⇧');  // U+21E7, UPWARDS WHITE ARROW
  }
  if (IsCmdDown()) {
    result.push_back(u'⌘');  // U+2318, PLACE OF INTEREST SIGN
  }

  if (IsFunctionDown()) {
    // The real "fn" used by menus is actually U+E23E in the Private Use Area in
    // the keyboard font obtained with CTFontCreateUIFontForLanguage, with key
    // kCTFontUIFontMenuItemCmdKey. Because this function must return a raw
    // Unicode string with no specified font, return a string of characters.
    //
    // Newer Mac keyboards have a globe symbol on the fn key that is used in
    // menus instead of "fn". That globe symbol is actually U+1F310 + U+FE0E,
    // the emoji globe + the variation selector that indicates the text-style
    // presentation. (🌐︎)
    //
    // Whether or not "fn" or the globe is displayed as the menu shortcut
    // modifier depends on whether there is an attached keyboard with a globe
    // symbol on it. Rather than rummaging around in the IORegistry, where the
    // HID driver for the keyboard has a SupportsGlobeKey = True property, it's
    // probably best to just make a call to the HIServices function
    // HIS_XPC_GetGlobeKeyAvailability() and let it do the magic. See AppKit's
    // -[NSKeyboardShortcut localizedModifierMaskDisplayName] for an example of
    // this.
    //
    // TODO(http://crbug.com/40800376): Implement all of this when text-style
    // presentations are implemented for Views in https://crbug.com/40137571.
    result.append(u"(fn) ");
  }

  result.append(shortcut);

  return result;
}

}  // namespace ui
