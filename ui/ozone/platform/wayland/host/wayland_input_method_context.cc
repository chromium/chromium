// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_input_method_context.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/i18n/char_iterator.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/ime_text_span.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/range/range.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/host/zwp_text_input_wrapper_v1.h"
#include "ui/ozone/public/ozone_switches.h"

#if BUILDFLAG(USE_XKBCOMMON)
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/check.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif

namespace ui {
namespace {

absl::optional<size_t> OffsetFromUTF8Offset(const base::StringPiece& text,
                                            uint32_t offset) {
  if (offset > text.length())
    return absl::nullopt;

  std::u16string converted;
  if (!base::UTF8ToUTF16(text.data(), offset, &converted))
    return absl::nullopt;

  return converted.size();
}

bool IsImeEnabled() {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  // We do not expect both switches are set at the same time.
  DCHECK(!cmd_line->HasSwitch(switches::kEnableWaylandIme) ||
         !cmd_line->HasSwitch(switches::kDisableWaylandIme));
  // Force enable/disable wayland IMEs, when explictly specified via commandline
  // arguments.
  if (cmd_line->HasSwitch(switches::kEnableWaylandIme))
    return true;
  if (cmd_line->HasSwitch(switches::kDisableWaylandIme))
    return false;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // On Lacros chrome, we check whether ash-chrome supports IME, then
  // enable IME if so. This allows us to control IME enabling state in
  // Lacros-chrome side, which helps us on releasing.
  // TODO(crbug.com/1159237): In the future, we may want to unify the behavior
  // of ozone/wayland across platforms.
  const auto* lacros_service = chromeos::LacrosService::Get();

  // Note: |init_params| may be null, if ash-chrome is too old.
  // TODO(crbug.com/1156033): Clean up the condition, after ash-chrome in the
  // world becomes new enough.
  const crosapi::mojom::BrowserInitParams* init_params =
      lacros_service ? lacros_service->init_params() : nullptr;
  if (init_params && init_params->exo_ime_support !=
                         crosapi::mojom::ExoImeSupport::kUnsupported) {
    return true;
  }
#endif

  // Do not enable wayland IME by default.
  return false;
}

// Returns ImeTextSpan style to be assigned. Maybe nullopt if it is not
// supported.
absl::optional<std::pair<ImeTextSpan::Type, ImeTextSpan::Thickness>>
ConvertStyle(uint32_t style) {
  switch (style) {
    case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_DEFAULT:
      return absl::make_optional(std::make_pair(ImeTextSpan::Type::kComposition,
                                                ImeTextSpan::Thickness::kNone));
    case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_HIGHLIGHT:
      return absl::make_optional(std::make_pair(
          ImeTextSpan::Type::kComposition, ImeTextSpan::Thickness::kThick));
    case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_UNDERLINE:
      return absl::make_optional(std::make_pair(ImeTextSpan::Type::kComposition,
                                                ImeTextSpan::Thickness::kThin));
    case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_SELECTION:
      return absl::make_optional(std::make_pair(ImeTextSpan::Type::kSuggestion,
                                                ImeTextSpan::Thickness::kNone));
    case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_INCORRECT:
      return absl::make_optional(
          std::make_pair(ImeTextSpan::Type::kMisspellingSuggestion,
                         ImeTextSpan::Thickness::kNone));
    case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_NONE:
    case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_ACTIVE:
    case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_INACTIVE:
    default:
      VLOG(1) << "Unsupported style. Skipped: " << style;
  }
  return absl::nullopt;
}

// Converts Chrome's TextInputType into wayland's content_purpose.
// Some of TextInputType values do not have clearly corresponding wayland value,
// and they fallback to closer type.
uint32_t InputTypeToContentPurpose(TextInputType input_type) {
  switch (input_type) {
    case TEXT_INPUT_TYPE_NONE:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NORMAL;
    case TEXT_INPUT_TYPE_TEXT:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NORMAL;
    case TEXT_INPUT_TYPE_PASSWORD:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_PASSWORD;
    case TEXT_INPUT_TYPE_SEARCH:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NORMAL;
    case TEXT_INPUT_TYPE_EMAIL:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_EMAIL;
    case TEXT_INPUT_TYPE_NUMBER:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NUMBER;
    case TEXT_INPUT_TYPE_TELEPHONE:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_PHONE;
    case TEXT_INPUT_TYPE_URL:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_URL;
    case TEXT_INPUT_TYPE_DATE:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_DATE;
    case TEXT_INPUT_TYPE_DATE_TIME:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_DATETIME;
    case TEXT_INPUT_TYPE_DATE_TIME_LOCAL:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_DATETIME;
    case TEXT_INPUT_TYPE_MONTH:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_DATE;
    case TEXT_INPUT_TYPE_TIME:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_TIME;
    case TEXT_INPUT_TYPE_WEEK:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_DATE;
    case TEXT_INPUT_TYPE_TEXT_AREA:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NORMAL;
    case TEXT_INPUT_TYPE_CONTENT_EDITABLE:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NORMAL;
    case TEXT_INPUT_TYPE_DATE_TIME_FIELD:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_DATETIME;
    case TEXT_INPUT_TYPE_NULL:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NORMAL;
  }
}

// Converts Chrome's TextInputType into wayland's content_hint.
uint32_t InputFlagsToContentHint(int input_flags) {
  uint32_t hint = 0;
  if (input_flags & TEXT_INPUT_FLAG_AUTOCOMPLETE_ON)
    hint |= ZWP_TEXT_INPUT_V1_CONTENT_HINT_AUTO_COMPLETION;
  if (input_flags & TEXT_INPUT_FLAG_AUTOCORRECT_ON)
    hint |= ZWP_TEXT_INPUT_V1_CONTENT_HINT_AUTO_CORRECTION;
  // No good match. Fallback to AUTO_CORRECTION.
  if (input_flags & TEXT_INPUT_FLAG_SPELLCHECK_ON)
    hint |= ZWP_TEXT_INPUT_V1_CONTENT_HINT_AUTO_CORRECTION;
  if (input_flags & TEXT_INPUT_FLAG_AUTOCAPITALIZE_CHARACTERS)
    hint |= ZWP_TEXT_INPUT_V1_CONTENT_HINT_UPPERCASE;
  if (input_flags & TEXT_INPUT_FLAG_AUTOCAPITALIZE_WORDS)
    hint |= ZWP_TEXT_INPUT_V1_CONTENT_HINT_TITLECASE;
  if (input_flags & TEXT_INPUT_FLAG_AUTOCAPITALIZE_SENTENCES)
    hint |= ZWP_TEXT_INPUT_V1_CONTENT_HINT_AUTO_CAPITALIZATION;
  if (input_flags & TEXT_INPUT_FLAG_HAS_BEEN_PASSWORD)
    hint |= ZWP_TEXT_INPUT_V1_CONTENT_HINT_PASSWORD;
  return hint;
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
  connection_->wayland_window_manager()->AddObserver(this);
  Init();
}

WaylandInputMethodContext::~WaylandInputMethodContext() {
  if (text_input_) {
    text_input_->Deactivate();
    text_input_->HideInputPanel();
  }
  connection_->wayland_window_manager()->RemoveObserver(this);
}

void WaylandInputMethodContext::Init(bool initialize_for_testing) {
  bool use_ozone_wayland_vkb = initialize_for_testing || IsImeEnabled();

  // If text input instance is not created then all ime context operations
  // are noop. This option is because in some environments someone might not
  // want to enable ime/virtual keyboard even if it's available.
  if (use_ozone_wayland_vkb && !is_simple_ && !text_input_ &&
      connection_->text_input_manager_v1()) {
    text_input_ = std::make_unique<ZWPTextInputWrapperV1>(
        connection_, this, connection_->text_input_manager_v1(),
        connection_->text_input_extension_v1());
  }
}

bool WaylandInputMethodContext::DispatchKeyEvent(
    const ui::KeyEvent& key_event) {
  if (key_event.type() != ET_KEY_PRESSED)
    return false;

  // Consume all peek key event.
  if (IsPeekKeyEvent(key_event))
    return true;

  // This is the fallback key event which was not consumed by IME.
  // So, process it inside Chrome.
  if (!character_composer_.FilterKeyPress(key_event))
    return false;

  // CharacterComposer consumed the key event. Update the composition text.
  UpdatePreeditText(character_composer_.preedit_string());
  auto composed = character_composer_.composed_character();
  if (!composed.empty())
    ime_delegate_->OnCommit(composed);
  return true;
}

bool WaylandInputMethodContext::IsPeekKeyEvent(const ui::KeyEvent& key_event) {
  const auto* properties = key_event.properties();
  if (!properties)
    return true;
  auto it = properties->find(kPropertyKeyboardImeFlag);
  if (it == properties->end())
    return true;
  return !(it->second[0] & kPropertyKeyboardImeIgnoredFlag);
}

void WaylandInputMethodContext::UpdatePreeditText(
    const std::u16string& preedit_text) {
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
  focused_ = true;
  MaybeUpdateActivated();
}

void WaylandInputMethodContext::Blur() {
  focused_ = false;
  MaybeUpdateActivated();
}

void WaylandInputMethodContext::SetCursorLocation(const gfx::Rect& rect) {
  if (text_input_)
    text_input_->SetCursorRect(rect);
}

void WaylandInputMethodContext::SetSurroundingText(
    const std::u16string& text,
    const gfx::Range& selection_range) {
  if (!text_input_)
    return;

  // The text length for set_surrounding_text can not be longer than the maximum
  // length of wayland messages. The maximum length of the text is explicitly
  // specified as 4000 in the protocol spec of text-input-unstable-v3.
  static constexpr size_t kWaylandMessageDataMaxLength = 4000;

  // Convert |text| and |selection_range| into UTF8 form.
  std::vector<size_t> offsets_for_adjustment = {selection_range.start(),
                                                selection_range.end()};
  const std::string text_utf8 =
      base::UTF16ToUTF8AndAdjustOffsets(text, &offsets_for_adjustment);
  if (offsets_for_adjustment[0] == std::u16string::npos ||
      offsets_for_adjustment[1] == std::u16string::npos) {
    LOG(DFATAL) << "The selection range is invalid.";
    return;
  }
  gfx::Range selection_range_utf8 = {
      static_cast<uint32_t>(offsets_for_adjustment[0]),
      static_cast<uint32_t>(offsets_for_adjustment[1])};

  // If the selection range in UTF8 form is longer than the maximum length of
  // wayland messages, skip sending set_surrounding_text requests.
  if (selection_range_utf8.length() > kWaylandMessageDataMaxLength)
    return;

  surrounding_text_ = text_utf8;
  selection_range_utf8_ = selection_range_utf8;

  if (text_utf8.size() <= kWaylandMessageDataMaxLength) {
    // We separate this case to run the function simpler and faster since this
    // condition is satisfied in most cases.
    surrounding_text_offset_ = 0;
    text_input_->SetSurroundingText(text_utf8, selection_range_utf8);
    return;
  }

  // If the text in UTF8 form is longer than the maximum length of wayland
  // messages while the selection range in UTF8 form is not, truncate the text
  // into the limitation and adjust indices of |selection_range|.

  // Decide where to start. The truncated text should be around the selection
  // range. We choose a text whose center point is same to the center of the
  // selection range unless this chosen text is shorter than the maximum
  // length of wayland messages because of the original text position.
  uint32_t selection_range_utf8_center =
      selection_range_utf8.start() + selection_range_utf8.length() / 2;
  // The substring starting with |start_index| might be invalid as UTF8.
  size_t start_index;
  if (selection_range_utf8_center <= kWaylandMessageDataMaxLength / 2) {
    // The selection range is near enough to the start point of original text.
    start_index = 0;
  } else if (text_utf8.size() - selection_range_utf8_center <
             kWaylandMessageDataMaxLength / 2) {
    // The selection range is near enough to the end point of original text.
    start_index = text_utf8.size() - kWaylandMessageDataMaxLength;
  } else {
    // Choose a text whose center point is same to the center of the selection
    // range.
    start_index =
        selection_range_utf8_center - kWaylandMessageDataMaxLength / 2;
  }

  // Truncate the text to fit into the wayland message size and adjust indices
  // of |selection_range|. Since the text is in UTF8 form, we need to adjust
  // the text and selection range positions where all characters are valid.
  //
  // TODO(crbug.com/1214957): We should use base::i18n::BreakIterator
  // to get the offsets and convert it into UTF8 form instead of using
  // UTF8CharIterator.
  base::i18n::UTF8CharIterator iter(text_utf8);
  while (iter.array_pos() < start_index)
    iter.Advance();
  size_t truncated_text_start = iter.array_pos();
  size_t truncated_text_end;
  while (iter.array_pos() <= start_index + kWaylandMessageDataMaxLength) {
    truncated_text_end = iter.array_pos();
    if (!iter.Advance())
      break;
  }

  std::string truncated_text = text_utf8.substr(
      truncated_text_start, truncated_text_end - truncated_text_start);
  gfx::Range relocated_selection_range(
      selection_range_utf8.start() - truncated_text_start,
      selection_range_utf8.end() - truncated_text_start);
  DCHECK(relocated_selection_range.IsBoundedBy(
      gfx::Range(0, kWaylandMessageDataMaxLength)));
  surrounding_text_offset_ = truncated_text_start;
  text_input_->SetSurroundingText(truncated_text, relocated_selection_range);
}

void WaylandInputMethodContext::SetContentType(TextInputType input_type,
                                               int input_flags) {
  if (!text_input_)
    return;

  uint32_t content_purpose = InputTypeToContentPurpose(input_type);
  uint32_t content_hint = InputFlagsToContentHint(input_flags);
  text_input_->SetContentType(content_hint, content_purpose);
}

VirtualKeyboardController*
WaylandInputMethodContext::GetVirtualKeyboardController() {
  if (!text_input_)
    return nullptr;
  return this;
}

bool WaylandInputMethodContext::DisplayVirtualKeyboard() {
  if (!text_input_)
    return false;

  text_input_->ShowInputPanel();
  return true;
}

void WaylandInputMethodContext::DismissVirtualKeyboard() {
  if (!text_input_)
    return;

  text_input_->HideInputPanel();
}

void WaylandInputMethodContext::AddObserver(
    VirtualKeyboardControllerObserver* observer) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WaylandInputMethodContext::RemoveObserver(
    VirtualKeyboardControllerObserver* observer) {
  NOTIMPLEMENTED_LOG_ONCE();
}

bool WaylandInputMethodContext::IsKeyboardVisible() {
  return virtual_keyboard_visible_;
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
    auto end_offset = OffsetFromUTF8Offset(text, span.index + span.length);
    if (!end_offset)
      continue;
    auto style = ConvertStyle(span.style);
    if (!style.has_value())
      continue;
    composition_text.ime_text_spans.emplace_back(
        /* type= */ style->first, *start_offset, *end_offset,
        /* thickness = */ style->second);
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
  // |index| and |length| are expected to be in UTF8 form, so we convert these
  // into UTF16 form.
  // Here, we use the surrounding text stored in SetSurroundingText which should
  // be called before OnDeleteSurroundingText.
  if (surrounding_text_.empty()) {
    LOG(DFATAL)
        << "SetSurroundingText should run before OnDeleteSurroundingText.";
    return;
  }

  // TODO(crbug.com/1227590): Currently data sent from delete surrounding text
  // from exo is broken. Currently this broken behavior is supported to prevent
  // visible regressions, but should be fixed in the future, specifically the
  // compatibility with non-exo wayland compositors.
  std::vector<size_t> offsets_for_adjustment = {
      selection_range_utf8_.GetMin(),
      selection_range_utf8_.GetMax(),
      surrounding_text_offset_ + index,
      surrounding_text_offset_ + index + length,
  };
  base::UTF8ToUTF16AndAdjustOffsets(surrounding_text_, &offsets_for_adjustment);
  if (base::Contains(offsets_for_adjustment, std::u16string::npos)) {
    LOG(DFATAL) << "The selection range for surrounding text is invalid.";
    return;
  }

  if (offsets_for_adjustment[0] < offsets_for_adjustment[2] ||
      offsets_for_adjustment[1] > offsets_for_adjustment[3]) {
    // The range is started after the selection, or ended before the selection,
    // which is not supported.
    LOG(DFATAL) << "The deletion range needs to cover whole selection range.";
    return;
  }

  // Move by offset calculated in SetSurroundingText to adjust to the original
  // text place.
  ime_delegate_->OnDeleteSurroundingText(
      /* before= */ offsets_for_adjustment[0] - offsets_for_adjustment[2],
      /* after= */ offsets_for_adjustment[3] - offsets_for_adjustment[1]);
}

void WaylandInputMethodContext::OnKeysym(uint32_t keysym,
                                         uint32_t state,
                                         uint32_t modifiers) {
#if BUILDFLAG(USE_XKBCOMMON)
  auto* layout_engine = KeyboardLayoutEngineManager::GetKeyboardLayoutEngine();
  if (!layout_engine)
    return;

  // TODO(crbug.com/1079353): Handle modifiers.
  DomCode dom_code = static_cast<XkbKeyboardLayoutEngine*>(layout_engine)
                         ->GetDomCodeByKeysym(keysym);
  if (dom_code == DomCode::NONE)
    return;

  // Keyboard might not exist.
  int device_id = connection_->seat()->keyboard()
                      ? connection_->seat()->keyboard()->device_id()
                      : 0;

  EventType type =
      state == WL_KEYBOARD_KEY_STATE_PRESSED ? ET_KEY_PRESSED : ET_KEY_RELEASED;
  key_delegate_->OnKeyboardKeyEvent(type, dom_code, /*repeat=*/false,
                                    EventTimeForNow(), device_id,
                                    WaylandKeyboard::KeyEventKind::kKey);
#else
  NOTIMPLEMENTED();
#endif
}

void WaylandInputMethodContext::OnSetPreeditRegion(
    int32_t index,
    uint32_t length,
    const std::vector<SpanStyle>& spans) {
  // |index| and |length| are expected to be in UTF8. |index| is relative to the
  // current cursor position.
  if (surrounding_text_.empty() || !selection_range_utf8_.IsValid()) {
    LOG(ERROR) << "SetSurroundingText should run before OnSetPreeditRegion.";
    return;
  }

  // Validation of index and length.
  if (index < 0 &&
      selection_range_utf8_.end() < static_cast<uint32_t>(-index)) {
    LOG(ERROR) << "Invalid starting point is specified";
    return;
  }
  size_t begin_utf8 = static_cast<size_t>(
      static_cast<ssize_t>(selection_range_utf8_.end()) + index);
  size_t end_utf8 = begin_utf8 + length;
  if (end_utf8 > surrounding_text_.size()) {
    LOG(ERROR) << "Too long preedit range is specified";
    return;
  }

  std::vector<size_t> offsets = {begin_utf8, end_utf8};
  for (const auto& span : spans) {
    offsets.push_back(begin_utf8 + span.index);
    offsets.push_back(begin_utf8 + span.index + span.length);
  }
  base::UTF8ToUTF16AndAdjustOffsets(surrounding_text_, &offsets);
  if (offsets[0] == std::u16string::npos ||
      offsets[1] == std::u16string::npos) {
    LOG(ERROR) << "Invalid range is specified";
    return;
  }

  std::vector<ui::ImeTextSpan> ime_text_spans;
  for (size_t i = 0; i < spans.size(); ++i) {
    size_t begin_span = offsets[i * 2 + 2];
    size_t end_span = offsets[i * 2 + 3];
    if (begin_span == std::u16string::npos || end_span == std::u16string::npos)
      continue;
    if (begin_span < offsets[0] || end_span < offsets[0] ||
        begin_span > offsets[1] || end_span > offsets[1]) {
      // Out of composition range.
      continue;
    }

    auto style = ConvertStyle(spans[i].style);
    if (!style.has_value())
      continue;
    ime_text_spans.emplace_back(/* type= */ style->first,
                                begin_span - offsets[0], end_span - offsets[0],
                                /* thickness = */ style->second);
  }

  ime_delegate_->OnSetPreeditRegion(gfx::Range(offsets[0], offsets[1]),
                                    ime_text_spans);
}

void WaylandInputMethodContext::OnInputPanelState(uint32_t state) {
  virtual_keyboard_visible_ = (state & 1) != 0;
  // Note: Currently there's no support of VirtualKeyboardControllerObserver.
  // In the future, we may need to support it. Specifically,
  // RenderWidgetHostViewAura would like to know the VirtualKeyboard's
  // region somehow.
}

void WaylandInputMethodContext::OnKeyboardFocusedWindowChanged() {
  MaybeUpdateActivated();
}

void WaylandInputMethodContext::MaybeUpdateActivated() {
  if (!text_input_)
    return;

  WaylandWindow* window =
      connection_->wayland_window_manager()->GetCurrentKeyboardFocusedWindow();
  if (!window && !connection_->seat()->keyboard())
    window = connection_->wayland_window_manager()->GetCurrentActiveWindow();
  // Activate Wayland IME only if 1) InputMethod in Chrome has some
  // TextInputClient connected, and 2) the actual keyboard focus of Wayland
  // is given to Chrome, which is notified via wl_keyboard::enter.
  // If no keyboard is connected, the current active window is used for 2)
  // instead (https://crbug.com/1168411).
  bool activated = focused_ && window;
  if (activated_ == activated)
    return;

  activated_ = activated;
  if (activated) {
    text_input_->Activate(window);
    text_input_->ShowInputPanel();
  } else {
    text_input_->Deactivate();
    text_input_->HideInputPanel();
  }
}

}  // namespace ui
