// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_EXTENSION_IME_UTIL_H_
#define UI_BASE_IME_ASH_EXTENSION_IME_UTIL_H_

#include <string>

#include "base/auto_reset.h"
#include "base/component_export.h"

namespace ash {

// Extension IME related utilities.
namespace extension_ime_util {

COMPONENT_EXPORT(UI_BASE_IME_ASH) extern const char kXkbExtensionId[];
COMPONENT_EXPORT(UI_BASE_IME_ASH) extern const char kM17nExtensionId[];
COMPONENT_EXPORT(UI_BASE_IME_ASH) extern const char kHangulExtensionId[];
COMPONENT_EXPORT(UI_BASE_IME_ASH) extern const char kMozcExtensionId[];
COMPONENT_EXPORT(UI_BASE_IME_ASH) extern const char kT13nExtensionId[];
COMPONENT_EXPORT(UI_BASE_IME_ASH)
extern const char kChinesePinyinExtensionId[];
COMPONENT_EXPORT(UI_BASE_IME_ASH)
extern const char kChineseZhuyinExtensionId[];
COMPONENT_EXPORT(UI_BASE_IME_ASH)
extern const char kChineseCangjieExtensionId[];

// Extension id, path (relative to |chrome::DIR_RESOURCES|) and IME engine
// id for the builtin-in Braille IME extension.
COMPONENT_EXPORT(UI_BASE_IME_ASH)
extern const char kBrailleImeExtensionId[];
COMPONENT_EXPORT(UI_BASE_IME_ASH)
extern const char kBrailleImeExtensionPath[];
COMPONENT_EXPORT(UI_BASE_IME_ASH) extern const char kBrailleImeEngineId[];

// The fake language name used for ARC IMEs.
COMPONENT_EXPORT(UI_BASE_IME_ASH) extern const char kArcImeLanguage[];

// Returns InputMethodID for |engine_id| in |extension_id| of extension IME.
// This function does not check |extension_id| is installed extension IME nor
// |engine_id| is really a member of |extension_id|.
std::string COMPONENT_EXPORT(UI_BASE_IME_ASH)
    GetInputMethodID(const std::string& extension_id,
                     const std::string& engine_id);

// Returns InputMethodID for |engine_id| in |extension_id| of component
// extension IME, This function does not check |extension_id| is component one
// nor |engine_id| is really a member of |extension_id|.
std::string COMPONENT_EXPORT(UI_BASE_IME_ASH)
    GetComponentInputMethodID(const std::string& extension_id,
                              const std::string& engine_id);

// Returns InputMethodID for |engine_id| in |extension_id| of ARC IME.
// This function does not check |extension_id| is one for ARC IME nor
// |engine_id| is really an installed ARC IME.
std::string COMPONENT_EXPORT(UI_BASE_IME_ASH)
    GetArcInputMethodID(const std::string& extension_id,
                        const std::string& engine_id);

// Returns extension ID if |input_method_id| is extension IME ID or component
// extension IME ID. Otherwise returns an empty string ("").
std::string COMPONENT_EXPORT(UI_BASE_IME_ASH)
    GetExtensionIDFromInputMethodID(const std::string& input_method_id);

// Returns InputMethodID from engine id (e.g. xkb:fr:fra), or returns itself if
// the |engine_id| is not a known engine id.
// The caller must make sure the |engine_id| is from system input methods
// instead of 3rd party input methods.
std::string COMPONENT_EXPORT(UI_BASE_IME_ASH)
    GetInputMethodIDByEngineID(const std::string& engine_id);

// Returns true if |input_method_id| is extension IME ID. This function does not
// check |input_method_id| is installed extension IME.
bool COMPONENT_EXPORT(UI_BASE_IME_ASH)
    IsExtensionIME(const std::string& input_method_id);

// Returns true if |input_method_id| is component extension IME ID. This
// function does not check |input_method_id| is really allowlisted one or not.
// If you want to check |input_method_id| is allowlisted component extension
// IME, please use ComponentExtensionIMEManager::Isallowlisted instead.
bool COMPONENT_EXPORT(UI_BASE_IME_ASH)
    IsComponentExtensionIME(const std::string& input_method_id);

// Returns true if |input_method_id| is a Arc IME ID. This function does not
// check |input_method_id| is really a installed Arc IME.
bool COMPONENT_EXPORT(UI_BASE_IME_ASH)
    IsArcIME(const std::string& input_method_id);

// Returns true if the |input_method_id| is the extension based xkb keyboard,
// otherwise returns false.
bool COMPONENT_EXPORT(UI_BASE_IME_ASH)
    IsKeyboardLayoutExtension(const std::string& input_method_id);

// Returns input method component id from the extension-based InputMethodID
// for component IME extensions. This function does not check that
// |input_method_id| is installed.
std::string COMPONENT_EXPORT(UI_BASE_IME_ASH)
    GetComponentIDByInputMethodID(const std::string& input_method_id);

// Only used when ash::features::kImeKoreanModeSwitchDebug flag is enabled.
// TODO(b/302460634): Remove when no longer needed.
bool COMPONENT_EXPORT(UI_BASE_IME_ASH)
    IsCros1pKorean(const std::string& input_method_id);

}  // namespace extension_ime_util
}  // namespace ash

#endif  // UI_BASE_IME_ASH_EXTENSION_IME_UTIL_H_
