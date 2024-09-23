// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_MOCK_INPUT_METHOD_MANAGER_H_
#define UI_BASE_IME_ASH_MOCK_INPUT_METHOD_MANAGER_H_

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/observer_list.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/virtual_keyboard_controller.h"
#include "ui/base/ime/virtual_keyboard_controller_observer.h"

namespace ash {
namespace input_method {

class ImeKeyboard;

// The mock InputMethodManager for testing.
class COMPONENT_EXPORT(UI_BASE_IME_ASH) MockInputMethodManager
    : public InputMethodManager {
 public:
 public:
  class State : public InputMethodManager::State {
   public:
    State();

    State(const State&) = delete;
    State& operator=(const State&) = delete;

    scoped_refptr<InputMethodManager::State> Clone() const override;
    void AddInputMethodExtension(const std::string& extension_id,
                                 const InputMethodDescriptors& descriptors,
                                 TextInputMethod* instance) override;
    void RemoveInputMethodExtension(const std::string& extension_id) override;
    void ChangeInputMethod(const std::string& input_method_id,
                           bool show_message) override;
    void ChangeInputMethodToJpKeyboard() override;
    void ChangeInputMethodToJpIme() override;
    void ToggleInputMethodForJpIme() override;
    bool EnableInputMethod(
        const std::string& new_enabled_input_method_id) override;
    void EnableLoginLayouts(
        const std::string& language_code,
        const std::vector<std::string>& initial_layouts) override;
    void DisableNonLockScreenLayouts() override;
    void GetInputMethodExtensions(InputMethodDescriptors* result) override;
    InputMethodDescriptors GetEnabledInputMethodsSortedByLocalizedDisplayNames()
        const override;
    InputMethodDescriptors GetEnabledInputMethods() const override;
    const std::vector<std::string>& GetEnabledInputMethodIds() const override;
    const InputMethodDescriptor* GetInputMethodFromId(
        const std::string& input_method_id) const override;
    size_t GetNumEnabledInputMethods() const override;
    void SetEnabledExtensionImes(base::span<const std::string> ids) override;
    void SetInputMethodLoginDefault() override;
    void SetInputMethodLoginDefaultFromVPD(const std::string& locale,
                                           const std::string& layout) override;
    void SwitchToNextInputMethod() override;
    void SwitchToLastUsedInputMethod() override;
    InputMethodDescriptor GetCurrentInputMethod() const override;
    bool ReplaceEnabledInputMethods(
        const std::vector<std::string>& new_enabled_input_method_ids) override;
    bool SetAllowedInputMethods(
        const std::vector<std::string>& new_allowed_input_method_ids) override;
    const std::vector<std::string>& GetAllowedInputMethodIds() const override;
    std::string GetAllowedFallBackKeyboardLayout() const override;
    void EnableInputView() override;
    void DisableInputView() override;
    const GURL& GetInputViewUrl() const override;
    InputMethodManager::UIStyle GetUIStyle() const override;
    void SetUIStyle(InputMethodManager::UIStyle ui_style) override;

    // The enabled input method ids cache (actually default only)
    std::vector<std::string> enabled_input_method_ids;

   protected:
    friend base::RefCounted<InputMethodManager::State>;
    ~State() override;

   private:
    // Allowed input methods ids
    std::vector<std::string> allowed_input_method_ids_;

    InputMethodManager::UIStyle ui_style_ =
        InputMethodManager::UIStyle::kNormal;
  };

  MockInputMethodManager();

  MockInputMethodManager(const MockInputMethodManager&) = delete;
  MockInputMethodManager& operator=(const MockInputMethodManager&) = delete;

  ~MockInputMethodManager() override;

  // InputMethodManager:
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
  void ActivateInputMethodMenuItem(const std::string& key) override;
  void ConnectInputEngineManager(
      mojo::PendingReceiver<ime::mojom::InputEngineManager> receiver) override;
  void BindInputMethodUserDataService(
      mojo::PendingReceiver<ime::mojom::InputMethodUserDataService> receiver)
      override;
  bool IsISOLevel5ShiftUsedByCurrentInputMethod() const override;
  bool IsAltGrUsedByCurrentInputMethod() const override;
  bool ArePositionalShortcutsUsedByCurrentInputMethod() const override;
  ImeKeyboard* GetImeKeyboard() override;
  InputMethodUtil* GetInputMethodUtil() override;
  ComponentExtensionIMEManager* GetComponentExtensionIMEManager() override;
  bool IsLoginKeyboard(const std::string& layout) const override;
  std::string GetMigratedInputMethodID(
      const std::string& input_method_id) override;
  bool GetMigratedInputMethodIDs(
      std::vector<std::string>* input_method_ids) override;
  scoped_refptr<InputMethodManager::State> CreateNewState(
      Profile* profile) override;
  scoped_refptr<InputMethodManager::State> GetActiveIMEState() override;
  void SetState(scoped_refptr<InputMethodManager::State> state) override;
  void ImeMenuActivationChanged(bool is_active) override;
  void NotifyImeMenuItemsChanged(
      const std::string& engine_id,
      const std::vector<InputMethodManager::MenuItem>& items) override;
  void MaybeNotifyImeMenuActivationChanged() override;
  void OverrideKeyboardKeyset(ImeKeyset keyset) override;
  void SetImeMenuFeatureEnabled(ImeMenuFeature feature, bool enabled) override;
  bool GetImeMenuFeatureEnabled(ImeMenuFeature feature) const override;
  void NotifyObserversImeExtraInputStateChange() override;
  void NotifyInputMethodExtensionAdded(
      const std::string& extension_id) override;
  void NotifyInputMethodExtensionRemoved(
      const std::string& extension_id) override;

 private:
  scoped_refptr<State> state_;
  uint32_t features_enabled_state_;
};

}  // namespace input_method
}  // namespace ash

#endif  // UI_BASE_IME_ASH_MOCK_INPUT_METHOD_MANAGER_H_
