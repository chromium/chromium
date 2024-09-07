// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/zwp_text_input_wrapper_v1.h"

#include <sys/mman.h>

#include <string>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "ui/base/wayland/wayland_client_input_types.h"
#include "ui/gfx/range/range.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {
namespace {

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

// Parses the content of |array|, and creates a map of modifiers.
// The content of array is just a concat of modifier names in c-style string
// (i.e., '\0' terminated string), thus this splits the whole byte array by
// '\0' character.
std::vector<std::string> ParseModifiersMap(wl_array* array) {
  return base::SplitString(
      std::string_view(static_cast<char*>(array->data),
                       array->size - 1),  // exclude trailing '\0'.
      std::string_view("\0", 1),          // '\0' as a delimiter.
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
}

// Returns ImeTextSpan style to be assigned. Maybe nullopt if it is not
// supported.
std::optional<ZWPTextInputWrapperClient::SpanStyle::Style> ConvertStyle(
    uint32_t style) {
  switch (style) {
    case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_DEFAULT:
      return {{ImeTextSpan::Type::kComposition, ImeTextSpan::Thickness::kNone}};
    case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_HIGHLIGHT:
      return {
          {ImeTextSpan::Type::kComposition, ImeTextSpan::Thickness::kThick}};
    case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_UNDERLINE:
      return {{ImeTextSpan::Type::kComposition, ImeTextSpan::Thickness::kThin}};
    case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_SELECTION:
      return {{ImeTextSpan::Type::kSuggestion, ImeTextSpan::Thickness::kNone}};
    case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_INCORRECT:
      return {{ImeTextSpan::Type::kMisspellingSuggestion,
               ImeTextSpan::Thickness::kNone}};
    case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_NONE:
    case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_ACTIVE:
    case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_INACTIVE:
    default:
      VLOG(1) << "Unsupported style. Skipped: " << style;
  }
  return std::nullopt;
}

}  // namespace

ZWPTextInputWrapperV1::ZWPTextInputWrapperV1(
    WaylandConnection* connection,
    ZWPTextInputWrapperClient* client,
    zwp_text_input_manager_v1* text_input_manager,
    zcr_text_input_extension_v1* text_input_extension)
    : connection_(connection), client_(client) {
  static constexpr zwp_text_input_v1_listener kTextInputListener = {
      .enter = &OnEnter,
      .leave = &OnLeave,
      .modifiers_map = &OnModifiersMap,
      .input_panel_state = &OnInputPanelState,
      .preedit_string = &OnPreeditString,
      .preedit_styling = &OnPreeditStyling,
      .preedit_cursor = &OnPreeditCursor,
      .commit_string = &OnCommitString,
      .cursor_position = &OnCursorPosition,
      .delete_surrounding_text = &OnDeleteSurroundingText,
      .keysym = &OnKeysym,
      .language = &OnLanguage,
      .text_direction = &OnTextDirection,
  };

  static constexpr zcr_extended_text_input_v1_listener
      kExtendedTextInputListener = {
          .set_preedit_region = &OnSetPreeditRegion,
          .clear_grammar_fragments = &OnClearGrammarFragments,
          .add_grammar_fragment = &OnAddGrammarFragment,
          .set_autocorrect_range = &OnSetAutocorrectRange,
          .set_virtual_keyboard_occluded_bounds =
              &OnSetVirtualKeyboardOccludedBounds,
          .confirm_preedit = &OnConfirmPreedit,
          .insert_image = &OnInsertImage,
          .insert_image_with_large_url = &OnInsertImageWithLargeURL,
      };

  obj_ = wl::Object<zwp_text_input_v1>(
      zwp_text_input_manager_v1_create_text_input(text_input_manager));
  DCHECK(obj_.get());
  zwp_text_input_v1_add_listener(obj_.get(), &kTextInputListener, this);

  if (text_input_extension) {
    extended_obj_ = wl::Object<zcr_extended_text_input_v1>(
        zcr_text_input_extension_v1_get_extended_text_input(
            text_input_extension, obj_.get()));
    DCHECK(extended_obj_.get());
    zcr_extended_text_input_v1_add_listener(extended_obj_.get(),
                                            &kExtendedTextInputListener, this);
  }
}

ZWPTextInputWrapperV1::~ZWPTextInputWrapperV1() = default;

void ZWPTextInputWrapperV1::Reset() {
  ResetInputEventState();
  zwp_text_input_v1_reset(obj_.get());
}

void ZWPTextInputWrapperV1::Activate(WaylandWindow* window,
                                     TextInputClient::FocusReason reason) {
  DCHECK(connection_->seat());
  if (extended_obj_.get() &&
      wl::get_version_of_object(extended_obj_.get()) >=
          ZCR_EXTENDED_TEXT_INPUT_V1_SET_FOCUS_REASON_SINCE_VERSION) {
    std::optional<uint32_t> wayland_focus_reason;
    switch (reason) {
      case ui::TextInputClient::FocusReason::FOCUS_REASON_NONE:
        wayland_focus_reason =
            ZCR_EXTENDED_TEXT_INPUT_V1_FOCUS_REASON_TYPE_NONE;
        break;
      case ui::TextInputClient::FocusReason::FOCUS_REASON_MOUSE:
        wayland_focus_reason =
            ZCR_EXTENDED_TEXT_INPUT_V1_FOCUS_REASON_TYPE_MOUSE;
        break;
      case ui::TextInputClient::FocusReason::FOCUS_REASON_TOUCH:
        wayland_focus_reason =
            ZCR_EXTENDED_TEXT_INPUT_V1_FOCUS_REASON_TYPE_TOUCH;
        break;
      case ui::TextInputClient::FocusReason::FOCUS_REASON_PEN:
        wayland_focus_reason = ZCR_EXTENDED_TEXT_INPUT_V1_FOCUS_REASON_TYPE_PEN;
        break;
      case ui::TextInputClient::FocusReason::FOCUS_REASON_OTHER:
        wayland_focus_reason =
            ZCR_EXTENDED_TEXT_INPUT_V1_FOCUS_REASON_TYPE_OTHER;
        break;
    }

    if (wayland_focus_reason.has_value()) {
      zcr_extended_text_input_v1_set_focus_reason(extended_obj_.get(),
                                                  wayland_focus_reason.value());
    }
  }
  zwp_text_input_v1_activate(obj_.get(), connection_->seat()->wl_object(),
                             window->root_surface()->surface());
}

void ZWPTextInputWrapperV1::Deactivate() {
  DCHECK(connection_->seat());

  zwp_text_input_v1_deactivate(obj_.get(), connection_->seat()->wl_object());
}

void ZWPTextInputWrapperV1::ShowInputPanel() {
  zwp_text_input_v1_show_input_panel(obj_.get());
  TryScheduleFinalizeVirtualKeyboardChanges();
}

void ZWPTextInputWrapperV1::HideInputPanel() {
  zwp_text_input_v1_hide_input_panel(obj_.get());
  TryScheduleFinalizeVirtualKeyboardChanges();
}

void ZWPTextInputWrapperV1::SetCursorRect(const gfx::Rect& rect) {
  zwp_text_input_v1_set_cursor_rectangle(obj_.get(), rect.x(), rect.y(),
                                         rect.width(), rect.height());
}

void ZWPTextInputWrapperV1::SetSurroundingText(
    const std::string& text,
    const gfx::Range& preedit_range,
    const gfx::Range& selection_range) {
  // Wayland packet has a limit of size due to its serialization format,
  // so if it exceeds 16 bits, it may be broken.
  static constexpr size_t kSizeLimit = 60000;
  if (HasAdvancedSurroundingTextSupport() && text.length() > kSizeLimit) {
    base::ScopedFD memfd(memfd_create("surrounding_text", MFD_CLOEXEC));
    if (!memfd.get()) {
      PLOG(ERROR) << "Failed to create memfd";
      return;
    }
    if (!base::WriteFileDescriptor(memfd.get(), text)) {
      LOG(ERROR) << "Failed to write into memfd";
      return;
    }
    zcr_extended_text_input_v1_set_large_surrounding_text(
        extended_obj_.get(), memfd.get(), text.length(),
        selection_range.start(), selection_range.end());
  } else {
    zwp_text_input_v1_set_surrounding_text(obj_.get(), text.c_str(),
                                           selection_range.start(),
                                           selection_range.end());
  }
}

bool ZWPTextInputWrapperV1::HasAdvancedSurroundingTextSupport() const {
  return extended_obj_.get() &&
         wl::get_version_of_object(extended_obj_.get()) >=
             ZCR_EXTENDED_TEXT_INPUT_V1_SET_LARGE_SURROUNDING_TEXT_SINCE_VERSION;
}

void ZWPTextInputWrapperV1::SetSurroundingTextOffsetUtf16(
    uint32_t offset_utf16) {
  if (HasAdvancedSurroundingTextSupport()) {
    zcr_extended_text_input_v1_set_surrounding_text_offset_utf16(
        extended_obj_.get(), offset_utf16);
  }
}

void ZWPTextInputWrapperV1::SetContentType(ui::TextInputType type,
                                           ui::TextInputMode mode,
                                           uint32_t flags,
                                           bool should_do_learning,
                                           bool can_compose_inline) {
  // If wayland compositor supports the extended version of set input type,
  // use it to avoid losing the info.
  if (extended_obj_.get()) {
    uint32_t wl_server_version = wl::get_version_of_object(extended_obj_.get());
    if (wl_server_version >=
        ZCR_EXTENDED_TEXT_INPUT_V1_SET_INPUT_TYPE_SINCE_VERSION) {
      zcr_extended_text_input_v1_set_input_type(
          extended_obj_.get(), ui::wayland::ConvertFromTextInputType(type),
          ui::wayland::ConvertFromTextInputMode(mode),
          ui::wayland::ConvertFromTextInputFlags(flags),
          should_do_learning
              ? ZCR_EXTENDED_TEXT_INPUT_V1_LEARNING_MODE_ENABLED
              : ZCR_EXTENDED_TEXT_INPUT_V1_LEARNING_MODE_DISABLED,
          can_compose_inline
              ? ZCR_EXTENDED_TEXT_INPUT_V1_INLINE_COMPOSITION_SUPPORT_SUPPORTED
              : ZCR_EXTENDED_TEXT_INPUT_V1_INLINE_COMPOSITION_SUPPORT_UNSUPPORTED);
      return;
    }
    if (wl_server_version >=
        ZCR_EXTENDED_TEXT_INPUT_V1_DEPRECATED_SET_INPUT_TYPE_SINCE_VERSION) {
      // TODO(crbug.com/40258785) This deprecated method is used here only to
      // maintain backwards compatibility with an older version of Exo. Once
      // Exo has stabilized on the new set_input_type, remove this call.
      zcr_extended_text_input_v1_deprecated_set_input_type(
          extended_obj_.get(), ui::wayland::ConvertFromTextInputType(type),
          ui::wayland::ConvertFromTextInputMode(mode),
          ui::wayland::ConvertFromTextInputFlags(flags),
          should_do_learning
              ? ZCR_EXTENDED_TEXT_INPUT_V1_LEARNING_MODE_ENABLED
              : ZCR_EXTENDED_TEXT_INPUT_V1_LEARNING_MODE_DISABLED);
      return;
    }
  }

  // Otherwise, fallback to the standard set_content_type.
  uint32_t content_purpose = InputTypeToContentPurpose(type);
  uint32_t content_hint = InputFlagsToContentHint(flags);
  if (!should_do_learning)
    content_hint |= ZWP_TEXT_INPUT_V1_CONTENT_HINT_SENSITIVE_DATA;
  zwp_text_input_v1_set_content_type(obj_.get(), content_hint, content_purpose);
}

void ZWPTextInputWrapperV1::SetGrammarFragmentAtCursor(
    const ui::GrammarFragment& fragment) {
  if (extended_obj_.get() &&
      wl::get_version_of_object(extended_obj_.get()) >=
          ZCR_EXTENDED_TEXT_INPUT_V1_SET_GRAMMAR_FRAGMENT_AT_CURSOR_SINCE_VERSION) {
    zcr_extended_text_input_v1_set_grammar_fragment_at_cursor(
        extended_obj_.get(), fragment.range.start(), fragment.range.end(),
        fragment.suggestion.c_str());
  }
}

void ZWPTextInputWrapperV1::SetAutocorrectInfo(
    const gfx::Range& autocorrect_range,
    const gfx::Rect& autocorrect_bounds) {
  if (extended_obj_.get() &&
      wl::get_version_of_object(extended_obj_.get()) >=
          ZCR_EXTENDED_TEXT_INPUT_V1_SET_AUTOCORRECT_INFO_SINCE_VERSION) {
    zcr_extended_text_input_v1_set_autocorrect_info(
        extended_obj_.get(), autocorrect_range.start(), autocorrect_range.end(),
        autocorrect_bounds.x(), autocorrect_bounds.y(),
        autocorrect_bounds.width(), autocorrect_bounds.height());
  }
}

void ZWPTextInputWrapperV1::ResetInputEventState() {
  spans_.clear();
  preedit_cursor_ = -1;
}

void ZWPTextInputWrapperV1::TryScheduleFinalizeVirtualKeyboardChanges() {
  if (!SupportsFinalizeVirtualKeyboardChanges() ||
      send_vk_finalize_timer_.IsRunning()) {
    return;
  }

  send_vk_finalize_timer_.Start(
      FROM_HERE, base::Microseconds(0), this,
      &ZWPTextInputWrapperV1::FinalizeVirtualKeyboardChanges);
}

void ZWPTextInputWrapperV1::FinalizeVirtualKeyboardChanges() {
  DCHECK(SupportsFinalizeVirtualKeyboardChanges());
  zcr_extended_text_input_v1_finalize_virtual_keyboard_changes(
      extended_obj_.get());
}

bool ZWPTextInputWrapperV1::SupportsFinalizeVirtualKeyboardChanges() {
  return extended_obj_.get() &&
         wl::get_version_of_object(extended_obj_.get()) >=
             ZCR_EXTENDED_TEXT_INPUT_V1_FINALIZE_VIRTUAL_KEYBOARD_CHANGES_SINCE_VERSION;
}

// static
void ZWPTextInputWrapperV1::OnEnter(void* data,
                                    struct zwp_text_input_v1* text_input,
                                    struct wl_surface* surface) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void ZWPTextInputWrapperV1::OnLeave(void* data,
                                    struct zwp_text_input_v1* text_input) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void ZWPTextInputWrapperV1::OnModifiersMap(void* data,
                                           struct zwp_text_input_v1* text_input,
                                           struct wl_array* map) {
  auto* self = static_cast<ZWPTextInputWrapperV1*>(data);
  self->client_->OnModifiersMap(ParseModifiersMap(map));
}

// static
void ZWPTextInputWrapperV1::OnInputPanelState(
    void* data,
    struct zwp_text_input_v1* text_input,
    uint32_t state) {
  auto* self = static_cast<ZWPTextInputWrapperV1*>(data);
  self->client_->OnInputPanelState(state);
}

// static
void ZWPTextInputWrapperV1::OnPreeditString(
    void* data,
    struct zwp_text_input_v1* text_input,
    uint32_t serial,
    const char* text,
    const char* commit) {
  auto* self = static_cast<ZWPTextInputWrapperV1*>(data);
  auto spans = std::move(self->spans_);
  int32_t preedit_cursor = self->preedit_cursor_;
  self->ResetInputEventState();
  self->client_->OnPreeditString(text, spans,
                                 preedit_cursor < 0
                                     ? gfx::Range::InvalidRange()
                                     : gfx::Range(preedit_cursor));
}

// static
void ZWPTextInputWrapperV1::OnPreeditStyling(
    void* data,
    struct zwp_text_input_v1* text_input,
    uint32_t index,
    uint32_t length,
    uint32_t style) {
  auto* self = static_cast<ZWPTextInputWrapperV1*>(data);
  self->spans_.push_back(
      ZWPTextInputWrapperClient::SpanStyle{index, length, ConvertStyle(style)});
}

// static
void ZWPTextInputWrapperV1::OnPreeditCursor(
    void* data,
    struct zwp_text_input_v1* text_input,
    int32_t index) {
  auto* self = static_cast<ZWPTextInputWrapperV1*>(data);
  self->preedit_cursor_ = index;
}

// static
void ZWPTextInputWrapperV1::OnCommitString(void* data,
                                           struct zwp_text_input_v1* text_input,
                                           uint32_t serial,
                                           const char* text) {
  auto* self = static_cast<ZWPTextInputWrapperV1*>(data);
  self->ResetInputEventState();
  self->client_->OnCommitString(text);
}

// static
void ZWPTextInputWrapperV1::OnCursorPosition(
    void* data,
    struct zwp_text_input_v1* text_input,
    int32_t index,
    int32_t anchor) {
  auto* self = static_cast<ZWPTextInputWrapperV1*>(data);
  self->client_->OnCursorPosition(index, anchor);
}

// static
void ZWPTextInputWrapperV1::OnDeleteSurroundingText(
    void* data,
    struct zwp_text_input_v1* text_input,
    int32_t index,
    uint32_t length) {
  auto* self = static_cast<ZWPTextInputWrapperV1*>(data);
  self->client_->OnDeleteSurroundingText(index, length);
}

// static
void ZWPTextInputWrapperV1::OnKeysym(void* data,
                                     struct zwp_text_input_v1* text_input,
                                     uint32_t serial,
                                     uint32_t time,
                                     uint32_t key,
                                     uint32_t state,
                                     uint32_t modifiers) {
  auto* self = static_cast<ZWPTextInputWrapperV1*>(data);
  self->client_->OnKeysym(key, state, modifiers, time);
}

// static
void ZWPTextInputWrapperV1::OnLanguage(void* data,
                                       struct zwp_text_input_v1* text_input,
                                       uint32_t serial,
                                       const char* language) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void ZWPTextInputWrapperV1::OnTextDirection(
    void* data,
    struct zwp_text_input_v1* text_input,
    uint32_t serial,
    uint32_t direction) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void ZWPTextInputWrapperV1::OnSetPreeditRegion(
    void* data,
    struct zcr_extended_text_input_v1* extended_text_input,
    int32_t index,
    uint32_t length) {
  auto* self = static_cast<ZWPTextInputWrapperV1*>(data);
  auto spans = std::move(self->spans_);
  self->ResetInputEventState();
  self->client_->OnSetPreeditRegion(index, length, spans);
}

// static
void ZWPTextInputWrapperV1::OnClearGrammarFragments(
    void* data,
    struct zcr_extended_text_input_v1* extended_text_input,
    uint32_t start,
    uint32_t end) {
  auto* self = static_cast<ZWPTextInputWrapperV1*>(data);
  self->client_->OnClearGrammarFragments(gfx::Range(start, end));
}

// static
void ZWPTextInputWrapperV1::OnAddGrammarFragment(
    void* data,
    struct zcr_extended_text_input_v1* extended_text_input,
    uint32_t start,
    uint32_t end,
    const char* suggestion) {
  auto* self = static_cast<ZWPTextInputWrapperV1*>(data);
  self->client_->OnAddGrammarFragment(
      ui::GrammarFragment(gfx::Range(start, end), suggestion));
}

// static
void ZWPTextInputWrapperV1::OnSetAutocorrectRange(
    void* data,
    struct zcr_extended_text_input_v1* extended_text_input,
    uint32_t start,
    uint32_t end) {
  auto* self = static_cast<ZWPTextInputWrapperV1*>(data);
  self->client_->OnSetAutocorrectRange(gfx::Range(start, end));
}

// static
void ZWPTextInputWrapperV1::OnSetVirtualKeyboardOccludedBounds(
    void* data,
    struct zcr_extended_text_input_v1* extended_text_input,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height) {
  auto* self = static_cast<ZWPTextInputWrapperV1*>(data);
  gfx::Rect screen_bounds(x, y, width, height);
  self->client_->OnSetVirtualKeyboardOccludedBounds(screen_bounds);
}

// static
void ZWPTextInputWrapperV1::OnConfirmPreedit(
    void* data,
    struct zcr_extended_text_input_v1* extended_text_input,
    uint32_t selection_behavior) {
  auto* self = static_cast<ZWPTextInputWrapperV1*>(data);
  switch (selection_behavior) {
    case ZCR_EXTENDED_TEXT_INPUT_V1_CONFIRM_PREEDIT_SELECTION_BEHAVIOR_AFTER_PREEDIT:
      self->client_->OnConfirmPreedit(/*keep_selection=*/false);
      break;
    case ZCR_EXTENDED_TEXT_INPUT_V1_CONFIRM_PREEDIT_SELECTION_BEHAVIOR_UNCHANGED:
      self->client_->OnConfirmPreedit(/*keep_selection=*/true);
      break;
    default:
      self->client_->OnConfirmPreedit(/*keep_selection=*/false);
      break;
  }
}

// static
void ZWPTextInputWrapperV1::OnInsertImage(
    void* data,
    struct zcr_extended_text_input_v1* extended_text_input,
    const char* src) {
  auto* self = static_cast<ZWPTextInputWrapperV1*>(data);
  self->client_->OnInsertImage(GURL(src));
}

// static
void ZWPTextInputWrapperV1::OnInsertImageWithLargeURL(
    void* data,
    struct zcr_extended_text_input_v1* extended_text_input,
    const char* mime_type,
    const char* charset,
    const int32_t raw_fd,
    const uint32_t size) {
  // Read raw data from fd.
  std::string raw_data;
  raw_data.resize(size);
  base::ScopedFD fd(raw_fd);
  if (!base::ReadFromFD(fd.get(), raw_data)) {
    LOG(ERROR) << "Failed to read file descriptor for image insertion";
    return;
  }

  // Re-construct data url.
  std::string src = "data:";
  if (mime_type) {
    base::StrAppend(&src, {mime_type});
  }
  if (charset && strlen(charset) > 0) {
    base::StrAppend(&src, {";charset=", charset});
  }
  base::StrAppend(&src, {";base64,"});

  base::Base64EncodeAppend(base::as_byte_span(raw_data), &src);

  // Dispatch image insertion request.
  auto* self = static_cast<ZWPTextInputWrapperV1*>(data);
  self->client_->OnInsertImage(GURL(src));
}

}  // namespace ui
