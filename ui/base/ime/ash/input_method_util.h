// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_INPUT_METHOD_UTIL_H_
#define UI_BASE_IME_ASH_INPUT_METHOD_UTIL_H_

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "ui/base/ime/ash/input_method_descriptor.h"

namespace ash {
namespace input_method {

// Map from language code to associated input method IDs, etc.
using LanguageCodeToIdsMap =
    std::multimap<std::string, std::string, std::less<>>;

class InputMethodDelegate;

enum InputMethodType {
  kKeyboardLayoutsOnly,
  kAllInputMethods,
};

// A class which provides miscellaneous input method utility functions.
class COMPONENT_EXPORT(UI_BASE_IME_ASH) InputMethodUtil {
 public:
  explicit InputMethodUtil(InputMethodDelegate* delegate);

  InputMethodUtil(const InputMethodUtil&) = delete;
  InputMethodUtil& operator=(const InputMethodUtil&) = delete;

  ~InputMethodUtil();

  std::u16string GetInputMethodMediumName(
      const InputMethodDescriptor& input_method) const;
  std::u16string GetInputMethodLongNameStripped(
      const InputMethodDescriptor& input_method) const;
  std::u16string GetInputMethodLongName(
      const InputMethodDescriptor& input_method) const;

  // Converts an input method ID to an input method descriptor. Returns nullptr
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
  // chromeos/platform/assets/input_methods/allowlist.txt.
  bool GetInputMethodIdsFromLanguageCode(
      std::string_view language_code,
      InputMethodType type,
      std::vector<std::string>* out_input_method_ids) const;

  std::vector<std::string> GetInputMethodIdsFromHandwritingLanguage(
      std::string_view handwriting_language);

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
  static std::string GetMigratedInputMethod(const std::string& input_method_id);

  // Migrates the input method IDs.
  // Returns true if the given input method id list is modified,
  // returns false otherwise.
  // This method should not be removed because it's required to transfer XKB
  // input method ID from VPD into extension-based XKB input method ID.
  static bool GetMigratedInputMethodIDs(
      std::vector<std::string>* input_method_ids);

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
  static std::string GetLocalizedDisplayName(
      const InputMethodDescriptor& descriptor);

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
  using InputMethodIdToDescriptorMap =
      std::map<std::string, InputMethodDescriptor>;

  // Returns the fallback input method descriptor (the very basic US
  // keyboard). This function is mostly used for testing, but may be used
  // as the fallback, when there is no other choice.
  static InputMethodDescriptor GetFallbackInputMethodDescriptor();

 protected:
  // protected: for unit testing as well.
  static bool GetInputMethodIdsFromLanguageCodeInternal(
      const LanguageCodeToIdsMap& language_code_to_ids,
      std::string_view normalized_language_code,
      InputMethodType type,
      std::vector<std::string>* out_input_method_ids);

 private:
  // Get long name of the given input method. |short_name| is to specify whether
  // to get the long name for OOBE screen, because OOBE screen displays shorter
  // name (e.g. 'US' instead of 'US keyboard').
  std::u16string GetInputMethodLongNameInternal(
      const InputMethodDescriptor& input_method,
      bool short_name) const;

  LanguageCodeToIdsMap language_code_to_ids_;
  LanguageCodeToIdsMap handwriting_language_to_ids_;
  InputMethodIdToDescriptorMap id_to_descriptor_;

  using EnglishToIDMap = base::flat_map<std::string, int>;
  EnglishToIDMap english_to_resource_id_;

  raw_ptr<InputMethodDelegate> delegate_;

  base::ThreadChecker thread_checker_;
  std::vector<std::string> hardware_layouts_;
  std::vector<std::string> hardware_login_layouts_;
  std::vector<std::string> cached_hardware_layouts_;
};

}  // namespace input_method
}  // namespace ash

#endif  // UI_BASE_IME_ASH_INPUT_METHOD_UTIL_H_
