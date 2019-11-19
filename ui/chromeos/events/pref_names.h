// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_EVENTS_PREF_NAMES_H_
#define UI_CHROMEOS_EVENTS_PREF_NAMES_H_

namespace prefs {

// Constants for the names of various preferences, for easier changing.
// TODO:: Those pref names probably need to live in a better place.

extern const char kLanguageRemapCapsLockKeyTo[];
extern const char kLanguageRemapSearchKeyTo[];
extern const char kLanguageRemapControlKeyTo[];
extern const char kLanguageRemapAltKeyTo[];
extern const char kLanguageRemapEscapeKeyTo[];
extern const char kLanguageRemapBackspaceKeyTo[];
extern const char kLanguageRemapAssistantKeyTo[];
extern const char kLanguageRemapExternalCommandKeyTo[];
extern const char kLanguageRemapExternalMetaKeyTo[];

}  // namespace prefs

#endif  // UI_CHROMEOS_EVENTS_PREF_NAMES_H_
