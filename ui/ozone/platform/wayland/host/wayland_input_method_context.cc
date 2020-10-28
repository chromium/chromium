// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_input_method_context.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/ime_text_span.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/range/range.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/zwp_text_input_wrapper_v1.h"
#include "ui/ozone/public/ozone_switches.h"

#if BUILDFLAG(USE_XKBCOMMON)
#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"
#endif

namespace ui {
namespace {

base::Optional<size_t> OffsetFromUTF8Offset(const base::StringPiece& text,
                                            uint32_t offset) {
  if (offset > text.length())
    return base::nullopt;

  base::string16 converted;
  if (!base::UTF8ToUTF16(text.data(), offset, &converted))
    return base::nullopt;

  return converted.size();
}

}  // namespace

WaylandInputMethodContext::WaylandInputMethodContext(
    WaylandConnection* connection,
    WaylandKeyboard::Delegate* key_delegate,
    LinuxInputMethodContextDelegate* ime_delegate,
    bool is_simple)
    : connection_(connection),
      key_delegate_(key_delegate),
      ime_delegate_(ime_delegate),
      is_simple_(is_simple),
      text_input_(nullptr) {
  Init();
}

WaylandInputMethodContext::~WaylandInputMethodContext() {
  if (text_input_) {
    text_input_->Deactivate();
    text_input_->HideInputPanel();
  }
}

void WaylandInputMethodContext::Init(bool initialize_for_testing) {
  bool use_ozone_wayland_vkb =
      initialize_for_testing ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWaylandIme);
  // If text input instance is not created then all ime context operations
  // are noop. This option is because in some environments someone might not
  // want to enable ime/virtual keyboard even if it's available.
  if (use_ozone_wayland_vkb && !is_simple_ && !text_input_ &&
      connection_->text_input_manager_v1()) {
    text_input_ = std::make_unique<ZWPTextInputWrapperV1>(
        connection_->text_input_manager_v1());
    text_input_->Initialize(connection_, this);
  }
}

bool WaylandInputMethodContext::DispatchKeyEvent(
    const ui::KeyEvent& key_event) {
  if (key_event.type() != ET_KEY_PRESSED ||
      !character_composer_.FilterKeyPress(key_event))
    return false;

  // CharacterComposer consumed the key event. Update the composition text.
  UpdatePreeditText(character_composer_.preedit_string());
  auto composed = character_composer_.composed_character();
  if (!composed.empty())
    ime_delegate_->OnCommit(composed);
  return true;
}

void WaylandInputMethodContext::UpdatePreeditText(
    const base::string16& preedit_text) {
  CompositionText preedit;
  preedit.text = preedit_text;
  auto length = preedit.text.size();

  preedit.selection = gfx::Range(length);
  preedit.ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, length, ImeTextSpan::Thickness::kThin,
      ImeTextSpan::UnderlineStyle::kSolid, SK_ColorTRANSPARENT));
  ime_delegate_->OnPreeditChanged(preedit);
}

void WaylandInputMethodContext::Reset() {
  character_composer_.Reset();
  if (text_input_)
    text_input_->Reset();
}

void WaylandInputMethodContext::Focus() {
  WaylandWindow* window =
      connection_->wayland_window_manager()->GetCurrentKeyboardFocusedWindow();
  if (!text_input_ || !window)
    return;

  text_input_->Activate(window);
  text_input_->ShowInputPanel();
}

void WaylandInputMethodContext::Blur() {
  if (text_input_) {
    text_input_->Deactivate();
    text_input_->HideInputPanel();
  }
}

void WaylandInputMethodContext::SetCursorLocation(const gfx::Rect& rect) {
  if (text_input_)
    text_input_->SetCursorRect(rect);
}

void WaylandInputMethodContext::SetSurroundingText(
    const base::string16& text,
    const gfx::Range& selection_range) {
  if (text_input_)
    text_input_->SetSurroundingText(text, selection_range);
}

void WaylandInputMethodContext::OnPreeditString(
    base::StringPiece text,
    const std::vector<SpanStyle>& spans,
    int32_t preedit_cursor) {
  ui::CompositionText composition_text;
  composition_text.text = base::UTF8ToUTF16(text);
  for (const auto& span : spans) {
    ImeTextSpan text_span;
    auto start_offset = OffsetFromUTF8Offset(text, span.index);
    if (!start_offset)
      continue;
    text_span.start_offset = *start_offset;
    auto end_offset = OffsetFromUTF8Offset(text, span.index + span.length);
    if (!end_offset)
      continue;
    text_span.end_offset = *end_offset;
    bool supported = true;
    switch (span.style) {
      case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_DEFAULT:
        break;
      case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_HIGHLIGHT:
        text_span.thickness = ImeTextSpan::Thickness::kThick;
        break;
      case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_UNDERLINE:
        text_span.thickness = ImeTextSpan::Thickness::kThin;
        break;
      case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_SELECTION:
        text_span.type = ImeTextSpan::Type::kSuggestion;
        break;
      case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_INCORRECT:
        text_span.type = ImeTextSpan::Type::kMisspellingSuggestion;
        break;
      case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_NONE:
      case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_ACTIVE:
      case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_INACTIVE:
      default:
        VLOG(1) << "Unsupported style. Skipped: " << span.style;
        supported = false;
        break;
    }
    if (!supported)
      continue;
    composition_text.ime_text_spans.push_back(std::move(text_span));
  }

  if (preedit_cursor < 0) {
    composition_text.selection = gfx::Range::InvalidRange();
  } else {
    auto cursor =
        OffsetFromUTF8Offset(text, static_cast<uint32_t>(preedit_cursor));
    if (!cursor) {
      // Invalid cursor position. Do nothing.
      return;
    }
    composition_text.selection = gfx::Range(*cursor);
  }

  ime_delegate_->OnPreeditChanged(composition_text);
}

void WaylandInputMethodContext::OnCommitString(base::StringPiece text) {
  ime_delegate_->OnCommit(base::UTF8ToUTF16(text));
}

void WaylandInputMethodContext::OnDeleteSurroundingText(int32_t index,
                                                        uint32_t length) {
  ime_delegate_->OnDeleteSurroundingText(index, length);
}

void WaylandInputMethodContext::OnKeysym(uint32_t keysym,
                                         uint32_t state,
                                         uint32_t modifiers) {
#if BUILDFLAG(USE_XKBCOMMON)
  // TODO(crbug.com/1079353): Handle modifiers.
  DomCode dom_code =
      connection_->keyboard()->layout_engine()->GetDomCodeByKeysym(keysym);
  if (dom_code == DomCode::NONE)
    return;

  EventType type =
      state == WL_KEYBOARD_KEY_STATE_PRESSED ? ET_KEY_PRESSED : ET_KEY_RELEASED;
  key_delegate_->OnKeyboardKeyEvent(type, dom_code, /*repeat=*/false,
                                    EventTimeForNow());
#else
  NOTIMPLEMENTED();
#endif
}

}  // namespace ui
