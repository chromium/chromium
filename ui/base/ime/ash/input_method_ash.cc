// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/ash/input_method_ash.h"

#include <stddef.h>

#include <algorithm>
#include <cstring>
#include <set>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/char_iterator.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/third_party/icu/icu_utf.h"
#include "base/time/default_clock.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/ime_keyboard.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/text_input_method.h"
#include "ui/base/ime/ash/typing_session_manager.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/constants.h"
#include "ui/base/ime/events.h"
#include "ui/base/ime/ime_key_event_dispatcher.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

namespace {

using ::ui::CompositionText;
using ::ui::TextInputClient;

template <typename T>
T ConvertTextInputFlagToEnum(int flags, int flag_on_value, int flag_off_value) {
  if (flags & flag_on_value) {
    return T::kEnabled;
  }
  if (flags & flag_off_value) {
    return T::kDisabled;
  }
  return T::kUnspecified;
}

AutocapitalizationMode ConvertAutocapitalizationMode(int flags) {
  if (flags & ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_NONE) {
    return AutocapitalizationMode::kNone;
  }
  if (flags & ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_CHARACTERS) {
    return AutocapitalizationMode::kCharacters;
  }
  if (flags & ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_WORDS) {
    return AutocapitalizationMode::kWords;
  }
  if (flags & ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_SENTENCES) {
    return AutocapitalizationMode::kSentences;
  }
  return AutocapitalizationMode::kUnspecified;
}

// Returns whether `url` refers to Terminal/crosh.
bool IsTerminalOrCrosh(const GURL& url) {
  return base::StartsWith(url.spec(), "chrome-untrusted://terminal") ||
         base::StartsWith(url.spec(), "chrome-untrusted://crosh");
}

}  // namespace

TextInputMethod* GetEngine() {
  auto* bridge = IMEBridge::Get();
  return bridge ? bridge->GetCurrentEngineHandler() : nullptr;
}

// InputMethodAsh implementation -----------------------------------------
InputMethodAsh::InputMethodAsh(
    ui::ImeKeyEventDispatcher* ime_key_event_dispatcher)
    : InputMethodBase(ime_key_event_dispatcher),
      typing_session_manager_(base::DefaultClock::GetInstance()) {
  ResetContext();
}

InputMethodAsh::~InputMethodAsh() {
  ConfirmComposition(/* reset_engine */ true);
  // We are dead, so we need to ask the client to stop relying on us.
  OnInputMethodChanged();

  if (IMEBridge::Get() && IMEBridge::Get()->GetInputContextHandler() == this) {
    IMEBridge::Get()->SetInputContextHandler(nullptr);
  }
  typing_session_manager_.EndAndRecordSession();
}

InputMethodAsh::PendingSetCompositionRange::PendingSetCompositionRange(
    const gfx::Range& range,
    const std::vector<ui::ImeTextSpan>& text_spans)
    : range(range), text_spans(text_spans) {}

InputMethodAsh::PendingSetCompositionRange::PendingSetCompositionRange(
    const PendingSetCompositionRange& other) = default;

InputMethodAsh::PendingSetCompositionRange::~PendingSetCompositionRange() =
    default;

InputMethodAsh::PendingAutocorrectRange::PendingAutocorrectRange(
    const gfx::Range& range,
    TextInputTarget::SetAutocorrectRangeDoneCallback callback)
    : range(range), callback(std::move(callback)) {}

InputMethodAsh::PendingAutocorrectRange::~PendingAutocorrectRange() = default;

ui::EventDispatchDetails InputMethodAsh::DispatchKeyEvent(ui::KeyEvent* event) {
  DCHECK(!(event->flags() & ui::EF_IS_SYNTHESIZED));

  // For OS_CHROMEOS build of Chrome running on Linux, the IME keyboard cannot
  // track the Caps Lock state by itself, so need to call SetCapsLockEnabled()
  // method to reflect the Caps Lock state by the key event.
  auto* manager = input_method::InputMethodManager::Get();
  if (manager) {
    input_method::ImeKeyboard* keyboard = manager->GetImeKeyboard();
    if (keyboard && event->type() == ui::EventType::kKeyPressed &&
        event->key_code() != ui::VKEY_CAPITAL &&
        keyboard->IsCapsLockEnabled() != event->IsCapsLockOn()) {
      // Synchronize the keyboard state with event's state if they do not
      // match. Do not synchronize for Caps Lock key because it is already
      // handled in event rewriter.
      keyboard->SetCapsLockEnabled(event->IsCapsLockOn());
    }

    // For JP106 language input keys, makes sure they can be passed to the app
    // so that the VDI web apps can be supported. See https://crbug.com/816341.
    // VKEY_CONVERT: Henkan key
    // VKEY_NONCONVERT: Muhenkan key
    // VKEY_DBE_SBCSCHAR/VKEY_DBE_DBCSCHAR: ZenkakuHankaku key
    input_method::InputMethodManager::State* state =
        manager->GetActiveIMEState().get();
    if (event->type() == ui::EventType::kKeyPressed && state) {
      bool language_input_key = true;
      switch (event->key_code()) {
        case ui::VKEY_CONVERT:
          state->ChangeInputMethodToJpIme();
          break;
        case ui::VKEY_NONCONVERT:
          state->ChangeInputMethodToJpKeyboard();
          break;
        case ui::VKEY_DBE_SBCSCHAR:
        case ui::VKEY_DBE_DBCSCHAR:
          state->ToggleInputMethodForJpIme();
          break;
        default:
          language_input_key = false;
          break;
      }
      if (language_input_key) {
        // Dispatches the event to app/blink.
        // TODO(shuchen): Eventually, the language input keys should be handed
        // over to the IME extension to process. And IMF can handle if the IME
        // extension didn't handle.
        return DispatchKeyEventPostIME(event);
      }
    }
  }

  // Simply forward the key event if there's no focused TextInputClient.
  // Dead keys cannot be supported in this case because composition and commit
  // are not supported.
  if (base::FeatureList::IsEnabled(
          features::kInputMethodDeadKeyFixForNoInputField) &&
      GetTextInputClient() == nullptr) {
    return DispatchKeyEventPostIME(event);
  }

  // If |context_| is not usable, then we can only dispatch the key event as is.
  // We only dispatch the key event to input method when the |context_| is an
  // normal input field (not a password field).
  // Note: We need to send the key event to ibus even if the |context_| is not
  // enabled, so that ibus can have a chance to enable the |context_|.
  if (IsPasswordOrNoneInputFieldFocused() || !GetEngine()) {
    if (event->type() == ui::EventType::kKeyPressed) {
      if (ExecuteCharacterComposer(*event)) {
        // Treating as PostIME event if character composer handles key event and
        // generates some IME event,
        return ProcessKeyEventPostIME(
            event, ui::ime::KeyEventHandledState::kHandledByIME,
            /* stopped_propagation */ true);
      }
      return ProcessUnfilteredKeyPressEvent(event);
    }
    return DispatchKeyEventPostIME(event);
  }

  // Resets previous event dispatch details before invoking IME engine's
  // `ProcessKeyEvent`. `ProcessKeyEventDone` sets its value when it
  // re-dispatches the key events. If `ProcessKeyEventDone` is invoked
  // synchronously by `ProcessKeyEvent`, `dispatch_details_` would have correct
  // dispatch details data to return. This is important because an `EventTarget`
  // could be destroyed under `ProcessKeyEventDone`.
  // See http://crbug.com/1392491.
  dispatch_details_.reset();

  handling_key_event_ = true;
  GetEngine()->ProcessKeyEvent(
      *event, base::BindOnce(&InputMethodAsh::ProcessKeyEventDone,
                             weak_ptr_factory_.GetWeakPtr(),
                             // Pass the ownership of the new copied event.
                             base::Owned(new ui::KeyEvent(*event))));
  return dispatch_details_.value_or(ui::EventDispatchDetails());
}

void InputMethodAsh::ProcessKeyEventDone(
    ui::KeyEvent* event,
    ui::ime::KeyEventHandledState handled_state) {
  DCHECK(event);
  bool is_handled_by_char_composer = false;
  if (event->type() == ui::EventType::kKeyPressed) {
    if (handled_state != ui::ime::KeyEventHandledState::kNotHandled) {
      // IME event has a priority to be handled, so that character composer
      // should be reset.
      character_composer_.Reset();
    } else {
      // If IME does not handle key event, passes keyevent to character composer
      // to be able to compose complex characters.
      is_handled_by_char_composer = ExecuteCharacterComposer(*event);

      if (!is_handled_by_char_composer &&
          !ui::KeycodeConverter::IsDomKeyForModifier(event->GetDomKey())) {
        // If the character composer didn't handle it either, then confirm any
        // composition text before forwarding the key event. We ignore modifier
        // keys because, for example, if the IME handles Shift+A, then we don't
        // want the Shift key to confirm the composition text. Only confirm the
        // composition text when the IME does not handle the full key combo.
        ConfirmComposition(/* reset_engine */ true);
      }
    }
  }
  if (event->type() == ui::EventType::kKeyPressed ||
      event->type() == ui::EventType::kKeyReleased) {
    ui::ime::KeyEventHandledState handled_state_to_process =
        is_handled_by_char_composer
            ? ui::ime::KeyEventHandledState::kHandledByIME
            : handled_state;
    dispatch_details_ = ProcessKeyEventPostIME(event, handled_state_to_process,
                                               /* stopped_propagation */ false);
  }
  handling_key_event_ = false;
}

void InputMethodAsh::OnTextInputTypeChanged(TextInputClient* client) {
  if (!IsTextInputClientFocused(client)) {
    return;
  }

  UpdateContextFocusState();

  TextInputMethod* engine = GetEngine();
  if (engine) {
    const TextInputMethod::InputContext context = GetInputContext();
    // When focused input client is not changed, a text input type change
    // should cause blur/focus events to engine. The focus in to or out from
    // password field should also notify engine.
    engine->Blur();
    engine->Focus(context);
  }

  OnCaretBoundsChanged(client);

  InputMethodBase::OnTextInputTypeChanged(client);
}

void InputMethodAsh::OnCaretBoundsChanged(const TextInputClient* client) {
  if (IsTextInputTypeNone() || !IsTextInputClientFocused(client)) {
    return;
  }

  NotifyTextInputCaretBoundsChanged(client);

  if (IsPasswordOrNoneInputFieldFocused()) {
    return;
  }

  // The current text input type should not be NONE if |context_| is focused.
  DCHECK(client == GetTextInputClient());
  DCHECK(!IsTextInputTypeNone());

  TextInputMethod* engine = GetEngine();
  if (engine) {
    engine->SetCaretBounds(client->GetCaretBounds());
  }

  IMECandidateWindowHandlerInterface* candidate_window =
      IMEBridge::Get()->GetCandidateWindowHandler();
  IMEAssistiveWindowHandlerInterface* assistive_window =
      IMEBridge::Get()->GetAssistiveWindowHandler();
  if (!candidate_window && !assistive_window) {
    return;
  }

  const gfx::Rect caret_rect = client->GetCaretBounds();

  gfx::Rect composition_bounds;
  if (client->HasCompositionText()) {
    client->GetCompositionCharacterBounds(0, &composition_bounds);
  }

  // Pepper doesn't support composition bounds, so fall back to caret bounds to
  // avoid a bad user experience (the IME window moved to upper left corner).
  if (composition_bounds.IsEmpty()) {
    composition_bounds = caret_rect;
  }
  if (candidate_window) {
    candidate_window->SetCursorAndCompositionBounds(caret_rect,
                                                    composition_bounds);
  }

  if (assistive_window) {
    Bounds bounds;
    bounds.caret = caret_rect;
    bounds.autocorrect = client->GetAutocorrectCharacterBounds();
    assistive_window->SetBounds(bounds);
  }

  gfx::Range text_range;
  gfx::Range selection_range;
  std::u16string surrounding_text;
  if (!client->GetTextRange(&text_range) ||
      !client->GetTextFromRange(text_range, &surrounding_text) ||
      !client->GetEditableSelectionRange(&selection_range)) {
    previous_surrounding_text_.clear();
    previous_selection_range_ = gfx::Range::InvalidRange();
    return;
  }

  if (previous_selection_range_ == selection_range &&
      previous_surrounding_text_ == surrounding_text) {
    return;
  }

  previous_selection_range_ = selection_range;
  previous_surrounding_text_ = surrounding_text;

  if (!selection_range.IsValid()) {
    // TODO(nona): Ideally selection_range should not be invalid.
    // TODO(nona): If javascript changes the focus on page loading, even (0,0)
    //             can not be obtained. Need investigation.
    return;
  }

  // Here SetSurroundingText accepts relative position of |surrounding_text|, so
  // we have to convert |selection_range| from node coordinates to
  // |surrounding_text| coordinates.
  if (GetEngine()) {
    // TODO(b/245020074): Handle the case where selection is before the offset.
    const uint32_t offset = text_range.start();
    DCHECK_GE(selection_range.start(), offset);
    DCHECK_GE(selection_range.end(), offset);
    const gfx::Range relative_selection_range(selection_range.start() - offset,
                                              selection_range.end() - offset);
    GetEngine()->SetSurroundingText(surrounding_text, relative_selection_range,
                                    offset);
  }
}

void InputMethodAsh::CancelComposition(const TextInputClient* client) {
  if (!IsPasswordOrNoneInputFieldFocused() &&
      IsTextInputClientFocused(client)) {
    ResetContext();
  }
}

bool InputMethodAsh::IsCandidatePopupOpen() const {
  // TODO(yukishiino): Implement this method.
  return false;
}

ui::VirtualKeyboardController* InputMethodAsh::GetVirtualKeyboardController() {
  if (auto* engine = GetEngine()) {
    if (auto* controller = engine->GetVirtualKeyboardController()) {
      return controller;
    }
  }
  return InputMethodBase::GetVirtualKeyboardController();
}

void InputMethodAsh::OnFocus() {
  auto* bridge = IMEBridge::Get();
  if (bridge) {
    bridge->SetInputContextHandler(this);
  }
}

void InputMethodAsh::OnBlur() {
  if (IMEBridge::Get() && IMEBridge::Get()->GetInputContextHandler() == this) {
    IMEBridge::Get()->SetInputContextHandler(nullptr);
  }
}

void InputMethodAsh::OnWillChangeFocusedClient(TextInputClient* focused_before,
                                               TextInputClient* focused) {
  ConfirmComposition(/* reset_engine */ true);

  // Remove any autocorrect range in the unfocused TextInputClient.
  if (focused_before) {
    focused_before->SetAutocorrectRange(gfx::Range());
  }

  if (GetEngine()) {
    GetEngine()->Blur();
  }
}

void InputMethodAsh::OnDidChangeFocusedClient(TextInputClient* focused_before,
                                              TextInputClient* focused) {
  // Force to update the input type since client's TextInputStateChanged()
  // function might not be called if text input types before the client loses
  // focus and after it acquires focus again are the same.
  UpdateContextFocusState();

  if (GetEngine()) {
    GetEngine()->Focus(GetInputContext());
  }

  OnCaretBoundsChanged(GetTextInputClient());
}

bool InputMethodAsh::SetCompositionRange(
    uint32_t before,
    uint32_t after,
    const std::vector<ui::ImeTextSpan>& text_spans) {
  TextInputClient* client = GetTextInputClient();

  if (IsTextInputTypeNone()) {
    return false;
  }
  typing_session_manager_.Heartbeat();
  // The given range and spans are relative to the current selection.
  gfx::Range range;
  if (!client->GetEditableSelectionRange(&range)) {
    return false;
  }

  const gfx::Range composition_range(
      range.start() >= before ? range.start() - before : 0,
      range.end() + after);

  // Check that the composition range is valid.
  gfx::Range text_range;
  client->GetTextRange(&text_range);
  if (!text_range.Contains(composition_range)) {
    return false;
  }

  return SetComposingRange(composition_range.start(), composition_range.end(),
                           text_spans);
}

bool InputMethodAsh::SetComposingRange(
    uint32_t start,
    uint32_t end,
    const std::vector<ui::ImeTextSpan>& text_spans) {
  TextInputClient* client = GetTextInputClient();

  if (IsTextInputTypeNone()) {
    return false;
  }

  const auto ordered_range = std::minmax(start, end);
  const gfx::Range composition_range(ordered_range.first, ordered_range.second);

  // Use a default text span that spans across the whole composition range.
  auto non_empty_text_spans =
      !text_spans.empty()
          ? text_spans
          : std::vector<ui::ImeTextSpan>{ui::ImeTextSpan(
                ui::ImeTextSpan::Type::kComposition,
                /*start_offset=*/0, /*end_offset=*/composition_range.length())};

  // If we have pending key events, then delay the operation until
  // |ProcessKeyEventPostIME|. Otherwise, process it immediately.
  if (handling_key_event_) {
    composition_changed_ = true;
    pending_composition_range_ =
        PendingSetCompositionRange{composition_range, non_empty_text_spans};
    return true;
  } else {
    composing_text_ = true;
    return client->SetCompositionFromExistingText(composition_range,
                                                  non_empty_text_spans);
  }
}

gfx::Range InputMethodAsh::GetAutocorrectRange() {
  if (IsTextInputTypeNone()) {
    return gfx::Range();
  }
  return GetTextInputClient()->GetAutocorrectRange();
}

void InputMethodAsh::SetAutocorrectRange(
    const gfx::Range& range,
    SetAutocorrectRangeDoneCallback callback) {
  if (IsTextInputTypeNone()) {
    std::move(callback).Run(false);
    return;
  }

  // If we have pending key events, then delay the operation until
  // |ProcessKeyEventPostIME|. Otherwise, process it immediately.
  if (handling_key_event_) {
    if (pending_autocorrect_range_) {
      std::move(pending_autocorrect_range_->callback).Run(false);
    }

    pending_autocorrect_range_ =
        std::make_unique<InputMethodAsh::PendingAutocorrectRange>(
            range, std::move(callback));
  } else {
    std::move(callback).Run(GetTextInputClient()->SetAutocorrectRange(range));
  }
}

std::optional<ui::GrammarFragment>
InputMethodAsh::GetGrammarFragmentAtCursor() {
  if (IsTextInputTypeNone()) {
    return std::nullopt;
  }
  return GetTextInputClient()->GetGrammarFragmentAtCursor();
}

bool InputMethodAsh::ClearGrammarFragments(const gfx::Range& range) {
  if (IsTextInputTypeNone()) {
    return false;
  }
  return GetTextInputClient()->ClearGrammarFragments(range);
}

bool InputMethodAsh::AddGrammarFragments(
    const std::vector<ui::GrammarFragment>& fragments) {
  if (IsTextInputTypeNone()) {
    return false;
  }
  return GetTextInputClient()->AddGrammarFragments(fragments);
}

void InputMethodAsh::ConfirmComposition(bool reset_engine) {
  TextInputClient* client = GetTextInputClient();
  // TODO(b/223075193): Quick fix for the case where we have a pending commit.
  // Without this, then we would lose the pending commit after confirming the
  // composition text.
  // Fix this properly by getting rid of the pending mechanism completely.
  if (pending_commit_ && !pending_composition_range_ && !pending_composition_) {
    // Only a pending commit, so confirming the composition is a no-op.
    return;
  }
  // TODO(b/225723475): Similar to the comment above, this is a quick fix to
  // solve the autocorrect issue outlined in the linked bug. This is due to the
  // pending composition being reset before it could be applied to the current
  // text. Again we need to fix this properly by removing the pending mechanism.
  if (pending_composition_ && !pending_commit_ && !pending_composition_range_) {
    GetTextInputClient()->SetCompositionText(*pending_composition_);
    pending_composition_ = std::nullopt;
    composition_changed_ = false;
  }
  if (client && (client->HasCompositionText() ||
                 client->SupportsAlwaysConfirmComposition())) {
    const size_t characters_committed =
        client->ConfirmCompositionText(/*keep_selection*/ true);
    typing_session_manager_.CommitCharacters(characters_committed);
  }
  // See https://crbug.com/984472.
  ResetContext(reset_engine);
}

void InputMethodAsh::ResetContext(bool reset_engine) {
  if (IsPasswordOrNoneInputFieldFocused() || !GetTextInputClient()) {
    return;
  }

  const bool was_composing = composing_text_;

  pending_composition_ = std::nullopt;
  pending_commit_ = std::nullopt;
  composing_text_ = false;
  composition_changed_ = false;

  if (reset_engine && was_composing && GetEngine()) {
    GetEngine()->Reset();
  }

  character_composer_.Reset();
}

void InputMethodAsh::UpdateContextFocusState() {
  ResetContext();
  OnInputMethodChanged();

  // Propagate the focus event to the candidate window handler which also
  // manages the input method mode indicator.
  IMECandidateWindowHandlerInterface* candidate_window =
      IMEBridge::Get()->GetCandidateWindowHandler();
  if (candidate_window) {
    candidate_window->FocusStateChanged(!IsPasswordOrNoneInputFieldFocused());
  }

  // Propagate focus event to assistive window handler.
  IMEAssistiveWindowHandlerInterface* assistive_window =
      IMEBridge::Get()->GetAssistiveWindowHandler();
  if (assistive_window) {
    assistive_window->FocusStateChanged();
  }

  IMEBridge::Get()->SetCurrentInputContext(GetInputContext());

  TextInputClient* client = GetTextInputClient();
  focused_url_ = client && !IsPasswordOrNoneInputFieldFocused()
                     ? client->GetTextEditingContext().page_url
                     : GURL();
}

ui::EventDispatchDetails InputMethodAsh::ProcessKeyEventPostIME(
    ui::KeyEvent* event,
    ui::ime::KeyEventHandledState handled_state,
    bool stopped_propagation) {
  bool handled =
      handled_state == ui::ime::KeyEventHandledState::kHandledByIME ||
      handled_state ==
          ui::ime::KeyEventHandledState::kHandledByAssistiveSuggester;

  auto properties =
      event->properties() ? *event->properties() : ui::Event::Properties();
  // Mark whether the key is handled by IME or not.
  ui::SetKeyboardImeFlagProperty(&properties,
                                 handled ? ui::kPropertyKeyboardImeHandledFlag
                                         : ui::kPropertyKeyboardImeIgnoredFlag);
  // Mark whether autorepeat needs to be suppressed.
  if (handled_state ==
      ui::ime::KeyEventHandledState::kNotHandledSuppressAutoRepeat) {
    ui::SetKeyEventSuppressAutoRepeat(properties);
  }
  event->SetProperties(properties);

  TextInputClient* client = GetTextInputClient();
  if (!client) {
    // As ibus works asynchronously, there is a chance that the focused client
    // loses focus before this method gets called.
    return DispatchKeyEventPostIME(event);
  }

  if (event->type() == ui::EventType::kKeyPressed && handled) {
    bool only_dispatch_vkey_processkey =
        (handled_state ==
         ui::ime::KeyEventHandledState::kHandledByAssistiveSuggester);
    ui::EventDispatchDetails dispatch_details =
        ProcessFilteredKeyPressEvent(event, only_dispatch_vkey_processkey);
    if (event->stopped_propagation()) {
      ResetContext();
      return dispatch_details;
    }
  }
  ui::EventDispatchDetails dispatch_details;

  // In case the focus was changed by the key event. The |context_| should have
  // been reset when the focused window changed.
  if (client != GetTextInputClient()) {
    return dispatch_details;
  }

  MaybeProcessPendingInputMethodResult(event, handled);

  // In case the focus was changed when sending input method results to the
  // focused window.
  if (client != GetTextInputClient()) {
    return dispatch_details;
  }

  if (handled) {
    return dispatch_details;  // IME handled the key event. do not forward.
  }

  if (event->type() == ui::EventType::kKeyPressed) {
    return ProcessUnfilteredKeyPressEvent(event);
  }

  if (event->type() == ui::EventType::kKeyReleased) {
    return DispatchKeyEventPostIME(event);
  }
  return dispatch_details;
}

ui::EventDispatchDetails InputMethodAsh::ProcessFilteredKeyPressEvent(
    ui::KeyEvent* event,
    bool only_dispatch_vkey_processkey) {
  if (!only_dispatch_vkey_processkey) {
    if (NeedInsertChar()) {
      return DispatchKeyEventPostIME(event);
    }

    // For dead keys, it is possible to dispatch a fake Process key, but it is
    // better to dispatch the real dead key, as it is more specific and allows
    // apps to have dead key specific behavior.
    // TODO(b/289319217): Investigate if we need to distinguish between a dead
    // key that is handled by the character composer or is handled by the input
    // method.
    if ((base::FeatureList::IsEnabled(features::kInputMethodDeadKeyFix) ||
         (focused_url_.is_valid() && IsTerminalOrCrosh(focused_url_))) &&
        event->GetDomKey().IsDeadKey()) {
      return DispatchKeyEventPostIME(event);
    }
  }

  ui::KeyEvent fabricated_event(ui::EventType::kKeyPressed, ui::VKEY_PROCESSKEY,
                                event->code(), event->flags(),
                                ui::DomKey::PROCESS, event->time_stamp());
  if (const auto* properties = event->properties()) {
    fabricated_event.SetProperties(*properties);
  }
  ui::EventDispatchDetails dispatch_details =
      DispatchKeyEventPostIME(&fabricated_event);
  if (fabricated_event.stopped_propagation()) {
    event->StopPropagation();
  }
  return dispatch_details;
}

ui::EventDispatchDetails InputMethodAsh::ProcessUnfilteredKeyPressEvent(
    ui::KeyEvent* event) {
  TextInputClient* prev_client = GetTextInputClient();
  ui::EventDispatchDetails details = DispatchKeyEventPostIME(event);
  if (event->stopped_propagation()) {
    ResetContext();
    return details;
  }

  // We shouldn't dispatch the character anymore if the key event dispatch
  // caused focus change. For example, in the following scenario,
  // 1. visit a web page which has a <textarea>.
  // 2. click Omnibox.
  // 3. enable Korean IME, press A, then press Tab to move the focus to the web
  //    page.
  // We should return here not to send the Tab key event to RWHV.
  TextInputClient* client = GetTextInputClient();
  if (!client || client != prev_client) {
    return details;
  }

  // If a key event was not filtered by |context_| and |character_composer_|,
  // then it means the key event didn't generate any result text. So we need
  // to send corresponding character to the focused text input client.
  if (event->GetCharacter()) {
    client->InsertChar(*event);
    typing_session_manager_.CommitCharacters(1);
  }
  return details;
}

void InputMethodAsh::MaybeProcessPendingInputMethodResult(ui::KeyEvent* event,
                                                          bool handled) {
  TextInputClient* client = GetTextInputClient();
  DCHECK(client);

  if (pending_commit_) {
    if (handled && NeedInsertChar()) {
      for (const auto& ch : pending_commit_->text) {
        ui::KeyEvent ch_event(ui::EventType::kKeyPressed, ui::VKEY_UNKNOWN,
                              ui::EF_NONE);
        ch_event.set_character(ch);
        ui::SetKeyboardImeFlags(&ch_event, ui::kPropertyKeyboardImeHandledFlag);
        client->InsertChar(ch_event);
      }
    } else if (pending_commit_->text.empty()) {
      client->InsertText(
          u"", TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
      composing_text_ = false;
    } else {
      // Split the commit into two separate commits, one for the substring
      // before the cursor and one for the substring after.
      const std::u16string before_cursor =
          pending_commit_->text.substr(0, pending_commit_->cursor);
      if (!before_cursor.empty()) {
        client->InsertText(
            before_cursor,
            TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
      }
      const std::u16string after_cursor =
          pending_commit_->text.substr(pending_commit_->cursor);
      if (!after_cursor.empty()) {
        client->InsertText(
            after_cursor,
            TextInputClient::InsertTextCursorBehavior::kMoveCursorBeforeText);
      }
      composing_text_ = false;
    }
    typing_session_manager_.CommitCharacters(pending_commit_->text.length());
  }

  // TODO(crbug.com/40623107): Refactor this code to be clearer and less
  // error-prone.
  if (composition_changed_ && !IsTextInputTypeNone()) {
    if (pending_composition_range_) {
      client->SetCompositionFromExistingText(
          pending_composition_range_->range,
          pending_composition_range_->text_spans);
    }
    if (pending_composition_) {
      composing_text_ = true;
      client->SetCompositionText(*pending_composition_);
    } else if (!pending_commit_ && !pending_composition_range_) {
      client->ClearCompositionText();
    }

    pending_composition_ = std::nullopt;
    pending_composition_range_.reset();
  }

  if (pending_autocorrect_range_) {
    std::move(pending_autocorrect_range_->callback)
        .Run(client->SetAutocorrectRange(pending_autocorrect_range_->range));
    pending_autocorrect_range_.reset();
  }

  // We should not clear composition text here, as it may belong to the next
  // composition session.
  pending_commit_ = std::nullopt;
  composition_changed_ = false;
}

bool InputMethodAsh::NeedInsertChar() const {
  return GetTextInputClient() &&
         (IsTextInputTypeNone() || (!composing_text_ && pending_commit_ &&
                                    pending_commit_->text.length() == 1 &&
                                    pending_commit_->cursor == 1));
}

bool InputMethodAsh::HasInputMethodResult() const {
  return pending_commit_ || composition_changed_;
}

void InputMethodAsh::CommitText(
    const std::u16string& text,
    TextInputClient::InsertTextCursorBehavior cursor_behavior) {
  // We need to receive input method result even if the text input type is
  // `ui::TEXT_INPUT_TYPE_NONE`, to make sure we can always send correct
  // character for each key event to the focused text input client.
  if (!GetTextInputClient()) {
    return;
  }

  if (!GetTextInputClient()->CanComposeInline()) {
    // Hides the candidate window for preedit text.
    UpdateCompositionText(CompositionText(), 0, false);
  }

  // Append the text to the buffer, because commit signal might be fired
  // multiple times when processing a key event.
  if (!pending_commit_) {
    pending_commit_ = PendingCommit();
  }
  pending_commit_->text.insert(pending_commit_->cursor, text);
  if (cursor_behavior ==
      TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText) {
    pending_commit_->cursor += text.length();
  }

  // If we are not handling key event, do not bother sending text result if the
  // focused text input client does not support text input.
  if (!handling_key_event_ && !IsTextInputTypeNone()) {
    if (!SendFakeProcessKeyEvent(true)) {
      GetTextInputClient()->InsertText(text, cursor_behavior);
      typing_session_manager_.CommitCharacters(text.length());
    }
    SendFakeProcessKeyEvent(false);
    pending_commit_ = std::nullopt;
  }
}

void InputMethodAsh::UpdateCompositionText(const CompositionText& text,
                                           uint32_t cursor_pos,
                                           bool visible) {
  if (IsTextInputTypeNone()) {
    return;
  }

  if (!GetTextInputClient()->CanComposeInline()) {
    IMECandidateWindowHandlerInterface* candidate_window =
        IMEBridge::Get()->GetCandidateWindowHandler();
    if (candidate_window) {
      candidate_window->UpdatePreeditText(text.text, cursor_pos, visible);
    }
  }

  // |visible| argument is very confusing. For example, what's the correct
  // behavior when:
  // 1. OnUpdatePreeditText() is called with a text and visible == false, then
  // 2. OnShowPreeditText() is called afterwards.
  //
  // If it's only for clearing the current preedit text, then why not just use
  // OnHidePreeditText()?
  if (!visible) {
    HidePreeditText();
    return;
  }

  pending_composition_ = ExtractCompositionText(text, cursor_pos);
  composition_changed_ = true;

  // In case OnShowPreeditText() is not called.
  if (pending_composition_->text.length()) {
    composing_text_ = true;
  }

  if (!handling_key_event_) {
    // If we receive a composition text without pending key event, then we need
    // to send it to the focused text input client directly.
    if (!SendFakeProcessKeyEvent(true)) {
      GetTextInputClient()->SetCompositionText(*pending_composition_);
    }
    SendFakeProcessKeyEvent(false);
    composition_changed_ = false;
    pending_composition_ = std::nullopt;
  }
}

void InputMethodAsh::HidePreeditText() {
  if (IsTextInputTypeNone()) {
    return;
  }

  // Intentionally leaves |composing_text_| unchanged.
  composition_changed_ = true;
  pending_composition_ = std::nullopt;

  if (!handling_key_event_) {
    TextInputClient* client = GetTextInputClient();
    if (client && client->HasCompositionText()) {
      if (!SendFakeProcessKeyEvent(true)) {
        client->ClearCompositionText();
      }
      SendFakeProcessKeyEvent(false);
    }
    composition_changed_ = false;
  }
}

TextInputMethod::InputContext InputMethodAsh::GetInputContext() const {
  TextInputClient* client = GetTextInputClient();
  if (!client) {
    return TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_NONE);
  }

  const int flags = client->GetTextInputFlags();
  TextInputMethod::InputContext input_context(
      flags & ui::TEXT_INPUT_FLAG_HAS_BEEN_PASSWORD
          ? ui::TEXT_INPUT_TYPE_PASSWORD
          : client->GetTextInputType());
  input_context.mode = client->GetTextInputMode();
  input_context.autocompletion_mode =
      ConvertTextInputFlagToEnum<AutocompletionMode>(
          flags, ui::TEXT_INPUT_FLAG_AUTOCOMPLETE_ON,
          ui::TEXT_INPUT_FLAG_AUTOCOMPLETE_OFF);
  input_context.autocorrection_mode =
      ConvertTextInputFlagToEnum<AutocorrectionMode>(
          flags, ui::TEXT_INPUT_FLAG_AUTOCORRECT_ON,
          ui::TEXT_INPUT_FLAG_AUTOCORRECT_OFF);
  input_context.autocapitalization_mode = ConvertAutocapitalizationMode(flags);
  input_context.spellcheck_mode = ConvertTextInputFlagToEnum<SpellcheckMode>(
      flags, ui::TEXT_INPUT_FLAG_SPELLCHECK_ON,
      ui::TEXT_INPUT_FLAG_SPELLCHECK_OFF);
  input_context.focus_reason = client->GetFocusReason();
  input_context.personalization_mode = client->ShouldDoLearning()
                                           ? PersonalizationMode::kEnabled
                                           : PersonalizationMode::kDisabled;
  return input_context;
}

void InputMethodAsh::SendKeyEvent(ui::KeyEvent* event) {
  ui::EventDispatchDetails details = DispatchKeyEvent(event);
  DCHECK(!details.dispatcher_destroyed);
}

SurroundingTextInfo InputMethodAsh::GetSurroundingTextInfo() {
  gfx::Range text_range;
  SurroundingTextInfo info;
  TextInputClient* client = GetTextInputClient();
  if (!client || !client->GetTextRange(&text_range) ||
      !client->GetTextFromRange(text_range, &info.surrounding_text) ||
      !client->GetEditableSelectionRange(&info.selection_range)) {
    return SurroundingTextInfo();
  }
  // Makes the |selection_range| be relative to the |surrounding_text|.
  info.selection_range.set_start(info.selection_range.start() -
                                 text_range.start());
  info.selection_range.set_end(info.selection_range.end() - text_range.start());
  info.offset = text_range.start();
  return info;
}

void InputMethodAsh::DeleteSurroundingText(uint32_t num_char16s_before_cursor,
                                           uint32_t num_char16s_after_cursor) {
  if (!GetTextInputClient()) {
    return;
  }

  if (GetTextInputClient()->HasCompositionText()) {
    return;
  }

  GetTextInputClient()->ExtendSelectionAndDelete(num_char16s_before_cursor,
                                                 num_char16s_after_cursor);
}

void InputMethodAsh::ReplaceSurroundingText(
    uint32_t length_before_selection,
    uint32_t length_after_selection,
    std::u16string_view replacement_text) {
  if (!GetTextInputClient()) {
    return;
  }

  GetTextInputClient()->ExtendSelectionAndReplace(
      length_before_selection, length_after_selection, replacement_text);
}

bool InputMethodAsh::ExecuteCharacterComposer(const ui::KeyEvent& event) {
  if (!character_composer_.FilterKeyPress(event)) {
    return false;
  }

  // `ui::CharacterComposer` consumed the key event. Update the composition
  // text.
  CompositionText preedit;
  preedit.text = character_composer_.preedit_string();
  UpdateCompositionText(preedit, preedit.text.size(), !preedit.text.empty());
  const std::u16string& commit_text = character_composer_.composed_character();
  if (!commit_text.empty()) {
    CommitText(commit_text,
               TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  }
  return true;
}

CompositionText InputMethodAsh::ExtractCompositionText(
    const CompositionText& text,
    uint32_t cursor_position) const {
  CompositionText composition;
  composition.text = text.text;

  if (composition.text.empty()) {
    return composition;
  }

  // ibus uses character index for cursor position and attribute range, but we
  // use char16 offset for them. So we need to do conversion here.
  std::vector<size_t> char16_offsets;
  size_t length = composition.text.length();
  for (base::i18n::UTF16CharIterator char_iterator(composition.text);
       !char_iterator.end(); char_iterator.Advance()) {
    char16_offsets.push_back(char_iterator.array_pos());
  }

  // The text length in Unicode characters.
  auto char_length = static_cast<uint32_t>(char16_offsets.size());
  // Make sure we can convert the value of |char_length| as well.
  char16_offsets.push_back(length);

  size_t cursor_offset = char16_offsets[std::min(char_length, cursor_position)];

  composition.selection = gfx::Range(cursor_offset);

  const ui::ImeTextSpans text_ime_text_spans = text.ime_text_spans;
  if (!text_ime_text_spans.empty()) {
    for (const auto& text_ime_text_span : text_ime_text_spans) {
      const uint32_t start = text_ime_text_span.start_offset;
      const uint32_t end = text_ime_text_span.end_offset;
      if (start >= end || end >= char16_offsets.size()) {
        LOG(ERROR) << "IME composition invalid bounds.";
        continue;
      }
      ui::ImeTextSpan ime_text_span(ui::ImeTextSpan::Type::kComposition,
                                    char16_offsets[start], char16_offsets[end],
                                    text_ime_text_span.thickness,
                                    ui::ImeTextSpan::UnderlineStyle::kSolid,
                                    text_ime_text_span.background_color);
      ime_text_span.underline_color = text_ime_text_span.underline_color;
      composition.ime_text_spans.push_back(ime_text_span);
    }
  }

  DCHECK(text.selection.start() <= text.selection.end());
  DCHECK(text.selection.end() <= char_length);
  if (text.selection.start() < text.selection.end()) {
    const size_t start =
        std::min(text.selection.start(), static_cast<size_t>(char_length));
    const size_t end =
        std::min(text.selection.end(), static_cast<size_t>(char_length));
    ui::ImeTextSpan ime_text_span(
        ui::ImeTextSpan::Type::kComposition, char16_offsets[start],
        char16_offsets[end], ui::ImeTextSpan::Thickness::kThick,
        ui::ImeTextSpan::UnderlineStyle::kSolid, SK_ColorTRANSPARENT);
    composition.ime_text_spans.push_back(ime_text_span);

    // If the cursor is at start or end of this ime_text_span, then we treat
    // it as the selection range as well, but make sure to set the cursor
    // position to the selection end.
    if (ime_text_span.start_offset == cursor_offset) {
      composition.selection.set_start(ime_text_span.end_offset);
      composition.selection.set_end(cursor_offset);
    } else if (ime_text_span.end_offset == cursor_offset) {
      composition.selection.set_start(ime_text_span.start_offset);
      composition.selection.set_end(cursor_offset);
    }
  }

  // Use a thin underline with text color by default.
  if (composition.ime_text_spans.empty()) {
    composition.ime_text_spans.push_back(ui::ImeTextSpan(
        ui::ImeTextSpan::Type::kComposition, 0, length,
        ui::ImeTextSpan::Thickness::kThin,
        ui::ImeTextSpan::UnderlineStyle::kSolid, SK_ColorTRANSPARENT));
  }

  return composition;
}

bool InputMethodAsh::IsPasswordOrNoneInputFieldFocused() {
  ui::TextInputType type = GetTextInputType();
  return type == ui::TEXT_INPUT_TYPE_NONE ||
         type == ui::TEXT_INPUT_TYPE_PASSWORD;
}

bool InputMethodAsh::HasCompositionText() {
  TextInputClient* client = GetTextInputClient();
  return client && client->HasCompositionText();
}

ukm::SourceId InputMethodAsh::GetClientSourceForMetrics() {
  TextInputClient* client = GetTextInputClient();
  return client ? client->GetClientSourceForMetrics() : ukm::kInvalidSourceId;
}

ui::InputMethod* InputMethodAsh::GetInputMethod() {
  return this;
}

bool InputMethodAsh::SendFakeProcessKeyEvent(bool pressed) const {
  ui::KeyEvent evt(
      pressed ? ui::EventType::kKeyPressed : ui::EventType::kKeyReleased,
      pressed ? ui::VKEY_PROCESSKEY : ui::VKEY_UNKNOWN,
      ui::EF_IME_FABRICATED_KEY);
  ui::SetKeyboardImeFlags(&evt, ui::kPropertyKeyboardImeHandledFlag);

  std::ignore = DispatchKeyEventPostIME(&evt);
  return evt.stopped_propagation();
}

}  // namespace ash
