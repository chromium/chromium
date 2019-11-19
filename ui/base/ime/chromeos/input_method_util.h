// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_CHROMEOS_INPUT_METHOD_UTIL_H_
#define UI_BASE_IME_CHROMEOS_INPUT_METHOD_UTIL_H_

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/threading/thread_checker.h"
#include "ui/base/ime/chromeos/input_method_descriptor.h"

namespace chromeos {
namespace input_method {

class InputMethodDelegate;

enum InputMethodType {
  kKeyboardLayoutsOnly,
  kAllInputMethods,
};

// A class which provides miscellaneous input method utility functions.
class COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS) InputMethodUtil {
 public:
  explicit InputMethodUtil(InputMethodDelegate* delegate);
  ~InputMethodUtil();

  // Converts a string sent from IBus IME engines, which is written in English,
  // into Chrome's string ID, then pulls internationalized resource string from
  // the resource bundle and returns it. These functions are not thread-safe.
  // Non-UI threads are not allowed to call them.
  // The english_string to should be a xkb id with "xkb:...:...:..." format.
  // TODO(shuchen): this method should be removed when finish the wrapping of
  // xkb to extension.
  base::string16 TranslateString(const std::string& english_string) const;

  // Converts an input method ID to a language code of the IME. Returns "Eng"
  // when |input_method_id| is unknown.
  // Example: "hangul" => "ko"
  std::string GetLanguageCodeFromInputMethodId(
      const std::string& input_method_id) const;

  // Converts an input method ID to a display name of the IME. Returns
  // an empty strng when |input_method_id| is unknown.
  // Examples: "pinyin" => "Pinyin"
  std::string GetInputMethodDisplayNameFromId(
      const std::string& input_method_id) const;

  base::string16 GetInputMethodShortName(
      const InputMethodDescriptor& input_method) const;
  base::string16 GetInputMethodMediumName(
      const InputMethodDescriptor& input_method) const;
  base::string16 GetInputMethodLongNameStripped(
      const InputMethodDescriptor& input_method) const;
  base::string16 GetInputMethodLongName(
      const InputMethodDescriptor& input_method) const;

  // Converts an input method ID to an input method descriptor. Returns NULL
  // when |input_method_id| is unknown.
  // Example: "pinyin" => { id: "pinyin", display_name: "Pinyin",
  //                        keyboard_layout: "us", language_code: "zh" }
  const InputMethodDescriptor* GetInputMethodDescriptorFromId(
      const std::string& input_method_id) const;

  // Gets input method IDs that belong to |language_code|.
  // If |type| is |kKeyboardLayoutsOnly|, the function does not return input
  // methods that are not for keybord layout switching. Returns true on success.
  // Note that the function might return false or |language_code| is unknown.
  //
  // The retured input method IDs are sorted by populalirty per
  // chromeos/platform/assets/input_methods/whitelist.txt.
  bool GetInputMethodIdsFromLanguageCode(
      const std::string& language_code,
      InputMethodType type,
      std::vector<std::string>* out_input_method_ids) const;

  // Gets the input method IDs suitable for the first user login, based on
  // the given language code (UI language), and the descriptor of the
  // preferred input method.
  void GetFirstLoginInputMethodIds(
      const std::string& language_code,
      const InputMethodDescriptor& preferred_input_method,
      std::vector<std::string>* out_input_method_ids) const;

  // Gets the language codes associated with the given input method IDs.
  // The returned language codes won't have duplicates.
  void GetLanguageCodesFromInputMethodIds(
      const std::vector<std::string>& input_method_ids,
      std::vector<std::string>* out_language_codes) const;

  // Gets first input method associated with the language.
  // Returns empty string on error.
  std::string GetLanguageDefaultInputMethodId(const std::string& language_code);

  // Migrates the input method id as below:
  //  - Legacy xkb id to extension based id, e.g.
  //    xkb:us::eng -> _comp_ime_...xkb:us::eng
  //  - VPD well formatted id to extension based input method id, e.g.
  //    m17n:vi_telex -> _comp_ime_...vkd_vi_telex
  //  - ChromiumOS input method ID to ChromeOS one, or vice versa, e.g.
  //    _comp_ime_xxxxxx...xkb:us::eng -> _comp_ime_yyyyyy...xkb:us::eng
  std::string MigrateInputMethod(const std::string& input_method_id);

  // Migrates the input method IDs.
  // Returns true if the given input method id list is modified,
  // returns false otherwise.
  // This method should not be removed because it's required to transfer XKB
  // input method ID from VPD into extension-based XKB input method ID.
  bool MigrateInputMethods(std::vector<std::string>* input_method_ids);

  // Updates the internal cache of hardware layouts.
  void UpdateHardwareLayoutCache();

  // Set hardware keyboard layout for testing purpose. This is for simulating
  // "keyboard_layout" entry in VPD values.
  void SetHardwareKeyboardLayoutForTesting(const std::string& layout);

  // Fills the input method IDs of the hardware keyboard. e.g. "xkb:us::eng"
  // for US Qwerty keyboard or "xkb:ru::rus" for Russian keyboard.
  const std::vector<std::string>& GetHardwareInputMethodIds();

  // Returns the login-allowed input method ID of the hardware keyboard, e.g.
  // "xkb:us::eng" but not include non-login keyboard like "xkb:ru::rus". Please
  // note that this is not a subset of returned value of
  // GetHardwareInputMethodIds. If GetHardwareInputMethodIds returns only
  // non-login keyboard, this function will returns "xkb:us::eng" as the
  // fallback keyboard.
  const std::vector<std::string>& GetHardwareLoginInputMethodIds();

  // Returns the localized display name for the given input method.
  std::string GetLocalizedDisplayName(
      const InputMethodDescriptor& descriptor) const;

  // Returns true if given input method can be used to input login data.
  bool IsLoginKeyboard(const std::string& input_method_id) const;

  // Returns true if the given input method id is supported.
  bool IsValidInputMethodId(const std::string& input_method_id) const;

  // Returns true if the given input method id is for a keyboard layout.
  static bool IsKeyboardLayout(const std::string& input_method_id);

  // Resets the list of component extension IMEs.
  void ResetInputMethods(const InputMethodDescriptors& imes);

  // Appends the additional list of component extension IMEs.
  void AppendInputMethods(const InputMethodDescriptors& imes);

  // Initializes the extension based xkb IMEs for testing.
  void InitXkbInputMethodsForTesting(const InputMethodDescriptors& imes);

  // Map from input method ID to associated input method descriptor.
  typedef std::map<
    std::string, InputMethodDescriptor> InputMethodIdToDescriptorMap;

  // Gets the id to desctiptor map for testing.
  const InputMethodIdToDescriptorMap& GetIdToDesciptorMapForTesting();

  // Returns the fallback input method descriptor (the very basic US
  // keyboard). This function is mostly used for testing, but may be used
  // as the fallback, when there is no other choice.
  static InputMethodDescriptor GetFallbackInputMethodDescriptor();

 protected:
  // protected: for unit testing as well.
  bool GetInputMethodIdsFromLanguageCodeInternal(
      const std::multimap<std::string, std::string>& language_code_to_ids,
      const std::string& normalized_language_code,
      InputMethodType type,
      std::vector<std::string>* out_input_method_ids) const;

  // Gets the keyboard layout name from the given input method ID.
  // If the ID is invalid, an empty string will be returned.
  // This function only supports xkb layouts.
  //
  // Examples:
  //
  // "xkb:us::eng"       => "us"
  // "xkb:us:dvorak:eng" => "us(dvorak)"
  // "xkb:gb::eng"       => "gb"
  // "pinyin"            => "us" (because Pinyin uses US keyboard layout)
  std::string GetKeyboardLayoutName(const std::string& input_method_id) const;

 private:
  bool TranslateStringInternal(const std::string& english_string,
                               base::string16 *out_string) const;

  // Get long name of the given input method. |short_name| is to specify whether
  // to get the long name for OOBE screen, because OOBE screen displays shorter
  // name (e.g. 'US' instead of 'US keyboard').
  base::string16 GetInputMethodLongNameInternal(
      const InputMethodDescriptor& input_method, bool short_name) const;

  // Map from language code to associated input method IDs, etc.
  typedef std::multimap<std::string, std::string> LanguageCodeToIdsMap;

  LanguageCodeToIdsMap language_code_to_ids_;
  InputMethodIdToDescriptorMap id_to_descriptor_;

  using EnglishToIDMap = base::flat_map<std::string, int>;
  EnglishToIDMap english_to_resource_id_;

  InputMethodDelegate* delegate_;

  base::ThreadChecker thread_checker_;
  std::vector<std::string> hardware_layouts_;
  std::vector<std::string> hardware_login_layouts_;
  std::vector<std::string> cached_hardware_layouts_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodUtil);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // UI_BASE_IME_CHROMEOS_INPUT_METHOD_UTIL_H_
