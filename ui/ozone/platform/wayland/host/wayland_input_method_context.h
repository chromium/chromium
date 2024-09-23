// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_INPUT_METHOD_CONTEXT_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_INPUT_METHOD_CONTEXT_H_

#include <memory>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/nix/xdg_util.h"
#include "ui/base/ime/character_composer.h"
#include "ui/base/ime/linux/linux_input_method_context.h"
#include "ui/base/ime/surrounding_text_tracker.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/virtual_keyboard_controller.h"
#include "ui/gfx/range/range.h"
#include "ui/ozone/platform/wayland/host/wayland_keyboard.h"
#include "ui/ozone/platform/wayland/host/wayland_window_observer.h"
#include "ui/ozone/platform/wayland/host/zwp_text_input_wrapper.h"

namespace ui {

class WaylandConnection;

class WaylandInputMethodContext : public LinuxInputMethodContext,
                                  public VirtualKeyboardController,
                                  public WaylandWindowObserver,
                                  public ZWPTextInputWrapperClient {
 public:
  class Delegate;

  WaylandInputMethodContext(WaylandConnection* connection,
                            WaylandKeyboard::Delegate* key_delegate,
                            LinuxInputMethodContextDelegate* ime_delegate);
  WaylandInputMethodContext(const WaylandInputMethodContext&) = delete;
  WaylandInputMethodContext& operator=(const WaylandInputMethodContext&) =
      delete;
  ~WaylandInputMethodContext() override;

  void Init(bool initialize_for_testing = false,
            std::unique_ptr<ZWPTextInputWrapper> wrapper_for_testing = nullptr,
            std::optional<base::nix::DesktopEnvironment> desktop_for_testing =
                std::nullopt);

  // LinuxInputMethodContext overrides:
  bool DispatchKeyEvent(const KeyEvent& key_event) override;
  // Returns true if this event comes from extended_keyboard::peek_key.
  // See also WaylandEventSource::OnKeyboardKeyEvent about how the flag is set.
  bool IsPeekKeyEvent(const KeyEvent& key_event) override;
  void SetCursorLocation(const gfx::Rect& rect) override;
  void SetSurroundingText(
      const std::u16string& text,
      const gfx::Range& text_range,
      const gfx::Range& composition_range,
      const gfx::Range& selection_range,
      const std::optional<GrammarFragment>& fragment,
      const std::optional<AutocorrectInfo>& autocorrect) override;
  void WillUpdateFocus(TextInputClient* old_client,
                       TextInputClient* new_client) override;
  void UpdateFocus(bool has_client,
                   TextInputType old_type,
                   const TextInputClientAttributes& new_client_attributes,
                   TextInputClient::FocusReason reason) override;
  void Reset() override;
  VirtualKeyboardController* GetVirtualKeyboardController() override;

  // VirtualKeyboardController overrides:
  bool DisplayVirtualKeyboard() override;
  void DismissVirtualKeyboard() override;
  void AddObserver(VirtualKeyboardControllerObserver* observer) override;
  void RemoveObserver(VirtualKeyboardControllerObserver* observer) override;
  bool IsKeyboardVisible() override;

  // WaylandWindowObserver overrides:
  void OnKeyboardFocusedWindowChanged() override;

  // ZWPTextInputWrapperClient overrides:
  void OnPreeditString(std::string_view text,
                       const std::vector<SpanStyle>& spans,
                       const gfx::Range& preedit_cursor) override;
  void OnCommitString(std::string_view text) override;
  void OnCursorPosition(int32_t index, int32_t anchor) override;
  void OnDeleteSurroundingText(int32_t index, uint32_t length) override;
  void OnKeysym(uint32_t keysym,
                uint32_t state,
                uint32_t modifiers,
                uint32_t time) override;
  void OnSetPreeditRegion(int32_t index,
                          uint32_t length,
                          const std::vector<SpanStyle>& spans) override;
  void OnClearGrammarFragments(const gfx::Range& range) override;
  void OnAddGrammarFragment(const GrammarFragment& fragment) override;
  void OnSetAutocorrectRange(const gfx::Range& range) override;
  void OnSetVirtualKeyboardOccludedBounds(
      const gfx::Rect& screen_bounds) override;
  void OnConfirmPreedit(bool keep_selection) override;
  void OnInputPanelState(uint32_t state) override;
  void OnModifiersMap(std::vector<std::string> modifiers_map) override;
  void OnInsertImage(const GURL& src) override;

  const SurroundingTextTracker::State& predicted_state_for_testing() const {
    return surrounding_text_tracker_.predicted_state();
  }

 private:
  void CreateTextInputWrapper();
  void Focus(bool skip_virtual_keyboard_update,
             TextInputClient::FocusReason reason);
  void Blur(bool skip_virtual_keyboard_update);
  void UpdatePreeditText(const std::u16string& preedit_text);
  // If |skip_virtual_keyboard_update| is true, no virtual keyboard show/hide
  // requests will be sent. This is used to prevent flickering the virtual
  // keyboard when it would be immediately reshown anyway, e.g. when changing
  // focus from one text input to another.
  void MaybeUpdateActivated(bool skip_virtual_keyboard_update,
                            TextInputClient::FocusReason reason);

  const raw_ptr<WaylandConnection>
      connection_;  // TODO(jani) Handle this better

  // Delegate key events to be injected into PlatformEvent system.
  const raw_ptr<WaylandKeyboard::Delegate> key_delegate_;

  // Delegate IME-specific events to be handled by //ui code.
  const raw_ptr<LinuxInputMethodContextDelegate> ime_delegate_;

  std::unique_ptr<ZWPTextInputWrapper> text_input_;

  // Tracks whether InputMethod in Chrome has some focus.
  bool focused_ = false;

  // Tracks whether a request to activate InputMethod is sent to wayland
  // compositor.
  bool activated_ = false;

  // Cache of current TextInputClient's attributes.
  TextInputClientAttributes attributes_;

  // An object to compose a character from a sequence of key presses
  // including dead key etc.
  CharacterComposer character_composer_;

  // Stores the parameters required for OnDeleteSurroundingText.
  // The index moved by SetSurroundingText due to the maximum length of wayland
  // messages limitation. It is specified as 4000 bytes in the protocol spec of
  // text-input-unstable-v3.
  // This is byte-offset in UTF8 form.
  size_t surrounding_text_offset_ = 0;

  // Tracks the surrounding text. Surrounding text and its selection is NOT
  // trimmed by the wayland message size limitation in SurroundingTextTracker.
  SurroundingTextTracker surrounding_text_tracker_;

  // Whether the next CommitString should be treated as part of a
  // ConfirmCompositionText operation which keeps the current selection. This
  // allows ConfirmCompositionText to be implemented as an atomic operation
  // using CursorPosition and CommitString.
  bool pending_keep_selection_ = false;

  // Caches VirtualKeyboard visibility.
  bool virtual_keyboard_visible_ = false;

  // Cached desktop environment obtained from env.
  base::nix::DesktopEnvironment desktop_environment_;

  // Stores whether an invalid cursor end is sent by compositor, in which
  // case cursor end values should be ignored in preedit string.
  bool compositor_sends_invalid_cursor_end_ = false;

  // Keeps track of past text input clients to forward virtual keyboard changes
  // to, since unfocusing text inputs will detach clients immediately, but the
  // virtual keyboard bounds updates come later.
  // Using map instead of set, because WeakPtr doesn't support comparison.
  base::flat_map<TextInputClient*, base::WeakPtr<TextInputClient>>
      past_clients_;

  // Keeps modifiers_map sent from the wayland compositor.
  std::vector<std::string> modifiers_map_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_INPUT_METHOD_CONTEXT_H_
