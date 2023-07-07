// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ASH_PREF_NAMES_H_
#define UI_EVENTS_ASH_PREF_NAMES_H_

namespace prefs {

// Constants for the names of various preferences, for easier changing.
// TODO:: Those pref names probably need to live in a better place.

// *************** PROFILE PREFS ***************
// These are attached to the user profile

// Integer prefs which determine how we remap modifier keys (e.g. swap Alt and
// Control.) Possible values for these prefs are 0-7. See ModifierKey enum in
// src/ui/events/ash/mojom/modifier_key.mojom
inline constexpr char kLanguageRemapSearchKeyTo[] =
    // Note: we no longer use XKB for remapping these keys, but we can't change
    // the pref names since the names are already synced with the cloud.
    "settings.language.xkb_remap_search_key_to";
inline constexpr char kLanguageRemapControlKeyTo[] =
    "settings.language.xkb_remap_control_key_to";
inline constexpr char kLanguageRemapAltKeyTo[] =
    "settings.language.xkb_remap_alt_key_to";
inline constexpr char kLanguageRemapCapsLockKeyTo[] =
    "settings.language.remap_caps_lock_key_to";
inline constexpr char kLanguageRemapEscapeKeyTo[] =
    "settings.language.remap_escape_key_to";
inline constexpr char kLanguageRemapBackspaceKeyTo[] =
    "settings.language.remap_backspace_key_to";
inline constexpr char kLanguageRemapAssistantKeyTo[] =
    "settings.language.xkb_remap_assistant_key_to";
inline constexpr char kLanguageRemapExternalCommandKeyTo[] =
    "settings.language.remap_external_command_key_to";
inline constexpr char kLanguageRemapExternalMetaKeyTo[] =
    "settings.language.remap_external_meta_key_to";

}  // namespace prefs

#endif  // UI_EVENTS_ASH_PREF_NAMES_H_
