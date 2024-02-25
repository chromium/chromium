// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_MOCK_INPUT_METHOD_MANAGER_IMPL_H_
#define UI_BASE_IME_ASH_MOCK_INPUT_METHOD_MANAGER_IMPL_H_

#include <stddef.h>

#include "base/memory/raw_ptr.h"
#include "ui/base/ime/ash/component_extension_ime_manager.h"
#include "ui/base/ime/ash/fake_ime_keyboard.h"
#include "ui/base/ime/ash/fake_input_method_delegate.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/mock_input_method_manager.h"

namespace ash {
namespace input_method {

// The mock implementation of InputMethodManager for testing.
class MockInputMethodManagerImpl : public MockInputMethodManager {
 public:
  class State : public MockInputMethodManager::State {
   public:
    explicit State(MockInputMethodManagerImpl* manager);

    State(const State&) = delete;
    State& operator=(const State&) = delete;

    // MockInputMethodManager::State:
    scoped_refptr<InputMethodManager::State> Clone() const override;
    InputMethodDescriptors GetEnabledInputMethodsSortedByLocalizedDisplayNames()
        const override;
    InputMethodDescriptors GetEnabledInputMethods() const override;
    const InputMethodDescriptor* GetInputMethodFromId(
        const std::string& input_method_id) const override;
    InputMethodDescriptor GetCurrentInputMethod() const override;

    // The value GetCurrentInputMethod().id() will return.
    std::string current_input_method_id;

   protected:
    friend base::RefCounted<InputMethodManager::State>;
    ~State() override;

   private:
    const raw_ptr<MockInputMethodManager> manager_;
  };

  MockInputMethodManagerImpl();

  MockInputMethodManagerImpl(const MockInputMethodManagerImpl&) = delete;
  MockInputMethodManagerImpl& operator=(const MockInputMethodManagerImpl&) =
      delete;

  ~MockInputMethodManagerImpl() override;

  // MockInputMethodManager:
  void AddObserver(InputMethodManager::Observer* observer) override;
  void AddImeMenuObserver(ImeMenuObserver* observer) override;
  void RemoveObserver(InputMethodManager::Observer* observer) override;
  void RemoveImeMenuObserver(ImeMenuObserver* observer) override;
  bool IsISOLevel5ShiftUsedByCurrentInputMethod() const override;
  ImeKeyboard* GetImeKeyboard() override;
  InputMethodUtil* GetInputMethodUtil() override;
  ComponentExtensionIMEManager* GetComponentExtensionIMEManager() override;
  scoped_refptr<InputMethodManager::State> CreateNewState(
      Profile* profile) override;
  scoped_refptr<InputMethodManager::State> GetActiveIMEState() override;
  void SetState(scoped_refptr<InputMethodManager::State> state) override;

  // Sets an input method ID which will be returned by GetCurrentInputMethod().
  void SetCurrentInputMethodId(const std::string& input_method_id);

  void SetComponentExtensionIMEManager(
      std::unique_ptr<ComponentExtensionIMEManager> comp_ime_manager);

  // Set the value returned by IsISOLevel5ShiftUsedByCurrentInputMethod
  void set_mod3_used(bool value) { mod3_used_ = value; }

  int add_observer_count() const { return add_observer_count_; }
  int remove_observer_count() const { return remove_observer_count_; }

  int add_menu_observer_count() const { return add_menu_observer_count_; }
  int remove_menu_observer_count() const { return remove_menu_observer_count_; }

 protected:
  scoped_refptr<State> state_;

 private:
  // TODO(yusukes): Add more variables for counting the numbers of the API calls
  int add_observer_count_ = 0;
  int remove_observer_count_ = 0;
  int add_menu_observer_count_ = 0;
  int remove_menu_observer_count_ = 0;
  FakeInputMethodDelegate delegate_;  // used by util_
  std::unique_ptr<InputMethodUtil> util_;
  FakeImeKeyboard keyboard_;
  bool mod3_used_ = false;
  std::unique_ptr<ComponentExtensionIMEManager> comp_ime_manager_;
};

}  // namespace input_method
}  // namespace ash

#endif  // UI_BASE_IME_ASH_MOCK_INPUT_METHOD_MANAGER_IMPL_H_
