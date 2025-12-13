// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_EXTENSION_IME_UTIL_H_
#define UI_BASE_IME_ASH_EXTENSION_IME_UTIL_H_

#include <string>

#include "base/auto_reset.h"
#include "base/component_export.h"
#include "build/branding_buildflags.h"

// Extension IME related utilities.
namespace ash::extension_ime_util {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
inline constexpr char kXkbExtensionId[] = "jkghodnilhceideoidjikpgommlajknk";
inline constexpr char kM17nExtensionId[] = "jkghodnilhceideoidjikpgommlajknk";
inline constexpr char kHangulExtensionId[] = "bdgdidmhaijohebebipajioienkglgfo";
inline constexpr char kMozcExtensionId[] = "jkghodnilhceideoidjikpgommlajknk";
inline constexpr char kT13nExtensionId[] = "jkghodnilhceideoidjikpgommlajknk";
inline constexpr char kChinesePinyinExtensionId[] =
    "jkghodnilhceideoidjikpgommlajknk";
inline constexpr char kChineseZhuyinExtensionId[] =
    "jkghodnilhceideoidjikpgommlajknk";
inline constexpr char kChineseCangjieExtensionId[] =
    "jkghodnilhceideoidjikpgommlajknk";
#else
inline constexpr char kXkbExtensionId[] = "fgoepimhcoialccpbmpnnblemnepkkao";
inline constexpr char kM17nExtensionId[] = "jhffeifommiaekmbkkjlpmilogcfdohp";
inline constexpr char kHangulExtensionId[] = "bdgdidmhaijohebebipajioienkglgfo";
inline constexpr char kMozcExtensionId[] = "bbaiamgfapehflhememkfglaehiobjnk";
inline constexpr char kT13nExtensionId[] = "gjaehgfemfahhmlgpdfknkhdnemmolop";
inline constexpr char kChinesePinyinExtensionId[] =
    "cpgalbafkoofkjmaeonnfijgpfennjjn";
inline constexpr char kChineseZhuyinExtensionId[] =
    "ekbifjdfhkmdeeajnolmgdlmkllopefi";
inline constexpr char kChineseCangjieExtensionId[] =
    "aeebooiibjahgpgmhkeocbeekccfknbj";
#endif

// Extension id, path (relative to |chrome::DIR_RESOURCES|) and IME engine
// id for the builtin-in Braille IME extension.
inline constexpr char kBrailleImeExtensionId[] =
    "jddehjeebkoimngcbdkaahpobgicbffp";
inline constexpr char kBrailleImeExtensionPath[] =
    "chromeos/accessibility/braille_ime";
inline constexpr char kBrailleImeEngineId[] =
    "_comp_ime_jddehjeebkoimngcbdkaahpobgicbffpbraille";

// The fake language name used for ARC IMEs.
inline constexpr char kArcImeLanguage[] = "_arc_ime_language_";

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

bool COMPONENT_EXPORT(UI_BASE_IME_ASH)
    IsCros1pKorean(const std::string& input_method_id);

}  // namespace ash::extension_ime_util

#endif  // UI_BASE_IME_ASH_EXTENSION_IME_UTIL_H_
