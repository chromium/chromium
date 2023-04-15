// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ash/pref_names.h"

namespace prefs {

// *************** PROFILE PREFS ***************
// These are attached to the user profile

// Integer prefs which determine how we remap modifier keys (e.g. swap Alt and
// Control.) Possible values for these prefs are 0-7. See ModifierKey enum in
// src/ui/events/ash/mojom/modifier_key.mojom
const char kLanguageRemapSearchKeyTo[] =
    // Note: we no longer use XKB for remapping these keys, but we can't change
    // the pref names since the names are already synced with the cloud.
    "settings.language.xkb_remap_search_key_to";
const char kLanguageRemapControlKeyTo[] =
    "settings.language.xkb_remap_control_key_to";
const char kLanguageRemapAltKeyTo[] = "settings.language.xkb_remap_alt_key_to";
const char kLanguageRemapCapsLockKeyTo[] =
    "settings.language.remap_caps_lock_key_to";
const char kLanguageRemapEscapeKeyTo[] =
    "settings.language.remap_escape_key_to";
const char kLanguageRemapBackspaceKeyTo[] =
    "settings.language.remap_backspace_key_to";
const char kLanguageRemapAssistantKeyTo[] =
    "settings.language.xkb_remap_assistant_key_to";
const char kLanguageRemapExternalCommandKeyTo[] =
    "settings.language.remap_external_command_key_to";
const char kLanguageRemapExternalMetaKeyTo[] =
    "settings.language.remap_external_meta_key_to";

}  // namespace prefs
