// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/linux/input_method_auralinux.h"

#include "base/auto_reset.h"
#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/ime/constants.h"
#include "ui/base/ime/linux/linux_input_method_context_factory.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/events/event.h"

namespace {

constexpr base::TimeDelta kIgnoreCommitsDuration = base::Milliseconds(100);

bool IsEventFromVK(const ui::KeyEvent& event) {
  if (event.HasNativeEvent())
    return false;

  const auto* properties = event.properties();
  return properties &&
         properties->find(ui::kPropertyFromVK) != properties->end();
}

bool IsSameKeyEvent(const ui::KeyEvent& lhs, const ui::KeyEvent& rhs) {
  // Note that we do not check timestamp here in order to support wayland's
  // text_input::keysym, which does not have timestamp.
  // Ignore EF_IS_REPEAT here, because they may be calculated in KeyEvent's
  // ctor, so we cannot rely on it to detect whether key events come from
  // the same native event.
  return lhs.type() == rhs.type() && lhs.code() == rhs.code() &&
         (lhs.flags() & ~ui::EF_IS_REPEAT) == (rhs.flags() & ~ui::EF_IS_REPEAT);
}

}  // namespace

namespace ui {

InputMethodAuraLinux::InputMethodAuraLinux(
    ImeKeyEventDispatcher* ime_key_event_dispatcher)
    : InputMethodBase(ime_key_event_dispatcher),
      text_input_type_(TEXT_INPUT_TYPE_NONE),
      is_sync_mode_(false),
      composition_changed_(false) {
  context_ = CreateLinuxInputMethodContext(this);
}

InputMethodAuraLinux::~InputMethodAuraLinux() = default;

LinuxInputMethodContext* InputMethodAuraLinux::GetContextForTesting() {
  return context_.get();
}

// Overriden from InputMethod.

ui::EventDispatchDetails InputMethodAuraLinux::DispatchKeyEvent(
    ui::KeyEvent* event) {
  DCHECK(event->type() == EventType::kKeyPressed ||
         event->type() == EventType::kKeyReleased);
  // If there's pending deadkey event, i.e. a key event which is expected to
  // trigger input method actions (like OnCommit, OnPreedit* invocation)
  // and to be dispatched from there, but not yet, dispatch the pending event
  // first.
  // Dead keys are considered to be consumed by IME. Actually, it updates
  // input method's internal state. However, it makes no input method actions,
  // so the event won't be dispatched without this handling.
  // Note that this is the earliest timing to find the pending deadkey event
  // needs to be dispatched. It is because InputMethodAuraLinux cannot find
  // whether input method actions will be followed or not on holding the event.
  //
  // Note that we do not apply this for non-deadkey events intentionally.
  // It is because some input framework sends key events twice to fill the gap
  // of synchronous API v.s. asynchronous operations.
  // Specifically:
  // - The first key event is passed to input method via |context_| below.
  // - Inside the function, it triggers asynchronous input method operation.
  //   However, the function needs to return whether the event is filtered
  //   or not synchronously, it returns "filtered" regardless of the event
  //   will be actually filtered or not.
  // - On completion of the input method action, specifically if the input
  //   method does not consume the event, the framework internally re-generates
  //   the same key event, and post it back again to the application.
  // This happens some common input method framework, such as iBus/fcitx and
  // GTK-IMmodule. Also, wayland extension implemented by exosphere in
  // ash-chrome for Lacros behaves in the same way from InputMethodAuraLinux's
  // point of view.
  // To avoid dispatching twice, do not dispatch it here. Following code
  // will handle the second (i.e. fallback) key event, including event
  // dispatching.
  // Importantly, the second key press event may be arrived after the first
  // key release event, because everything is working in asynchronous ways.
  if (ime_filtered_key_event_.has_value() &&
      !IsSameKeyEvent(*ime_filtered_key_event_, *event) &&
      ime_filtered_key_event_->GetDomKey().IsDeadKey()) {
    std::ignore = DispatchKeyEventPostIME(&*ime_filtered_key_event_);
  }
  ime_filtered_key_event_.reset();

  // If no text input client, dispatch immediately.
  if (!GetTextInputClient()) {
    // For Wayland, wl_keyboard::key will be sent following the peek key event
    // if the event is not consumed by IME, so peek key events should not be
    // dispatched. crbug.com/1225747
    // Do not keep release events. Non-peek Release key event is dispatched,
    // so the event will be stale. See WaylandKeyboard::OnKey for details.
    if (event->type() == EventType::kKeyPressed &&
        context_->IsPeekKeyEvent(*event)) {
      ime_filtered_key_event_ = std::move(*event);
      return ui::EventDispatchDetails();
    }
    return DispatchKeyEventPostIME(event);
  }

  if (IsEventFromVK(*event)) {
    // Faked key events that are sent from input.ime.sendKeyEvents.
    ui::EventDispatchDetails details = DispatchKeyEventPostIME(event);
    if (details.dispatcher_destroyed || details.target_destroyed ||
        event->stopped_propagation()) {
      return details;
    }
    if ((event->is_char() || event->GetDomKey().IsCharacter()) &&
        event->type() == ui::EventType::kKeyPressed) {
      GetTextInputClient()->InsertChar(*event);
    }
    return details;
  }

  // Forward key event to IME.
  bool filtered = false;
  {
    suppress_non_key_input_until_ = base::TimeTicks::UnixEpoch();
    composition_changed_ = false;
    last_commit_result_.reset();
    result_text_ = std::nullopt;
    base::AutoReset<bool> flipper(&is_sync_mode_, true);
    filtered = context_->DispatchKeyEvent(*event);
  }

  // There are four cases here. They are a pair of two conditions:
  // - Whether KeyEvent is consumed by IME, which is represented by filtered.
  // - Whether IME updates the commit/preedit string synchronously
  //   (i.e. which is already completed here), or asynchronously (i.e. which
  //   will be done afterwords, so not yet done).
  //
  // Note that there's a case that KeyEvent is reported as NOT consumed by IME,
  // but IME still updates the commit/preedit. Please see below comment
  // for more details.
  //
  // Conceptually, after IME's update, there're three things to be done.
  // - Continue to dispatch the KeyEvent.
  // - Update TextInputClient by using committed text.
  // - Update TextInputClient by using preedit text.
  // The following code does those three, except in the case that KeyEvent is
  // consumed by IME and commit/preedit string update will happen
  // asynchronously. The remaining case is covered in OnCommit and
  // OnPreeditChanged/End.
  // TODO(crbug.com/40761214): On Lacros CTRL+TAB events are sent twice if
  // user types it on loading page, because the connected client is considered
  // None type, and so the peek key event is not held here.
  // To derisk the regression in other platform, and to prioritize the fix
  // on Lacros, we conditionally do not check whether the connected client
  // is None type for Lacros only. We should remove this soon.
  if (filtered && !HasInputMethodResult()
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
      && !IsTextInputTypeNone()
#endif
  ) {
    ime_filtered_key_event_ = std::move(*event);
    return ui::EventDispatchDetails();
  }

  // First, if KeyEvent is consumed by IME, continue to dispatch it,
  // before updating commit/preedit string so that, e.g., JavaScript keydown
  // event is delivered to the page before keypress.
  ui::EventDispatchDetails details;
  if (event->type() == ui::EventType::kKeyPressed && filtered) {
    details = DispatchImeFilteredKeyPressEvent(event);
    if (details.target_destroyed || details.dispatcher_destroyed ||
        event->stopped_propagation()) {
      return details;
    }
  }

  // Processes the result text before composition for sync mode.
  const auto commit_result = MaybeCommitResult(filtered, *event);
  if (commit_result == CommitResult::kTargetDestroyed) {
    details.target_destroyed = true;
    event->StopPropagation();
    return details;
  }
  // Stop the propagation if there's some committed characters.
  // Note that this have to be done after the key event dispatching,
  // specifically if key event is not reported as filtered.
  bool should_stop_propagation = commit_result == CommitResult::kSuccess;

  // Then update the composition, if necessary.
  // Should stop propagation of the event when composition is updated,
  // because the event is considered to be used for the composition.
  should_stop_propagation |=
      UpdateCompositionIfChanged(commit_result == CommitResult::kSuccess);

  // If the IME has not handled the key event, passes the keyevent back to the
  // previous processing flow.
  if (!filtered) {
    details = DispatchKeyEventPostIME(event);
    if (details.dispatcher_destroyed) {
      if (should_stop_propagation)
        event->StopPropagation();
      return details;
    }
    if (event->stopped_propagation() || details.target_destroyed) {
      ResetContext();
    } else if (event->type() == ui::EventType::kKeyPressed) {
      // If a key event was not filtered by |context_|,
      // then it means the key event didn't generate any result text. For some
      // cases, the key event may still generate a valid character, eg. a
      // control-key event (ctrl-a, return, tab, etc.). We need to send the
      // character to the focused text input client by calling
      // TextInputClient::InsertChar().
      // Note: don't use |client| and use GetTextInputClient() here because
      // DispatchKeyEventPostIME may cause the current text input client change.
      char16_t ch = event->GetCharacter();
      if (ch && GetTextInputClient())
        GetTextInputClient()->InsertChar(*event);
      should_stop_propagation = true;
    }
  }

  if (should_stop_propagation)
    event->StopPropagation();

  return details;
}

ui::EventDispatchDetails InputMethodAuraLinux::DispatchImeFilteredKeyPressEvent(
    ui::KeyEvent* event) {
  // In general, 229 (VKEY_PROCESSKEY) should be used. However, in some IME
  // framework, such as iBus/fcitx + GTK, the behavior is not simple as follows,
  // in order to deal with synchronous API on asynchronous IME backend:
  // - First, IM module reports the KeyEvent is filtered synchronously.
  // - Then, it forwards the event to the IME engine asynchronously.
  // - When IM module receives the result, and it turns out the event is not
  //   consumed, then IM module generates the same key event (with a special
  //   flag), and sent it to the application (Chrome in our case).
  // - Then, the application forwards the event to IM module again, and in this
  //   time IM module synchronously commit the character.
  // (Note: new iBus GTK IMModule changed the behavior, so the second event
  // dispatch to the application won't happen).
  // InputMethodAuraLinux detects this case by the following condition:
  // - If result text is only one character, and
  // - there's no composing text, and no updated.
  // If the condition meets, that means IME did not consume the key event
  // conceptually, so continue to dispatch KeyEvent without overwriting by 229.
  // But in some chinese IME framework such as fcitx + GTK,
  // if the condition meets and last event is 13(VKEY_RETURN), that means IME
  // consume the key event conceptually(want to insert the only one character).
  // So in this condition, should to dispatch KeyEvent with overwriting by 229.
  ui::EventDispatchDetails details;
  if (event->key_code() == VKEY_RETURN)
    details = SendFakeProcessKeyEvent(event);
  else {
    details = NeedInsertChar(result_text_) ? DispatchKeyEventPostIME(event)
                                           : SendFakeProcessKeyEvent(event);
  }
  if (details.dispatcher_destroyed)
    return details;
  // If the KEYDOWN is stopped propagation (e.g. triggered an accelerator),
  // don't InsertChar/InsertText to the input field.
  if (event->stopped_propagation() || details.target_destroyed)
    ResetContext();

  return details;
}

InputMethodAuraLinux::CommitResult InputMethodAuraLinux::MaybeCommitResult(
    bool filtered,
    const KeyEvent& event) {
  // Note: |client| could be NULL because DispatchKeyEventPostIME could have
  // changed the text input client.
  TextInputClient* client = GetTextInputClient();
  if (!client || !result_text_)
    return CommitResult::kNoCommitString;

  // Take the ownership of |result_text_|.
  std::u16string result_text = std::move(*result_text_);
  result_text_ = std::nullopt;

  if (filtered && NeedInsertChar(result_text)) {
    for (const auto ch : result_text) {
      ui::KeyEvent ch_event(event);
      ch_event.set_character(ch);
      client->InsertChar(ch_event);
      // If the client changes we assume that the original target has been
      // destroyed.
      if (client != GetTextInputClient())
        return CommitResult::kTargetDestroyed;
    }
  } else {
    // If |filtered| is false, that means the IME wants to commit some text
    // but still release the key to the application. For example, Korean IME
    // handles ENTER key to confirm its composition but still release it for
    // the default behavior (e.g. trigger search, etc.)
    // In such case, don't do InsertChar because a key should only trigger the
    // keydown event once.
    client->InsertText(
        result_text,
        ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
    // If the client changes we assume that the original target has been
    // destroyed.
    if (client != GetTextInputClient())
      return CommitResult::kTargetDestroyed;
  }

  return CommitResult::kSuccess;
}

bool InputMethodAuraLinux::UpdateCompositionIfTextSelected() {
  TextInputClient* client = GetTextInputClient();
  if (!client || IsTextInputTypeNone()) {
    return false;
  }
  // In the special case where (1) there is no composition and (2) there is a
  // non-empty selection, calling SetCompositionText should delete the
  // selection, even when the call would otherwise be considered redundant.
  // For example, calling SetCompositionText('') when there is no composition
  // seems like it would have no effect, but it does if there is a non-empty
  // selection, so we make sure it is called in such cases. See b/223500609.
  if (!client->HasCompositionText() && composition_.text.empty() &&
      selection_range_.IsValid() && !selection_range_.is_empty()) {
    client->SetCompositionText(composition_);
    return true;
  }
  return false;
}

bool InputMethodAuraLinux::UpdateCompositionIfChanged(bool text_committed) {
  TextInputClient* client = GetTextInputClient();
  bool update_composition =
      client && composition_changed_ && !IsTextInputTypeNone();
  if (update_composition) {
    // If composition changed, does SetComposition if composition is not empty.
    // And ClearComposition if composition is empty.
    if (!composition_.text.empty())
      client->SetCompositionText(composition_);
    else if (!text_committed)
      client->ClearCompositionText();
  }

  // Makes sure the cached composition is cleared after committing any text or
  // cleared composition.
  if (client && !client->HasCompositionText())
    composition_ = CompositionText();

  return update_composition;
}

void InputMethodAuraLinux::UpdateContextFocusState() {
  surrounding_text_.reset();
  text_range_ = gfx::Range::InvalidRange();
  selection_range_ = gfx::Range::InvalidRange();

  auto old_text_input_type = text_input_type_;
  text_input_type_ = GetTextInputType();

  auto* client = GetTextInputClient();
  bool has_client = client != nullptr;
  TextInputClient::FocusReason reason;
  LinuxInputMethodContext::TextInputClientAttributes attributes;
  attributes.input_type = text_input_type_;
  if (client) {
    reason = client->GetFocusReason();
    attributes.input_mode = client->GetTextInputMode();
    attributes.flags = client->GetTextInputFlags();
    attributes.should_do_learning = client->ShouldDoLearning();
    attributes.can_compose_inline = client->CanComposeInline();
  } else {
    reason = text_input_type_ == TEXT_INPUT_TYPE_NONE
                 ? TextInputClient::FocusReason::FOCUS_REASON_NONE
                 : TextInputClient::FocusReason::FOCUS_REASON_OTHER;
  }

  context_->UpdateFocus(has_client, old_text_input_type, attributes, reason);
}

void InputMethodAuraLinux::OnTextInputTypeChanged(TextInputClient* client) {
  UpdateContextFocusState();
  InputMethodBase::OnTextInputTypeChanged(client);
  // TODO(yoichio): Support inputmode HTML attribute.
}

void InputMethodAuraLinux::OnCaretBoundsChanged(const TextInputClient* client) {
  if (!IsTextInputClientFocused(client))
    return;
  NotifyTextInputCaretBoundsChanged(client);
  context_->SetCursorLocation(GetTextInputClient()->GetCaretBounds());

  gfx::Range text_range, composition_range, selection_range;
  std::u16string text;
  if (client->GetTextRange(&text_range) &&
      client->GetTextFromRange(text_range, &text) &&
      client->GetEditableSelectionRange(&selection_range)) {
    if (!client->GetCompositionTextRange(&composition_range)) {
      // Some TextInputClients, like ARC for ChromeOS, may not support getting
      // composition text. So set it to invalid range in that case.
      composition_range = gfx::Range::InvalidRange();
    }
    std::optional<GrammarFragment> fragment;
    std::optional<AutocorrectInfo> autocorrect;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    fragment = client->GetGrammarFragmentAtCursor();
    autocorrect = AutocorrectInfo{
        client->GetAutocorrectRange(),
        client->GetAutocorrectCharacterBounds(),
    };
#endif
    if (surrounding_text_ != text || text_range_ != text_range ||
        selection_range_ != selection_range) {
      surrounding_text_ = text;
      text_range_ = text_range;
      selection_range_ = selection_range;
      context_->SetSurroundingText(text, text_range, composition_range,
                                   selection_range, fragment, autocorrect);
    }
  }
}

void InputMethodAuraLinux::CancelComposition(const TextInputClient* client) {
  if (!IsTextInputClientFocused(client))
    return;

  ResetContext();
}

void InputMethodAuraLinux::ResetContext() {
  if (!GetTextInputClient())
    return;

  is_sync_mode_ = true;

  if (!composition_.text.empty()) {
    // If the IME has an open composition, ignore non-synchronous attempts to
    // commit text for a brief duration of time.
    suppress_non_key_input_until_ =
        base::TimeTicks::Now() + kIgnoreCommitsDuration;
  }

  context_->Reset();

  composition_ = CompositionText();
  result_text_ = std::nullopt;
  is_sync_mode_ = false;
  composition_changed_ = false;
}

bool InputMethodAuraLinux::IgnoringNonKeyInput() const {
  return !is_sync_mode_ &&
         base::TimeTicks::Now() < suppress_non_key_input_until_;
}

bool InputMethodAuraLinux::IsCandidatePopupOpen() const {
  // There seems no way to detect candidate windows or any popups.
  return false;
}

VirtualKeyboardController*
InputMethodAuraLinux::GetVirtualKeyboardController() {
  // This should only be not null when set via testing.
  if (auto* controller = InputMethodBase::GetVirtualKeyboardController())
    return controller;
  return context_->GetVirtualKeyboardController();
}

// Overriden from ui::LinuxInputMethodContextDelegate

void InputMethodAuraLinux::OnCommit(const std::u16string& text) {
  if (IgnoringNonKeyInput() || !GetTextInputClient())
    return;

  // Discard the result iff in async-mode and the TextInputType is None
  // for backward compatibility.
  if (is_sync_mode_ || !IsTextInputTypeNone()) {
    if (result_text_) {
      result_text_->append(text);
    } else {
      result_text_ = text;
    }
  }

  // Sync mode means this is called on a stack of DispatchKeyEvent(), so its
  // following code should handle the key dispatch and actual committing.
  // If we are not handling key event, do not bother sending text result if
  // the focused text input client does not support text input.
  if (!is_sync_mode_ && !IsTextInputTypeNone()) {
    ui::KeyEvent event =
        ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_PROCESSKEY, 0);
    if (ime_filtered_key_event_.has_value()) {
      event = std::move(*ime_filtered_key_event_);
      ime_filtered_key_event_.reset();
      ui::EventDispatchDetails details =
          DispatchImeFilteredKeyPressEvent(&event);
      if (details.target_destroyed || details.dispatcher_destroyed ||
          event.stopped_propagation()) {
        return;
      }
    }
    last_commit_result_ = MaybeCommitResult(/*filtered=*/true, event);
    composition_ = CompositionText();
  }
}

void InputMethodAuraLinux::OnInsertImage(const GURL& src) {
  if (auto* text_input_client = GetTextInputClient()) {
    text_input_client->InsertImage(src);
  }
}

void InputMethodAuraLinux::OnConfirmCompositionText(bool keep_selection) {
  ConfirmCompositionText(keep_selection);
}

void InputMethodAuraLinux::OnDeleteSurroundingText(size_t before,
                                                   size_t after) {
  auto* client = GetTextInputClient();
  if (client && composition_.text.empty())
    client->ExtendSelectionAndDelete(before, after);
}

void InputMethodAuraLinux::OnPreeditChanged(
    const CompositionText& composition_text) {
  OnPreeditUpdate(composition_text, !is_sync_mode_);
}

void InputMethodAuraLinux::OnPreeditEnd() {
  TextInputClient* client = GetTextInputClient();
  OnPreeditUpdate(CompositionText(),
                  !is_sync_mode_ && client && client->HasCompositionText());
}

void InputMethodAuraLinux::OnSetPreeditRegion(
    const gfx::Range& range,
    const std::vector<ImeTextSpan>& spans) {
  auto* text_input_client = GetTextInputClient();
  if (!text_input_client)
    return;
  text_input_client->SetCompositionFromExistingText(range, spans);

  std::u16string text;
  if (text_input_client->GetTextFromRange(range, &text)) {
    composition_changed_ |= composition_.text != text;
    composition_.text = text;
  }
  last_commit_result_.reset();
}

void InputMethodAuraLinux::OnClearGrammarFragments(const gfx::Range& range) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* text_input_client = GetTextInputClient();
  if (!text_input_client)
    return;

  text_input_client->ClearGrammarFragments(range);
#endif
}

void InputMethodAuraLinux::OnAddGrammarFragment(
    const ui::GrammarFragment& fragment) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* text_input_client = GetTextInputClient();
  if (!text_input_client)
    return;

  text_input_client->AddGrammarFragments({fragment});
#endif
}

void InputMethodAuraLinux::OnSetAutocorrectRange(const gfx::Range& range) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* text_input_client = GetTextInputClient();
  if (!text_input_client)
    return;
  text_input_client->SetAutocorrectRange(range);
#endif
}

void InputMethodAuraLinux::OnSetVirtualKeyboardOccludedBounds(
    const gfx::Rect& screen_bounds) {
  SetVirtualKeyboardBounds(screen_bounds);
}

// Overridden from InputMethodBase.

void InputMethodAuraLinux::OnWillChangeFocusedClient(
    TextInputClient* focused_before,
    TextInputClient* focused) {
  ResetContext();
  context_->WillUpdateFocus(focused_before, focused);
}

void InputMethodAuraLinux::OnDidChangeFocusedClient(
    TextInputClient* focused_before,
    TextInputClient* focused) {
  UpdateContextFocusState();

  // Force to update caret bounds, in case the View thinks that the caret
  // bounds has not changed.
  if (text_input_type_ != TEXT_INPUT_TYPE_NONE)
    OnCaretBoundsChanged(GetTextInputClient());

  InputMethodBase::OnDidChangeFocusedClient(focused_before, focused);
}

// private

void InputMethodAuraLinux::OnPreeditUpdate(
    const ui::CompositionText& composition_text,
    bool force_update_client) {
  if (IgnoringNonKeyInput() || IsTextInputTypeNone())
    return;

  composition_changed_ |= composition_ != composition_text;
  composition_ = composition_text;

  if (!force_update_client)
    return;

  if (ime_filtered_key_event_.has_value()) {
    ui::KeyEvent event = std::move(*ime_filtered_key_event_);
    ime_filtered_key_event_.reset();
    ui::EventDispatchDetails details = DispatchImeFilteredKeyPressEvent(&event);
    if (details.target_destroyed || details.dispatcher_destroyed ||
        event.stopped_propagation()) {
      return;
    }
  }
  if (!UpdateCompositionIfTextSelected()) {
    UpdateCompositionIfChanged(last_commit_result_ == CommitResult::kSuccess);
  }
  last_commit_result_.reset();
}

bool InputMethodAuraLinux::HasInputMethodResult() {
  return result_text_ || composition_changed_;
}

bool InputMethodAuraLinux::NeedInsertChar(
    const std::optional<std::u16string>& result_text) const {
  return IsTextInputTypeNone() ||
         (!composition_changed_ && composition_.text.empty() && result_text &&
          result_text->length() == 1);
}

ui::EventDispatchDetails InputMethodAuraLinux::SendFakeProcessKeyEvent(
    ui::KeyEvent* event) const {
  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_PROCESSKEY,
                         event->code(), event->flags(), ui::DomKey::PROCESS,
                         event->time_stamp());
  ui::EventDispatchDetails details = DispatchKeyEventPostIME(&key_event);
  if (key_event.stopped_propagation())
    event->StopPropagation();
  return details;
}

void InputMethodAuraLinux::ConfirmCompositionText(bool keep_selection) {
  auto* client = GetTextInputClient();
  if (client)
    client->ConfirmCompositionText(keep_selection);
  composition_ = CompositionText();
  composition_changed_ = false;
  result_text_.reset();
}

}  // namespace ui
