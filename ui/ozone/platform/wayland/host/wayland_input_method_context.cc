// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_input_method_context.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/debug/dump_without_crashing.h"
#include "base/environment.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/char_iterator.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/nix/xdg_util.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/ime_text_span.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/range/range.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/zwp_text_input_wrapper_v1.h"
#include "ui/ozone/platform/wayland/host/zwp_text_input_wrapper_v3.h"
#include "ui/ozone/public/ozone_switches.h"

#if BUILDFLAG(USE_XKBCOMMON)
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/check.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace ui {
namespace {

// Only enable the preedit string for sequence mode (i.e. when using dead keys
// or the Compose key) on Linux ozone/wayland (see b/220370007).
constexpr CharacterComposer::PreeditStringMode kPreeditStringMode =
#if BUILDFLAG(IS_LINUX)
    CharacterComposer::PreeditStringMode::kAlwaysEnabled;
#else
    CharacterComposer::PreeditStringMode::kHexModeOnly;
#endif  // BUILDFLAG(IS_LINUX)

std::optional<size_t> OffsetFromUTF8Offset(std::string_view text,
                                           uint32_t offset) {
  if (offset > text.length())
    return std::nullopt;

  std::u16string converted;
  if (!base::UTF8ToUTF16(text.data(), offset, &converted))
    return std::nullopt;

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
  if (base::FeatureList::IsEnabled(features::kWaylandTextInputV3)) {
    return true;
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // On Lacros chrome, we check whether ash-chrome supports IME, then
  // enable IME if so. This allows us to control IME enabling state in
  // Lacros-chrome side, which helps us on releasing.
  // TODO(crbug.com/40737321): In the future, we may want to unify the behavior
  // of ozone/wayland across platforms.
  const chromeos::BrowserParamsProxy* init_params =
      chromeos::BrowserParamsProxy::Get();
  if (init_params->ExoImeSupport() !=
      crosapi::mojom::ExoImeSupport::kUnsupported) {
    return true;
  }
#endif

  // Do not enable wayland IME by default.
  return false;
}

// Returns the biggest range that is included in the |range|,
// but whose start/end points are at the UTF-8 boundary.
// If the given range is bigger than the given text_utf8,
// it will be trimmed to the text_utf8 size.
gfx::Range AdjustUtf8Alignment(std::string_view text_utf8, gfx::Range range) {
  // Truncate the text to fit into the wayland message size and adjust indices
  // of |selection_range|. Since the text is in UTF8 form, we need to adjust
  // the text and selection range positions where all characters are valid.
  //
  // TODO(crbug.com/40184185): We should use base::i18n::BreakIterator
  // to get the offsets and convert it into UTF8 form instead of using
  // UTF8CharIterator.
  base::i18n::UTF8CharIterator iter(text_utf8);
  while (iter.array_pos() < range.start()) {
    iter.Advance();
  }
  size_t adjusted_start = iter.array_pos();
  size_t adjusted_end = adjusted_start;
  while (iter.array_pos() <= range.end()) {
    adjusted_end = iter.array_pos();
    if (!iter.Advance()) {
      break;
    }
  }
  return {adjusted_start, adjusted_end};
}

struct OffsetText {
  std::string text;
  size_t offset;
};

// Trims surrounding text for standard text_input. There is the limit of length
// of the surrounding text, which is 4000 bytes. This gives it a try to keep
// the surrounding text around the selection with respecting UTF-8 boundary.
// Returns the trimmed string and UTF-8 offset.
std::optional<OffsetText> TrimSurroundingTextForStandard(
    std::string_view text_utf8,
    gfx::Range selection_utf8) {
  // The text length for set_surrounding_text can not be longer than the maximum
  // length of wayland messages. The maximum length of the text is explicitly
  // specified as 4000 in the protocol spec of text-input-unstable-v3.
  static constexpr size_t kWaylandMessageDataMaxLength = 4000;

  // If the selection range in UTF8 form is longer than the maximum length of
  // wayland messages, skip sending set_surrounding_text requests.
  if (selection_utf8.length() > kWaylandMessageDataMaxLength) {
    return std::nullopt;
  }

  if (text_utf8.size() <= kWaylandMessageDataMaxLength) {
    // We separate this case to run the function simpler and faster since this
    // condition is satisfied in most cases.
    return OffsetText{std::string(text_utf8), 0u};
  }

  // If the text in UTF8 form is longer than the maximum length of wayland
  // messages while the selection range in UTF8 form is not, truncate the text
  // into the limitation and adjust indices of |selection_range|.

  // Decide where to start. The truncated text should be around the selection
  // range. We choose a text whose center point is same to the center of the
  // selection range unless this chosen text is shorter than the maximum
  // length of wayland messages because of the original text position.
  uint32_t selection_range_utf8_center =
      selection_utf8.start() + selection_utf8.length() / 2;
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

  gfx::Range truncated_range = AdjustUtf8Alignment(
      text_utf8,
      gfx::Range(start_index, start_index + kWaylandMessageDataMaxLength));

  return OffsetText{std::string(text_utf8.substr(truncated_range.start(),
                                                 truncated_range.length())),
                    truncated_range.start()};
}

std::optional<OffsetText> TrimSurroundingTextForExtension(
    std::string_view text_utf8,
    const base::span<size_t> offsets) {
  // Heuristically, send leading/trailing (almost) 500 bytes in addition to
  // offsets.
  // Note: exo's implementation does not have 4000 bytes limit of the message.
  static constexpr size_t kMaxSurroundingTextBytes = 500;
  const auto& [min_offset, max_offset] = base::ranges::minmax(offsets);

  size_t start_index =
      min_offset - std::min(min_offset, kMaxSurroundingTextBytes);
  size_t end_index = max_offset + kMaxSurroundingTextBytes;

  gfx::Range truncated_range =
      AdjustUtf8Alignment(text_utf8, gfx::Range(start_index, end_index));

  return OffsetText{std::string(text_utf8.substr(truncated_range.start(),
                                                 truncated_range.length())),
                    truncated_range.start()};
}

// TODO(crbug.com/40251329): Add TrimSurroundingTextForExtension.

}  // namespace

WaylandInputMethodContext::WaylandInputMethodContext(
    WaylandConnection* connection,
    WaylandKeyboard::Delegate* key_delegate,
    LinuxInputMethodContextDelegate* ime_delegate)
    : connection_(connection),
      key_delegate_(key_delegate),
      ime_delegate_(ime_delegate),
      text_input_(nullptr),
      character_composer_(kPreeditStringMode) {
  connection_->window_manager()->AddObserver(this);
  Init();
}

WaylandInputMethodContext::~WaylandInputMethodContext() {
  if (text_input_) {
    DismissVirtualKeyboard();
    text_input_->Deactivate();
  }
  connection_->window_manager()->RemoveObserver(this);
}

void WaylandInputMethodContext::CreateTextInputWrapper() {
  // Can be specified as value for --wayland-ime-version to use text-input-v1 or
  // text-input-v3.
  constexpr char kWaylandTextInputVersion1[] = "1";
  constexpr char kWaylandTextInputVersion3[] = "3";

  const auto* cmd_line = base::CommandLine::ForCurrentProcess();
  const std::string version_from_cmd_line =
      cmd_line->GetSwitchValueASCII(switches::kWaylandTextInputVersion);
  const bool enable_using_cmd_line_version =
      cmd_line->HasSwitch(switches::kEnableWaylandIme) &&
      !version_from_cmd_line.empty();

  if (base::FeatureList::IsEnabled(features::kWaylandTextInputV3) ||
      (enable_using_cmd_line_version &&
       version_from_cmd_line == kWaylandTextInputVersion3)) {
    if (connection_->text_input_manager_v3()) {
      text_input_ = std::make_unique<ZWPTextInputWrapperV3>(
          connection_, this, connection_->text_input_manager_v3());
    } else {
      LOG(WARNING) << "text-input-v3 not available.";
    }
    return;
  } else if (enable_using_cmd_line_version &&
             version_from_cmd_line != kWaylandTextInputVersion1) {
    LOG(WARNING) << "text input version should be either 1 or 3. Defaulting to "
                    "text-input-v1.";
  }
  if (connection_->text_input_manager_v1()) {
    text_input_ = std::make_unique<ZWPTextInputWrapperV1>(
        connection_, this, connection_->text_input_manager_v1(),
        connection_->text_input_extension_v1());
  } else {
    LOG(WARNING) << "text-input-v1 not available.";
  }
}

void WaylandInputMethodContext::Init(
    bool initialize_for_testing,
    std::unique_ptr<ZWPTextInputWrapper> wrapper_for_testing,
    std::optional<base::nix::DesktopEnvironment> desktop_for_testing) {
  desktop_environment_ = desktop_for_testing.value_or(
      base::nix::GetDesktopEnvironment(base::Environment::Create().get()));
  if (wrapper_for_testing) {
    text_input_ = std::move(wrapper_for_testing);
    return;
  }

  bool use_ozone_wayland_vkb = initialize_for_testing || IsImeEnabled();
  // If text input instance is not created then all ime context operations
  // are noop. This option is because in some environments someone might not
  // want to enable ime/virtual keyboard even if it's available.
  if (!use_ozone_wayland_vkb || text_input_) {
    return;
  }

  CreateTextInputWrapper();
}

bool WaylandInputMethodContext::DispatchKeyEvent(const KeyEvent& key_event) {
  if (key_event.type() != EventType::kKeyPressed) {
    return false;
  }

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

bool WaylandInputMethodContext::IsPeekKeyEvent(const KeyEvent& key_event) {
  return !(GetKeyboardImeFlags(key_event) & kPropertyKeyboardImeIgnoredFlag);
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
  surrounding_text_tracker_.OnSetCompositionText(preedit);
  ime_delegate_->OnPreeditChanged(preedit);
}

void WaylandInputMethodContext::Reset() {
  character_composer_.Reset();
  // TODO(b/269964109): In ChromeOS, 'reset' means to reset the composition
  // only, excluding surrounding text etc. In Wayland, text-input-v1 doesn't
  // define what state is reset in a 'reset' call. However, based on the
  // description in text-input-v3, the state likely includes the surrounding
  // text. Therefore, the call below is likely not compliant with Wayland's
  // intentions. Introduce a dedicated extended Wayland API for resetting only
  // the composition.
  surrounding_text_tracker_.CancelComposition();
  if (text_input_)
    text_input_->Reset();
}

void WaylandInputMethodContext::WillUpdateFocus(TextInputClient* old_client,
                                                TextInputClient* new_client) {
  if (old_client) {
    past_clients_.try_emplace(old_client, old_client->AsWeakPtr());
  }
}

void WaylandInputMethodContext::UpdateFocus(
    bool has_client,
    TextInputType old_type,
    const TextInputClientAttributes& new_client_attributes,
    TextInputClient::FocusReason reason) {
  attributes_ = new_client_attributes;

  // This prevents unnecessarily hiding/showing the virtual keyboard.
  TextInputType new_type = new_client_attributes.input_type;
  bool skip_vk_update =
      old_type != TEXT_INPUT_TYPE_NONE && new_type != TEXT_INPUT_TYPE_NONE;

  if (old_type != TEXT_INPUT_TYPE_NONE)
    Blur(skip_vk_update);
  if (new_type != TEXT_INPUT_TYPE_NONE)
    Focus(skip_vk_update, reason);
}

void WaylandInputMethodContext::Focus(bool skip_virtual_keyboard_update,
                                      TextInputClient::FocusReason reason) {
  focused_ = true;
  MaybeUpdateActivated(skip_virtual_keyboard_update, reason);
}

void WaylandInputMethodContext::Blur(bool skip_virtual_keyboard_update) {
  focused_ = false;
  MaybeUpdateActivated(skip_virtual_keyboard_update,
                       TextInputClient::FOCUS_REASON_NONE);
}

void WaylandInputMethodContext::SetCursorLocation(const gfx::Rect& rect) {
  if (!text_input_) {
    return;
  }
  WaylandWindow* focused_window =
      connection_->window_manager()->GetCurrentKeyboardFocusedWindow();
  if (!focused_window) {
    return;
  }
  text_input_->SetCursorRect(
      rect - focused_window->GetBoundsInDIP().OffsetFromOrigin());
}

void WaylandInputMethodContext::SetSurroundingText(
    const std::u16string& text,
    const gfx::Range& text_range,
    const gfx::Range& composition_range,
    const gfx::Range& selection_range,
    const std::optional<GrammarFragment>& fragment,
    const std::optional<AutocorrectInfo>& autocorrect) {
  DVLOG(1) << __func__ << " text=" << text << " text_range=" << text_range
           << " composition_range=" << composition_range
           << " selection_range=" << selection_range;
  if (!selection_range.IsBoundedBy(text_range)) {
    // There seems some edge case that selection_range is outside of text_range.
    // In the case we ignore it temporarily, wishing the next event will
    // update the tracking correctly.
    // See also crbug.com/1457178.
    LOG(ERROR) << "selection_range is not bounded by text_range: "
               << selection_range.ToString() << ", " << text_range.ToString();
    // Make a crash report for further investigation in the future.
    // Temporarily disabling crash dump for release.
    // TODO(crbug.com/40066238): restore this.
    // base::debug::DumpWithoutCrashing();
    return;
  }

  size_t utf16_offset = text_range.GetMin();
  surrounding_text_tracker_.Update(text, utf16_offset, selection_range);

  if (!text_input_)
    return;

  // Convert into UTF8 unit.
  std::vector<size_t> offsets_for_adjustment = {
      selection_range.start() - utf16_offset,
      selection_range.end() - utf16_offset};
  if (fragment.has_value()) {
    offsets_for_adjustment.push_back(fragment->range.start() - utf16_offset);
    offsets_for_adjustment.push_back(fragment->range.end() - utf16_offset);
  }
  if (autocorrect.has_value()) {
    offsets_for_adjustment.push_back(autocorrect->range.start() - utf16_offset);
    offsets_for_adjustment.push_back(autocorrect->range.end() - utf16_offset);
  }

  std::string text_utf8 =
      base::UTF16ToUTF8AndAdjustOffsets(text, &offsets_for_adjustment);
  if (offsets_for_adjustment[0] == std::u16string::npos ||
      offsets_for_adjustment[1] == std::u16string::npos) {
    LOG(DFATAL) << "The selection range is invalid.";
    return;
  }
  gfx::Range selection_range_utf8 = {
      static_cast<uint32_t>(offsets_for_adjustment[0]),
      static_cast<uint32_t>(offsets_for_adjustment[1])};

  // To ensure trimming around cursor position, selection is used and not
  // preedit.
  auto trimmed =
      text_input_->HasAdvancedSurroundingTextSupport()
          ? TrimSurroundingTextForExtension(text_utf8, offsets_for_adjustment)
          : TrimSurroundingTextForStandard(text_utf8, selection_range_utf8);
  if (!trimmed.has_value()) {
    surrounding_text_tracker_.Reset();
    return;
  }

  size_t extra_offset_utf16 =
      base::UTF8ToUTF16(std::string_view(text_utf8).substr(0, trimmed->offset))
          .length();
  text_utf8 = std::move(trimmed->text);
  surrounding_text_offset_ = trimmed->offset;

  if (fragment.has_value()) {
    // SetGrammarFragmentAtCursor must happen before SetSurroundingText to make
    // sure it is properly updated before IME needs it.
    DCHECK_GE(offsets_for_adjustment.size(), 4u);
    text_input_->SetGrammarFragmentAtCursor(GrammarFragment(
        gfx::Range(static_cast<uint32_t>(offsets_for_adjustment[2] -
                                         surrounding_text_offset_),
                   static_cast<uint32_t>(offsets_for_adjustment[3] -
                                         surrounding_text_offset_)),
        fragment->suggestion));
  } else {
    // Invalidate the grammar fragment.
    text_input_->SetGrammarFragmentAtCursor(GrammarFragment(gfx::Range(), ""));
  }

  if (autocorrect.has_value()) {
    size_t index = fragment.has_value() ? 4u : 2u;
    // Send the updated autocorrect information before surrounding text,
    // as surrounding text changes may trigger the IME to ask for the
    // autocorrect information.
    gfx::Range autocorrect_range = autocorrect->range;
    if (text_input_->HasAdvancedSurroundingTextSupport()) {
      // The old implementation sent the original UTF-16 range as is, and
      // the compositor also assumed it.
      autocorrect_range =
          gfx::Range(static_cast<uint32_t>(offsets_for_adjustment[index] -
                                           surrounding_text_offset_),
                     static_cast<uint32_t>(offsets_for_adjustment[index + 1] -
                                           surrounding_text_offset_));
    }

    text_input_->SetAutocorrectInfo(autocorrect_range, autocorrect->bounds);
  }

  text_input_->SetSurroundingTextOffsetUtf16(utf16_offset + extra_offset_utf16);

  gfx::Range relocated_preedit_range;
  if (composition_range.IsValid()) {
    if (!composition_range.IsBoundedBy(text_range)) {
      // This is caused by incorrect value passed from the caller.
      // As this likely indicates something went wrong in the input method stack
      // ignore this request.
      LOG(ERROR) << "composition_range is not bounded by text_range: "
                 << composition_range.ToString() << ", "
                 << text_range.ToString();
      return;
    }
    std::vector<size_t> preedit_range = {
        composition_range.start() - utf16_offset,
        composition_range.end() - utf16_offset};
    base::UTF16ToUTF8AndAdjustOffsets(text, &preedit_range);
    if (preedit_range[0] < surrounding_text_offset_ ||
        preedit_range[1] < surrounding_text_offset_ ||
        preedit_range[0] > (surrounding_text_offset_ + text_utf8.size()) ||
        preedit_range[1] > (surrounding_text_offset_ + text_utf8.size())) {
      // The preedit range is outside of the surrounding text range.
      // This can happen when the surrounding text is trimmed.
      // In this case, the preedit range is invalid.
      relocated_preedit_range = gfx::Range::InvalidRange();
    } else {
      relocated_preedit_range = {preedit_range[0] - surrounding_text_offset_,
                                 preedit_range[1] - surrounding_text_offset_};
    }
  } else {
    relocated_preedit_range = gfx::Range::InvalidRange();
  }

  gfx::Range relocated_selection_range(
      selection_range_utf8.start() - surrounding_text_offset_,
      selection_range_utf8.end() - surrounding_text_offset_);
  text_input_->SetSurroundingText(text_utf8, relocated_preedit_range,
                                  relocated_selection_range);
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
    std::string_view text,
    const std::vector<SpanStyle>& spans,
    const gfx::Range& preedit_cursor) {
  CompositionText composition_text;
  composition_text.text = base::UTF8ToUTF16(text);
  for (const auto& span : spans) {
    auto start_offset = OffsetFromUTF8Offset(text, span.index);
    if (!start_offset)
      continue;
    auto end_offset = OffsetFromUTF8Offset(text, span.index + span.length);
    if (!end_offset)
      continue;
    const auto& style = span.style;
    if (!style.has_value())
      continue;
    composition_text.ime_text_spans.emplace_back(style->type, *start_offset,
                                                 *end_offset, style->thickness);
  }
  if (!preedit_cursor.IsValid()) {
    // This is the case if a preceding preedit_cursor event in text-input-v1 was
    // not received or an explicit negative value was requested to hide the
    // cursor.
    // TODO (crbug.com/40263583) Evaluate if InvalidRange should be set here and
    // make surrounding text tracker handle that. Currently surrounding text
    // tracker does not support invalid ranges and would result in a crash if
    // so. So set the cursor at the end of composition text as a fallback.
    composition_text.selection = gfx::Range(composition_text.text.length());
  } else {
    std::vector<size_t> offsets = {
        static_cast<uint32_t>(preedit_cursor.start()),
        static_cast<uint32_t>(preedit_cursor.end())};
    base::UTF8ToUTF16AndAdjustOffsets(text, &offsets);
    if (desktop_environment_ == base::nix::DESKTOP_ENVIRONMENT_GNOME) {
      if (!compositor_sends_invalid_cursor_end_) {
        // This was seen in gnome where it sends erroneous value for cursor_end
        // in text-input-v3 [1].
        // Currently only way to detect this is by checking if cursor end is
        // less than cursor start or the value is invalid.
        //
        // [1] https://gitlab.gnome.org/GNOME/mutter/-/issues/3547
        if (offsets[1] == std::u16string::npos || offsets[1] < offsets[0]) {
          DVLOG(1) << "Detected invalid cursor end in gnome. Will disable "
                      "preedit selection";
          compositor_sends_invalid_cursor_end_ = true;
        }
      }
      // Once an erroneous cursor end value is detected, it always be wrong
      // going forward.
      // So set it equal to cursor begin as workaround, i.e. default to cursor
      // position at cursor_begin instead of using a selection.
      if (compositor_sends_invalid_cursor_end_) {
        offsets[1] = offsets[0];
      }
    }
    if (offsets[0] == std::u16string::npos ||
        offsets[1] == std::u16string::npos) {
      DVLOG(1) << "got invalid cursor position (byte offset)="
               << preedit_cursor.start() << "-" << preedit_cursor.end();
      // Invalid cursor position. Do nothing.
      return;
    }
    composition_text.selection = gfx::Range(offsets[0], offsets[1]);
  }

  surrounding_text_tracker_.OnSetCompositionText(composition_text);
  ime_delegate_->OnPreeditChanged(composition_text);
}

void WaylandInputMethodContext::OnCommitString(std::string_view text) {
  if (pending_keep_selection_) {
    surrounding_text_tracker_.OnConfirmCompositionText(true);
    ime_delegate_->OnConfirmCompositionText(true);
    pending_keep_selection_ = false;
    return;
  }
  std::u16string text_utf16 = base::UTF8ToUTF16(text);
  surrounding_text_tracker_.OnInsertText(
      text_utf16,
      TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  ime_delegate_->OnCommit(text_utf16);
}

void WaylandInputMethodContext::OnCursorPosition(int32_t index,
                                                 int32_t anchor) {
  const auto& [surrounding_text, utf16_offset, selection,
               unused_composition_text] =
      surrounding_text_tracker_.predicted_state();

  if (surrounding_text.empty()) {
    LOG(ERROR) << "SetSurroundingText should run before OnCursorPosition.";
    return;
  }

  // Adjust index and anchor to the position in `surrounding_text_`.
  // `index` and `anchor` sent from Exo is for the surrounding text sent to Exo
  // which could be trimmed when the actual surrounding text is longer than 4000
  // bytes.
  // Note that `index` and `anchor` is guaranteed to be under 4000 bytes,
  // adjusted index and anchor below won't overflow.
  std::vector<size_t> offsets = {index + surrounding_text_offset_,
                                 anchor + surrounding_text_offset_};
  base::UTF8ToUTF16AndAdjustOffsets(base::UTF16ToUTF8(surrounding_text),
                                    &offsets);
  if (offsets[0] == std::u16string::npos ||
      offsets[0] > surrounding_text.size()) {
    LOG(ERROR) << "Invalid index is specified.";
    return;
  }
  if (offsets[1] == std::u16string::npos ||
      offsets[1] > surrounding_text.size()) {
    LOG(ERROR) << "Invalid anchor is specified.";
    return;
  }

  const gfx::Range new_selection_range =
      gfx::Range(offsets[1] + utf16_offset, offsets[0] + utf16_offset);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Cursor position may be wrong on Lacros due to timing issue for some
  // scenario when surrounding text is longer than wayland message size
  // limitation (4000 bytes) such as:
  // 1. Set surrounding text with 8000 bytes and send the selection adjusted to
  // 4000 bytes (wayland message size maximum).
  // 2. Exo requests to delete surrounding text sent from 1.
  // 3. Before receiving OnDeleteSurrounding, move the selection to 4000 bytes
  // (this implies that surrounding text sent to Exo is changed) on wayland and
  // set surrounding text.
  // In this case, Exo can only know the relative position to the offset trimmed
  // on Wayland, so the position is mismatched to Wayland.
  //
  // This timing issue will be fixed by sending whole surrounding text instead
  // of trimmed text.
  if (selection == new_selection_range) {
    pending_keep_selection_ = true;
  } else {
    NOTIMPLEMENTED_LOG_ONCE();
  }
#endif

  surrounding_text_tracker_.OnSetEditableSelectionRange(new_selection_range);
}

void WaylandInputMethodContext::OnDeleteSurroundingText(int32_t index,
                                                        uint32_t length) {
  const auto& [surrounding_text, utf16_offset, selection, unsused_composition] =
      surrounding_text_tracker_.predicted_state();
  DCHECK(selection.IsValid());

  // TODO(crbug.com/40189286): Currently data sent from delete surrounding text
  // from exo is broken. Currently this broken behavior is supported to prevent
  // visible regressions, but should be fixed in the future, specifically the
  // compatibility with non-exo wayland compositors.
  std::vector<size_t> offsets_for_adjustment = {
      surrounding_text_offset_ + index,
      surrounding_text_offset_ + index + length,
  };
  base::UTF8ToUTF16AndAdjustOffsets(base::UTF16ToUTF8(surrounding_text),
                                    &offsets_for_adjustment);
  if (base::Contains(offsets_for_adjustment, std::u16string::npos)) {
    LOG(DFATAL) << "The selection range for surrounding text is invalid.";
    return;
  }

  if (selection.GetMin() < offsets_for_adjustment[0] + utf16_offset ||
      selection.GetMax() > offsets_for_adjustment[1] + utf16_offset) {
    // The range is started after the selection, or ended before the selection,
    // which is not supported.
    LOG(DFATAL) << "The deletion range needs to cover whole selection range.";
    return;
  }

  // Move by offset calculated in SetSurroundingText to adjust to the original
  // text place.
  size_t before = selection.GetMin() - offsets_for_adjustment[0] - utf16_offset;
  size_t after = offsets_for_adjustment[1] + utf16_offset - selection.GetMax();

  surrounding_text_tracker_.OnExtendSelectionAndDelete(before, after);
  ime_delegate_->OnDeleteSurroundingText(before, after);
}

void WaylandInputMethodContext::OnKeysym(uint32_t keysym,
                                         uint32_t state,
                                         uint32_t modifiers_bits,
                                         uint32_t time) {
#if BUILDFLAG(USE_XKBCOMMON)
  auto* layout_engine = KeyboardLayoutEngineManager::GetKeyboardLayoutEngine();
  if (!layout_engine)
    return;

  // TODO(crbug.com/40817413): This is for the backward compatibility with older
  // ash-chrome (M101 and earlier). In that version of ash-chrome didn't send
  // CapsLock so that we hit an issue on using it.
  // Because newer ash-chrome always sends CapsLock modifier map, as short term
  // workaround, check the condition to identify whether Lacros is running
  // on top of enough newer ash-chrome.
  // To avoid accident, we also check text_input_extension, which is available
  // only on ash-chrome.
  // We can remove this workaround check in M104 or later.
  std::optional<std::vector<std::string_view>> modifiers;
  if (!connection_->text_input_extension_v1() ||
      base::Contains(modifiers_map_, XKB_MOD_NAME_CAPS)) {
    std::vector<std::string_view> modifier_content;
    for (size_t i = 0; i < modifiers_map_.size(); ++i) {
      if (modifiers_bits & (1 << i))
        modifier_content.emplace_back(modifiers_map_[i]);
    }
    modifiers = std::move(modifier_content);
  }

  DomCode dom_code = static_cast<XkbKeyboardLayoutEngine*>(layout_engine)
                         ->GetDomCodeByKeysym(keysym, modifiers);
  if (dom_code == DomCode::NONE)
    return;

  // Keyboard might not exist.
  int device_id = connection_->seat()->keyboard()
                      ? connection_->seat()->keyboard()->device_id()
                      : 0;

  EventType type = state == WL_KEYBOARD_KEY_STATE_PRESSED
                       ? EventType::kKeyPressed
                       : EventType::kKeyReleased;
  key_delegate_->OnKeyboardKeyEvent(
      type, dom_code, /*repeat=*/false, std::nullopt,
      wl::EventMillisecondsToTimeTicks(time), device_id,
      WaylandKeyboard::KeyEventKind::kKey);
#else
  NOTIMPLEMENTED();
#endif
}

void WaylandInputMethodContext::OnSetPreeditRegion(
    int32_t index,
    uint32_t length,
    const std::vector<SpanStyle>& spans) {
  const auto& [surrounding_text, utf16_offset, selection,
               unused_composition_text] =
      surrounding_text_tracker_.predicted_state();

  std::vector<size_t> selection_utf8_offsets = {
      selection.start() - utf16_offset, selection.end() - utf16_offset};
  std::string surrounding_text_utf8 = base::UTF16ToUTF8AndAdjustOffsets(
      surrounding_text, &selection_utf8_offsets);

  if (surrounding_text.empty() || !selection.IsValid()) {
    LOG(ERROR) << "SetSurroundingText should run before OnSetPreeditRegion.";
    return;
  }

  // |index| and |length| are expected to be in UTF8. |index| is relative to the
  // current cursor position.

  // Validation of index and length.
  if (index < 0 && selection_utf8_offsets[1] < static_cast<uint32_t>(-index)) {
    LOG(ERROR) << "Invalid starting point is specified";
    return;
  }
  size_t begin_utf8 = static_cast<size_t>(
      static_cast<ssize_t>(selection_utf8_offsets[1]) + index);
  size_t end_utf8 = begin_utf8 + length;
  if (end_utf8 > surrounding_text_utf8.size()) {
    LOG(ERROR) << "Too long preedit range is specified";
    return;
  }

  std::vector<size_t> offsets = {begin_utf8, end_utf8};
  for (const auto& span : spans) {
    offsets.push_back(begin_utf8 + span.index);
    offsets.push_back(begin_utf8 + span.index + span.length);
  }
  base::UTF8ToUTF16AndAdjustOffsets(surrounding_text_utf8, &offsets);
  if (offsets[0] == std::u16string::npos ||
      offsets[1] == std::u16string::npos) {
    LOG(ERROR) << "Invalid range is specified";
    return;
  }

  std::vector<ImeTextSpan> ime_text_spans;
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

    const auto& style = spans[i].style;
    if (!style.has_value())
      continue;
    ime_text_spans.emplace_back(style->type, begin_span - offsets[0],
                                end_span - offsets[0], style->thickness);
  }

  surrounding_text_tracker_.OnSetCompositionFromExistingText(
      gfx::Range(offsets[0] + utf16_offset, offsets[1] + utf16_offset));
  ime_delegate_->OnSetPreeditRegion(
      gfx::Range(offsets[0] + utf16_offset, offsets[1] + utf16_offset),
      ime_text_spans);
}

void WaylandInputMethodContext::OnClearGrammarFragments(
    const gfx::Range& range) {
  const auto& [surrounding_text, utf16_offset, selection, composition] =
      surrounding_text_tracker_.predicted_state();

  std::vector<size_t> offsets = {range.start() + surrounding_text_offset_,
                                 range.end() + surrounding_text_offset_};
  base::UTF8ToUTF16AndAdjustOffsets(base::UTF16ToUTF8(surrounding_text),
                                    &offsets);
  ime_delegate_->OnClearGrammarFragments(
      gfx::Range(static_cast<uint32_t>(offsets[0]) + utf16_offset,
                 static_cast<uint32_t>(offsets[1]) + utf16_offset));
}

void WaylandInputMethodContext::OnAddGrammarFragment(
    const GrammarFragment& fragment) {
  const auto& [surrounding_text, utf16_offset, selection, composition] =
      surrounding_text_tracker_.predicted_state();

  std::vector<size_t> offsets = {
      fragment.range.start() + surrounding_text_offset_,
      fragment.range.end() + surrounding_text_offset_};
  base::UTF8ToUTF16AndAdjustOffsets(base::UTF16ToUTF8(surrounding_text),
                                    &offsets);
  ime_delegate_->OnAddGrammarFragment({GrammarFragment(
      gfx::Range(static_cast<uint32_t>(offsets[0]) + utf16_offset,
                 static_cast<uint32_t>(offsets[1]) + utf16_offset),
      fragment.suggestion)});
}

void WaylandInputMethodContext::OnSetAutocorrectRange(const gfx::Range& range) {
  if (range.is_empty()) {
    ime_delegate_->OnSetAutocorrectRange(range);
    return;
  }

  const auto& [surrounding_text, utf16_offset, selection, composition] =
      surrounding_text_tracker_.predicted_state();
  std::vector<size_t> offsets = {range.start() + surrounding_text_offset_,
                                 range.end() + surrounding_text_offset_};
  base::UTF8ToUTF16AndAdjustOffsets(base::UTF16ToUTF8(surrounding_text),
                                    &offsets);
  ime_delegate_->OnSetAutocorrectRange(
      gfx::Range(static_cast<uint32_t>(offsets[0]) + utf16_offset,
                 static_cast<uint32_t>(offsets[1]) + utf16_offset));
}

void WaylandInputMethodContext::OnSetVirtualKeyboardOccludedBounds(
    const gfx::Rect& screen_bounds) {
  ime_delegate_->OnSetVirtualKeyboardOccludedBounds(screen_bounds);

  for (auto& client : past_clients_) {
    if (client.second)
      client.second->EnsureCaretNotInRect(screen_bounds);
  }
  if (screen_bounds.IsEmpty())
    past_clients_.clear();
}

void WaylandInputMethodContext::OnConfirmPreedit(bool keep_selection) {
  surrounding_text_tracker_.OnConfirmCompositionText(keep_selection);
  ime_delegate_->OnConfirmCompositionText(keep_selection);
}

void WaylandInputMethodContext::OnInputPanelState(uint32_t state) {
  virtual_keyboard_visible_ = (state & 1) != 0;
  // Note: Currently there's no support of VirtualKeyboardControllerObserver.
  // In the future, we may need to support it. Specifically,
  // RenderWidgetHostViewAura would like to know the VirtualKeyboard's
  // region somehow.
}

void WaylandInputMethodContext::OnModifiersMap(
    std::vector<std::string> modifiers_map) {
  modifiers_map_ = std::move(modifiers_map);
}

void WaylandInputMethodContext::OnInsertImage(const GURL& src) {
  ime_delegate_->OnInsertImage(src);
}

void WaylandInputMethodContext::OnKeyboardFocusedWindowChanged() {
  MaybeUpdateActivated(false, TextInputClient::FOCUS_REASON_OTHER);
}

void WaylandInputMethodContext::MaybeUpdateActivated(
    bool skip_virtual_keyboard_update,
    TextInputClient::FocusReason reason) {
  if (!text_input_)
    return;

  WaylandWindow* window =
      connection_->window_manager()->GetCurrentKeyboardFocusedWindow();
  if (!window && !connection_->seat()->keyboard())
    window = connection_->window_manager()->GetCurrentActiveWindow();
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
    text_input_->Activate(window, reason);
    text_input_->SetContentType(
        attributes_.input_type, attributes_.input_mode, attributes_.flags,
        attributes_.should_do_learning, attributes_.can_compose_inline);
    if (!skip_virtual_keyboard_update)
      DisplayVirtualKeyboard();
  } else {
    if (!skip_virtual_keyboard_update)
      DismissVirtualKeyboard();
    text_input_->Deactivate();
  }
}

}  // namespace ui
