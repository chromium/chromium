// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/command.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command_constants.h"
#include "ui/base/accelerators/media_keys_listener.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {

namespace {

// Maximum number of tokens a shortcut can have if it allows the
// Ctrl+Alt shortcut combination.
#if BUILDFLAG(IS_CHROMEOS)
// ChromeOS supports an additional modifier 'Search', which can result in longer
// sequences.
static const int kMaxTokenSize = 5;
#else
static const int kMaxTokenSize = 4;
#endif  // BUILDFLAG(IS_CHROMEOS)

bool DoesRequireModifier(ui::Accelerator accelerator) {
  const KeyboardCode key_code = accelerator.key_code();
  return key_code != ui::VKEY_MEDIA_NEXT_TRACK &&
         key_code != ui::VKEY_MEDIA_PLAY_PAUSE &&
         key_code != ui::VKEY_MEDIA_PREV_TRACK &&
         key_code != ui::VKEY_MEDIA_STOP;
}

bool HasAnyModifierKeys(ui::Accelerator accelerator) {
  return ui::Accelerator::MaskOutKeyEventFlags(accelerator.modifiers()) != 0;
}

bool HasValidModifierCombination(ui::Accelerator accelerator,
                                 bool allow_ctrl_alt) {
  // Must have a modifier
  if (DoesRequireModifier(accelerator) && !HasAnyModifierKeys(accelerator)) {
    return false;
  }

  // Usually Ctrl+Alt/Cmd+Option key combinations are not supported. See this
  // article: https://devblogs.microsoft.com/oldnewthing/20040329-00/?p=40003
  if (!allow_ctrl_alt && accelerator.IsAltDown() &&
      (accelerator.IsCtrlDown() || accelerator.IsCmdDown())) {
    return false;
  }

  if (accelerator.IsShiftDown()) {
    return accelerator.IsCtrlDown() || accelerator.IsAltDown() ||
           accelerator.IsCmdDown();
  }

  return true;
}
}  // namespace

Command::Command(std::string_view command_name,
                 std::u16string_view description,
                 bool global)
    : command_name_(command_name), description_(description), global_(global) {}

// static
std::string Command::CommandPlatform() {
#if BUILDFLAG(IS_WIN)
  return ui::kKeybindingPlatformWin;
#elif BUILDFLAG(IS_MAC)
  return ui::kKeybindingPlatformMac;
#elif BUILDFLAG(IS_CHROMEOS)
  return ui::kKeybindingPlatformChromeOs;
#elif BUILDFLAG(IS_LINUX)
  return ui::kKeybindingPlatformLinux;
#elif BUILDFLAG(IS_FUCHSIA)
  // TODO(crbug.com/40220501): Change this once we decide what string should be
  // used for Fuchsia.
  return ui::kKeybindingPlatformLinux;
#elif BUILDFLAG(IS_DESKTOP_ANDROID)
  // For now, we use linux keybindings on desktop android.
  // TODO(https://crbug.com/356905053): Should this be ChromeOS keybindings?
  return ui::kKeybindingPlatformLinux;
#else
  return "";
#endif
}

// static
ui::Accelerator Command::StringToAccelerator(std::string_view accelerator) {
  std::u16string error;
  ui::Accelerator parsed =
      ParseImpl(accelerator, CommandPlatform(), false, base::DoNothing(),
                /*allow_ctrl_alt=*/true);
  return parsed;
}

// static
std::string Command::AcceleratorToString(const ui::Accelerator& accelerator) {
  if (!HasValidModifierCombination(accelerator, true)) {
    return "";
  }

  std::string shortcut;

  if (accelerator.IsCtrlDown()) {
    shortcut += ui::kKeyCtrl;
    shortcut += ui::kKeySeparator;
  }

  if (accelerator.IsAltDown()) {
    shortcut += ui::kKeyAlt;
    shortcut += ui::kKeySeparator;
  }

  if (accelerator.IsCmdDown()) {
#if BUILDFLAG(IS_CHROMEOS)
    // Chrome OS treats the Search key like the Command key.
    shortcut += ui::kKeySearch;
#else
    shortcut += ui::kKeyCommand;
#endif
    shortcut += ui::kKeySeparator;
  }

  if (accelerator.IsShiftDown()) {
    shortcut += ui::kKeyShift;
    shortcut += ui::kKeySeparator;
  }

  if (accelerator.key_code() >= ui::VKEY_0 &&
      accelerator.key_code() <= ui::VKEY_9) {
    shortcut += static_cast<char>('0' + (accelerator.key_code() - ui::VKEY_0));
  } else if (accelerator.key_code() >= ui::VKEY_A &&
             accelerator.key_code() <= ui::VKEY_Z) {
    shortcut += static_cast<char>('A' + (accelerator.key_code() - ui::VKEY_A));
  } else {
    switch (accelerator.key_code()) {
      case ui::VKEY_OEM_COMMA:
        shortcut += ui::kKeyComma;
        break;
      case ui::VKEY_OEM_PERIOD:
        shortcut += ui::kKeyPeriod;
        break;
      case ui::VKEY_UP:
        shortcut += ui::kKeyUp;
        break;
      case ui::VKEY_DOWN:
        shortcut += ui::kKeyDown;
        break;
      case ui::VKEY_LEFT:
        shortcut += ui::kKeyLeft;
        break;
      case ui::VKEY_RIGHT:
        shortcut += ui::kKeyRight;
        break;
      case ui::VKEY_INSERT:
        shortcut += ui::kKeyIns;
        break;
      case ui::VKEY_DELETE:
        shortcut += ui::kKeyDel;
        break;
      case ui::VKEY_HOME:
        shortcut += ui::kKeyHome;
        break;
      case ui::VKEY_END:
        shortcut += ui::kKeyEnd;
        break;
      case ui::VKEY_PRIOR:
        shortcut += ui::kKeyPgUp;
        break;
      case ui::VKEY_NEXT:
        shortcut += ui::kKeyPgDwn;
        break;
      case ui::VKEY_SPACE:
        shortcut += ui::kKeySpace;
        break;
      case ui::VKEY_TAB:
        shortcut += ui::kKeyTab;
        break;
      case ui::VKEY_MEDIA_NEXT_TRACK:
        shortcut += ui::kKeyMediaNextTrack;
        break;
      case ui::VKEY_MEDIA_PLAY_PAUSE:
        shortcut += ui::kKeyMediaPlayPause;
        break;
      case ui::VKEY_MEDIA_PREV_TRACK:
        shortcut += ui::kKeyMediaPrevTrack;
        break;
      case ui::VKEY_MEDIA_STOP:
        shortcut += ui::kKeyMediaStop;
        break;
      default:
        return "";
    }
  }
  return shortcut;
}

ui::Accelerator Command::ParseImpl(std::string_view accelerator,
                                   std::string_view platform_key,
                                   bool should_parse_media_keys,
                                   AcceleratorParseErrorCallback error_callback,
                                   bool allow_ctrl_alt) {
  if (platform_key != ui::kKeybindingPlatformWin &&
      platform_key != ui::kKeybindingPlatformMac &&
      platform_key != ui::kKeybindingPlatformChromeOs &&
      platform_key != ui::kKeybindingPlatformLinux &&
      platform_key != ui::kKeybindingPlatformDefault) {
    std::move(error_callback)
        .Run(ui::AcceleratorParseError::kUnsupportedPlatform);
    return ui::Accelerator();
  }

  // The max token size is reduced by one if the Ctrl+Alt shortcut combination
  // is not allowed.
  const size_t max_token_size =
      allow_ctrl_alt ? kMaxTokenSize : kMaxTokenSize - 1;
  std::vector<std::string> tokens = base::SplitString(
      accelerator, "+", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (tokens.empty() || tokens.size() > max_token_size) {
    std::move(error_callback).Run(ui::AcceleratorParseError::kMalformedInput);
    return ui::Accelerator();
  }

  // Now, parse it into an accelerator.
  int modifiers = ui::EF_NONE;
  ui::KeyboardCode key = ui::VKEY_UNKNOWN;
  for (const std::string& token : tokens) {
    if (token == ui::kKeyCtrl) {
      modifiers |= ui::EF_CONTROL_DOWN;
    } else if (token == ui::kKeyCommand) {
      if (platform_key == ui::kKeybindingPlatformMac) {
        // Either the developer specified Command+foo in the manifest for Mac or
        // they specified Ctrl and it got normalized to Command (to get Ctrl on
        // Mac the developer has to specify MacCtrl). Therefore we treat this
        // as Command.
        modifiers |= ui::EF_COMMAND_DOWN;
#if BUILDFLAG(IS_MAC)
      } else if (platform_key == ui::kKeybindingPlatformDefault) {
        // If we see "Command+foo" in the Default section it can mean two
        // things, depending on the platform:
        // The developer specified "Ctrl+foo" for Default and it got normalized
        // on Mac to "Command+foo". This is fine. Treat it as Command.
        modifiers |= ui::EF_COMMAND_DOWN;
#endif
      } else {
        // No other platform supports Command.
        key = ui::VKEY_UNKNOWN;
        break;
      }
    } else if (token == ui::kKeySearch) {
      // Search is a special modifier only on ChromeOS and maps to 'Command'.
      if (platform_key == ui::kKeybindingPlatformChromeOs) {
        modifiers |= ui::EF_COMMAND_DOWN;
      } else {
        // No other platform supports Search.
        key = ui::VKEY_UNKNOWN;
        break;
      }
    } else if (token == ui::kKeyAlt) {
      modifiers |= ui::EF_ALT_DOWN;
    } else if (token == ui::kKeyShift) {
      modifiers |= ui::EF_SHIFT_DOWN;
    } else if (token.size() == 1 ||  // A-Z, 0-9.
               token == ui::kKeyComma || token == ui::kKeyPeriod ||
               token == ui::kKeyUp || token == ui::kKeyDown ||
               token == ui::kKeyLeft || token == ui::kKeyRight ||
               token == ui::kKeyIns || token == ui::kKeyDel ||
               token == ui::kKeyHome || token == ui::kKeyEnd ||
               token == ui::kKeyPgUp || token == ui::kKeyPgDwn ||
               token == ui::kKeySpace || token == ui::kKeyTab ||
               token == ui::kKeyMediaNextTrack ||
               token == ui::kKeyMediaPlayPause ||
               token == ui::kKeyMediaPrevTrack || token == ui::kKeyMediaStop) {
      if (key != ui::VKEY_UNKNOWN) {
        // Multiple key assignments.
        key = ui::VKEY_UNKNOWN;
        break;
      }

      if (token == ui::kKeyComma) {
        key = ui::VKEY_OEM_COMMA;
      } else if (token == ui::kKeyPeriod) {
        key = ui::VKEY_OEM_PERIOD;
      } else if (token == ui::kKeyUp) {
        key = ui::VKEY_UP;
      } else if (token == ui::kKeyDown) {
        key = ui::VKEY_DOWN;
      } else if (token == ui::kKeyLeft) {
        key = ui::VKEY_LEFT;
      } else if (token == ui::kKeyRight) {
        key = ui::VKEY_RIGHT;
      } else if (token == ui::kKeyIns) {
        key = ui::VKEY_INSERT;
      } else if (token == ui::kKeyDel) {
        key = ui::VKEY_DELETE;
      } else if (token == ui::kKeyHome) {
        key = ui::VKEY_HOME;
      } else if (token == ui::kKeyEnd) {
        key = ui::VKEY_END;
      } else if (token == ui::kKeyPgUp) {
        key = ui::VKEY_PRIOR;
      } else if (token == ui::kKeyPgDwn) {
        key = ui::VKEY_NEXT;
      } else if (token == ui::kKeySpace) {
        key = ui::VKEY_SPACE;
      } else if (token == ui::kKeyTab) {
        key = ui::VKEY_TAB;
      } else if (token == ui::kKeyMediaNextTrack && should_parse_media_keys) {
        key = ui::VKEY_MEDIA_NEXT_TRACK;
      } else if (token == ui::kKeyMediaPlayPause && should_parse_media_keys) {
        key = ui::VKEY_MEDIA_PLAY_PAUSE;
      } else if (token == ui::kKeyMediaPrevTrack && should_parse_media_keys) {
        key = ui::VKEY_MEDIA_PREV_TRACK;
      } else if (token == ui::kKeyMediaStop && should_parse_media_keys) {
        key = ui::VKEY_MEDIA_STOP;
      } else if (token.size() == 1 && base::IsAsciiUpper(token[0])) {
        key = static_cast<ui::KeyboardCode>(ui::VKEY_A + (token[0] - 'A'));
      } else if (token.size() == 1 && base::IsAsciiDigit(token[0])) {
        key = static_cast<ui::KeyboardCode>(ui::VKEY_0 + (token[0] - '0'));
      } else {
        key = ui::VKEY_UNKNOWN;
        break;
      }
    } else {
      std::move(error_callback).Run(ui::AcceleratorParseError::kMalformedInput);
      return ui::Accelerator();
    }
  }

  const ui::Accelerator parsed_accelerator(key, modifiers);
  if (key == ui::VKEY_UNKNOWN ||
      !HasValidModifierCombination(parsed_accelerator, allow_ctrl_alt)) {
    std::move(error_callback).Run(ui::AcceleratorParseError::kMalformedInput);
    return ui::Accelerator();
  }

  if (ui::MediaKeysListener::IsMediaKeycode(key) &&
      HasAnyModifierKeys(parsed_accelerator)) {
    std::move(error_callback)
        .Run(ui::AcceleratorParseError::kMediaKeyWithModifier);
    return ui::Accelerator();
  }

  return parsed_accelerator;
}

}  // namespace ui
