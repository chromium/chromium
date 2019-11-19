// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/cocoa/text_input_host.h"

#include "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"

namespace {

// Returns the boundary rectangle for composition characters in the
// |requested_range|. Sets |actual_range| corresponding to the returned
// rectangle. For cases, where there is no composition text or the
// |requested_range| lies outside the composition range, a zero width rectangle
// corresponding to the caret bounds is returned. Logic used is similar to
// RenderWidgetHostViewMac::GetCachedFirstRectForCharacterRange(...).
gfx::Rect GetFirstRectForRangeHelper(const ui::TextInputClient* client,
                                     const gfx::Range& requested_range,
                                     gfx::Range* actual_range) {
  // NSRange doesn't support reversed ranges.
  DCHECK(!requested_range.is_reversed());
  DCHECK(actual_range);

  // Set up default return values, to be returned in case of unusual cases.
  gfx::Rect default_rect;
  *actual_range = gfx::Range::InvalidRange();
  if (!client)
    return default_rect;

  default_rect = client->GetCaretBounds();
  default_rect.set_width(0);

  // If possible, modify actual_range to correspond to caret position.
  gfx::Range selection_range;
  if (client->GetEditableSelectionRange(&selection_range)) {
    // Caret bounds correspond to end index of selection_range.
    *actual_range = gfx::Range(selection_range.end());
  }

  gfx::Range composition_range;
  if (!client->HasCompositionText() ||
      !client->GetCompositionTextRange(&composition_range) ||
      !composition_range.Contains(requested_range))
    return default_rect;

  DCHECK(!composition_range.is_reversed());

  const size_t from = requested_range.start() - composition_range.start();
  const size_t to = requested_range.end() - composition_range.start();

  // Pick the first character's bounds as the initial rectangle, then grow it to
  // the full |requested_range| if possible.
  const bool request_is_composition_end = from == composition_range.length();
  const size_t first_index = request_is_composition_end ? from - 1 : from;
  gfx::Rect union_rect;
  if (!client->GetCompositionCharacterBounds(first_index, &union_rect))
    return default_rect;

  // If requested_range is empty, return a zero width rectangle corresponding to
  // it.
  if (from == to) {
    if (request_is_composition_end &&
        client->GetTextDirection() != base::i18n::RIGHT_TO_LEFT) {
      // In case of an empty requested range at end of composition, return the
      // rectangle to the right of the last compositioned character.
      union_rect.set_origin(union_rect.top_right());
    }
    union_rect.set_width(0);
    *actual_range = requested_range;
    return union_rect;
  }

  // Toolkit-views textfields are always single-line, so no need to check for
  // line breaks.
  for (size_t i = from + 1; i < to; i++) {
    gfx::Rect current_rect;
    if (client->GetCompositionCharacterBounds(i, &current_rect)) {
      union_rect.Union(current_rect);
    } else {
      *actual_range =
          gfx::Range(requested_range.start(), i + composition_range.start());
      return union_rect;
    }
  }
  *actual_range = requested_range;
  return union_rect;
}

// Returns the string corresponding to |requested_range| for the given |client|.
// If a gfx::Range::InvalidRange() is passed, the full string stored by |client|
// is returned. Sets |actual_range| corresponding to the returned string.
base::string16 AttributedSubstringForRangeHelper(
    const ui::TextInputClient* client,
    const gfx::Range& requested_range,
    gfx::Range* actual_range) {
  // NSRange doesn't support reversed ranges.
  DCHECK(!requested_range.is_reversed());
  DCHECK(actual_range);

  base::string16 substring;
  gfx::Range text_range;
  *actual_range = gfx::Range::InvalidRange();
  if (!client || !client->GetTextRange(&text_range))
    return substring;

  // gfx::Range::Intersect() behaves a bit weirdly. If B is an empty range
  // contained inside a non-empty range A, B intersection A returns
  // gfx::Range::InvalidRange(), instead of returning B.
  *actual_range = text_range.Contains(requested_range)
                      ? requested_range
                      : text_range.Intersect(requested_range);

  // This is a special case for which the complete string should should be
  // returned. NSTextView also follows this, though the same is not mentioned in
  // NSTextInputClient documentation.
  if (!requested_range.IsValid())
    *actual_range = text_range;

  client->GetTextFromRange(*actual_range, &substring);
  return substring;
}

}  // namespace

namespace views {

////////////////////////////////////////////////////////////////////////////////
// TextInputHost, public:

TextInputHost::TextInputHost(NativeWidgetMacNSWindowHost* host_impl)
    : host_impl_(host_impl) {}

TextInputHost::~TextInputHost() = default;

void TextInputHost::BindReceiver(
    mojo::PendingAssociatedReceiver<remote_cocoa::mojom::TextInputHost>
        receiver) {
  mojo_receiver_.Bind(std::move(receiver),
                      ui::WindowResizeHelperMac::Get()->task_runner());
}

ui::TextInputClient* TextInputHost::GetTextInputClient() const {
  return text_input_client_;
}

void TextInputHost::SetTextInputClient(
    ui::TextInputClient* new_text_input_client) {
  if (pending_text_input_client_ == new_text_input_client)
    return;

  // This method may cause the IME window to dismiss, which may cause it to
  // insert text (e.g. to replace marked text with "real" text). That should
  // happen in the old -inputContext (which AppKit stores a reference to).
  // Unfortunately, the only way to invalidate the the old -inputContext is to
  // invoke -[NSApp updateWindows], which also wants a reference to the _new_
  // -inputContext. So put the new inputContext in |pendingTextInputClient_| and
  // only use it for -inputContext.
  ui::TextInputClient* old_text_input_client = text_input_client_;

  // Since dismissing an IME may insert text, a misbehaving IME or a
  // ui::TextInputClient that acts on InsertChar() to change focus a second time
  // may invoke -setTextInputClient: recursively; with [NSApp updateWindows]
  // still on the stack. Calling [NSApp updateWindows] recursively may upset
  // an IME. Since the rest of this method is only to decide whether to call
  // updateWindows, and we're already calling it, just bail out.
  if (text_input_client_ != pending_text_input_client_) {
    pending_text_input_client_ = new_text_input_client;
    return;
  }

  // Start by assuming no need to invoke -updateWindows.
  text_input_client_ = new_text_input_client;
  pending_text_input_client_ = new_text_input_client;

  if (host_impl_->in_process_ns_window_bridge_ &&
      host_impl_->in_process_ns_window_bridge_->NeedsUpdateWindows()) {
    text_input_client_ = old_text_input_client;
    [NSApp updateWindows];
    // Note: |pending_text_input_client_| (and therefore +[NSTextInputContext
    // currentInputContext] may have changed if called recursively.
    text_input_client_ = pending_text_input_client_;
  }
}

////////////////////////////////////////////////////////////////////////////////
// TextInputHost, remote_cocoa::mojom::TextInputHost:

bool TextInputHost::HasClient(bool* out_has_client) {
  *out_has_client = text_input_client_ != nullptr;
  return true;
}

bool TextInputHost::HasInputContext(bool* out_has_input_context) {
  *out_has_input_context = false;

  // If the textInputClient_ does not exist, return nil since this view does not
  // conform to NSTextInputClient protocol.
  if (!pending_text_input_client_)
    return true;

  // If a menu is active, and -[NSView interpretKeyEvents:] asks for the
  // input context, return nil. This ensures the action message is sent to
  // the view, rather than any NSTextInputClient a subview has installed.
  bool has_menu_controller = false;
  host_impl_->GetHasMenuController(&has_menu_controller);
  if (has_menu_controller)
    return true;

  // When not in an editable mode, or while entering passwords
  // (http://crbug.com/23219), we don't want to show IME candidate windows.
  // Returning nil prevents this view from getting messages defined as part of
  // the NSTextInputClient protocol.
  switch (pending_text_input_client_->GetTextInputType()) {
    case ui::TEXT_INPUT_TYPE_NONE:
    case ui::TEXT_INPUT_TYPE_PASSWORD:
      return true;
    default:
      *out_has_input_context = true;
  }
  return true;
}

bool TextInputHost::IsRTL(bool* out_is_rtl) {
  *out_is_rtl = text_input_client_ && text_input_client_->GetTextDirection() ==
                                          base::i18n::RIGHT_TO_LEFT;
  return true;
}

bool TextInputHost::GetSelectionRange(gfx::Range* out_range) {
  if (!text_input_client_ ||
      !text_input_client_->GetEditableSelectionRange(out_range)) {
    *out_range = gfx::Range::InvalidRange();
  }
  return true;
}

bool TextInputHost::GetSelectionText(bool* out_result,
                                     base::string16* out_text) {
  *out_result = false;
  if (!text_input_client_)
    return true;
  gfx::Range selection_range;
  if (!text_input_client_->GetEditableSelectionRange(&selection_range))
    return true;
  *out_result = text_input_client_->GetTextFromRange(selection_range, out_text);
  return true;
}

void TextInputHost::InsertText(const base::string16& text, bool as_character) {
  if (!text_input_client_)
    return;
  if (as_character) {
    // If a single character is inserted by keyDown's call to
    // interpretKeyEvents: then use InsertChar() to allow editing events to be
    // merged. We use ui::VKEY_UNKNOWN as the key code since it's not feasible
    // to determine the correct key code for each unicode character. Also a
    // correct keycode is not needed in the current context. Send ui::EF_NONE as
    // the key modifier since |text| already accounts for the pressed key
    // modifiers.
    text_input_client_->InsertChar(ui::KeyEvent(
        text[0], ui::VKEY_UNKNOWN, ui::DomCode::NONE, ui::EF_NONE));
  } else {
    text_input_client_->InsertText(text);
  }
}

void TextInputHost::DeleteRange(const gfx::Range& range) {
  if (!text_input_client_)
    return;
  text_input_client_->DeleteRange(range);
}

void TextInputHost::SetCompositionText(const base::string16& text,
                                       const gfx::Range& selected_range,
                                       const gfx::Range& replacement_range) {
  if (!text_input_client_)
    return;

  text_input_client_->DeleteRange(replacement_range);
  ui::CompositionText composition;
  composition.text = text;
  composition.selection = selected_range;

  // Add an underline with text color and a transparent background to the
  // composition text. TODO(karandeepb): On Cocoa textfields, the target clause
  // of the composition has a thick underlines. The composition text also has
  // discontinuous underlines for different clauses. This is also supported in
  // the Chrome renderer. Add code to extract underlines from |text| once our
  // render text implementation supports thick underlines and discontinuous
  // underlines for consecutive characters. See http://crbug.com/612675.
  composition.ime_text_spans.push_back(
      ui::ImeTextSpan(ui::ImeTextSpan::Type::kComposition, 0, text.length(),
                      ui::ImeTextSpan::Thickness::kThin, SK_ColorTRANSPARENT));
  text_input_client_->SetCompositionText(composition);
}

void TextInputHost::ConfirmCompositionText() {
  if (!text_input_client_)
    return;
  text_input_client_->ConfirmCompositionText(/* keep_selection */ false);
}

bool TextInputHost::HasCompositionText(bool* out_has_composition_text) {
  *out_has_composition_text = false;
  if (!text_input_client_)
    return true;
  *out_has_composition_text = text_input_client_->HasCompositionText();
  return true;
  return true;
}

bool TextInputHost::GetCompositionTextRange(gfx::Range* out_composition_range) {
  *out_composition_range = gfx::Range::InvalidRange();
  if (!text_input_client_)
    return true;
  if (!text_input_client_->HasCompositionText())
    return true;
  text_input_client_->GetCompositionTextRange(out_composition_range);
  return true;
}

bool TextInputHost::GetAttributedSubstringForRange(
    const gfx::Range& requested_range,
    base::string16* out_text,
    gfx::Range* out_actual_range) {
  *out_text = AttributedSubstringForRangeHelper(
      text_input_client_, requested_range, out_actual_range);
  return true;
}

bool TextInputHost::GetFirstRectForRange(const gfx::Range& requested_range,
                                         gfx::Rect* out_rect,
                                         gfx::Range* out_actual_range) {
  *out_rect = GetFirstRectForRangeHelper(text_input_client_, requested_range,
                                         out_actual_range);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// TextInputHost, remote_cocoa::mojom::TextInputHost synchronous methods:

void TextInputHost::HasClient(HasClientCallback callback) {
  bool has_client = false;
  HasClient(&has_client);
  std::move(callback).Run(has_client);
}

void TextInputHost::HasInputContext(HasInputContextCallback callback) {
  bool has_input_context = false;
  HasClient(&has_input_context);
  std::move(callback).Run(has_input_context);
}

void TextInputHost::IsRTL(IsRTLCallback callback) {
  bool is_rtl = false;
  IsRTL(&is_rtl);
  std::move(callback).Run(is_rtl);
}

void TextInputHost::GetSelectionRange(GetSelectionRangeCallback callback) {
  gfx::Range range = gfx::Range::InvalidRange();
  GetSelectionRange(&range);
  std::move(callback).Run(range);
}

void TextInputHost::GetSelectionText(GetSelectionTextCallback callback) {
  bool result = false;
  base::string16 text;
  GetSelectionText(&result, &text);
  std::move(callback).Run(result, text);
}

void TextInputHost::HasCompositionText(HasCompositionTextCallback callback) {
  bool has_composition_text = false;
  IsRTL(&has_composition_text);
  std::move(callback).Run(has_composition_text);
}

void TextInputHost::GetCompositionTextRange(
    GetCompositionTextRangeCallback callback) {
  gfx::Range range = gfx::Range::InvalidRange();
  GetCompositionTextRange(&range);
  std::move(callback).Run(range);
}

void TextInputHost::GetAttributedSubstringForRange(
    const gfx::Range& requested_range,
    GetAttributedSubstringForRangeCallback callback) {
  base::string16 text;
  gfx::Range actual_range = gfx::Range::InvalidRange();
  GetAttributedSubstringForRange(requested_range, &text, &actual_range);
  std::move(callback).Run(text, actual_range);
}

void TextInputHost::GetFirstRectForRange(
    const gfx::Range& requested_range,
    GetFirstRectForRangeCallback callback) {
  gfx::Rect rect;
  gfx::Range actual_range;
  GetFirstRectForRange(requested_range, &rect, &actual_range);
  std::move(callback).Run(rect, actual_range);
}

}  // namespace views
