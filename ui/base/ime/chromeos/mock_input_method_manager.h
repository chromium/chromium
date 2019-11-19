// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_CHROMEOS_MOCK_INPUT_METHOD_MANAGER_H_
#define UI_BASE_IME_CHROMEOS_MOCK_INPUT_METHOD_MANAGER_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/input_method_keyboard_controller.h"

namespace chromeos {
namespace input_method {
class InputMethodUtil;
class ImeKeyboard;

// The mock InputMethodManager for testing.
class COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS) MockInputMethodManager
    : public InputMethodManager,
      public ui::InputMethodKeyboardController {
 public:
 public:
  class State : public InputMethodManager::State {
   public:
    State();

    scoped_refptr<InputMethodManager::State> Clone() const override;
    void AddInputMethodExtension(
        const std::string& extension_id,
        const InputMethodDescriptors& descriptors,
        ui::IMEEngineHandlerInterface* instance) override;
    void RemoveInputMethodExtension(const std::string& extension_id) override;
    void ChangeInputMethod(const std::string& input_method_id,
                           bool show_message) override;
    void ChangeInputMethodToJpKeyboard() override;
    void ChangeInputMethodToJpIme() override;
    void ToggleInputMethodForJpIme() override;
    bool EnableInputMethod(
        const std::string& new_active_input_method_id) override;
    void EnableLoginLayouts(
        const std::string& language_code,
        const std::vector<std::string>& initial_layouts) override;
    void EnableLockScreenLayouts() override;
    void GetInputMethodExtensions(InputMethodDescriptors* result) override;
    std::unique_ptr<InputMethodDescriptors> GetActiveInputMethods()
        const override;
    const std::vector<std::string>& GetActiveInputMethodIds() const override;
    const InputMethodDescriptor* GetInputMethodFromId(
        const std::string& input_method_id) const override;
    size_t GetNumActiveInputMethods() const override;
    void SetEnabledExtensionImes(std::vector<std::string>* ids) override;
    void SetInputMethodLoginDefault() override;
    void SetInputMethodLoginDefaultFromVPD(const std::string& locale,
                                           const std::string& layout) override;
    void SwitchToNextInputMethod() override;
    void SwitchToLastUsedInputMethod() override;
    InputMethodDescriptor GetCurrentInputMethod() const override;
    bool ReplaceEnabledInputMethods(
        const std::vector<std::string>& new_active_input_method_ids) override;
    bool SetAllowedInputMethods(
        const std::vector<std::string>& new_allowed_input_method_ids,
        bool enable_allowed_input_methods) override;
    const std::vector<std::string>& GetAllowedInputMethods() override;
    void EnableInputView() override;
    void DisableInputView() override;
    const GURL& GetInputViewUrl() const override;

    // The active input method ids cache (actually default only)
    std::vector<std::string> active_input_method_ids;

   protected:
    friend base::RefCounted<InputMethodManager::State>;
    ~State() override;

   private:
    // Allowed input methods ids
    std::vector<std::string> allowed_input_method_ids_;

    DISALLOW_COPY_AND_ASSIGN(State);
  };

  MockInputMethodManager();
  ~MockInputMethodManager() override;

  // InputMethodManager:
  UISessionState GetUISessionState() override;
  void AddObserver(InputMethodManager::Observer* observer) override;
  void AddCandidateWindowObserver(
      InputMethodManager::CandidateWindowObserver* observer) override;
  void AddImeMenuObserver(
      InputMethodManager::ImeMenuObserver* observer) override;
  void RemoveObserver(InputMethodManager::Observer* observer) override;
  void RemoveCandidateWindowObserver(
      InputMethodManager::CandidateWindowObserver* observer) override;
  void RemoveImeMenuObserver(
      InputMethodManager::ImeMenuObserver* observer) override;
  std::unique_ptr<InputMethodDescriptors> GetSupportedInputMethods()
      const override;
  void ActivateInputMethodMenuItem(const std::string& key) override;
  void ConnectInputEngineManager(
      mojo::PendingReceiver<chromeos::ime::mojom::InputEngineManager> receiver)
      override;
  bool IsISOLevel5ShiftUsedByCurrentInputMethod() const override;
  bool IsAltGrUsedByCurrentInputMethod() const override;
  ImeKeyboard* GetImeKeyboard() override;
  InputMethodUtil* GetInputMethodUtil() override;
  ComponentExtensionIMEManager* GetComponentExtensionIMEManager() override;
  bool IsLoginKeyboard(const std::string& layout) const override;
  bool MigrateInputMethods(std::vector<std::string>* input_method_ids) override;
  scoped_refptr<InputMethodManager::State> CreateNewState(
      Profile* profile) override;
  scoped_refptr<InputMethodManager::State> GetActiveIMEState() override;
  void SetState(scoped_refptr<InputMethodManager::State> state) override;
  void ImeMenuActivationChanged(bool is_active) override;
  void NotifyImeMenuItemsChanged(
      const std::string& engine_id,
      const std::vector<InputMethodManager::MenuItem>& items) override;
  void MaybeNotifyImeMenuActivationChanged() override;
  void OverrideKeyboardKeyset(mojom::ImeKeyset keyset) override;
  void SetImeMenuFeatureEnabled(ImeMenuFeature feature, bool enabled) override;
  bool GetImeMenuFeatureEnabled(ImeMenuFeature feature) const override;
  void NotifyObserversImeExtraInputStateChange() override;
  ui::InputMethodKeyboardController* GetInputMethodKeyboardController()
      override;
  void NotifyInputMethodExtensionAdded(
      const std::string& extension_id) override;
  void NotifyInputMethodExtensionRemoved(
      const std::string& extension_id) override;

  // ui::InputMethodKeyboardController overrides.
  bool DisplayVirtualKeyboard() override;
  void DismissVirtualKeyboard() override;
  void AddObserver(
      ui::InputMethodKeyboardControllerObserver* observer) override;
  void RemoveObserver(
      ui::InputMethodKeyboardControllerObserver* observer) override;
  bool IsKeyboardVisible() override;

 private:
  uint32_t features_enabled_state_;

  DISALLOW_COPY_AND_ASSIGN(MockInputMethodManager);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // UI_BASE_IME_CHROMEOS_MOCK_INPUT_METHOD_MANAGER_H_
