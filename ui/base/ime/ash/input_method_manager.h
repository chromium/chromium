// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_INPUT_METHOD_MANAGER_H_
#define UI_BASE_IME_ASH_INPUT_METHOD_MANAGER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/memory/ref_counted.h"
#include "chromeos/ash/services/ime/public/mojom/ime_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/base/ime/ash/ime_keyset.h"
#include "ui/base/ime/ash/input_method_descriptor.h"

class Profile;

namespace ash {

class ComponentExtensionIMEManager;
class TextInputMethod;

namespace input_method {

class InputMethodUtil;
class ImeKeyboard;

// This class manages input methods handles. Classes can add themselves as
// observers. Clients can get an instance of this library class by:
// InputMethodManager::Get().
class COMPONENT_EXPORT(UI_BASE_IME_ASH) InputMethodManager {
 public:
  enum class UIStyle {
    kLogin,
    kSecondaryLogin,
    kLock,
    kNormal,
  };

  enum MenuItemStyle {
    MENU_ITEM_STYLE_NONE,
    MENU_ITEM_STYLE_CHECK,
    MENU_ITEM_STYLE_RADIO,
    MENU_ITEM_STYLE_SEPARATOR,
  };

  struct MenuItem {
    MenuItem();
    MenuItem(const MenuItem& other);
    virtual ~MenuItem();

    std::string id;
    std::string label;
    MenuItemStyle style;
    bool visible;
    bool enabled;
    bool checked;

    unsigned int modified;
  };

  enum ImeMenuFeature {
    FEATURE_EMOJI = 1 << 0,
    FEATURE_HANDWRITING = 1 << 1,
    FEATURE_VOICE = 1 << 2,
    FEATURE_ALL = ~0,
  };

  class Observer {
   public:
    virtual ~Observer() = default;
    // Called when the current input method is changed.  |show_message|
    // indicates whether the user should be notified of this change.
    virtual void InputMethodChanged(InputMethodManager* manager,
                                    Profile* profile,
                                    bool show_message) = 0;

    // Called when the availability of any of the extra input methods (emoji,
    // handwriting, voice) has changed. The overall state is toggle-able
    // independently of the individual options.
    virtual void OnExtraInputEnabledStateChange(
        bool is_extra_input_options_enabled,
        bool is_emoji_enabled,
        bool is_handwriting_enabled,
        bool is_voice_enabled) {}

    // Called when an input method extension is added or removed.
    virtual void OnInputMethodExtensionAdded(const std::string& extension_id) {}
    virtual void OnInputMethodExtensionRemoved(
        const std::string& extension_id) {}
  };

  // CandidateWindowObserver is notified of events related to the candidate
  // window.  The "suggestion window" used by IMEs such as ibus-mozc does not
  // count as the candidate window (this may change if we later want suggestion
  // window events as well).  These events also won't occur when the virtual
  // keyboard is used, since it controls its own candidate window.
  class CandidateWindowObserver {
   public:
    virtual ~CandidateWindowObserver() = default;
    // Called when the candidate window is opened.
    virtual void CandidateWindowOpened(InputMethodManager* manager) = 0;
    // Called when the candidate window is closed.
    virtual void CandidateWindowClosed(InputMethodManager* manager) = 0;
  };

  // ImeMenuObserver is notified of events related to the IME menu on the shelf
  // bar.
  class ImeMenuObserver {
   public:
    ImeMenuObserver& operator=(const ImeMenuObserver&) = delete;

    virtual ~ImeMenuObserver() = default;

    // Called when the IME menu is activated or deactivated.
    virtual void ImeMenuActivationChanged(bool is_active) = 0;
    // Called when the current input method or the list of enabled input method
    // IDs is changed.
    virtual void ImeMenuListChanged() = 0;
    // Called when the input.ime.setMenuItems or input.ime.updateMenuItems API
    // is called.
    virtual void ImeMenuItemsChanged(const std::string& engine_id,
                                     const std::vector<MenuItem>& items) = 0;
  };

  class State : public base::RefCounted<InputMethodManager::State> {
   public:
    // Returns a copy of state.
    virtual scoped_refptr<State> Clone() const = 0;

    // Adds an input method extension. This function does not takes ownership of
    // |instance|.
    virtual void AddInputMethodExtension(
        const std::string& extension_id,
        const InputMethodDescriptors& descriptors,
        TextInputMethod* instance) = 0;

    // Removes an input method extension.
    virtual void RemoveInputMethodExtension(
        const std::string& extension_id) = 0;

    // Changes the current (active) input method to |input_method_id|. If
    // |input_method_id| is not enabled, switch to the first one in the enabled
    // input method list.
    virtual void ChangeInputMethod(const std::string& input_method_id,
                                   bool show_message) = 0;

    // Switching the input methods for JP106 language input keys.
    virtual void ChangeInputMethodToJpKeyboard() = 0;
    virtual void ChangeInputMethodToJpIme() = 0;
    virtual void ToggleInputMethodForJpIme() = 0;

    // Adds one entry to the list of enabled input method IDs, and then starts
    // or stops the system input method framework as needed.
    virtual bool EnableInputMethod(
        const std::string& new_enabled_input_method_id) = 0;

    // Enables "login" keyboard layouts (e.g. US Qwerty, US Dvorak, French
    // Azerty) that are necessary for the |language_code| and then switches to
    // |initial_layouts| if the given list is not empty. For example, if
    // |language_code| is "en-US", US Qwerty, US International, US Extended, US
    // Dvorak, and US Colemak layouts would be enabled. Likewise, for Germany
    // locale, US Qwerty which corresponds to the hardware keyboard layout and
    // several keyboard layouts for Germany would be enabled.
    // Only layouts suitable for login screen are enabled.
    virtual void EnableLoginLayouts(
        const std::string& language_code,
        const std::vector<std::string>& initial_layouts) = 0;

    // Filters current state layouts and leaves only suitable for lock screen.
    virtual void DisableNonLockScreenLayouts() = 0;

    // Returns a list of descriptors for all Input Method Extensions.
    virtual void GetInputMethodExtensions(InputMethodDescriptors* result) = 0;

    // Returns the list of enabled input methods, including extension input
    // methods, sorted in ascending order of their localized full display names,
    // according to the lexicographical order defined by the current system
    // locale (aka. display language).
    virtual InputMethodDescriptors
    GetEnabledInputMethodsSortedByLocalizedDisplayNames() const = 0;

    // Returns enabled input methods, including extension input methods.
    // Although presented as a list, the result is NOT sorted in any specific
    // order; the ordering is arbitrary and undefined.
    virtual InputMethodDescriptors GetEnabledInputMethods() const = 0;

    // Returns IDs of enabled input methods, including extension input methods.
    // Although presented as a list, the result is NOT sorted in any specific
    // order; the ordering is arbitrary and undefined.
    virtual const std::vector<std::string>& GetEnabledInputMethodIds()
        const = 0;

    // Returns the number of enabled input methods including extension input
    // methods.
    virtual size_t GetNumEnabledInputMethods() const = 0;

    // Returns the input method descriptor from the given input method id
    // string.
    // If the given input method id is invalid, returns nullptr.
    virtual const InputMethodDescriptor* GetInputMethodFromId(
        const std::string& input_method_id) const = 0;

    // Sets the list of extension IME ids which should be enabled.
    virtual void SetEnabledExtensionImes(base::span<const std::string> ids) = 0;

    // Sets current input method to login default (first owners, then hardware).
    virtual void SetInputMethodLoginDefault() = 0;

    // Sets current input method to login default with the given locale and
    // layout info from VPD.
    virtual void SetInputMethodLoginDefaultFromVPD(
        const std::string& locale,
        const std::string& layout) = 0;

    // Switches the current input method to the next one on the list of enabled
    // input methods sorted in ascending order of their localized full display
    // names, according to the lexicographical order defined by the current
    // system locale. In other words, "next" is based on the list returned by
    // |GetEnabledInputMethodsSortedByLocalizedDisplayNames()|.
    virtual void SwitchToNextInputMethod() = 0;

    // Switches the current input method to the last used one.
    virtual void SwitchToLastUsedInputMethod() = 0;

    // Gets the descriptor of the input method which is currently selected.
    virtual InputMethodDescriptor GetCurrentInputMethod() const = 0;

    // Updates the list of enabled input method IDs (checking that they are
    // valid and allowed by policy), and then starts or stops the system input
    // method framework as needed.
    virtual bool ReplaceEnabledInputMethods(
        const std::vector<std::string>& new_enabled_input_method_ids) = 0;

    // Sets the currently allowed input methods due to policy. Invalid
    // input method ids are ignored. Passing an empty vector means that all
    // input methods are allowed, which is the default.
    // Automatically enables allowed methods in Kiosk sessions if the vector is
    // non-empty.
    virtual bool SetAllowedInputMethods(
        const std::vector<std::string>& allowed_input_method_ids) = 0;

    // Returns IDs of currently allowed input methods, as set by
    // `SetAllowedInputMethods()`. An empty vector means that all input methods
    // are allowed.
    virtual const std::vector<std::string>& GetAllowedInputMethodIds()
        const = 0;

    // Returns the first hardware input method that is allowed or the first
    // allowed input method, if no hardware input method is allowed.
    virtual std::string GetAllowedFallBackKeyboardLayout() const = 0;

    // Methods related to custom input view of the input method.
    // Enables custom input view of the current (active) input method.
    virtual void EnableInputView() = 0;
    // Disables custom input view of the current (active) input method.
    // The fallback system input view will be used.
    virtual void DisableInputView() = 0;
    // Returns the URL of the input view of the current (active) input method.
    virtual const GURL& GetInputViewUrl() const = 0;

    // Get the current UI screen type (e.g. login screen, lock screen, etc.).
    virtual InputMethodManager::UIStyle GetUIStyle() const = 0;
    virtual void SetUIStyle(InputMethodManager::UIStyle ui_style) = 0;

   protected:
    friend base::RefCounted<InputMethodManager::State>;

    virtual ~State();
  };

  virtual ~InputMethodManager() = default;

  // Gets the global instance of InputMethodManager. Initialize() must be called
  // first.
  // TODO(crbug/1279743): This is a stateful global. Make it into true global
  // singleton first, then use dependency injection instead in the next step.
  static COMPONENT_EXPORT(UI_BASE_IME_ASH) InputMethodManager* Get();

  // Sets the global instance. |instance| will be owned by the internal pointer
  // and deleted by Shutdown().
  // TODO(nona): Instanciate InputMethodManagerImpl inside of this function once
  //             crbug.com/164375 is fixed.
  // TODO(crbug/1279743): This is a stateful global. Make it into true global
  // singleton first, then use dependency injection instead in the next step.
  static COMPONENT_EXPORT(UI_BASE_IME_ASH) void Initialize(
      InputMethodManager* instance);

  // Destroy the global instance.
  // TODO(crbug/1279743): This is a stateful global. Make it into true global
  // singleton first, then use dependency injection instead in the next step.
  static COMPONENT_EXPORT(UI_BASE_IME_ASH) void Shutdown();

  // Adds an observer to receive notifications of input method related
  // changes as desribed in the Observer class above.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void AddCandidateWindowObserver(
      CandidateWindowObserver* observer) = 0;
  virtual void AddImeMenuObserver(ImeMenuObserver* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual void RemoveCandidateWindowObserver(
      CandidateWindowObserver* observer) = 0;
  virtual void RemoveImeMenuObserver(ImeMenuObserver* observer) = 0;

  // Activates the input method property specified by the |key|.
  virtual void ActivateInputMethodMenuItem(const std::string& key) = 0;

  // Connects a receiver to the InputEngineManager instance.
  virtual void ConnectInputEngineManager(
      mojo::PendingReceiver<ime::mojom::InputEngineManager> receiver) = 0;

  // Connects a receiver to the InputMethodUserDataService instance.
  virtual void BindInputMethodUserDataService(
      mojo::PendingReceiver<ime::mojom::InputMethodUserDataService>
          receiver) = 0;

  virtual bool IsISOLevel5ShiftUsedByCurrentInputMethod() const = 0;

  virtual bool IsAltGrUsedByCurrentInputMethod() const = 0;

  // Returns true if the current input method uses position based shortcuts.
  // This is true for most layouts, with the exception of layouts that have
  // non-standard locations for punctuation such as dvorak. See
  // crbug.com/1174326 for more information.
  virtual bool ArePositionalShortcutsUsedByCurrentInputMethod() const = 0;

  // Returns an X keyboard object which could be used to change the current XKB
  // layout, change the caps lock status, and set the auto repeat rate/interval.
  virtual ImeKeyboard* GetImeKeyboard() = 0;

  // Returns an InputMethodUtil object.
  virtual InputMethodUtil* GetInputMethodUtil() = 0;

  // Returns a ComponentExtentionIMEManager object.
  virtual ComponentExtensionIMEManager* GetComponentExtensionIMEManager() = 0;

  // If keyboard layout can be uset at login screen
  virtual bool IsLoginKeyboard(const std::string& layout) const = 0;

  // Returns an extension-based input method id if |input_method_id| is a valid
  // engine id. Otherwise, returns |input_method_id|.
  virtual std::string GetMigratedInputMethodID(
      const std::string& input_method_id) = 0;

  // Replaces the input list with the extension-based input method ids for valid
  // engine ids in the input list. Returns true if the given input method id
  // list is modified, returns false otherwise.
  virtual bool GetMigratedInputMethodIDs(
      std::vector<std::string>* input_method_ids) = 0;

  // Returns new empty state for the |profile|.
  virtual scoped_refptr<State> CreateNewState(Profile* profile) = 0;

  // Returns active state.
  virtual scoped_refptr<InputMethodManager::State> GetActiveIMEState() = 0;

  // Replaces active state.
  virtual void SetState(scoped_refptr<State> state) = 0;

  // Activates or deactivates the IME Menu.
  virtual void ImeMenuActivationChanged(bool is_active) = 0;

  // Notifies the input.ime.setMenuItems or input.ime.updateMenuItems API is
  // called to update the IME menu items.
  virtual void NotifyImeMenuItemsChanged(
      const std::string& engine_id,
      const std::vector<MenuItem>& items) = 0;

  // Notify the IME menu activation changed if the current profile's activation
  // is different from previous.
  virtual void MaybeNotifyImeMenuActivationChanged() = 0;

  // Overrides active keyset with the given keyset if the current (active) IME
  // supports the given keyset.
  virtual void OverrideKeyboardKeyset(ImeKeyset keyset) = 0;

  // Enables or disables some advanced features, e.g. handwiring, voices input.
  virtual void SetImeMenuFeatureEnabled(ImeMenuFeature feature,
                                        bool enabled) = 0;

  // Returns the true if the given feature is enabled.
  virtual bool GetImeMenuFeatureEnabled(ImeMenuFeature feature) const = 0;

  // Notifies when any of the extra inputs (emoji, handwriting, voice) enabled
  // status has changed.
  virtual void NotifyObserversImeExtraInputStateChange() = 0;

  // Notifies an input method extension is added or removed.
  virtual void NotifyInputMethodExtensionAdded(
      const std::string& extension_id) = 0;
  virtual void NotifyInputMethodExtensionRemoved(
      const std::string& extension_id) = 0;
};

}  // namespace input_method
}  // namespace ash

#endif  // UI_BASE_IME_ASH_INPUT_METHOD_MANAGER_H_
