// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/accelerator.h"

#include <stdint.h>
#include <tuple>

#include "base/check_op.h"
#include "base/i18n/rtl.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/strings/grit/ui_strings.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#if !defined(OS_WIN) && (defined(USE_AURA) || defined(OS_APPLE))
#include "ui/events/keycodes/keyboard_code_conversion.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/base/ui_base_features.h"
#endif

namespace ui {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
template <DomKey::Base T>
using DomKeyConst = typename ui::DomKey::Constant<T>;

// ChromeOS has several shortcuts that uses ASCII punctuation key as a main key
// to triger them (e.g. ctrl+shift+alt+/). However, many of these keys have
// different VKEY on different keyboard layouts, (some require shift or altgr
// to type in), so using these keys combined with shift may not work well on
// non-US layouts.  Instead of using VKEY, the new mapping uses DomKey as a key
// to trigger and maps to VKEY+modifier that would have generated the same key
// on US-keyboard.  See crbug.com/1067269 for more details.
struct {
  KeyboardCode vkey;
  const DomKey::Base dom_key;
  const DomKey::Base shifted_dom_key;
} kAccelConversionMap[] = {
    {VKEY_1, DomKeyConst<'1'>::Character, DomKeyConst<'!'>::Character},
    {VKEY_2, DomKeyConst<'2'>::Character, DomKeyConst<'@'>::Character},
    {VKEY_3, DomKeyConst<'3'>::Character, DomKeyConst<'#'>::Character},
    {VKEY_4, DomKeyConst<'4'>::Character, DomKeyConst<'$'>::Character},
    {VKEY_5, DomKeyConst<'5'>::Character, DomKeyConst<'%'>::Character},
    {VKEY_6, DomKeyConst<'6'>::Character, DomKeyConst<'&'>::Character},
    {VKEY_7, DomKeyConst<'7'>::Character, DomKeyConst<'^'>::Character},
    {VKEY_8, DomKeyConst<'8'>::Character, DomKeyConst<'*'>::Character},
    {VKEY_9, DomKeyConst<'9'>::Character, DomKeyConst<'('>::Character},
    {VKEY_0, DomKeyConst<'0'>::Character, DomKeyConst<')'>::Character},
    {VKEY_OEM_MINUS, DomKeyConst<'-'>::Character, DomKeyConst<'_'>::Character},
    {VKEY_OEM_PLUS, DomKeyConst<'='>::Character, DomKeyConst<'+'>::Character},
    {VKEY_OEM_4, DomKeyConst<'['>::Character, DomKeyConst<'{'>::Character},
    {VKEY_OEM_6, DomKeyConst<']'>::Character, DomKeyConst<'}'>::Character},
    {VKEY_OEM_5, DomKeyConst<'\\'>::Character, DomKeyConst<'|'>::Character},
    {VKEY_OEM_1, DomKeyConst<';'>::Character, DomKeyConst<':'>::Character},
    {VKEY_OEM_7, DomKeyConst<'\''>::Character, DomKeyConst<'\"'>::Character},
    {VKEY_OEM_3, DomKeyConst<'`'>::Character, DomKeyConst<'~'>::Character},
    {VKEY_OEM_COMMA, DomKeyConst<','>::Character, DomKeyConst<'<'>::Character},
    {VKEY_OEM_PERIOD, DomKeyConst<'.'>::Character, DomKeyConst<'>'>::Character},
    {VKEY_OEM_2, DomKeyConst<'/'>::Character, DomKeyConst<'?'>::Character},
};

#endif

const int kModifierMask = EF_SHIFT_DOWN | EF_CONTROL_DOWN | EF_ALT_DOWN |
                          EF_COMMAND_DOWN | EF_ALTGR_DOWN;

const int kInterestingFlagsMask =
    kModifierMask | EF_IS_SYNTHESIZED | EF_IS_REPEAT;

base::string16 ApplyModifierToAcceleratorString(
    const base::string16& accelerator,
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

Accelerator::Accelerator(const KeyEvent& key_event)
    : key_code_(key_event.key_code()),
      key_state_(key_event.type() == ET_KEY_PRESSED ? KeyState::PRESSED
                                                    : KeyState::RELEASED),
      // |modifiers_| may include the repeat flag.
      modifiers_(key_event.flags() & kInterestingFlagsMask),
      time_stamp_(key_event.time_stamp()),
      interrupted_by_mouse_event_(false),
      source_device_id_(key_event.source_device_id()) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (features::IsNewShortcutMappingEnabled()) {
    DomKey dom_key = key_event.GetDomKey();
    if (!dom_key.IsCharacter())
      return;
    for (auto entry : kAccelConversionMap) {
      // ALTGR is always canceled because it's not required on US Keyboard.
      if (entry.dom_key == dom_key) {
        // No shift punctuation key on US keyboard.
        key_code_ = entry.vkey;
        modifiers_ &= ~(ui::EF_SHIFT_DOWN | ui::EF_ALTGR_DOWN);
      }
      if (entry.shifted_dom_key == dom_key) {
        // Punctuation key with shift on US keyboard.
        key_code_ = entry.vkey;
        modifiers_ = (modifiers_ | ui::EF_SHIFT_DOWN) & ~ui::EF_ALTGR_DOWN;
      }
    }
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
                      ? ET_KEY_PRESSED
                      : ET_KEY_RELEASED,
                  key_code(), modifiers(), time_stamp());
}

bool Accelerator::operator <(const Accelerator& rhs) const {
  const int modifiers_with_mask = MaskOutKeyEventFlags(modifiers_);
  const int rhs_modifiers_with_mask = MaskOutKeyEventFlags(rhs.modifiers_);
  return std::tie(key_code_, key_state_, modifiers_with_mask) <
         std::tie(rhs.key_code_, rhs.key_state_, rhs_modifiers_with_mask);
}

bool Accelerator::operator ==(const Accelerator& rhs) const {
  return (key_code_ == rhs.key_code_) && (key_state_ == rhs.key_state_) &&
         (MaskOutKeyEventFlags(modifiers_) ==
          MaskOutKeyEventFlags(rhs.modifiers_)) &&
         interrupted_by_mouse_event_ == rhs.interrupted_by_mouse_event_;
}

bool Accelerator::operator !=(const Accelerator& rhs) const {
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

bool Accelerator::IsRepeat() const {
  return (modifiers_ & EF_IS_REPEAT) != 0;
}

base::string16 Accelerator::GetShortcutText() const {
  base::string16 shortcut;

#if defined(OS_APPLE)
  shortcut = KeyCodeToMacSymbol();
#else
  shortcut = KeyCodeToName();
#endif

  if (shortcut.empty()) {
#if defined(OS_WIN)
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
#elif defined(USE_AURA) || defined(OS_APPLE) || defined(OS_ANDROID)
    const uint16_t c = DomCodeToUsLayoutCharacter(
        UsLayoutKeyboardCodeToDomCode(key_code_), false);
    if (c != 0)
      shortcut +=
          static_cast<base::string16::value_type>(base::ToUpperASCII(c));
#endif
  }

#if defined(OS_APPLE)
  shortcut = ApplyShortFormModifiers(shortcut);
#else
  // Checking whether the character used for the accelerator is alphanumeric.
  // If it is not, then we need to adjust the string later on if the locale is
  // right-to-left. See below for more information of why such adjustment is
  // required.
  base::string16 shortcut_rtl;
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
  // TODO(idana) bug# 1232732: this hack can be avoided if instead of using
  // views::Menu we use views::MenuItemView because the latter is a View
  // subclass and therefore it supports marking text as RTL or LTR using
  // standard Unicode directionality marks.
  if (adjust_shortcut_for_rtl) {
    int key_length = static_cast<int>(shortcut_rtl.length());
    DCHECK_GT(key_length, 0);
    shortcut_rtl.append(base::ASCIIToUTF16("+"));

    // Subtracting the size of the shortcut key and 1 for the '+' sign.
    shortcut_rtl.append(shortcut, 0, shortcut.length() - key_length - 1);
    shortcut.swap(shortcut_rtl);
  }
#endif  // OS_APPLE

  return shortcut;
}

#if defined(OS_APPLE)
base::string16 Accelerator::KeyCodeToMacSymbol() const {
  switch (key_code_) {
    case VKEY_CAPITAL:
      return base::string16({0x21ea});
    case VKEY_RETURN:
      return base::string16({0x2324});
    case VKEY_BACK:
      return base::string16({0x232b});
    case VKEY_ESCAPE:
      return base::string16({0x238b});
    case VKEY_RIGHT:
      return base::string16({0x2192});
    case VKEY_LEFT:
      return base::string16({0x2190});
    case VKEY_UP:
      return base::string16({0x2191});
    case VKEY_DOWN:
      return base::string16({0x2193});
    case VKEY_PRIOR:
      return base::string16({0x21de});
    case VKEY_NEXT:
      return base::string16({0x21df});
    case VKEY_HOME:
      return base::string16({0x2196});
    case VKEY_END:
      return base::string16({0x2198});
    case VKEY_TAB:
      return base::string16({0x21e5});
    // Mac has a shift-tab icon (0x21e4) but we don't use it.
    // "Space" and some other keys are written out; fall back to KeyCodeToName()
    // for those (and any other unhandled keys).
    default:
      return KeyCodeToName();
  }
}
#endif  // OS_APPLE

base::string16 Accelerator::KeyCodeToName() const {
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
    case VKEY_F11:
      string_id = IDS_APP_F11_KEY;
      break;
#if !defined(OS_APPLE)
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
  return string_id ? l10n_util::GetStringUTF16(string_id) : base::string16();
}

base::string16 Accelerator::ApplyLongFormModifiers(
    base::string16 shortcut) const {
  if (IsShiftDown())
    shortcut = ApplyModifierToAcceleratorString(shortcut, IDS_APP_SHIFT_KEY);

  // Note that we use 'else-if' in order to avoid using Ctrl+Alt as a shortcut.
  // See http://blogs.msdn.com/oldnewthing/archive/2004/03/29/101121.aspx for
  // more information.
  if (IsCtrlDown())
    shortcut = ApplyModifierToAcceleratorString(shortcut, IDS_APP_CTRL_KEY);
  else if (IsAltDown())
    shortcut = ApplyModifierToAcceleratorString(shortcut, IDS_APP_ALT_KEY);

  if (IsCmdDown()) {
#if defined(OS_APPLE)
    shortcut = ApplyModifierToAcceleratorString(shortcut, IDS_APP_COMMAND_KEY);
#elif BUILDFLAG(IS_CHROMEOS_ASH)
    shortcut = ApplyModifierToAcceleratorString(shortcut, IDS_APP_SEARCH_KEY);
#elif defined(OS_WIN)
    shortcut = ApplyModifierToAcceleratorString(shortcut, IDS_APP_WINDOWS_KEY);
#else
    NOTREACHED();
#endif
  }

  return shortcut;
}

base::string16 Accelerator::ApplyShortFormModifiers(
    base::string16 shortcut) const {
  const base::char16 kCommandSymbol[] = {0x2318, 0};
  const base::char16 kCtrlSymbol[] = {0x2303, 0};
  const base::char16 kShiftSymbol[] = {0x21e7, 0};
  const base::char16 kOptionSymbol[] = {0x2325, 0};
  const base::char16 kNoSymbol[] = {0};

  std::vector<base::string16> parts;
  parts.push_back(base::string16(IsCtrlDown() ? kCtrlSymbol : kNoSymbol));
  parts.push_back(base::string16(IsAltDown() ? kOptionSymbol : kNoSymbol));
  parts.push_back(base::string16(IsShiftDown() ? kShiftSymbol : kNoSymbol));
  parts.push_back(base::string16(IsCmdDown() ? kCommandSymbol : kNoSymbol));
  parts.push_back(shortcut);
  return base::StrCat(parts);
}

}  // namespace ui
