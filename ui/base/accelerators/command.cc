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
#include "ui/base/accelerators/command_constants.h"
#include "ui/base/accelerators/media_keys_listener.h"

namespace ui {

namespace {
#if BUILDFLAG(IS_CHROMEOS)
// ChromeOS supports an additional modifier 'Search', which can result in longer
// sequences.
static const int kMaxTokenSize = 4;
#else
static const int kMaxTokenSize = 3;
#endif  // BUILDFLAG(IS_CHROMEOS)

bool DoesRequireModifier(std::string_view accelerator) {
  return accelerator != ui::kKeyMediaNextTrack &&
         accelerator != ui::kKeyMediaPlayPause &&
         accelerator != ui::kKeyMediaPrevTrack &&
         accelerator != ui::kKeyMediaStop;
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
      ParseImpl(accelerator, CommandPlatform(), false, base::DoNothing());
  return parsed;
}

// static
std::string Command::AcceleratorToString(const ui::Accelerator& accelerator) {
  std::string shortcut;

  // Ctrl and Alt are mutually exclusive.
  if (accelerator.IsCtrlDown()) {
    shortcut += ui::kKeyCtrl;
  } else if (accelerator.IsAltDown()) {
    shortcut += ui::kKeyAlt;
  }
  if (!shortcut.empty()) {
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

ui::Accelerator Command::ParseImpl(
    std::string_view accelerator,
    std::string_view platform_key,
    bool should_parse_media_keys,
    AcceleratorParseErrorCallback error_callback) {
  if (platform_key != ui::kKeybindingPlatformWin &&
      platform_key != ui::kKeybindingPlatformMac &&
      platform_key != ui::kKeybindingPlatformChromeOs &&
      platform_key != ui::kKeybindingPlatformLinux &&
      platform_key != ui::kKeybindingPlatformDefault) {
    std::move(error_callback)
        .Run(ui::AcceleratorParseError::kUnsupportedPlatform);
    return ui::Accelerator();
  }

  std::vector<std::string> tokens = base::SplitString(
      accelerator, "+", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (tokens.size() == 0 ||
      (tokens.size() == 1 && DoesRequireModifier(accelerator)) ||
      tokens.size() > kMaxTokenSize) {
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

  bool command = (modifiers & ui::EF_COMMAND_DOWN) != 0;
  bool ctrl = (modifiers & ui::EF_CONTROL_DOWN) != 0;
  bool alt = (modifiers & ui::EF_ALT_DOWN) != 0;
  bool shift = (modifiers & ui::EF_SHIFT_DOWN) != 0;

  // We support Ctrl+foo, Alt+foo, Ctrl+Shift+foo, Alt+Shift+foo, but not
  // Ctrl+Alt+foo and not Shift+foo either. For a more detailed reason why we
  // don't support Ctrl+Alt+foo see this article:
  // http://blogs.msdn.com/b/oldnewthing/archive/2004/03/29/101121.aspx.
  // On Mac Command can also be used in combination with Shift or on its own,
  // as a modifier.
  if (key == ui::VKEY_UNKNOWN || (ctrl && alt) || (command && alt) ||
      (shift && !ctrl && !alt && !command)) {
    std::move(error_callback).Run(ui::AcceleratorParseError::kMalformedInput);
    return ui::Accelerator();
  }

  if (ui::MediaKeysListener::IsMediaKeycode(key) &&
      (shift || ctrl || alt || command)) {
    std::move(error_callback)
        .Run(ui::AcceleratorParseError::kMediaKeyWithModifier);
    return ui::Accelerator();
  }

  return ui::Accelerator(key, modifiers);
}

}  // namespace ui
