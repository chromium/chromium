// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/events/keycodes/platform_key_map_win.h"

#include <algorithm>
#include <utility>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_local.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

namespace ui {

namespace {

// List of modifiers mentioned in https://w3c.github.io/uievents/#keys-modifiers
// Some modifiers are commented out because they usually don't change keys.
const EventFlags modifier_flags[] = {
    EF_SHIFT_DOWN, EF_CONTROL_DOWN, EF_ALT_DOWN,
    // EF_COMMAND_DOWN,
    // EF_NUM_LOCK_ON,
    EF_CAPS_LOCK_ON,
    // EF_SCROLL_LOCK_ON,

    // Simulated as Control+Alt.
    // EF_ALTGR_DOWN,
};

void SetModifierState(BYTE* keyboard_state, int flags) {
  // According to MSDN GetKeyState():
  // 1. If the high-order bit is 1, the key is down; otherwise, it is up.
  // 2. If the low-order bit is 1, the key is toggled. A key, such as the
  //    CAPS LOCK key, is toggled if it is turned on. The key is off and
  //    untoggled if the low-order bit is 0.
  // See https://msdn.microsoft.com/en-us/library/windows/desktop/ms646301.aspx
  if (flags & EF_SHIFT_DOWN)
    keyboard_state[VK_SHIFT] |= 0x80;

  if (flags & EF_CONTROL_DOWN)
    keyboard_state[VK_CONTROL] |= 0x80;

  if (flags & EF_ALT_DOWN)
    keyboard_state[VK_MENU] |= 0x80;

  if (flags & EF_CAPS_LOCK_ON)
    keyboard_state[VK_CAPITAL] |= 0x01;

  DCHECK_EQ(flags & ~(EF_SHIFT_DOWN | EF_CONTROL_DOWN | EF_ALT_DOWN |
                      EF_CAPS_LOCK_ON),
            0);
}

constexpr int kControlAndAltFlags = EF_CONTROL_DOWN | EF_ALT_DOWN;

bool HasControlAndAlt(int flags) {
  return (flags & kControlAndAltFlags) == kControlAndAltFlags;
}

int ReplaceAltGraphWithControlAndAlt(int flags) {
  return (flags & EF_ALTGR_DOWN)
             ? ((flags & ~EF_ALTGR_DOWN) | kControlAndAltFlags)
             : flags;
}

const int kModifierFlagsCombinations = (1 << std::size(modifier_flags)) - 1;

int GetModifierFlags(int combination) {
  int flags = EF_NONE;
  for (size_t i = 0; i < std::size(modifier_flags); ++i) {
    if (combination & (1 << i))
      flags |= modifier_flags[i];
  }
  return flags;
}

// This table must be sorted by |key_code| for binary search.
const struct NonPrintableKeyEntry {
  KeyboardCode key_code;
  DomKey dom_key;
} kNonPrintableKeyMap[] = {
    {VKEY_CANCEL, DomKey::CANCEL},
    {VKEY_BACK, DomKey::BACKSPACE},
    {VKEY_TAB, DomKey::TAB},
    {VKEY_CLEAR, DomKey::CLEAR},
    {VKEY_RETURN, DomKey::ENTER},
    {VKEY_SHIFT, DomKey::SHIFT},
    {VKEY_CONTROL, DomKey::CONTROL},
    {VKEY_MENU, DomKey::ALT},
    {VKEY_PAUSE, DomKey::PAUSE},
    {VKEY_CAPITAL, DomKey::CAPS_LOCK},
    // VKEY_KANA == VKEY_HANGUL
    {VKEY_JUNJA, DomKey::JUNJA_MODE},
    {VKEY_FINAL, DomKey::FINAL_MODE},
    // VKEY_HANJA == VKEY_KANJI
    {VKEY_ESCAPE, DomKey::ESCAPE},
    {VKEY_CONVERT, DomKey::CONVERT},
    {VKEY_NONCONVERT, DomKey::NON_CONVERT},
    {VKEY_ACCEPT, DomKey::ACCEPT},
    {VKEY_MODECHANGE, DomKey::MODE_CHANGE},
    // VKEY_SPACE
    {VKEY_PRIOR, DomKey::PAGE_UP},
    {VKEY_NEXT, DomKey::PAGE_DOWN},
    {VKEY_END, DomKey::END},
    {VKEY_HOME, DomKey::HOME},
    {VKEY_LEFT, DomKey::ARROW_LEFT},
    {VKEY_UP, DomKey::ARROW_UP},
    {VKEY_RIGHT, DomKey::ARROW_RIGHT},
    {VKEY_DOWN, DomKey::ARROW_DOWN},
    {VKEY_SELECT, DomKey::SELECT},
    {VKEY_PRINT, DomKey::PRINT},
    {VKEY_EXECUTE, DomKey::EXECUTE},
    {VKEY_SNAPSHOT, DomKey::PRINT_SCREEN},
    {VKEY_INSERT, DomKey::INSERT},
    {VKEY_DELETE, DomKey::DEL},
    {VKEY_HELP, DomKey::HELP},
    // VKEY_0..9
    // VKEY_A..Z
    {VKEY_LWIN, DomKey::META},
    // VKEY_COMMAND == VKEY_LWIN
    {VKEY_RWIN, DomKey::META},
    {VKEY_APPS, DomKey::CONTEXT_MENU},
    {VKEY_SLEEP, DomKey::STANDBY},
    // VKEY_NUMPAD0..9
    // VKEY_MULTIPLY, VKEY_ADD, VKEY_SEPARATOR, VKEY_SUBTRACT, VKEY_DECIMAL,
    // VKEY_DIVIDE
    {VKEY_F1, DomKey::F1},
    {VKEY_F2, DomKey::F2},
    {VKEY_F3, DomKey::F3},
    {VKEY_F4, DomKey::F4},
    {VKEY_F5, DomKey::F5},
    {VKEY_F6, DomKey::F6},
    {VKEY_F7, DomKey::F7},
    {VKEY_F8, DomKey::F8},
    {VKEY_F9, DomKey::F9},
    {VKEY_F10, DomKey::F10},
    {VKEY_F11, DomKey::F11},
    {VKEY_F12, DomKey::F12},
    {VKEY_F13, DomKey::F13},
    {VKEY_F14, DomKey::F14},
    {VKEY_F15, DomKey::F15},
    {VKEY_F16, DomKey::F16},
    {VKEY_F17, DomKey::F17},
    {VKEY_F18, DomKey::F18},
    {VKEY_F19, DomKey::F19},
    {VKEY_F20, DomKey::F20},
    {VKEY_F21, DomKey::F21},
    {VKEY_F22, DomKey::F22},
    {VKEY_F23, DomKey::F23},
    {VKEY_F24, DomKey::F24},
    {VKEY_NUMLOCK, DomKey::NUM_LOCK},
    {VKEY_SCROLL, DomKey::SCROLL_LOCK},
    {VKEY_LSHIFT, DomKey::SHIFT},
    {VKEY_RSHIFT, DomKey::SHIFT},
    {VKEY_LCONTROL, DomKey::CONTROL},
    {VKEY_RCONTROL, DomKey::CONTROL},
    {VKEY_LMENU, DomKey::ALT},
    {VKEY_RMENU, DomKey::ALT},
    {VKEY_BROWSER_BACK, DomKey::BROWSER_BACK},
    {VKEY_BROWSER_FORWARD, DomKey::BROWSER_FORWARD},
    {VKEY_BROWSER_REFRESH, DomKey::BROWSER_REFRESH},
    {VKEY_BROWSER_STOP, DomKey::BROWSER_STOP},
    {VKEY_BROWSER_SEARCH, DomKey::BROWSER_SEARCH},
    {VKEY_BROWSER_FAVORITES, DomKey::BROWSER_FAVORITES},
    {VKEY_BROWSER_HOME, DomKey::BROWSER_HOME},
    {VKEY_VOLUME_MUTE, DomKey::AUDIO_VOLUME_MUTE},
    {VKEY_VOLUME_DOWN, DomKey::AUDIO_VOLUME_DOWN},
    {VKEY_VOLUME_UP, DomKey::AUDIO_VOLUME_UP},
    {VKEY_MEDIA_NEXT_TRACK, DomKey::MEDIA_TRACK_NEXT},
    {VKEY_MEDIA_PREV_TRACK, DomKey::MEDIA_TRACK_PREVIOUS},
    {VKEY_MEDIA_STOP, DomKey::MEDIA_STOP},
    {VKEY_MEDIA_PLAY_PAUSE, DomKey::MEDIA_PLAY_PAUSE},
    {VKEY_MEDIA_LAUNCH_MAIL, DomKey::LAUNCH_MAIL},
    {VKEY_MEDIA_LAUNCH_MEDIA_SELECT, DomKey::LAUNCH_MEDIA_PLAYER},
    {VKEY_MEDIA_LAUNCH_APP1, DomKey::LAUNCH_MY_COMPUTER},
    {VKEY_MEDIA_LAUNCH_APP2, DomKey::LAUNCH_CALCULATOR},
    // VKEY_OEM_1..8, 102, PLUS, COMMA, MINUS, PERIOD
    {VKEY_ALTGR, DomKey::ALT_GRAPH},
    {VKEY_PROCESSKEY, DomKey::PROCESS},
    // VKEY_PACKET - Used to pass Unicode char, considered as printable key.
    {VKEY_ATTN, DomKey::ATTN},
    {VKEY_CRSEL, DomKey::CR_SEL},
    {VKEY_EXSEL, DomKey::EX_SEL},
    {VKEY_EREOF, DomKey::ERASE_EOF},
    {VKEY_PLAY, DomKey::PLAY},
    {VKEY_ZOOM, DomKey::ZOOM_TOGGLE},
    // TODO(input-dev): Handle VKEY_NONAME, VKEY_PA1.
    // https://crbug.com/616910
    {VKEY_OEM_CLEAR, DomKey::CLEAR},
};

// Disambiguates the meaning of certain non-printable keys which have different
// meanings under different languages, but use the same VKEY code.
DomKey LanguageSpecificOemKeyboardCodeToDomKey(KeyboardCode key_code,
                                               HKL layout) {
  WORD language = LOWORD(layout);
  WORD primary_language = PRIMARYLANGID(language);
  if (primary_language == LANG_KOREAN) {
    switch (key_code) {
      case VKEY_HANGUL:
        return DomKey::HANGUL_MODE;
      case VKEY_HANJA:
        return DomKey::HANJA_MODE;
      default:
        return DomKey::NONE;
    }
  } else if (primary_language == LANG_JAPANESE) {
    switch (key_code) {
      // VKEY_KANA isn't generated by any modern layouts but is a listed value
      // that third-party apps might synthesize, so we handle it anyway.
      case VKEY_KANA:
      case VKEY_ATTN:
        return DomKey::KANA_MODE;
      case VKEY_KANJI:
        return DomKey::KANJI_MODE;
      case VKEY_OEM_ATTN:
        return DomKey::ALPHANUMERIC;
      case VKEY_OEM_FINISH:
        return DomKey::KATAKANA;
      case VKEY_OEM_COPY:
        return DomKey::HIRAGANA;
      case VKEY_DBE_SBCSCHAR:
        return DomKey::HANKAKU;
      case VKEY_DBE_DBCSCHAR:
        return DomKey::ZENKAKU;
      case VKEY_OEM_BACKTAB:
        return DomKey::ROMAJI;
      default:
        return DomKey::NONE;
    }
  }
  return DomKey::NONE;
}

DomKey NonPrintableKeyboardCodeToDomKey(KeyboardCode key_code, HKL layout) {
  // 1. Check if |key_code| has a |layout|-specific meaning.
  const DomKey key = LanguageSpecificOemKeyboardCodeToDomKey(key_code, layout);
  if (key != DomKey::NONE)
    return key;

  // 2. Most |key_codes| have the same meaning regardless of |layout|.
  const NonPrintableKeyEntry* result = std::lower_bound(
      std::begin(kNonPrintableKeyMap), std::end(kNonPrintableKeyMap), key_code,
      [](const NonPrintableKeyEntry& entry, KeyboardCode needle) {
        return entry.key_code < needle;
      });
  if (result != std::end(kNonPrintableKeyMap) && result->key_code == key_code)
    return result->dom_key;

  return DomKey::NONE;
}

}  // anonymous namespace

PlatformKeyMap::PlatformKeyMap() {}

PlatformKeyMap::PlatformKeyMap(HKL layout) {
  UpdateLayout(layout);
}

PlatformKeyMap::~PlatformKeyMap() {}

// static
PlatformKeyMap* PlatformKeyMap::GetThreadLocalPlatformKeyMap() {
  // DestructorAtExit so the main thread's instance is cleaned up between tests
  // in the same process.
  static base::LazyInstance<base::ThreadLocalOwnedPointer<PlatformKeyMap>>::
      DestructorAtExit platform_key_map_tls_instance =
          LAZY_INSTANCE_INITIALIZER;

  auto& platform_key_map_tls = platform_key_map_tls_instance.Get();
  PlatformKeyMap* platform_key_map = platform_key_map_tls.Get();
  if (!platform_key_map) {
    auto new_platform_key_map = base::WrapUnique(new PlatformKeyMap);
    platform_key_map = new_platform_key_map.get();
    platform_key_map_tls.Set(std::move(new_platform_key_map));
  }

  return platform_key_map;
}

DomKey PlatformKeyMap::DomKeyFromKeyboardCodeImpl(KeyboardCode key_code,
                                                  int* flags) const {
  // Windows expresses right-Alt as VKEY_MENU with the extended flag set.
  // This key should generate AltGraph under layouts which use that modifier.
  if (key_code == VKEY_MENU && has_alt_graph_ && (*flags & EF_IS_EXTENDED_KEY))
    return DomKey::ALT_GRAPH;

  DomKey key = NonPrintableKeyboardCodeToDomKey(key_code, keyboard_layout_);
  if (key != DomKey::NONE)
    return key;

  // AltGraph is expressed as Control & Alt modifiers in the lookup below.
  const int lookup_flags = ReplaceAltGraphWithControlAndAlt(*flags);
  const int flags_to_try[] = {
      // Trying to match Firefox's behavior and UIEvents DomKey guidelines.
      // If the combination doesn't produce a printable character, the key value
      // should be the key with no modifiers except for Shift and AltGr.
      // See https://w3c.github.io/uievents/#keys-guidelines
      lookup_flags, lookup_flags & (EF_SHIFT_DOWN | EF_CAPS_LOCK_ON), EF_NONE,
  };

  for (auto try_flags : flags_to_try) {
    const auto& it = printable_keycode_to_key_.find(
        std::make_pair(static_cast<int>(key_code), try_flags));
    if (it != printable_keycode_to_key_.end()) {
      key = it->second;
      if (key != DomKey::NONE) {
        // If we find a character with |try_flags| including Control and Alt
        // then this is an AltGraph-shifted event.
        if (HasControlAndAlt(try_flags))
          *flags = ReplaceControlAndAltWithAltGraph(*flags);
        return key;
      }
    }

    // If we found nothing with no flags set, there is nothing left to try.
    if (try_flags == EF_NONE)
      break;
  }

  // Return DomKey::UNIDENTIFIED to prevent US layout fall-back.
  return DomKey::UNIDENTIFIED;
}

// static
DomKey PlatformKeyMap::DomKeyFromKeyboardCode(KeyboardCode key_code,
                                              int* flags) {
  // Use TLS because KeyboardLayout is per thread.
  // However currently PlatformKeyMap will only be used by the host application,
  // which is just one process and one thread.
  PlatformKeyMap* platform_key_map = GetThreadLocalPlatformKeyMap();

  HKL current_layout = ::GetKeyboardLayout(0);
  platform_key_map->UpdateLayout(current_layout);
  return platform_key_map->DomKeyFromKeyboardCodeImpl(key_code, flags);
}

// static
int PlatformKeyMap::ReplaceControlAndAltWithAltGraph(int flags) {
  if (!HasControlAndAlt(flags))
    return flags;
  return (flags & ~kControlAndAltFlags) | EF_ALTGR_DOWN;
}

// static
bool PlatformKeyMap::UsesAltGraph() {
  PlatformKeyMap* platform_key_map = GetThreadLocalPlatformKeyMap();

  HKL current_layout = ::GetKeyboardLayout(0);
  platform_key_map->UpdateLayout(current_layout);
  return platform_key_map->has_alt_graph_;
}

void PlatformKeyMap::UpdateLayout(HKL layout) {
  if (layout == keyboard_layout_)
    return;

  BYTE keyboard_state_to_restore[256];
  if (!::GetKeyboardState(keyboard_state_to_restore))
    return;

  // TODO(input-dev): Optimize layout switching (see crbug.com/587147).
  keyboard_layout_ = layout;
  printable_keycode_to_key_.clear();
  has_alt_graph_ = false;

  // Map size for some sample keyboard layouts:
  // US: 476, French: 602, Persian: 482, Vietnamese: 1436
  printable_keycode_to_key_.reserve(1500);

  for (int modifier_combination = 0;
       modifier_combination <= kModifierFlagsCombinations;
       ++modifier_combination) {
    BYTE keyboard_state[256];
    memset(keyboard_state, 0, sizeof(keyboard_state));

    // Setting up keyboard state for modifiers.
    int flags = GetModifierFlags(modifier_combination);
    SetModifierState(keyboard_state, flags);

    for (int key_code = 0; key_code <= 0xFF; ++key_code) {
      wchar_t translated_chars[5];
      int rv = ::ToUnicodeEx(key_code, 0, keyboard_state, translated_chars,
                             std::size(translated_chars), 0, keyboard_layout_);

      if (rv == -1) {
        // Dead key, injecting VK_SPACE to get character representation.
        BYTE empty_state[256];
        memset(empty_state, 0, sizeof(empty_state));
        rv = ::ToUnicodeEx(VK_SPACE, 0, empty_state, translated_chars,
                           std::size(translated_chars), 0, keyboard_layout_);
        // Expecting a dead key character (not followed by a space).
        if (rv == 1) {
          printable_keycode_to_key_[std::make_pair(static_cast<int>(key_code),
                                                   flags)] =
              DomKey::DeadKeyFromCombiningCharacter(translated_chars[0]);
        } else {
          // TODO(input-dev): Check if this will actually happen.
        }
      } else if (rv == 1) {
        if (translated_chars[0] >= 0x20) {
          printable_keycode_to_key_[std::make_pair(static_cast<int>(key_code),
                                                   flags)] =
              DomKey::FromCharacter(translated_chars[0]);

          // Detect whether the layout makes use of AltGraph.
          if (HasControlAndAlt(flags)) {
            has_alt_graph_ = true;
          }
        } else {
          // Ignores legacy non-printable control characters.
        }
      } else {
        // TODO(input-dev): Handle rv <= -2 and rv >= 2.
      }
    }
  }
  ::SetKeyboardState(keyboard_state_to_restore);
}

}  // namespace ui
