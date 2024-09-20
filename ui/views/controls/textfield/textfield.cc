// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/textfield/textfield.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/default_style.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/ime/constants.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/canvas_painter.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/gesture_event_details.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/selection_bound.h"
#include "ui/native_theme/native_theme.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/touch_selection/touch_selection_metrics.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/accessibility/views_utilities_aura.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/controls/views_text_services_context_menu.h"
#include "ui/views/drag_utils.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/painter.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/touchui/touch_selection_controller.h"
#include "ui/views/views_delegate.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "ui/base/ime/linux/text_edit_command_auralinux.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/linux/linux_ui.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/aura/window.h"
#include "ui/wm/core/ime_util_chromeos.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "ui/base/cocoa/secure_password_input.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_gl_egl_utility.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/input_method_manager.h"
#endif

#if defined(USE_AURA)
#include "ui/views/touchui/touch_selection_controller_impl.h"
#endif

namespace views {
namespace {

using ::ui::mojom::DragOperation;

// An enum giving different model properties unique keys for the
// OnPropertyChanged call.
enum TextfieldPropertyKey {
  kTextfieldText = 1,
  kTextfieldTextColor,
  kTextfieldSelectionTextColor,
  kTextfieldBackgroundColor,
  kTextfieldSelectionBackgroundColor,
  kTextfieldCursorEnabled,
  kTextfieldHorizontalAlignment,
  kTextfieldSelectedRange,
};

// Returns the ui::TextEditCommand corresponding to the |command_id| menu
// action. |has_selection| is true if the textfield has an active selection.
// Keep in sync with UpdateContextMenu.
ui::TextEditCommand GetTextEditCommandFromMenuCommand(int command_id,
                                                      bool has_selection) {
  switch (command_id) {
    case Textfield::kUndo:
      return ui::TextEditCommand::UNDO;
    case Textfield::kCut:
      return ui::TextEditCommand::CUT;
    case Textfield::kCopy:
      return ui::TextEditCommand::COPY;
    case Textfield::kPaste:
      return ui::TextEditCommand::PASTE;
    case Textfield::kDelete:
      // The DELETE menu action only works in case of an active selection.
      if (has_selection)
        return ui::TextEditCommand::DELETE_FORWARD;
      break;
    case Textfield::kSelectAll:
      return ui::TextEditCommand::SELECT_ALL;
    case Textfield::kSelectWord:
      return ui::TextEditCommand::SELECT_WORD;
  }
  return ui::TextEditCommand::INVALID_COMMAND;
}

base::TimeDelta GetPasswordRevealDuration(const ui::KeyEvent& event) {
  // The key event may carries the property that indicates it was from the
  // virtual keyboard and mirroring is not occurring
  // In that case, reveal the password characters for 1 second.
  auto* properties = event.properties();
  bool from_vk =
      properties && properties->find(ui::kPropertyFromVK) != properties->end();
  if (from_vk) {
    std::vector<uint8_t> fromVKPropertyArray =
        properties->find(ui::kPropertyFromVK)->second;
    DCHECK_GT(fromVKPropertyArray.size(), ui::kPropertyFromVKIsMirroringIndex);
    uint8_t is_mirroring =
        fromVKPropertyArray[ui::kPropertyFromVKIsMirroringIndex];
    if (!is_mirroring)
      return base::Seconds(1);
  }
  return base::TimeDelta();
}

bool IsControlKeyModifier(int flags) {
// XKB layout doesn't natively generate printable characters from a
// Control-modified key combination, but we cannot extend it to other platforms
// as Control has different meanings and behaviors.
// https://crrev.com/2580483002/#msg46
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return flags & ui::EF_CONTROL_DOWN;
#else
  return false;
#endif
}

bool IsValidCharToInsert(const char16_t& ch) {
  // Filter out all control characters, including tab and new line characters.
  return (ch >= 0x20 && ch < 0x7F) || ch > 0x9F;
}

#if BUILDFLAG(IS_MAC)
const float kAlmostTransparent = 1.0 / 255.0;
const float kOpaque = 1.0;
#endif

}  // namespace

// static
base::TimeDelta Textfield::GetCaretBlinkInterval() {
  return ui::NativeTheme::GetInstanceForNativeUi()->GetCaretBlinkInterval();
}

// static
const gfx::FontList& Textfield::GetDefaultFontList() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetFontListWithDelta(ui::kLabelFontSizeDelta);
}

Textfield::Textfield()
    : model_(new TextfieldModel(this)),
      placeholder_text_draw_flags_(gfx::Canvas::DefaultCanvasTextAlignment()),
      selection_controller_(this) {
  set_context_menu_controller(this);
  set_drag_controller(this);
  GetViewAccessibility().set_needs_ax_tree_manager(true);
  auto cursor_view = std::make_unique<View>();
  cursor_view->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  cursor_view->GetViewAccessibility().SetIsIgnored(true);
  cursor_view_ = AddChildView(std::move(cursor_view));
  GetRenderText()->SetFontList(GetDefaultFontList());
  UpdateDefaultBorder();
  SetFocusBehavior(FocusBehavior::ALWAYS);
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                GetCornerRadius());
  FocusRing::Install(this);
  FocusRing::Get(this)->SetOutsetFocusRingDisabled(true);
  InkDropHost* ink_drop_host =
      InkDrop::Install(this, std::make_unique<views::InkDropHost>(this));
  ink_drop_host->SetMode(InkDropHost::InkDropMode::ON);
  ink_drop_host->SetLayerRegion(LayerRegion::kAbove);
  ink_drop_host->SetHighlightOpacity(1.0f);
  ink_drop_host->SetBaseColorCallback(base::BindRepeating(
      [](Textfield* host) {
        return host->HasFocus() ? SK_ColorTRANSPARENT
                                : host->GetColorProvider()->GetColor(
                                      ui::kColorTextfieldHover);
      },
      this));

#if !BUILDFLAG(IS_MAC)
  // Do not map accelerators on Mac. E.g. They might not reflect custom
  // keybindings that a user has set. But also on Mac, these commands dispatch
  // via the "responder chain" when the OS searches through menu items in the
  // menu bar. The menu then sends e.g., a "cut:" command to NativeWidgetMac,
  // which will pass it to Textfield via OnKeyEvent() after associating the
  // correct edit command.

  // These allow BrowserView to pass edit commands from the Chrome menu to us
  // when we're focused by simply asking the FocusManager to
  // ProcessAccelerator() with the relevant accelerators.
  AddAccelerator(ui::Accelerator(ui::VKEY_X, ui::EF_CONTROL_DOWN));
  AddAccelerator(ui::Accelerator(ui::VKEY_C, ui::EF_CONTROL_DOWN));
  AddAccelerator(ui::Accelerator(ui::VKEY_V, ui::EF_CONTROL_DOWN));
#endif

  GetViewAccessibility().SetRole(ax::mojom::Role::kTextField);

  // Sometimes there are additional ignored views, such as the View representing
  // the cursor, inside the text field. These should always be ignored by
  // accessibility since a plain text field should always be a leaf node in the
  // accessibility trees of all the platforms we support.
  GetViewAccessibility().SetIsLeaf(true);
  UpdateAccessibleDefaultActionVerb();
}

Textfield::~Textfield() {
  if (HasObserver(this)) {
    RemoveObserver(this);
  }

  if (GetInputMethod()) {
    // The textfield should have been blurred before destroy.
    DCHECK(this != GetInputMethod()->GetTextInputClient());
  }
}

void Textfield::SetController(TextfieldController* controller) {
  controller_ = controller;
}

void Textfield::AddedToWidget() {
  UpdateAccessibilityTextDirection();
  UpdateAccessibleValue();
}

bool Textfield::GetReadOnly() const {
  return read_only_;
}

void Textfield::SetReadOnly(bool read_only) {
  if (read_only_ == read_only)
    return;

  // Update read-only without changing the focusable state (or active, etc.).
  read_only_ = read_only;
  if (GetEnabled()) {
    GetViewAccessibility().SetReadOnly(read_only_);
  }
  if (GetInputMethod())
    GetInputMethod()->OnTextInputTypeChanged(this);
  if (GetWidget()) {
    SetColor(GetTextColor());
    UpdateBackgroundColor();
  }

  UpdateDefaultBorder();
  OnPropertyChanged(&read_only_, kPropertyEffectsPaint);
}

void Textfield::SetTextInputType(ui::TextInputType type) {
  if (text_input_type_ == type)
    return;

  GetRenderText()->SetObscured(type == ui::TEXT_INPUT_TYPE_PASSWORD);
  text_input_type_ = type;
  GetViewAccessibility().SetIsProtected(type == ui::TEXT_INPUT_TYPE_PASSWORD);
  if (GetInputMethod())
    GetInputMethod()->OnTextInputTypeChanged(this);
  OnCaretBoundsChanged();
  OnPropertyChanged(&text_input_type_, kPropertyEffectsPaint);
  UpdateAfterChange(TextChangeType::kInternal, false);
}

void Textfield::SetTextInputFlags(int flags) {
  if (text_input_flags_ == flags)
    return;

  text_input_flags_ = flags;
  OnPropertyChanged(&text_input_flags_, kPropertyEffectsNone);
}

const std::u16string& Textfield::GetText() const {
  return model_->text();
}

void Textfield::SetText(const std::u16string& new_text) {
  SetTextWithoutCaretBoundsChangeNotification(new_text, new_text.length());
  // The above call already notified for the text change; fire notifications
  // etc. for the cursor changes as well.
  UpdateAfterChange(TextChangeType::kNone, true);
}

void Textfield::SetTextWithoutCaretBoundsChangeNotification(
    const std::u16string& text,
    size_t cursor_position) {
  model_->SetText(text, cursor_position);
  UpdateAfterChange(TextChangeType::kInternal, false, false);
}

void Textfield::Scroll(const std::vector<size_t>& positions) {
  for (const auto position : positions) {
    model_->MoveCursorTo(position);
    GetRenderText()->GetUpdatedCursorBounds();
  }
}

void Textfield::AppendText(const std::u16string& new_text) {
  if (new_text.empty())
    return;
  model_->Append(new_text);
  UpdateAfterChange(TextChangeType::kInternal, false);
}

void Textfield::InsertOrReplaceText(const std::u16string& new_text) {
  if (new_text.empty())
    return;
  model_->InsertText(new_text);
  UpdateAfterChange(TextChangeType::kUserTriggered, true);
}

std::u16string Textfield::GetSelectedText() const {
  return model_->GetSelectedText();
}

void Textfield::SelectAll(bool reversed) {
  model_->SelectAll(reversed);
  if (HasSelection() && performing_user_action_)
    UpdateSelectionClipboard();
  UpdateAfterChange(TextChangeType::kNone, true);
}

void Textfield::SelectWord() {
  model_->SelectWord();
  if (HasSelection() && performing_user_action_) {
    UpdateSelectionClipboard();
  }
  UpdateAfterChange(TextChangeType::kNone, true);
}

void Textfield::SelectWordAt(const gfx::Point& point) {
  model_->MoveCursorTo(point, false);
  model_->SelectWord();
  UpdateAfterChange(TextChangeType::kNone, true);
}

void Textfield::ClearSelection() {
  model_->ClearSelection();
  UpdateAfterChange(TextChangeType::kNone, true);
}

bool Textfield::HasSelection(bool primary_only) const {
  return model_->HasSelection(primary_only);
}

SkColor Textfield::GetTextColor() const {
  return text_color_.value_or(
      GetColorProvider()->GetColor(TypographyProvider::Get().GetColorId(
          style::CONTEXT_TEXTFIELD, GetTextStyle())));
}

void Textfield::SetTextColor(SkColor color) {
  text_color_ = color;
  if (GetWidget())
    SetColor(color);
}

SkColor Textfield::GetBackgroundColor() const {
  return background_color_.value_or(GetColorProvider()->GetColor(
      GetReadOnly() || !GetEnabled() ? ui::kColorTextfieldBackgroundDisabled
                                     : ui::kColorTextfieldBackground));
}

void Textfield::SetBackgroundColor(SkColor color) {
  background_color_ = color;
  if (GetWidget())
    UpdateBackgroundColor();
}

bool Textfield::GetBackgroundEnabled() const {
  return is_background_enabled_;
}

void Textfield::SetBackgroundEnabled(bool enabled) {
  is_background_enabled_ = enabled;
}

SkColor Textfield::GetSelectionTextColor() const {
  return selection_text_color_.value_or(
      GetColorProvider()->GetColor(ui::kColorTextfieldSelectionForeground));
}

void Textfield::SetSelectionTextColor(SkColor color) {
  selection_text_color_ = color;
  UpdateSelectionTextColor();
}

SkColor Textfield::GetSelectionBackgroundColor() const {
  return selection_background_color_.value_or(
      GetColorProvider()->GetColor(ui::kColorTextfieldSelectionBackground));
}

void Textfield::SetSelectionBackgroundColor(SkColor color) {
  selection_background_color_ = color;
  UpdateSelectionBackgroundColor();
}

bool Textfield::GetCursorEnabled() const {
  return GetRenderText()->cursor_enabled();
}

void Textfield::SetCursorEnabled(bool enabled) {
  if (GetRenderText()->cursor_enabled() == enabled)
    return;

  GetRenderText()->SetCursorEnabled(enabled);
  UpdateAfterChange(TextChangeType::kNone, true, false);
  OnPropertyChanged(
      ui::metadata::MakeUniquePropertyKey(&model_, kTextfieldCursorEnabled),
      kPropertyEffectsPaint);
}

const gfx::FontList& Textfield::GetFontList() const {
  return GetRenderText()->font_list();
}

void Textfield::SetFontList(const gfx::FontList& font_list) {
  GetRenderText()->SetFontList(font_list);
  OnCaretBoundsChanged();
  PreferredSizeChanged();
}

void Textfield::SetDefaultWidthInChars(int default_width) {
  DCHECK_GE(default_width, 0);
  default_width_in_chars_ = default_width;
}

void Textfield::SetMinimumWidthInChars(int minimum_width) {
  DCHECK_GE(minimum_width, -1);
  minimum_width_in_chars_ = minimum_width;
}

const std::u16string& Textfield::GetPlaceholderText() const {
  return placeholder_text_;
}

void Textfield::SetPlaceholderText(const std::u16string& text) {
  if (placeholder_text_ == text)
    return;

  placeholder_text_ = text;
  GetViewAccessibility().SetPlaceholder(base::UTF16ToUTF8(text));
  OnPropertyChanged(&placeholder_text_, kPropertyEffectsPaint);
}

gfx::HorizontalAlignment Textfield::GetHorizontalAlignment() const {
  return GetRenderText()->horizontal_alignment();
}

void Textfield::SetHorizontalAlignment(gfx::HorizontalAlignment alignment) {
  GetRenderText()->SetHorizontalAlignment(alignment);

  OnPropertyChanged(ui::metadata::MakeUniquePropertyKey(
                        &model_, kTextfieldHorizontalAlignment),
                    kPropertyEffectsNone);
}

void Textfield::ShowVirtualKeyboardIfEnabled() {
  // GetInputMethod() may return nullptr in tests.
  if (GetEnabled() && !GetReadOnly() && GetInputMethod())
    GetInputMethod()->SetVirtualKeyboardVisibilityIfEnabled(true);
}

bool Textfield::IsIMEComposing() const {
  return model_->HasCompositionText();
}

const gfx::Range& Textfield::GetSelectedRange() const {
  return GetRenderText()->selection();
}

void Textfield::SetSelectedRange(const gfx::Range& range) {
  model_->SelectRange(range);
  UpdateAfterChange(TextChangeType::kNone, true);
  OnPropertyChanged(
      ui::metadata::MakeUniquePropertyKey(&model_, kTextfieldSelectedRange),
      kPropertyEffectsPaint);
}

void Textfield::AddSecondarySelectedRange(const gfx::Range& range) {
  model_->SelectRange(range, false);
  OnPropertyChanged(
      ui::metadata::MakeUniquePropertyKey(&model_, kTextfieldSelectedRange),
      kPropertyEffectsPaint);
}

const gfx::SelectionModel& Textfield::GetSelectionModel() const {
  return GetRenderText()->selection_model();
}

void Textfield::SelectSelectionModel(const gfx::SelectionModel& sel) {
  model_->SelectSelectionModel(sel);
  UpdateAfterChange(TextChangeType::kNone, true);
}

size_t Textfield::GetCursorPosition() const {
  return model_->GetCursorPosition();
}

void Textfield::SetColor(SkColor value) {
  GetRenderText()->SetColor(value);
  cursor_view_->layer()->SetColor(value);
  OnPropertyChanged(
      ui::metadata::MakeUniquePropertyKey(&model_, kTextfieldTextColor),
      kPropertyEffectsPaint);
}

void Textfield::ApplyColor(SkColor value, const gfx::Range& range) {
  GetRenderText()->ApplyColor(value, range);
  SchedulePaint();
}

void Textfield::SetStyle(gfx::TextStyle style, bool value) {
  GetRenderText()->SetStyle(style, value);
  SchedulePaint();
}

void Textfield::ApplyStyle(gfx::TextStyle style,
                           bool value,
                           const gfx::Range& range) {
  GetRenderText()->ApplyStyle(style, value, range);
  SchedulePaint();
}

bool Textfield::GetInvalid() const {
  return invalid_;
}

void Textfield::SetInvalid(bool invalid) {
  if (invalid == invalid_)
    return;
  invalid_ = invalid;
  UpdateDefaultBorder();
  if (FocusRing::Get(this))
    FocusRing::Get(this)->SetInvalid(invalid);
  OnPropertyChanged(&invalid_, kPropertyEffectsNone);
}

void Textfield::ClearEditHistory() {
  model_->ClearEditHistory();
}

void Textfield::SetObscuredGlyphSpacing(int spacing) {
  GetRenderText()->SetObscuredGlyphSpacing(spacing);
}

void Textfield::SetExtraInsets(const gfx::Insets& insets) {
  extra_insets_ = insets;
  UpdateDefaultBorder();
}

void Textfield::FitToLocalBounds() {
  // Textfield insets include a reasonable amount of whitespace on all sides of
  // the default font list. Fallback fonts with larger heights may paint over
  // the vertical whitespace as needed. Alternate solutions involve undesirable
  // behavior like changing the default font size, shrinking some fallback fonts
  // beyond their legibility, or enlarging controls dynamically with content.
  gfx::Rect bounds = GetLocalBounds();
  const gfx::Insets insets = GetInsets();

  if (GetRenderText()->multiline()) {
    bounds.Inset(insets);
  } else {
    // The text will draw with the correct vertical alignment if we don't apply
    // the vertical insets.
    bounds.Inset(gfx::Insets::TLBR(0, insets.left(), 0, insets.right()));
  }

  bounds.set_x(GetMirroredXForRect(bounds));
  GetRenderText()->SetDisplayRect(bounds);
  UpdateAfterChange(TextChangeType::kNone, true);
}

bool Textfield::GetUseDefaultBorder() const {
  return use_default_border_;
}
void Textfield::SetUseDefaultBorder(bool use_default_border) {
  use_default_border_ = use_default_border;
}

void Textfield::RemoveHoverEffect() {
  // If no Inkdrop has been installed, this will no-op.
  InkDrop::Remove(this);
}

////////////////////////////////////////////////////////////////////////////////
// Textfield, View overrides:

int Textfield::GetBaseline() const {
  return GetInsets().top() + GetRenderText()->GetBaseline();
}

gfx::Size Textfield::CalculatePreferredSize(
    const SizeBounds& available_size) const {
  DCHECK_GE(default_width_in_chars_, minimum_width_in_chars_);
  return gfx::Size(
      CharsToDips(default_width_in_chars_),
      LayoutProvider::GetControlHeightForFont(style::CONTEXT_TEXTFIELD,
                                              GetTextStyle(), GetFontList()));
}

gfx::Size Textfield::GetMinimumSize() const {
  DCHECK_LE(minimum_width_in_chars_, default_width_in_chars_);
  gfx::Size minimum_size = View::GetMinimumSize();
  if (minimum_width_in_chars_ >= 0)
    minimum_size.set_width(CharsToDips(minimum_width_in_chars_));
  return minimum_size;
}

void Textfield::SetBorder(std::unique_ptr<Border> b) {
  FocusRing::Remove(this);
  View::SetBorder(std::move(b));
  use_default_border_ = false;
}

ui::Cursor Textfield::GetCursor(const ui::MouseEvent& event) {
  bool platform_arrow = PlatformStyle::kTextfieldUsesDragCursorWhenDraggable;
  bool in_selection = GetRenderText()->IsPointInSelection(event.location());
  bool drag_event = event.type() == ui::EventType::kMouseDragged;
  bool text_cursor =
      !initiating_drag_ && (drag_event || !in_selection || !platform_arrow);
  return text_cursor ? ui::mojom::CursorType::kIBeam : ui::Cursor();
}

bool Textfield::OnMousePressed(const ui::MouseEvent& event) {
  const bool had_focus = HasFocus();
  bool handled = controller_ && controller_->HandleMouseEvent(this, event);
  if (InkDrop::Get(this)) {
    // When a textfield is pressed, the hover state should be off and the
    // background color should no longer have a mask.
    InkDrop::Get(this)->GetInkDrop()->SetHovered(false);
  }
  // If the controller triggered the focus, then record the focus reason as
  // other.
  if (!had_focus && HasFocus())
    focus_reason_ = ui::TextInputClient::FOCUS_REASON_OTHER;

  if (!handled &&
      (event.IsOnlyLeftMouseButton() || event.IsOnlyRightMouseButton())) {
    if (!had_focus)
      RequestFocusWithPointer(ui::EventPointerType::kMouse);
#if !BUILDFLAG(IS_WIN)
    ShowVirtualKeyboardIfEnabled();
#endif
  }

  if (ui::Clipboard::IsSupportedClipboardBuffer(
          ui::ClipboardBuffer::kSelection)) {
    if (!handled && !had_focus && event.IsOnlyMiddleMouseButton())
      RequestFocusWithPointer(ui::EventPointerType::kMouse);
  }

  return selection_controller_.OnMousePressed(
      event, handled,
      had_focus
          ? SelectionController::InitialFocusStateOnMousePress::kFocused
          : SelectionController::InitialFocusStateOnMousePress::kUnFocused);
}

bool Textfield::OnMouseDragged(const ui::MouseEvent& event) {
  return selection_controller_.OnMouseDragged(event);
}

void Textfield::OnMouseReleased(const ui::MouseEvent& event) {
  if (controller_)
    controller_->HandleMouseEvent(this, event);
  selection_controller_.OnMouseReleased(event);
}

void Textfield::OnMouseCaptureLost() {
  selection_controller_.OnMouseCaptureLost();
}

bool Textfield::OnMouseWheel(const ui::MouseWheelEvent& event) {
  return controller_ && controller_->HandleMouseEvent(this, event);
}

WordLookupClient* Textfield::GetWordLookupClient() {
  return this;
}

bool Textfield::OnKeyPressed(const ui::KeyEvent& event) {
  if (PreHandleKeyPressed(event))
    return true;

  ui::TextEditCommand edit_command = scheduled_text_edit_command_;
  scheduled_text_edit_command_ = ui::TextEditCommand::INVALID_COMMAND;

  // Since HandleKeyEvent() might destroy |this|, get a weak pointer and verify
  // it isn't null before proceeding.
  base::WeakPtr<Textfield> textfield(weak_ptr_factory_.GetWeakPtr());

  bool handled = controller_ && controller_->HandleKeyEvent(this, event);

  if (!textfield)
    return handled;

#if BUILDFLAG(IS_LINUX)
  auto* linux_ui = ui::LinuxUi::instance();
  std::vector<ui::TextEditCommandAuraLinux> commands;
  if (!handled && linux_ui &&
      linux_ui->GetTextEditCommandsForEvent(event, ui::TEXT_INPUT_FLAG_NONE,
                                            &commands)) {
    for (const auto& command : commands) {
      if (IsTextEditCommandEnabled(command.command())) {
        ExecuteTextEditCommand(command.command());
        handled = true;
      }
    }
    return handled;
  }
#endif

  base::AutoReset<bool> show_rejection_ui(&show_rejection_ui_if_any_, true);

  if (edit_command == ui::TextEditCommand::INVALID_COMMAND)
    edit_command = GetCommandForKeyEvent(event);

  if (!handled && IsTextEditCommandEnabled(edit_command)) {
    ExecuteTextEditCommand(edit_command);
    handled = true;
  }
  return handled;
}

bool Textfield::OnKeyReleased(const ui::KeyEvent& event) {
  return controller_ && controller_->HandleKeyEvent(this, event);
}

void Textfield::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::EventType::kGestureTap: {
      RequestFocusForGesture(event->details());
      if (controller_ && controller_->HandleGestureEvent(this, *event)) {
        StopSelectionDragging();
        event->SetHandled();
        return;
      }
      if (HandleGestureForSelectionDragging(event)) {
        return;
      }
      const size_t tap_pos =
          GetRenderText()->FindCursorPosition(event->location()).caret_pos();
      const bool should_toggle_menu = event->details().tap_count() == 1 &&
                                      GetSelectedRange() == gfx::Range(tap_pos);
      if (event->details().tap_count() == 1) {
        // If tap is on the selection and touch handles are not present,
        // handles should be shown without changing selection. Otherwise,
        // cursor should be moved to the tap location.
        if (touch_selection_controller_ ||
            !GetRenderText()->IsPointInSelection(event->location())) {
          OnBeforeUserAction();
          MoveCursorTo(event->location(), false);
          OnAfterUserAction();
        }
      } else if (event->details().tap_count() == 2) {
        OnBeforeUserAction();
        SelectWordAt(event->location());
        OnAfterUserAction();
      } else {
        OnBeforeUserAction();
        SelectAll(false);
        OnAfterUserAction();
      }
      CreateTouchSelectionControllerAndNotifyIt();
      if (touch_selection_controller_ && should_toggle_menu) {
        touch_selection_controller_->ToggleQuickMenu();
      }
      event->SetHandled();
      break;
    }
    case ui::EventType::kGestureTapDown: {
      if (HasFocus()) {
        if (HandleGestureForSelectionDragging(event)) {
          return;
        }
      }
      break;
    }
    case ui::EventType::kGestureLongPress:
      if (GetRenderText()->IsPointInSelection(event->location())) {
        // If long-press happens on the selection, deactivate touch selection
        // and try to initiate drag-drop. If drag-drop is not enabled, context
        // menu will be shown. Event is not marked as handled to let Views
        // handle drag-drop or context menu.
        DestroyTouchSelection();
        StopSelectionDragging();
        initiating_drag_ = switches::IsTouchDragDropEnabled();
        break;
      } else {
        // If long-press happens outside selection, select word and try to
        // activate touch selection.
        OnBeforeUserAction();
        SelectWordAt(event->location());
        OnAfterUserAction();
        CreateTouchSelectionControllerAndNotifyIt();

        if (HandleGestureForSelectionDragging(event)) {
          return;
        }

        // If touch selection is activated, mark the event as handled so that
        // the regular context menu is not shown.
        if (touch_selection_controller_) {
          event->SetHandled();
          return;
        }
      }
      break;
    case ui::EventType::kGestureLongTap:
      if (HandleGestureForSelectionDragging(event)) {
        return;
      }
      // If touch selection is enabled, the context menu on long tap will be
      // shown by the |touch_selection_controller_|, hence we mark the event
      // handled so Views does not try to show context menu on it.
      if (touch_selection_controller_)
        event->SetHandled();
      break;
    case ui::EventType::kGestureScrollBegin:
      if (HasFocus()) {
        if (HandleGestureForSelectionDragging(event)) {
          return;
        }
        OnGestureScrollBegin(event->location().x());
        event->SetHandled();
      }
      break;
    case ui::EventType::kGestureScrollUpdate:
      if (HasFocus()) {
        if (HandleGestureForSelectionDragging(event)) {
          return;
        }
        GestureScroll(event->location().x());
        event->SetHandled();
      }
      break;
    case ui::EventType::kGestureScrollEnd:
    case ui::EventType::kScrollFlingStart: {
      const bool gesture_handled = HandleGestureForSelectionDragging(event);
      CHECK(!gesture_handled);
      if (HasFocus()) {
        if (show_touch_handles_after_scroll_) {
          CreateTouchSelectionControllerAndNotifyIt();
          show_touch_handles_after_scroll_ = false;
        }
        event->SetHandled();
      }
      break;
    }
    case ui::EventType::kGestureEnd: {
      const bool gesture_handled = HandleGestureForSelectionDragging(event);
      CHECK(!gesture_handled);
      break;
    }
    default:
      return;
  }
}

// This function is called by BrowserView to execute clipboard commands.
bool Textfield::AcceleratorPressed(const ui::Accelerator& accelerator) {
  ui::KeyEvent event(
      accelerator.key_state() == ui::Accelerator::KeyState::PRESSED
          ? ui::EventType::kKeyPressed
          : ui::EventType::kKeyReleased,
      accelerator.key_code(), accelerator.modifiers());
  ExecuteTextEditCommand(GetCommandForKeyEvent(event));
  return true;
}

bool Textfield::CanHandleAccelerators() const {
  return GetRenderText()->focused() && View::CanHandleAccelerators();
}

void Textfield::AboutToRequestFocusFromTabTraversal(bool reverse) {
  SelectAll(false);
}

bool Textfield::SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) {
#if BUILDFLAG(IS_LINUX)
  // Skip any accelerator handling that conflicts with custom keybindings.
  auto* linux_ui = ui::LinuxUi::instance();
  std::vector<ui::TextEditCommandAuraLinux> commands;
  if (linux_ui && linux_ui->GetTextEditCommandsForEvent(
                      event, ui::TEXT_INPUT_FLAG_NONE, &commands)) {
    const auto is_enabled = [this](const auto& command) {
      return IsTextEditCommandEnabled(command.command());
    };
    if (base::ranges::any_of(commands, is_enabled))
      return true;
  }
#endif

  // Skip backspace accelerator handling; editable textfields handle this key.
  // Also skip processing Windows [Alt]+<num-pad digit> Unicode alt-codes.
  const bool is_backspace = event.key_code() == ui::VKEY_BACK;
  return (is_backspace && !GetReadOnly()) || event.IsUnicodeKeyCode();
}

bool Textfield::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  if (!GetEnabled() || GetReadOnly())
    return false;
  // TODO(msw): Can we support URL, FILENAME, etc.?
  *formats = ui::OSExchangeData::STRING;
  if (controller_)
    controller_->AppendDropFormats(formats, format_types);
  return true;
}

bool Textfield::CanDrop(const OSExchangeData& data) {
  int formats;
  std::set<ui::ClipboardFormatType> format_types;
  GetDropFormats(&formats, &format_types);
  return GetEnabled() && !GetReadOnly() &&
         data.HasAnyFormat(formats, format_types);
}

int Textfield::OnDragUpdated(const ui::DropTargetEvent& event) {
  DCHECK(CanDrop(event.data()));
  gfx::RenderText* render_text = GetRenderText();
  const gfx::Range& selection = render_text->selection();
  drop_cursor_position_ = render_text->FindCursorPosition(event.location());
  bool in_selection =
      !selection.is_empty() &&
      selection.Contains(gfx::Range(drop_cursor_position_.caret_pos()));
  drop_cursor_visible_ = !in_selection;
  // TODO(msw): Pan over text when the user drags to the visible text edge.
  OnCaretBoundsChanged();
  SchedulePaint();

  StopBlinkingCursor();

  if (initiating_drag_) {
    if (in_selection)
      return ui::DragDropTypes::DRAG_NONE;
    return event.IsControlDown() ? ui::DragDropTypes::DRAG_COPY
                                 : ui::DragDropTypes::DRAG_MOVE;
  }
  return ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_MOVE;
}

void Textfield::OnDragExited() {
  drop_cursor_visible_ = false;
  if (ShouldBlinkCursor())
    StartBlinkingCursor();
  SchedulePaint();
}

views::View::DropCallback Textfield::GetDropCallback(
    const ui::DropTargetEvent& event) {
  DCHECK(CanDrop(event.data()));

  drop_cursor_visible_ = false;

  if (controller_) {
    auto cb = controller_->CreateDropCallback(event);
    if (!cb.is_null())
      return cb;
  }

  DCHECK(!initiating_drag_ ||
         !GetRenderText()->IsPointInSelection(event.location()));

  return base::BindOnce(&Textfield::DropDraggedText,
                        drop_weak_ptr_factory_.GetWeakPtr());
}

void Textfield::OnDragDone() {
  initiating_drag_ = false;
  drop_cursor_visible_ = false;
}

void Textfield::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  View::GetAccessibleNodeData(node_data);

  // Editable state indicates support of editable interface, and is always set
  // for a textfield, even if disabled or readonly.
  node_data->AddState(ax::mojom::State::kEditable);

  const gfx::Range range = GetSelectedRange();
  node_data->AddIntAttribute(ax::mojom::IntAttribute::kTextSelStart,
                             base::checked_cast<int32_t>(range.start()));
  node_data->AddIntAttribute(ax::mojom::IntAttribute::kTextSelEnd,
                             base::checked_cast<int32_t>(range.end()));

#if BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)
  // TODO(https://crbug.com/325137417): Recompute the text offsets whenever
  // the value changes, not when GetAccessibleNodeData is different.
  std::u16string ax_value = GetViewAccessibility().GetValue();
  // If the accessible value changed since the last time we computed the text
  // offsets, we need to recompute them.
  if (::ui::AXPlatform::GetInstance().IsUiaProviderEnabled() &&
      (ax_value_used_to_compute_offsets_ != ax_value ||
       needs_ax_text_offsets_update_)) {
    GetViewAccessibility().ClearTextOffsets();

    // TODO(crbug.com/325137417): When this function is only used to initialize
    // the cache with these values, refactor this part to not rely on the cache
    // as it will cause a chicken and egg situation. For now, this is necessary
    // to keep the text offsets up to date.
    RefreshAccessibleTextOffsets();
    ax_value_used_to_compute_offsets_ = ax_value;
    needs_ax_text_offsets_update_ = false;

    node_data->AddIntListAttribute(
        ax::mojom::IntListAttribute::kCharacterOffsets,
        GetViewAccessibility().GetCharacterOffsets());
    node_data->AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts,
                                   GetViewAccessibility().GetWordStarts());
    node_data->AddIntListAttribute(ax::mojom::IntListAttribute::kWordEnds,
                                   GetViewAccessibility().GetWordEnds());
  }
#endif  // BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)
}

#if BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)
void Textfield::RefreshAccessibleTextOffsets() {
  // TODO(crbug.com/40933356): Add support for multiline textfields.
  if (GetRenderText()->multiline()) {
    return;
  }

  GetViewAccessibility().SetCharacterOffsets(
      ComputeTextOffsets(GetRenderText()));

  WordBoundaries boundaries = ComputeWordBoundaries(GetText());
  GetViewAccessibility().SetWordStarts(boundaries.starts);
  GetViewAccessibility().SetWordEnds(boundaries.ends);
}
#endif  // BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)

bool Textfield::HandleAccessibleAction(const ui::AXActionData& action_data) {
  if (action_data.action == ax::mojom::Action::kSetSelection) {
    DCHECK_EQ(action_data.anchor_node_id, action_data.focus_node_id);
    const gfx::Range range(static_cast<size_t>(action_data.anchor_offset),
                           static_cast<size_t>(action_data.focus_offset));
    return SetEditableSelectionRange(range);
  }

  // Remaining actions cannot be performed on readonly fields.
  if (GetReadOnly())
    return View::HandleAccessibleAction(action_data);

  if (action_data.action == ax::mojom::Action::kSetValue) {
    SetText(base::UTF8ToUTF16(action_data.value));
    ClearSelection();
    return true;
  } else if (action_data.action == ax::mojom::Action::kReplaceSelectedText) {
    InsertOrReplaceText(base::UTF8ToUTF16(action_data.value));
    ClearSelection();
    return true;
  }

  return View::HandleAccessibleAction(action_data);
}

void Textfield::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  FitToLocalBounds();
}

bool Textfield::GetNeedsNotificationWhenVisibleBoundsChange() const {
  return true;
}

void Textfield::OnVisibleBoundsChanged() {
  if (touch_selection_controller_)
    touch_selection_controller_->SelectionChanged();
}

void Textfield::OnPaint(gfx::Canvas* canvas) {
  OnPaintBackground(canvas);
  PaintTextAndCursor(canvas);
  OnPaintBorder(canvas);
}

void Textfield::OnFocus() {
  is_processing_focus_ = true;

  // Set focus reason if focused was gained without mouse or touch input.
  if (focus_reason_ == ui::TextInputClient::FOCUS_REASON_NONE)
    focus_reason_ = ui::TextInputClient::FOCUS_REASON_OTHER;

#if BUILDFLAG(IS_MAC)
  if (text_input_type_ == ui::TEXT_INPUT_TYPE_PASSWORD)
    password_input_enabler_ =
        std::make_unique<ui::ScopedPasswordInputEnabler>();
#endif  // BUILDFLAG(IS_MAC)

  GetRenderText()->set_focused(true);
  if (GetInputMethod())
    GetInputMethod()->SetFocusedTextInputClient(this);
  UpdateAfterChange(TextChangeType::kNone, true);
  View::OnFocus();

  is_processing_focus_ = false;
}

void Textfield::OnBlur() {
  focus_reason_ = ui::TextInputClient::FOCUS_REASON_NONE;

  gfx::RenderText* render_text = GetRenderText();
  render_text->set_focused(false);

  if (GetInputMethod()) {
    GetInputMethod()->DetachTextInputClient(this);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    wm::RestoreWindowBoundsOnClientFocusLost(
        GetNativeView()->GetToplevelWindow());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }
  StopBlinkingCursor();
  cursor_view_->SetVisible(false);

  DestroyTouchSelection();

  SchedulePaint();
  View::OnBlur();

#if BUILDFLAG(IS_MAC)
  password_input_enabler_.reset();
#endif  // BUILDFLAG(IS_MAC)
}

gfx::Point Textfield::GetKeyboardContextMenuLocation() {
  return GetCaretBounds().bottom_right();
}

void Textfield::OnThemeChanged() {
  View::OnThemeChanged();
  gfx::RenderText* render_text = GetRenderText();
  SetColor(GetTextColor());
  UpdateBackgroundColor();
  render_text->set_selection_color(GetSelectionTextColor());
  render_text->set_selection_background_focused_color(
      GetSelectionBackgroundColor());
  cursor_view_->layer()->SetColor(GetTextColor());
}

////////////////////////////////////////////////////////////////////////////////
// Textfield, TextfieldModel::Delegate overrides:

void Textfield::OnCompositionTextConfirmedOrCleared() {
  if (!skip_input_method_cancel_composition_)
    GetInputMethod()->CancelComposition(this);
}

void Textfield::OnTextChanged() {
  OnPropertyChanged(
      ui::metadata::MakeUniquePropertyKey(&model_, kTextfieldText),
      kPropertyEffectsPaint);
  drop_weak_ptr_factory_.InvalidateWeakPtrs();
}

////////////////////////////////////////////////////////////////////////////////
// Textfield, ContextMenuController overrides:

void Textfield::ShowContextMenuForViewImpl(View* source,
                                           const gfx::Point& point,
                                           ui::MenuSourceType source_type) {
  UpdateContextMenu();
  context_menu_runner_->RunMenuAt(GetWidget(), nullptr,
                                  gfx::Rect(point, gfx::Size()),
                                  MenuAnchorPosition::kTopLeft, source_type);
}

////////////////////////////////////////////////////////////////////////////////
// Textfield, DragController overrides:

void Textfield::WriteDragDataForView(View* sender,
                                     const gfx::Point& press_pt,
                                     OSExchangeData* data) {
  const std::u16string& selected_text(GetSelectedText());
  data->SetString(selected_text);
  Label label(selected_text, {GetFontList()});
  label.SetBackgroundColor(GetBackgroundColor());
  label.SetSubpixelRenderingEnabled(false);
  gfx::Size size(label.GetPreferredSize({}));
  gfx::NativeView native_view = GetWidget()->GetNativeView();
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestView(native_view);
  size.SetToMin(gfx::Size(display.size().width(), height()));
  label.SetBoundsRect(gfx::Rect(size));
  label.SetEnabledColor(GetTextColor());

  SkBitmap bitmap;
  float raster_scale = ScaleFactorForDragFromWidget(GetWidget());
  SkColor color = views::Widget::IsWindowCompositingSupported()
                      ? SK_ColorTRANSPARENT
                      : GetBackgroundColor();
  label.Paint(PaintInfo::CreateRootPaintInfo(
      ui::CanvasPainter(&bitmap, label.size(), raster_scale, color,
                        GetWidget()->GetCompositor()->is_pixel_canvas())
          .context(),
      label.size()));
  constexpr gfx::Vector2d kOffset(-15, 0);
  gfx::ImageSkia image = gfx::ImageSkia::CreateFromBitmap(bitmap, raster_scale);
  data->provider().SetDragImage(image, kOffset);
  if (controller_)
    controller_->OnWriteDragData(data);
}

int Textfield::GetDragOperationsForView(View* sender, const gfx::Point& p) {
  int drag_operations = ui::DragDropTypes::DRAG_COPY;
  if (!GetEnabled() || text_input_type_ == ui::TEXT_INPUT_TYPE_PASSWORD ||
      !GetRenderText()->IsPointInSelection(p)) {
    drag_operations = ui::DragDropTypes::DRAG_NONE;
  } else if (sender == this && !GetReadOnly()) {
    drag_operations =
        ui::DragDropTypes::DRAG_MOVE | ui::DragDropTypes::DRAG_COPY;
  }
  if (controller_)
    controller_->OnGetDragOperationsForTextfield(&drag_operations);
  return drag_operations;
}

bool Textfield::CanStartDragForView(View* sender,
                                    const gfx::Point& press_pt,
                                    const gfx::Point& p) {
  return initiating_drag_ && GetRenderText()->IsPointInSelection(press_pt);
}

////////////////////////////////////////////////////////////////////////////////
// Textfield, WordLookupClient overrides:

bool Textfield::GetWordLookupDataAtPoint(const gfx::Point& point,
                                         gfx::DecoratedText* decorated_word,
                                         gfx::Rect* rect) {
  return GetRenderText()->GetWordLookupDataAtPoint(point, decorated_word, rect);
}

bool Textfield::GetWordLookupDataFromSelection(
    gfx::DecoratedText* decorated_text,
    gfx::Rect* rect) {
  if (GetRenderText()->obscured()) {
    return false;
  }
  return GetRenderText()->GetLookupDataForRange(GetRenderText()->selection(),
                                                decorated_text, rect);
}

////////////////////////////////////////////////////////////////////////////////
// Textfield, SelectionControllerDelegate overrides:

bool Textfield::HasTextBeingDragged() const {
  return initiating_drag_;
}

////////////////////////////////////////////////////////////////////////////////
// Textfield, ui::TouchEditable overrides:

void Textfield::MoveCaret(const gfx::Point& position) {
  SelectBetweenCoordinates(position, position);
}

void Textfield::MoveRangeSelectionExtent(const gfx::Point& extent) {
  if (GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE) {
    return;
  }

  gfx::RenderText* render_text = GetRenderText();
  if (!::features::IsTouchTextEditingRedesignEnabled()) {
    gfx::SelectionModel base_caret =
        render_text->GetSelectionModelForSelectionStart();
    gfx::SelectionModel extent_caret = render_text->FindCursorPosition(extent);
    gfx::SelectionModel selection_model(
        gfx::Range(base_caret.caret_pos(), extent_caret.caret_pos()),
        extent_caret.caret_affinity());

    OnBeforeUserAction();
    SelectSelectionModel(selection_model);
    OnAfterUserAction();
    return;
  }

  const gfx::Range selection = GetSelectedRange();
  const gfx::SelectionModel cursor_position_at_old_extent =
      render_text->FindCursorPosition(selection_extent_);
  const gfx::SelectionModel cursor_position_at_new_extent =
      render_text->FindCursorPosition(extent);

  if (render_text->GetLineContainingCaret(cursor_position_at_old_extent) !=
      render_text->GetLineContainingCaret(cursor_position_at_new_extent)) {
    // Reset the offset if a line change has occurred.
    extent_offset_x_ = 0;
  } else {
    // Otherwise, if the extent has moved in the direction of the offset, reduce
    // the amount of offset.
    const int dx = extent.x() - selection_extent_.x();
    if (extent_offset_x_ > 0 && dx > 0) {
      extent_offset_x_ = std::max(0, extent_offset_x_ - dx);
    } else if (extent_offset_x_ < 0 && dx < 0) {
      extent_offset_x_ = std::min(0, extent_offset_x_ - dx);
    }
  }

  const gfx::Point old_extent_with_offset =
      selection_extent_ + gfx::Vector2d(extent_offset_x_, 0);
  const size_t caret_pos_at_old_extent_with_offset =
      render_text->FindCursorPosition(old_extent_with_offset).caret_pos();
  gfx::Point new_extent_with_offset =
      extent + gfx::Vector2d(extent_offset_x_, 0);
  size_t caret_pos_at_new_extent_with_offset =
      render_text->FindCursorPosition(new_extent_with_offset).caret_pos();

  // Determine whether we need to switch between character and word
  // granularity and update the offset again if necessary.
  if (break_type_ == gfx::CHARACTER_BREAK) {
    // Switch to word granularity only after the selection has expanded past a
    // word boundary. This ensures that the selection can be adjusted by
    // character within a word after the selection has shrunk.
    const bool selection_expanding =
        selection.is_reversed() ? caret_pos_at_new_extent_with_offset <
                                      caret_pos_at_old_extent_with_offset
                                : caret_pos_at_new_extent_with_offset >
                                      caret_pos_at_old_extent_with_offset;
    const gfx::Range nearest_word_boundaries =
        render_text->ExpandRangeToWordBoundary(selection);
    const bool extent_moved_past_next_word_boundary =
        caret_pos_at_new_extent_with_offset <=
            nearest_word_boundaries.GetMin() ||
        caret_pos_at_new_extent_with_offset >= nearest_word_boundaries.GetMax();
    if (selection_expanding && extent_moved_past_next_word_boundary) {
      break_type_ = gfx::WORD_BREAK;
      extent_offset_x_ = 0;
    }
  } else {
    const bool selection_shrinking =
        selection.is_reversed() ? caret_pos_at_new_extent_with_offset >
                                      caret_pos_at_old_extent_with_offset
                                : caret_pos_at_new_extent_with_offset <
                                      caret_pos_at_old_extent_with_offset;
    if (selection_shrinking) {
      break_type_ = gfx::CHARACTER_BREAK;
      const gfx::Rect cursor_bounds =
          render_text->GetCursorBounds(GetSelectionModel(), true);
      extent_offset_x_ =
          cursor_bounds.CenterPoint().x() - selection_extent_.x();
    }
  }
  selection_extent_ = extent;
  new_extent_with_offset = extent + gfx::Vector2d(extent_offset_x_, 0);
  caret_pos_at_new_extent_with_offset =
      render_text->FindCursorPosition(new_extent_with_offset).caret_pos();

  size_t end = caret_pos_at_new_extent_with_offset;
  if (break_type_ == gfx::WORD_BREAK) {
    // Move the selection end to the nearest word boundary.
    const gfx::Range nearest_word_boundaries =
        render_text->ExpandRangeToWordBoundary(gfx::Range(end));
    DCHECK(end >= nearest_word_boundaries.start() &&
           end <= nearest_word_boundaries.end());
    end = end - nearest_word_boundaries.start() <
                  nearest_word_boundaries.end() - end
              ? nearest_word_boundaries.start()
              : nearest_word_boundaries.end();
  }

  // No need to update the selection if the end is still the same.
  if (end == selection.end()) {
    return;
  }

  const size_t start = selection.start();
  // Don't let the selection become empty.
  if (start == end) {
    return;
  }

  const gfx::LogicalCursorDirection affinity =
      start > end ? gfx::CURSOR_FORWARD : gfx::CURSOR_BACKWARD;
  OnBeforeUserAction();
  SelectSelectionModel(gfx::SelectionModel(gfx::Range(start, end), affinity));
  OnAfterUserAction();
}

void Textfield::SelectBetweenCoordinates(const gfx::Point& base,
                                         const gfx::Point& extent) {
  if (GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE) {
    return;
  }

  gfx::SelectionModel base_caret = GetRenderText()->FindCursorPosition(base);
  gfx::SelectionModel extent_caret =
      GetRenderText()->FindCursorPosition(extent);
  gfx::SelectionModel selection(
      gfx::Range(base_caret.caret_pos(), extent_caret.caret_pos()),
      extent_caret.caret_affinity());

  OnBeforeUserAction();
  SelectSelectionModel(selection);
  OnAfterUserAction();

  selection_extent_ = extent;
  extent_offset_x_ = 0;
  break_type_ = gfx::CHARACTER_BREAK;
}

void Textfield::GetSelectionEndPoints(gfx::SelectionBound* anchor,
                                      gfx::SelectionBound* focus) {
  gfx::RenderText* render_text = GetRenderText();
  const gfx::SelectionModel& sel = render_text->selection_model();
  gfx::SelectionModel start_sel =
      render_text->GetSelectionModelForSelectionStart();
  gfx::Rect r1 = render_text->GetCursorBounds(start_sel, true);
  gfx::Rect r2 = render_text->GetCursorBounds(sel, true);

  anchor->SetEdge(gfx::PointF(r1.origin()), gfx::PointF(r1.bottom_left()));
  focus->SetEdge(gfx::PointF(r2.origin()), gfx::PointF(r2.bottom_left()));

  // Determine the SelectionBound's type for focus and anchor.
  // TODO(mfomitchev): Ideally we should have different logical directions for
  // start and end to support proper handle direction for mixed LTR/RTL text.
  const bool ltr = GetTextDirection() != base::i18n::RIGHT_TO_LEFT;
  size_t anchor_position_index = sel.selection().start();
  size_t focus_position_index = sel.selection().end();

  if (anchor_position_index == focus_position_index) {
    anchor->set_type(gfx::SelectionBound::CENTER);
    focus->set_type(gfx::SelectionBound::CENTER);
  } else if ((ltr && anchor_position_index < focus_position_index) ||
             (!ltr && anchor_position_index > focus_position_index)) {
    anchor->set_type(gfx::SelectionBound::LEFT);
    focus->set_type(gfx::SelectionBound::RIGHT);
  } else {
    anchor->set_type(gfx::SelectionBound::RIGHT);
    focus->set_type(gfx::SelectionBound::LEFT);
  }
}

gfx::Rect Textfield::GetBounds() {
  return GetLocalBounds();
}

gfx::NativeView Textfield::GetNativeView() const {
  return GetWidget()->GetNativeView();
}

bool Textfield::IsSelectionDragging() const {
  return selection_dragging_state_ == SelectionDraggingState::kDraggingCursor ||
         selection_dragging_state_ ==
             SelectionDraggingState::kDraggingSelectionExtent;
}

void Textfield::ConvertPointToScreen(gfx::Point* point) {
  View::ConvertPointToScreen(this, point);
}

void Textfield::ConvertPointFromScreen(gfx::Point* point) {
  View::ConvertPointFromScreen(this, point);
}

void Textfield::OpenContextMenu(const gfx::Point& anchor) {
  DestroyTouchSelection();
  ShowContextMenu(anchor, ui::MENU_SOURCE_TOUCH_EDIT_MENU);
}

void Textfield::DestroyTouchSelection() {
  touch_selection_controller_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// Textfield, ui::SimpleMenuModel::Delegate overrides:

bool Textfield::IsCommandIdChecked(int command_id) const {
  if (text_services_context_menu_ &&
      text_services_context_menu_->SupportsCommand(command_id)) {
    return text_services_context_menu_->IsCommandIdChecked(command_id);
  }

  return true;
}

bool Textfield::IsCommandIdEnabled(int command_id) const {
  if (text_services_context_menu_ &&
      text_services_context_menu_->SupportsCommand(command_id)) {
    return text_services_context_menu_->IsCommandIdEnabled(command_id);
  }

  return IsTextEditCommandEnabled(
      GetTextEditCommandFromMenuCommand(command_id, HasSelection()));
}

bool Textfield::GetAcceleratorForCommandId(int command_id,
                                           ui::Accelerator* accelerator) const {
  switch (command_id) {
    case kUndo:
      *accelerator = ui::Accelerator(ui::VKEY_Z, ui::EF_PLATFORM_ACCELERATOR);
      return true;

    case kCut:
      *accelerator = ui::Accelerator(ui::VKEY_X, ui::EF_PLATFORM_ACCELERATOR);
      return true;

    case kCopy:
      *accelerator = ui::Accelerator(ui::VKEY_C, ui::EF_PLATFORM_ACCELERATOR);
      return true;

    case kPaste:
      *accelerator = ui::Accelerator(ui::VKEY_V, ui::EF_PLATFORM_ACCELERATOR);
      return true;

    case kSelectAll:
      *accelerator = ui::Accelerator(ui::VKEY_A, ui::EF_PLATFORM_ACCELERATOR);
      return true;

    default:
      return text_services_context_menu_->GetAcceleratorForCommandId(
          command_id, accelerator);
  }
}

void Textfield::ExecuteCommand(int command_id, int event_flags) {
  if (text_services_context_menu_ &&
      text_services_context_menu_->SupportsCommand(command_id)) {
    text_services_context_menu_->ExecuteCommand(command_id, event_flags);
    return;
  }

  Textfield::ExecuteTextEditCommand(
      GetTextEditCommandFromMenuCommand(command_id, HasSelection()));

  if (::features::IsTouchTextEditingRedesignEnabled() &&
      (event_flags & ui::EF_FROM_TOUCH) &&
      (command_id == Textfield::kSelectAll ||
       command_id == Textfield::kSelectWord)) {
    CreateTouchSelectionControllerAndNotifyIt();
  }
}

////////////////////////////////////////////////////////////////////////////////
// Textfield, ui::TextInputClient overrides:

base::WeakPtr<ui::TextInputClient> Textfield::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void Textfield::SetCompositionText(const ui::CompositionText& composition) {
  if (GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE)
    return;

  OnBeforeUserAction();
  skip_input_method_cancel_composition_ = true;
  model_->SetCompositionText(composition);
  skip_input_method_cancel_composition_ = false;
  UpdateAfterChange(TextChangeType::kUserTriggered, true);
  OnAfterUserAction();
}

size_t Textfield::ConfirmCompositionText(bool keep_selection) {
  // TODO(b/134473433) Modify this function so that when keep_selection is
  // true, the selection is not changed when text committed
  if (keep_selection) {
    NOTIMPLEMENTED_LOG_ONCE();
  }
  if (!model_->HasCompositionText())
    return 0;
  OnBeforeUserAction();
  skip_input_method_cancel_composition_ = true;
  const size_t confirmed_text_length = model_->ConfirmCompositionText();
  skip_input_method_cancel_composition_ = false;
  UpdateAfterChange(TextChangeType::kUserTriggered, true);
  OnAfterUserAction();
  return confirmed_text_length;
}

void Textfield::ClearCompositionText() {
  if (!model_->HasCompositionText())
    return;

  OnBeforeUserAction();
  skip_input_method_cancel_composition_ = true;
  model_->CancelCompositionText();
  skip_input_method_cancel_composition_ = false;
  UpdateAfterChange(TextChangeType::kUserTriggered, true);
  OnAfterUserAction();
}

void Textfield::InsertText(const std::u16string& new_text,
                           InsertTextCursorBehavior cursor_behavior) {
  std::u16string filtered_new_text;
  base::ranges::copy_if(new_text, std::back_inserter(filtered_new_text),
                        IsValidCharToInsert);

  if (GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE ||
      filtered_new_text.empty())
    return;

  // TODO(crbug.com/1155331): Handle |cursor_behavior| correctly.
  OnBeforeUserAction();
  skip_input_method_cancel_composition_ = true;
  model_->InsertText(filtered_new_text);
  skip_input_method_cancel_composition_ = false;
  UpdateAfterChange(TextChangeType::kUserTriggered, true);
  OnAfterUserAction();
}

void Textfield::InsertChar(const ui::KeyEvent& event) {
  if (GetReadOnly()) {
    OnEditFailed();
    return;
  }

  // Filter all invalid chars and all characters with Alt modifier (and Search
  // on ChromeOS, Ctrl on Linux). But allow characters with the AltGr modifier.
  // On Windows AltGr is represented by Alt+Ctrl or Right Alt, and on Linux it's
  // a different flag that we don't care about.
  const char16_t ch = event.GetCharacter();
  const bool should_insert_char = IsValidCharToInsert(ch) &&
                                  !ui::IsSystemKeyModifier(event.flags()) &&
                                  !IsControlKeyModifier(event.flags());
  if (GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE || !should_insert_char)
    return;

  DoInsertChar(ch);

  if (text_input_type_ == ui::TEXT_INPUT_TYPE_PASSWORD) {
    password_char_reveal_index_ = std::nullopt;
    base::TimeDelta duration = GetPasswordRevealDuration(event);
    if (!duration.is_zero()) {
      const size_t change_offset = model_->GetCursorPosition();
      DCHECK_GT(change_offset, 0u);
      RevealPasswordChar(change_offset - 1, duration);
    }
  }
}

ui::TextInputType Textfield::GetTextInputType() const {
  if (GetReadOnly() || !GetEnabled())
    return ui::TEXT_INPUT_TYPE_NONE;
  return text_input_type_;
}

ui::TextInputMode Textfield::GetTextInputMode() const {
  return ui::TEXT_INPUT_MODE_DEFAULT;
}

base::i18n::TextDirection Textfield::GetTextDirection() const {
  return GetRenderText()->GetDisplayTextDirection();
}

int Textfield::GetTextInputFlags() const {
  return text_input_flags_;
}

bool Textfield::CanComposeInline() const {
  return true;
}

// TODO(mbid): GetCaretBounds is const but calls
// RenderText::GetUpdatedCursorBounds, which is not const and in fact mutates
// internal state. (Is it at least logically const?) Violation of const
// correctness?
gfx::Rect Textfield::GetCaretBounds() const {
  gfx::Rect rect = GetRenderText()->GetUpdatedCursorBounds();
  ConvertRectToScreen(this, &rect);
  return rect;
}

gfx::Rect Textfield::GetSelectionBoundingBox() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::Rect();
}

bool Textfield::GetCompositionCharacterBounds(size_t index,
                                              gfx::Rect* rect) const {
  DCHECK(rect);
  if (!HasCompositionText())
    return false;
  gfx::Range composition_range;
  model_->GetCompositionTextRange(&composition_range);
  DCHECK(!composition_range.is_empty());

  size_t text_index = composition_range.start() + index;
  if (composition_range.end() <= text_index)
    return false;
  gfx::RenderText* render_text = GetRenderText();
  if (!render_text->IsValidCursorIndex(text_index)) {
    text_index =
        render_text->IndexOfAdjacentGrapheme(text_index, gfx::CURSOR_BACKWARD);
  }
  if (text_index < composition_range.start())
    return false;
  const gfx::SelectionModel caret(text_index, gfx::CURSOR_BACKWARD);
  *rect = render_text->GetCursorBounds(caret, false);
  ConvertRectToScreen(this, rect);
  return true;
}

bool Textfield::HasCompositionText() const {
  return model_->HasCompositionText();
}

ui::TextInputClient::FocusReason Textfield::GetFocusReason() const {
  return focus_reason_;
}

bool Textfield::GetTextRange(gfx::Range* range) const {
  if (!ImeEditingAllowed())
    return false;

  model_->GetTextRange(range);
  return true;
}

bool Textfield::GetCompositionTextRange(gfx::Range* range) const {
  if (!ImeEditingAllowed())
    return false;

  model_->GetCompositionTextRange(range);
  return true;
}

bool Textfield::GetEditableSelectionRange(gfx::Range* range) const {
  if (!ImeEditingAllowed())
    return false;
  *range = GetRenderText()->selection();
  return true;
}

bool Textfield::SetEditableSelectionRange(const gfx::Range& range) {
  if (!ImeEditingAllowed() || !range.IsValid())
    return false;
  OnBeforeUserAction();
  SetSelectedRange(range);
  OnAfterUserAction();
  return true;
}

bool Textfield::DeleteRange(const gfx::Range& range) {
  if (!ImeEditingAllowed() || range.is_empty())
    return false;

  OnBeforeUserAction();
  model_->SelectRange(range);
  if (model_->HasSelection()) {
    model_->DeleteSelection();
    UpdateAfterChange(TextChangeType::kUserTriggered, true);
  }
  OnAfterUserAction();
  return true;
}

bool Textfield::GetTextFromRange(const gfx::Range& range,
                                 std::u16string* range_text) const {
  if (!ImeEditingAllowed() || !range.IsValid())
    return false;

  gfx::Range text_range;
  if (!GetTextRange(&text_range) || !range.IsBoundedBy(text_range))
    return false;

  *range_text = model_->GetTextFromRange(range);
  return true;
}

void Textfield::OnInputMethodChanged() {}

bool Textfield::ChangeTextDirectionAndLayoutAlignment(
    base::i18n::TextDirection direction) {
  DCHECK_NE(direction, base::i18n::UNKNOWN_DIRECTION);
  // Restore text directionality mode when the indicated direction matches the
  // current forced mode; otherwise, force the mode indicated. This helps users
  // manage BiDi text layout without getting stuck in forced LTR or RTL modes.
  const bool default_rtl = direction == base::i18n::RIGHT_TO_LEFT;
  const auto new_mode = default_rtl ? gfx::DIRECTIONALITY_FORCE_RTL
                                    : gfx::DIRECTIONALITY_FORCE_LTR;
  auto* render_text = GetRenderText();
  const bool modes_match = new_mode == render_text->directionality_mode();
  render_text->SetDirectionalityMode(modes_match ? gfx::DIRECTIONALITY_FROM_TEXT
                                                 : new_mode);
  // Do not reset horizontal alignment to left/right if it has been set to
  // center, or if it depends on the text direction and that direction has not
  // been modified.
  bool dir_from_text =
      modes_match && GetHorizontalAlignment() == gfx::ALIGN_TO_HEAD;
  if (!dir_from_text && GetHorizontalAlignment() != gfx::ALIGN_CENTER)
    SetHorizontalAlignment(default_rtl ? gfx::ALIGN_RIGHT : gfx::ALIGN_LEFT);
  SchedulePaint();
  UpdateAccessibilityTextDirection();
  return true;
}

void Textfield::ExtendSelectionAndDelete(size_t before, size_t after) {
  gfx::Range range = GetRenderText()->selection();
  // Discard out-of-bound operations.
  // TODO(crbug.com/40852753): this is a temporary fix to prevent the
  // checked_cast failure in gfx::Range. There does not seem to be any
  // observable bad behaviors before checked_cast was added. However, range
  // clipping or dropping should be the last resort because a checkfail
  // indicates that we run into bad states somewhere earlier on the stack.
  if (range.start() < before)
    return;

  range.set_start(range.start() - before);
  range.set_end(range.end() + after);
  gfx::Range text_range;
  if (GetTextRange(&text_range) && text_range.Contains(range))
    DeleteRange(range);
}

void Textfield::EnsureCaretNotInRect(const gfx::Rect& rect_in_screen) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  aura::Window* top_level_window = GetNativeView()->GetToplevelWindow();
  wm::EnsureWindowNotInRect(top_level_window, rect_in_screen);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

bool Textfield::IsTextEditCommandEnabled(ui::TextEditCommand command) const {
  std::u16string result;
  bool editable = !GetReadOnly();
  bool readable = text_input_type_ != ui::TEXT_INPUT_TYPE_PASSWORD;
  switch (command) {
    case ui::TextEditCommand::DELETE_BACKWARD:
    case ui::TextEditCommand::DELETE_FORWARD:
    case ui::TextEditCommand::DELETE_TO_BEGINNING_OF_LINE:
    case ui::TextEditCommand::DELETE_TO_BEGINNING_OF_PARAGRAPH:
    case ui::TextEditCommand::DELETE_TO_END_OF_LINE:
    case ui::TextEditCommand::DELETE_TO_END_OF_PARAGRAPH:
    case ui::TextEditCommand::DELETE_WORD_BACKWARD:
    case ui::TextEditCommand::DELETE_WORD_FORWARD:
      return editable;
    case ui::TextEditCommand::MOVE_BACKWARD:
    case ui::TextEditCommand::MOVE_BACKWARD_AND_MODIFY_SELECTION:
    case ui::TextEditCommand::MOVE_FORWARD:
    case ui::TextEditCommand::MOVE_FORWARD_AND_MODIFY_SELECTION:
    case ui::TextEditCommand::MOVE_LEFT:
    case ui::TextEditCommand::MOVE_LEFT_AND_MODIFY_SELECTION:
    case ui::TextEditCommand::MOVE_RIGHT:
    case ui::TextEditCommand::MOVE_RIGHT_AND_MODIFY_SELECTION:
    case ui::TextEditCommand::MOVE_TO_BEGINNING_OF_DOCUMENT:
    case ui::TextEditCommand::
        MOVE_TO_BEGINNING_OF_DOCUMENT_AND_MODIFY_SELECTION:
    case ui::TextEditCommand::MOVE_TO_BEGINNING_OF_LINE:
    case ui::TextEditCommand::MOVE_TO_BEGINNING_OF_LINE_AND_MODIFY_SELECTION:
    case ui::TextEditCommand::MOVE_TO_BEGINNING_OF_PARAGRAPH:
    case ui::TextEditCommand::
        MOVE_TO_BEGINNING_OF_PARAGRAPH_AND_MODIFY_SELECTION:
    case ui::TextEditCommand::MOVE_TO_END_OF_DOCUMENT:
    case ui::TextEditCommand::MOVE_TO_END_OF_DOCUMENT_AND_MODIFY_SELECTION:
    case ui::TextEditCommand::MOVE_TO_END_OF_LINE:
    case ui::TextEditCommand::MOVE_TO_END_OF_LINE_AND_MODIFY_SELECTION:
    case ui::TextEditCommand::MOVE_TO_END_OF_PARAGRAPH:
    case ui::TextEditCommand::MOVE_TO_END_OF_PARAGRAPH_AND_MODIFY_SELECTION:
    case ui::TextEditCommand::MOVE_PARAGRAPH_FORWARD_AND_MODIFY_SELECTION:
    case ui::TextEditCommand::MOVE_PARAGRAPH_BACKWARD_AND_MODIFY_SELECTION:
    case ui::TextEditCommand::MOVE_WORD_BACKWARD:
    case ui::TextEditCommand::MOVE_WORD_BACKWARD_AND_MODIFY_SELECTION:
    case ui::TextEditCommand::MOVE_WORD_FORWARD:
    case ui::TextEditCommand::MOVE_WORD_FORWARD_AND_MODIFY_SELECTION:
    case ui::TextEditCommand::MOVE_WORD_LEFT:
    case ui::TextEditCommand::MOVE_WORD_LEFT_AND_MODIFY_SELECTION:
    case ui::TextEditCommand::MOVE_WORD_RIGHT:
    case ui::TextEditCommand::MOVE_WORD_RIGHT_AND_MODIFY_SELECTION:
      return true;
    case ui::TextEditCommand::UNDO:
      return editable && model_->CanUndo();
    case ui::TextEditCommand::REDO:
      return editable && model_->CanRedo();
    case ui::TextEditCommand::CUT:
      return editable && readable && HasSelection();
    case ui::TextEditCommand::COPY:
      return readable && HasSelection();
    case ui::TextEditCommand::PASTE: {
      ui::DataTransferEndpoint data_dst(
          ui::EndpointType::kDefault,
          {.notify_if_restricted = show_rejection_ui_if_any_});
      ui::Clipboard::GetForCurrentThread()->ReadText(
          ui::ClipboardBuffer::kCopyPaste, &data_dst, &result);
    }
      return editable && !result.empty();
    case ui::TextEditCommand::SELECT_ALL:
      return !GetText().empty() &&
             GetSelectedRange().length() != GetText().length();
    case ui::TextEditCommand::SELECT_WORD:
      return readable && !GetText().empty() && !HasSelection();
    case ui::TextEditCommand::TRANSPOSE:
      return editable && !HasSelection() && !model_->HasCompositionText();
    case ui::TextEditCommand::YANK:
      return editable;
    case ui::TextEditCommand::MOVE_DOWN:
    case ui::TextEditCommand::MOVE_DOWN_AND_MODIFY_SELECTION:
    case ui::TextEditCommand::MOVE_PAGE_DOWN:
    case ui::TextEditCommand::MOVE_PAGE_DOWN_AND_MODIFY_SELECTION:
    case ui::TextEditCommand::MOVE_PAGE_UP:
    case ui::TextEditCommand::MOVE_PAGE_UP_AND_MODIFY_SELECTION:
    case ui::TextEditCommand::MOVE_UP:
    case ui::TextEditCommand::MOVE_UP_AND_MODIFY_SELECTION:
    case ui::TextEditCommand::SCROLL_TO_BEGINNING_OF_DOCUMENT:
    case ui::TextEditCommand::SCROLL_TO_END_OF_DOCUMENT:
    case ui::TextEditCommand::SCROLL_PAGE_DOWN:
    case ui::TextEditCommand::SCROLL_PAGE_UP:
// On Mac, the textfield should respond to Up/Down arrows keys and
// PageUp/PageDown.
#if BUILDFLAG(IS_MAC)
      return true;
#else
      return GetRenderText()->multiline();
#endif
    case ui::TextEditCommand::INSERT_TEXT:
    case ui::TextEditCommand::SET_MARK:
    case ui::TextEditCommand::UNSELECT:
    case ui::TextEditCommand::INVALID_COMMAND:
      return false;
  }
  NOTREACHED();
}

void Textfield::SetTextEditCommandForNextKeyEvent(ui::TextEditCommand command) {
  DCHECK_EQ(ui::TextEditCommand::INVALID_COMMAND, scheduled_text_edit_command_);
  scheduled_text_edit_command_ = command;
}

ukm::SourceId Textfield::GetClientSourceForMetrics() const {
  // TODO(shend): Implement this method.
  NOTIMPLEMENTED_LOG_ONCE();
  return ukm::SourceId();
}

bool Textfield::ShouldDoLearning() {
  if (should_do_learning_.has_value())
    return should_do_learning_.value();

  NOTIMPLEMENTED_LOG_ONCE();
  DVLOG(1) << "This Textfield instance does not support ShouldDoLearning";
  return false;
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/41452689): Implement this method to support Korean IME
// reconversion feature on native text fields (e.g. find bar).
bool Textfield::SetCompositionFromExistingText(
    const gfx::Range& range,
    const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) {
  // TODO(crbug.com/41452689): Support custom text spans.
  DCHECK(!model_->HasCompositionText());
  OnBeforeUserAction();
  model_->SetCompositionFromExistingText(range);
  SchedulePaint();
  OnAfterUserAction();
  return true;
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
gfx::Range Textfield::GetAutocorrectRange() const {
  // TODO(b/316461955): Implement autocorrect UI for native fields.
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::Range();
}

gfx::Rect Textfield::GetAutocorrectCharacterBounds() const {
  // TODO(b/316461955): Implement autocorrect UI for native fields.
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::Rect();
}

bool Textfield::SetAutocorrectRange(const gfx::Range& range) {
  if (!range.is_empty()) {
    base::UmaHistogramEnumeration("InputMethod.Assistive.Autocorrect.Count",
                                  TextInputClient::SubClass::kTextField);
  }
  // TODO(b/316461955): Implement autocorrect UI for native fields.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool Textfield::AddGrammarFragments(
    const std::vector<ui::GrammarFragment>& fragments) {
  if (!fragments.empty()) {
    base::UmaHistogramEnumeration("InputMethod.Assistive.Grammar.Count",
                                  TextInputClient::SubClass::kTextField);
  }
  // TODO(crbug.com/40178699): Implement this method for CrOS Grammar.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
void Textfield::GetActiveTextInputControlLayoutBounds(
    std::optional<gfx::Rect>* control_bounds,
    std::optional<gfx::Rect>* selection_bounds) {
  gfx::Rect origin = GetContentsBounds();
  ConvertRectToScreen(this, &origin);
  *control_bounds = origin;
}
#endif

#if BUILDFLAG(IS_WIN)
// TODO(crbug.com/41452689): Implement this method once TSF supports
// reconversion features on native text fields.
void Textfield::SetActiveCompositionForAccessibility(
    const gfx::Range& range,
    const std::u16string& active_composition_text,
    bool is_composition_committed) {}
#endif

////////////////////////////////////////////////////////////////////////////////
// Textfield, views::ViewObserver overrides:
void Textfield::OnViewFocused(views::View* observed_view) {
  observed_view->RemoveObserver(this);
  observed_view->NotifyAccessibilityEvent(
      ax::mojom::Event::kTextSelectionChanged, true);
}

////////////////////////////////////////////////////////////////////////////////
// Textfield, protected:

void Textfield::DoInsertChar(char16_t ch) {
  OnBeforeUserAction();
  skip_input_method_cancel_composition_ = true;
  model_->InsertChar(ch);
  skip_input_method_cancel_composition_ = false;

  UpdateAfterChange(TextChangeType::kUserTriggered, true);
  OnAfterUserAction();
}

gfx::RenderText* Textfield::GetRenderText() const {
  return model_->render_text();
}

gfx::Point Textfield::GetLastClickRootLocation() const {
  return selection_controller_.last_click_root_location();
}

std::u16string Textfield::GetSelectionClipboardText() const {
  std::u16string selection_clipboard_text;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kSelection, /* data_dst = */ nullptr,
      &selection_clipboard_text);
  return selection_clipboard_text;
}

void Textfield::ExecuteTextEditCommand(ui::TextEditCommand command) {
  DestroyTouchSelection();

  // We only execute the commands enabled in Textfield::IsTextEditCommandEnabled
  // below. Hence don't do a virtual IsTextEditCommandEnabled call.
  if (!IsTextEditCommandEnabled(command))
    return;

  OnBeforeUserAction();

  gfx::SelectionModel selection_model = GetSelectionModel();
  auto [text_changed, cursor_changed] = DoExecuteTextEditCommand(command);

  cursor_changed |= (GetSelectionModel() != selection_model);
  if (cursor_changed && HasSelection())
    UpdateSelectionClipboard();
  UpdateAfterChange(
      text_changed ? TextChangeType::kUserTriggered : TextChangeType::kNone,
      cursor_changed);
  OnAfterUserAction();
}

Textfield::EditCommandResult Textfield::DoExecuteTextEditCommand(
    ui::TextEditCommand command) {
  bool changed = false;
  bool cursor_changed = false;
  bool add_to_kill_buffer = false;

  base::AutoReset<bool> show_rejection_ui(&show_rejection_ui_if_any_, true);

  // Some codepaths may bypass GetCommandForKeyEvent, so any selection-dependent
  // modifications of the command should happen here.
  switch (command) {
    case ui::TextEditCommand::DELETE_TO_BEGINNING_OF_LINE:
    case ui::TextEditCommand::DELETE_TO_BEGINNING_OF_PARAGRAPH:
    case ui::TextEditCommand::DELETE_TO_END_OF_LINE:
    case ui::TextEditCommand::DELETE_TO_END_OF_PARAGRAPH:
      add_to_kill_buffer = text_input_type_ != ui::TEXT_INPUT_TYPE_PASSWORD;
      [[fallthrough]];
    case ui::TextEditCommand::DELETE_WORD_BACKWARD:
    case ui::TextEditCommand::DELETE_WORD_FORWARD:
      if (HasSelection())
        command = ui::TextEditCommand::DELETE_FORWARD;
      break;
    default:
      break;
  }

  bool rtl = GetTextDirection() == base::i18n::RIGHT_TO_LEFT;
  gfx::VisualCursorDirection begin = rtl ? gfx::CURSOR_RIGHT : gfx::CURSOR_LEFT;
  gfx::VisualCursorDirection end = rtl ? gfx::CURSOR_LEFT : gfx::CURSOR_RIGHT;

  switch (command) {
    case ui::TextEditCommand::DELETE_BACKWARD:
      changed = cursor_changed = model_->Backspace(add_to_kill_buffer);
      break;
    case ui::TextEditCommand::DELETE_FORWARD:
      changed = cursor_changed = model_->Delete(add_to_kill_buffer);
      break;
    case ui::TextEditCommand::DELETE_TO_BEGINNING_OF_LINE:
      model_->MoveCursor(gfx::LINE_BREAK, begin, gfx::SELECTION_RETAIN);
      changed = cursor_changed = model_->Backspace(add_to_kill_buffer);
      break;
    case ui::TextEditCommand::DELETE_TO_BEGINNING_OF_PARAGRAPH:
      model_->MoveCursor(gfx::FIELD_BREAK, begin, gfx::SELECTION_RETAIN);
      changed = cursor_changed = model_->Backspace(add_to_kill_buffer);
      break;
    case ui::TextEditCommand::DELETE_TO_END_OF_LINE:
      model_->MoveCursor(gfx::LINE_BREAK, end, gfx::SELECTION_RETAIN);
      changed = cursor_changed = model_->Delete(add_to_kill_buffer);
      break;
    case ui::TextEditCommand::DELETE_TO_END_OF_PARAGRAPH:
      model_->MoveCursor(gfx::FIELD_BREAK, end, gfx::SELECTION_RETAIN);
      changed = cursor_changed = model_->Delete(add_to_kill_buffer);
      break;
    case ui::TextEditCommand::DELETE_WORD_BACKWARD:
      model_->MoveCursor(gfx::WORD_BREAK, begin, gfx::SELECTION_RETAIN);
      changed = cursor_changed = model_->Backspace(add_to_kill_buffer);
      break;
    case ui::TextEditCommand::DELETE_WORD_FORWARD:
      model_->MoveCursor(gfx::WORD_BREAK, end, gfx::SELECTION_RETAIN);
      changed = cursor_changed = model_->Delete(add_to_kill_buffer);
      break;
    case ui::TextEditCommand::MOVE_BACKWARD:
      model_->MoveCursor(gfx::CHARACTER_BREAK, begin, gfx::SELECTION_NONE);
      break;
    case ui::TextEditCommand::MOVE_BACKWARD_AND_MODIFY_SELECTION:
      model_->MoveCursor(gfx::CHARACTER_BREAK, begin, gfx::SELECTION_RETAIN);
      break;
    case ui::TextEditCommand::MOVE_FORWARD:
      model_->MoveCursor(gfx::CHARACTER_BREAK, end, gfx::SELECTION_NONE);
      break;
    case ui::TextEditCommand::MOVE_FORWARD_AND_MODIFY_SELECTION:
      model_->MoveCursor(gfx::CHARACTER_BREAK, end, gfx::SELECTION_RETAIN);
      break;
    case ui::TextEditCommand::MOVE_LEFT:
      model_->MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT,
                         gfx::SELECTION_NONE);
      break;
    case ui::TextEditCommand::MOVE_LEFT_AND_MODIFY_SELECTION:
      model_->MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT,
                         gfx::SELECTION_RETAIN);
      break;
    case ui::TextEditCommand::MOVE_RIGHT:
      model_->MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                         gfx::SELECTION_NONE);
      break;
    case ui::TextEditCommand::MOVE_RIGHT_AND_MODIFY_SELECTION:
      model_->MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT,
                         gfx::SELECTION_RETAIN);
      break;
    case ui::TextEditCommand::MOVE_TO_BEGINNING_OF_LINE:
      model_->MoveCursor(gfx::LINE_BREAK, begin, gfx::SELECTION_NONE);
      break;
    case ui::TextEditCommand::MOVE_TO_BEGINNING_OF_DOCUMENT:
    case ui::TextEditCommand::MOVE_TO_BEGINNING_OF_PARAGRAPH:
    case ui::TextEditCommand::MOVE_UP:
    case ui::TextEditCommand::MOVE_PAGE_UP:
    case ui::TextEditCommand::SCROLL_TO_BEGINNING_OF_DOCUMENT:
    case ui::TextEditCommand::SCROLL_PAGE_UP:
      model_->MoveCursor(gfx::FIELD_BREAK, begin, gfx::SELECTION_NONE);
      break;
    case ui::TextEditCommand::MOVE_TO_BEGINNING_OF_LINE_AND_MODIFY_SELECTION:
      model_->MoveCursor(gfx::LINE_BREAK, begin, kLineSelectionBehavior);
      break;
    case ui::TextEditCommand::
        MOVE_TO_BEGINNING_OF_DOCUMENT_AND_MODIFY_SELECTION:
    case ui::TextEditCommand::
        MOVE_TO_BEGINNING_OF_PARAGRAPH_AND_MODIFY_SELECTION:
      model_->MoveCursor(gfx::FIELD_BREAK, begin, kLineSelectionBehavior);
      break;
    case ui::TextEditCommand::MOVE_PAGE_UP_AND_MODIFY_SELECTION:
    case ui::TextEditCommand::MOVE_UP_AND_MODIFY_SELECTION:
      model_->MoveCursor(gfx::FIELD_BREAK, begin, gfx::SELECTION_RETAIN);
      break;
    case ui::TextEditCommand::MOVE_TO_END_OF_LINE:
      model_->MoveCursor(gfx::LINE_BREAK, end, gfx::SELECTION_NONE);
      break;
    case ui::TextEditCommand::MOVE_TO_END_OF_DOCUMENT:
    case ui::TextEditCommand::MOVE_TO_END_OF_PARAGRAPH:
    case ui::TextEditCommand::MOVE_DOWN:
    case ui::TextEditCommand::MOVE_PAGE_DOWN:
    case ui::TextEditCommand::SCROLL_TO_END_OF_DOCUMENT:
    case ui::TextEditCommand::SCROLL_PAGE_DOWN:
      model_->MoveCursor(gfx::FIELD_BREAK, end, gfx::SELECTION_NONE);
      break;
    case ui::TextEditCommand::MOVE_TO_END_OF_LINE_AND_MODIFY_SELECTION:
      model_->MoveCursor(gfx::LINE_BREAK, end, kLineSelectionBehavior);
      break;
    case ui::TextEditCommand::MOVE_TO_END_OF_DOCUMENT_AND_MODIFY_SELECTION:
    case ui::TextEditCommand::MOVE_TO_END_OF_PARAGRAPH_AND_MODIFY_SELECTION:
      model_->MoveCursor(gfx::FIELD_BREAK, end, kLineSelectionBehavior);
      break;
    case ui::TextEditCommand::MOVE_PAGE_DOWN_AND_MODIFY_SELECTION:
    case ui::TextEditCommand::MOVE_DOWN_AND_MODIFY_SELECTION:
      model_->MoveCursor(gfx::FIELD_BREAK, end, gfx::SELECTION_RETAIN);
      break;
    case ui::TextEditCommand::MOVE_PARAGRAPH_BACKWARD_AND_MODIFY_SELECTION:
      model_->MoveCursor(gfx::FIELD_BREAK, begin,
                         kMoveParagraphSelectionBehavior);
      break;
    case ui::TextEditCommand::MOVE_PARAGRAPH_FORWARD_AND_MODIFY_SELECTION:
      model_->MoveCursor(gfx::FIELD_BREAK, end,
                         kMoveParagraphSelectionBehavior);
      break;
    case ui::TextEditCommand::MOVE_WORD_BACKWARD:
      model_->MoveCursor(gfx::WORD_BREAK, begin, gfx::SELECTION_NONE);
      break;
    case ui::TextEditCommand::MOVE_WORD_BACKWARD_AND_MODIFY_SELECTION:
      model_->MoveCursor(gfx::WORD_BREAK, begin, kWordSelectionBehavior);
      break;
    case ui::TextEditCommand::MOVE_WORD_FORWARD:
      model_->MoveCursor(gfx::WORD_BREAK, end, gfx::SELECTION_NONE);
      break;
    case ui::TextEditCommand::MOVE_WORD_FORWARD_AND_MODIFY_SELECTION:
      model_->MoveCursor(gfx::WORD_BREAK, end, kWordSelectionBehavior);
      break;
    case ui::TextEditCommand::MOVE_WORD_LEFT:
      model_->MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT,
                         gfx::SELECTION_NONE);
      break;
    case ui::TextEditCommand::MOVE_WORD_LEFT_AND_MODIFY_SELECTION:
      model_->MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT,
                         kWordSelectionBehavior);
      break;
    case ui::TextEditCommand::MOVE_WORD_RIGHT:
      model_->MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT,
                         gfx::SELECTION_NONE);
      break;
    case ui::TextEditCommand::MOVE_WORD_RIGHT_AND_MODIFY_SELECTION:
      model_->MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT,
                         kWordSelectionBehavior);
      break;
    case ui::TextEditCommand::UNDO:
      changed = cursor_changed = model_->Undo();
      break;
    case ui::TextEditCommand::REDO:
      changed = cursor_changed = model_->Redo();
      break;
    case ui::TextEditCommand::CUT:
      changed = cursor_changed = Cut();
      break;
    case ui::TextEditCommand::COPY:
      Copy();
      break;
    case ui::TextEditCommand::PASTE:
      changed = cursor_changed = Paste();
      break;
    case ui::TextEditCommand::SELECT_ALL:
      SelectAll(false);
      break;
    case ui::TextEditCommand::SELECT_WORD:
      SelectWord();
      break;
    case ui::TextEditCommand::TRANSPOSE:
      changed = cursor_changed = model_->Transpose();
      break;
    case ui::TextEditCommand::YANK:
      changed = cursor_changed = model_->Yank();
      break;
    case ui::TextEditCommand::INSERT_TEXT:
    case ui::TextEditCommand::SET_MARK:
    case ui::TextEditCommand::UNSELECT:
    case ui::TextEditCommand::INVALID_COMMAND:
      NOTREACHED();
  }

  return {changed, cursor_changed};
}

void Textfield::OffsetDoubleClickWord(size_t offset) {
  selection_controller_.OffsetDoubleClickWord(offset);
}

bool Textfield::IsDropCursorForInsertion() const {
  return true;
}

bool Textfield::ShouldShowPlaceholderText() const {
  return GetText().empty() && !GetPlaceholderText().empty();
}

void Textfield::RequestFocusWithPointer(ui::EventPointerType pointer_type) {
  if (HasFocus())
    return;

  switch (pointer_type) {
    case ui::EventPointerType::kMouse:
      focus_reason_ = ui::TextInputClient::FOCUS_REASON_MOUSE;
      break;
    case ui::EventPointerType::kPen:
      focus_reason_ = ui::TextInputClient::FOCUS_REASON_PEN;
      break;
    case ui::EventPointerType::kTouch:
      focus_reason_ = ui::TextInputClient::FOCUS_REASON_TOUCH;
      break;
    default:
      focus_reason_ = ui::TextInputClient::FOCUS_REASON_OTHER;
      break;
  }

  View::RequestFocus();
}

void Textfield::RequestFocusForGesture(const ui::GestureEventDetails& details) {
  bool show_virtual_keyboard = true;
#if BUILDFLAG(IS_WIN)
  show_virtual_keyboard =
      details.primary_pointer_type() == ui::EventPointerType::kTouch ||
      details.primary_pointer_type() == ui::EventPointerType::kPen;
#endif

  RequestFocusWithPointer(details.primary_pointer_type());
  if (show_virtual_keyboard)
    ShowVirtualKeyboardIfEnabled();
}

base::CallbackListSubscription Textfield::AddTextChangedCallback(
    views::PropertyChangedCallback callback) {
  return AddPropertyChangedCallback(
      ui::metadata::MakeUniquePropertyKey(&model_, kTextfieldText),
      std::move(callback));
}

bool Textfield::PreHandleKeyPressed(const ui::KeyEvent& event) {
  return false;
}

ui::TextEditCommand Textfield::GetCommandForKeyEvent(
    const ui::KeyEvent& event) {
  if (event.type() != ui::EventType::kKeyPressed || event.IsUnicodeKeyCode()) {
    return ui::TextEditCommand::INVALID_COMMAND;
  }

  const bool shift = event.IsShiftDown();
#if BUILDFLAG(IS_MAC)
  const bool command = event.IsCommandDown();
#endif
  const bool control = event.IsControlDown() || event.IsCommandDown();
  const bool alt = event.IsAltDown() || event.IsAltGrDown();
  switch (event.key_code()) {
    case ui::VKEY_Z:
      if (control && !shift && !alt)
        return ui::TextEditCommand::UNDO;
      return (control && shift && !alt) ? ui::TextEditCommand::REDO
                                        : ui::TextEditCommand::INVALID_COMMAND;
    case ui::VKEY_Y:
      return (control && !alt) ? ui::TextEditCommand::REDO
                               : ui::TextEditCommand::INVALID_COMMAND;
    case ui::VKEY_A:
      return (control && !alt) ? ui::TextEditCommand::SELECT_ALL
                               : ui::TextEditCommand::INVALID_COMMAND;
    case ui::VKEY_X:
      return (control && !alt) ? ui::TextEditCommand::CUT
                               : ui::TextEditCommand::INVALID_COMMAND;
    case ui::VKEY_C:
      return (control && !alt) ? ui::TextEditCommand::COPY
                               : ui::TextEditCommand::INVALID_COMMAND;
    case ui::VKEY_V:
      return (control && !alt) ? ui::TextEditCommand::PASTE
                               : ui::TextEditCommand::INVALID_COMMAND;
    case ui::VKEY_RIGHT:
      // Ignore alt+right, which may be a browser navigation shortcut.
      if (alt)
        return ui::TextEditCommand::INVALID_COMMAND;
      if (!shift) {
        return control ? ui::TextEditCommand::MOVE_WORD_RIGHT
                       : ui::TextEditCommand::MOVE_RIGHT;
      }
      return control ? ui::TextEditCommand::MOVE_WORD_RIGHT_AND_MODIFY_SELECTION
                     : ui::TextEditCommand::MOVE_RIGHT_AND_MODIFY_SELECTION;
    case ui::VKEY_LEFT:
      // Ignore alt+left, which may be a browser navigation shortcut.
      if (alt)
        return ui::TextEditCommand::INVALID_COMMAND;
      if (!shift) {
        return control ? ui::TextEditCommand::MOVE_WORD_LEFT
                       : ui::TextEditCommand::MOVE_LEFT;
      }
      return control ? ui::TextEditCommand::MOVE_WORD_LEFT_AND_MODIFY_SELECTION
                     : ui::TextEditCommand::MOVE_LEFT_AND_MODIFY_SELECTION;
    case ui::VKEY_HOME:
      if (shift) {
        return ui::TextEditCommand::
            MOVE_TO_BEGINNING_OF_LINE_AND_MODIFY_SELECTION;
      }
#if BUILDFLAG(IS_MAC)
      return ui::TextEditCommand::SCROLL_TO_BEGINNING_OF_DOCUMENT;
#else
      return ui::TextEditCommand::MOVE_TO_BEGINNING_OF_LINE;
#endif
    case ui::VKEY_END:
      if (shift)
        return ui::TextEditCommand::MOVE_TO_END_OF_LINE_AND_MODIFY_SELECTION;
#if BUILDFLAG(IS_MAC)
      return ui::TextEditCommand::SCROLL_TO_END_OF_DOCUMENT;
#else
      return ui::TextEditCommand::MOVE_TO_END_OF_LINE;
#endif
    case ui::VKEY_UP:
#if BUILDFLAG(IS_MAC)
      if (control && shift) {
        return ui::TextEditCommand::
            MOVE_PARAGRAPH_BACKWARD_AND_MODIFY_SELECTION;
      }
      if (command)
        return ui::TextEditCommand::MOVE_TO_BEGINNING_OF_DOCUMENT;
      return shift ? ui::TextEditCommand::
                         MOVE_TO_BEGINNING_OF_LINE_AND_MODIFY_SELECTION
                   : ui::TextEditCommand::MOVE_UP;
#else
      return shift ? ui::TextEditCommand::
                         MOVE_TO_BEGINNING_OF_LINE_AND_MODIFY_SELECTION
                   : ui::TextEditCommand::INVALID_COMMAND;
#endif
    case ui::VKEY_DOWN:
#if BUILDFLAG(IS_MAC)
      if (control && shift) {
        return ui::TextEditCommand::MOVE_PARAGRAPH_FORWARD_AND_MODIFY_SELECTION;
      }
      if (command)
        return ui::TextEditCommand::MOVE_TO_END_OF_DOCUMENT;
      return shift
                 ? ui::TextEditCommand::MOVE_TO_END_OF_LINE_AND_MODIFY_SELECTION
                 : ui::TextEditCommand::MOVE_DOWN;
#else
      return shift
                 ? ui::TextEditCommand::MOVE_TO_END_OF_LINE_AND_MODIFY_SELECTION
                 : ui::TextEditCommand::INVALID_COMMAND;
#endif
    case ui::VKEY_BACK:
      if (!control) {
#if BUILDFLAG(IS_WIN)
        if (alt)
          return shift ? ui::TextEditCommand::REDO : ui::TextEditCommand::UNDO;
#endif
        return ui::TextEditCommand::DELETE_BACKWARD;
      }
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
      // Only erase by line break on Linux and ChromeOS.
      if (shift)
        return ui::TextEditCommand::DELETE_TO_BEGINNING_OF_LINE;
#endif
      return ui::TextEditCommand::DELETE_WORD_BACKWARD;
    case ui::VKEY_DELETE:
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
      // Only erase by line break on Linux and ChromeOS.
      if (shift && control)
        return ui::TextEditCommand::DELETE_TO_END_OF_LINE;
#endif
      if (control)
        return ui::TextEditCommand::DELETE_WORD_FORWARD;
      return shift ? ui::TextEditCommand::CUT
                   : ui::TextEditCommand::DELETE_FORWARD;
    case ui::VKEY_INSERT:
      if (control && !shift)
        return ui::TextEditCommand::COPY;
      return (shift && !control) ? ui::TextEditCommand::PASTE
                                 : ui::TextEditCommand::INVALID_COMMAND;
    case ui::VKEY_PRIOR:
      return control ? ui::TextEditCommand::SCROLL_PAGE_UP
                     : ui::TextEditCommand::INVALID_COMMAND;
    case ui::VKEY_NEXT:
      return control ? ui::TextEditCommand::SCROLL_PAGE_DOWN
                     : ui::TextEditCommand::INVALID_COMMAND;
    default:
      return ui::TextEditCommand::INVALID_COMMAND;
  }
}

#if BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)
void Textfield::SetNeedsAccessibleTextOffsetsUpdate() {
  needs_ax_text_offsets_update_ = true;
}
#endif  // BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)

////////////////////////////////////////////////////////////////////////////////
// Textfield, private:

////////////////////////////////////////////////////////////////////////////////
// Textfield, SelectionControllerDelegate overrides:

gfx::RenderText* Textfield::GetRenderTextForSelectionController() {
  return GetRenderText();
}

bool Textfield::IsReadOnly() const {
  return GetReadOnly();
}

bool Textfield::SupportsDrag() const {
  return true;
}

void Textfield::SetTextBeingDragged(bool value) {
  initiating_drag_ = value;
}

int Textfield::GetViewHeight() const {
  return height();
}

int Textfield::GetViewWidth() const {
  return width();
}

int Textfield::GetDragSelectionDelay() const {
  if (ui::ScopedAnimationDurationScaleMode::duration_multiplier() ==
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION) {
    // NON_ZERO_DURATION is 1/20 by default, but we want 1/100 here.
    return 1;
  }
  return ui::ScopedAnimationDurationScaleMode::duration_multiplier() * 100;
}

void Textfield::OnBeforePointerAction() {
  OnBeforeUserAction();
  if (model_->HasCompositionText())
    model_->ConfirmCompositionText();
}

void Textfield::OnAfterPointerAction(bool text_changed,
                                     bool selection_changed) {
  OnAfterUserAction();
  const auto text_change_type =
      text_changed ? TextChangeType::kUserTriggered : TextChangeType::kNone;
  UpdateAfterChange(text_change_type, selection_changed);
}

bool Textfield::PasteSelectionClipboard() {
  DCHECK(performing_user_action_);
  DCHECK(!GetReadOnly());
  const std::u16string selection_clipboard_text = GetSelectionClipboardText();
  if (selection_clipboard_text.empty())
    return false;

  model_->InsertText(selection_clipboard_text);
  return true;
}

void Textfield::UpdateSelectionClipboard() {
  if (ui::Clipboard::IsSupportedClipboardBuffer(
          ui::ClipboardBuffer::kSelection)) {
    if (text_input_type_ != ui::TEXT_INPUT_TYPE_PASSWORD) {
      ui::ScopedClipboardWriter(ui::ClipboardBuffer::kSelection)
          .WriteText(GetSelectedText());
      if (controller_)
        controller_->OnAfterCutOrCopy(ui::ClipboardBuffer::kSelection);
    }
  }
}

void Textfield::UpdateBackgroundColor() {
  if (!is_background_enabled_) {
    if (GetBackground()) {
      SetBackground(nullptr);
      // If the parent for this textfield creates a non-opaque background they
      // are responsible for disabling subpixel rendering.
      GetRenderText()->set_subpixel_rendering_suppressed(false);
    }
    return;
  }

  const SkColor color = GetBackgroundColor();
  SetBackground(CreateBackgroundFromPainter(
      Painter::CreateSolidRoundRectPainter(color, GetCornerRadius())));
  // Disable subpixel rendering when the background color is not opaque because
  // it draws incorrect colors around the glyphs in that case.
  // See crbug.com/115198
  GetRenderText()->set_subpixel_rendering_suppressed(SkColorGetA(color) !=
                                                     SK_AlphaOPAQUE);
  OnPropertyChanged(
      ui::metadata::MakeUniquePropertyKey(&model_, kTextfieldBackgroundColor),
      kPropertyEffectsPaint);
}

void Textfield::UpdateDefaultBorder() {
  // Only update the border if SetBorder() has not been called. This is to avoid
  // overriding any custom borders.
  if (!use_default_border_) {
    return;
  }
  auto border = std::make_unique<views::FocusableBorder>();
  const LayoutProvider* provider = LayoutProvider::Get();
  border->SetColorId(ui::kColorTextfieldOutline);
  border->SetInsets(gfx::Insets::TLBR(
      extra_insets_.top() +
          provider->GetDistanceMetric(DISTANCE_CONTROL_VERTICAL_TEXT_PADDING),
      extra_insets_.left() + provider->GetDistanceMetric(
                                 DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING),
      extra_insets_.bottom() +
          provider->GetDistanceMetric(DISTANCE_CONTROL_VERTICAL_TEXT_PADDING),
      extra_insets_.right() + provider->GetDistanceMetric(
                                  DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING)));
  if (invalid_) {
    border->SetColorId(ui::kColorTextfieldOutlineInvalid);
  } else if (!GetEnabled() || GetReadOnly()) {
    border->SetColorId(ui::kColorTextfieldOutlineDisabled);
  }
  border->SetCornerRadius(GetCornerRadius());
  View::SetBorder(std::move(border));
}

void Textfield::UpdateSelectionTextColor() {
  if (!GetWidget()) {
    return;
  }
  GetRenderText()->set_selection_color(GetSelectionTextColor());
  OnPropertyChanged(ui::metadata::MakeUniquePropertyKey(
                        &model_, kTextfieldSelectionTextColor),
                    kPropertyEffectsPaint);
}

void Textfield::UpdateSelectionBackgroundColor() {
  if (!GetWidget()) {
    return;
  }
  GetRenderText()->set_selection_background_focused_color(
      GetSelectionBackgroundColor());
  OnPropertyChanged(ui::metadata::MakeUniquePropertyKey(
                        &model_, kTextfieldSelectionBackgroundColor),
                    kPropertyEffectsPaint);
}

void Textfield::UpdateAfterChange(
    TextChangeType text_change_type,
    bool cursor_changed,
    std::optional<bool> notify_caret_bounds_changed) {
  if (text_change_type != TextChangeType::kNone) {
    if ((text_change_type == TextChangeType::kUserTriggered) && controller_)
      controller_->ContentsChanged(this, GetText());
    UpdateAccessibleValue();
  }
  UpdateAccessibilityTextDirection();
  if (cursor_changed) {
    UpdateCursorViewPosition();
    UpdateCursorVisibility();
  }
  const bool anything_changed =
      (text_change_type != TextChangeType::kNone) || cursor_changed;
  if (notify_caret_bounds_changed.value_or(anything_changed))
    OnCaretBoundsChanged();
  if (anything_changed)
    SchedulePaint();
}

void Textfield::UpdateAccessibilityTextDirection() {
  GetViewAccessibility().SetTextDirection(
      static_cast<int32_t>(GetTextDirection() == base::i18n::RIGHT_TO_LEFT
                               ? ax::mojom::WritingDirection::kRtl
                               : ax::mojom::WritingDirection::kLtr));
}

void Textfield::UpdateAccessibleValue() {
  if (text_input_type_ == ui::TEXT_INPUT_TYPE_PASSWORD) {
    GetViewAccessibility().SetValue(std::u16string(
        GetText().size(), gfx::RenderText::kPasswordReplacementChar));
  } else {
    GetViewAccessibility().SetValue(GetText());
  }
}

void Textfield::UpdateCursorVisibility() {
  cursor_view_->SetVisible(ShouldShowCursor());
#if BUILDFLAG(IS_MAC)
  // If we called SetVisible(true) above we want the cursor layer to be opaque.
  // If we called SetVisible(false), making the layer opaque has no effect on
  // its visibility.
  cursor_view_->layer()->SetOpacity(kOpaque);
#endif
  if (ShouldBlinkCursor())
    StartBlinkingCursor();
  else
    StopBlinkingCursor();
}

bool Textfield::IsMenuShowing() const {
  return context_menu_runner_ && context_menu_runner_->IsRunning();
}

gfx::Rect Textfield::CalculateCursorViewBounds() const {
  gfx::Rect location(GetRenderText()->GetUpdatedCursorBounds());
  location.set_x(GetMirroredXForRect(location));
  // Shrink the cursor bounds to fit within the view.
  location.Intersect(GetLocalBounds());
  return location;
}

void Textfield::UpdateCursorViewPosition() {
  cursor_view_->SetBoundsRect(CalculateCursorViewBounds());
  GetViewAccessibility().SetScrollX(
      GetRenderText()->GetUpdatedDisplayOffset().x());
}

int Textfield::GetTextStyle() const {
  if (GetReadOnly() || !GetEnabled()) {
    return style::STYLE_DISABLED;
  } else if (GetInvalid()) {
    return style::STYLE_INVALID;
  } else {
    return style::STYLE_PRIMARY;
  }
}

void Textfield::PaintTextAndCursor(gfx::Canvas* canvas) {
  TRACE_EVENT0("views", "Textfield::PaintTextAndCursor");
  canvas->Save();

  // Draw placeholder text if needed.
  gfx::RenderText* render_text = GetRenderText();
  if (ShouldShowPlaceholderText()) {
    // Disable subpixel rendering when the background color is not opaque
    // because it draws incorrect colors around the glyphs in that case.
    // See crbug.com/786343
    int placeholder_text_draw_flags = placeholder_text_draw_flags_;
    if (SkColorGetA(GetBackgroundColor()) != SK_AlphaOPAQUE)
      placeholder_text_draw_flags |= gfx::Canvas::NO_SUBPIXEL_RENDERING;

    canvas->DrawStringRectWithFlags(
        GetPlaceholderText(), placeholder_font_list_.value_or(GetFontList()),
        placeholder_text_color_.value_or(
            GetColorProvider()->GetColor(TypographyProvider::Get().GetColorId(
                style::CONTEXT_TEXTFIELD_PLACEHOLDER,
                GetInvalid() ? style::STYLE_INVALID : style::STYLE_PRIMARY))),
        render_text->display_rect(), placeholder_text_draw_flags);
  }

  // If drop cursor is active, draw |render_text| with its text selected.
  const bool select_all = drop_cursor_visible_ && !IsDropCursorForInsertion();
  render_text->Draw(canvas, select_all);

  if (drop_cursor_visible_ && IsDropCursorForInsertion()) {
    // Draw a drop cursor that marks where the text will be dropped/inserted.
    canvas->FillRect(render_text->GetCursorBounds(drop_cursor_position_, true),
                     GetTextColor());
  }

  canvas->Restore();
}

void Textfield::MoveCursorTo(const gfx::Point& point, bool select) {
  if (model_->MoveCursorTo(point, select))
    UpdateAfterChange(TextChangeType::kNone, true);
}

void Textfield::OnCaretBoundsChanged() {
  if (GetInputMethod())
    GetInputMethod()->OnCaretBoundsChanged(this);
  if (touch_selection_controller_)
    touch_selection_controller_->SelectionChanged();

  // Screen reader users don't expect notifications about unfocused textfields.
  if (HasFocus()) {
    // If this control is in the process of receiving focus, even though it
    // 'HasFocus', the accessibility event to announce that it has focus has not
    // been fired yet. The kTextSelectionChanged event needs to be fired *after*
    // the focus event, so we attach our observer which will fire the event
    // after we finish receiving focus (which includes the accessibility focus
    // event being fired).
    if (is_processing_focus_) {
      if (!HasObserver(this)) {
        AddObserver(this);
      }
    } else {
      NotifyAccessibilityEvent(ax::mojom::Event::kTextSelectionChanged, true);
    }
  }

  UpdateCursorViewPosition();
}

void Textfield::OnBeforeUserAction() {
  DCHECK(!performing_user_action_);
  performing_user_action_ = true;
  if (controller_)
    controller_->OnBeforeUserAction(this);
}

void Textfield::OnAfterUserAction() {
  if (controller_)
    controller_->OnAfterUserAction(this);
  DCHECK(performing_user_action_);
  performing_user_action_ = false;
}

bool Textfield::Cut() {
  if (!GetReadOnly() && text_input_type_ != ui::TEXT_INPUT_TYPE_PASSWORD &&
      model_->Cut()) {
    if (controller_)
      controller_->OnAfterCutOrCopy(ui::ClipboardBuffer::kCopyPaste);

    return true;
  }
  return false;
}

bool Textfield::Copy() {
  if (text_input_type_ != ui::TEXT_INPUT_TYPE_PASSWORD && model_->Copy()) {
    if (controller_)
      controller_->OnAfterCutOrCopy(ui::ClipboardBuffer::kCopyPaste);
    return true;
  }
  return false;
}

bool Textfield::Paste() {
  if (!GetReadOnly() && model_->Paste()) {
    if (controller_)
      controller_->OnAfterPaste();

    return true;
  }
  return false;
}

void Textfield::UpdateContextMenu() {
  // TextfieldController may modify Textfield's menu, so the menu should be
  // recreated each time it's shown. Destroy the existing objects in the reverse
  // order of creation.
  context_menu_runner_.reset();
  context_menu_contents_.reset();

  context_menu_contents_ = std::make_unique<ui::SimpleMenuModel>(this);
  context_menu_contents_->AddItemWithStringId(kUndo, IDS_APP_UNDO);
  context_menu_contents_->AddSeparator(ui::NORMAL_SEPARATOR);
  context_menu_contents_->AddItemWithStringId(kCut, IDS_APP_CUT);
  context_menu_contents_->AddItemWithStringId(kCopy, IDS_APP_COPY);
  context_menu_contents_->AddItemWithStringId(kPaste, IDS_APP_PASTE);
  context_menu_contents_->AddItemWithStringId(kDelete, IDS_APP_DELETE);
  context_menu_contents_->AddSeparator(ui::NORMAL_SEPARATOR);
  context_menu_contents_->AddItemWithStringId(kSelectAll, IDS_APP_SELECT_ALL);

  // If the controller adds menu commands, also override ExecuteCommand() and
  // IsCommandIdEnabled() as appropriate, for the commands added.
  if (controller_)
    controller_->UpdateContextMenu(context_menu_contents_.get());

  text_services_context_menu_ =
      ViewsTextServicesContextMenu::Create(context_menu_contents_.get(), this);

  context_menu_runner_ = std::make_unique<MenuRunner>(
      context_menu_contents_.get(),
      MenuRunner::HAS_MNEMONICS | MenuRunner::CONTEXT_MENU);
}

bool Textfield::ImeEditingAllowed() const {
  // Disallow input method editing of password fields.
  ui::TextInputType t = GetTextInputType();
  return (t != ui::TEXT_INPUT_TYPE_NONE && t != ui::TEXT_INPUT_TYPE_PASSWORD);
}

void Textfield::RevealPasswordChar(std::optional<size_t> index,
                                   base::TimeDelta duration) {
  GetRenderText()->SetObscuredRevealIndex(index);
  SchedulePaint();
  password_char_reveal_index_ = index;
  UpdateCursorViewPosition();

  if (index.has_value()) {
    password_reveal_timer_.Start(
        FROM_HERE, duration,
        base::BindOnce(&Textfield::RevealPasswordChar,
                       weak_ptr_factory_.GetWeakPtr(), std::nullopt, duration));
  }
}

void Textfield::CreateTouchSelectionControllerAndNotifyIt() {
  if (!HasFocus())
    return;

  if (!touch_selection_controller_) {
#if defined(USE_AURA)
    touch_selection_controller_ =
        std::make_unique<TouchSelectionControllerImpl>(this);
#endif
  }
  if (touch_selection_controller_)
    touch_selection_controller_->SelectionChanged();
}

void Textfield::OnEditFailed() {
  PlatformStyle::OnTextfieldEditFailed();
}

bool Textfield::ShouldShowCursor() const {
  // Show the cursor when the primary selected range is empty; secondary
  // selections do not affect cursor visibility.
  return HasFocus() && !HasSelection(true) && GetEnabled() && !GetReadOnly() &&
         !drop_cursor_visible_ && GetRenderText()->cursor_enabled() &&
         !cursor_view_->bounds().IsEmpty();
}

int Textfield::CharsToDips(int width_in_chars) const {
  // Use a subset of the conditions in ShouldShowCursor() that are unlikely to
  // change dynamically.  Dynamic changes can result in glitchy-looking visual
  // effects like find boxes on different tabs being 1 DIP different width.
  const int cursor_width =
      (!GetReadOnly() && GetRenderText()->cursor_enabled()) ? 1 : 0;
  return GetFontList().GetExpectedTextWidth(width_in_chars) + cursor_width +
         GetInsets().width();
}

bool Textfield::ShouldBlinkCursor() const {
  return ShouldShowCursor() && !Textfield::GetCaretBlinkInterval().is_zero();
}

void Textfield::StartBlinkingCursor() {
  DCHECK(ShouldBlinkCursor());
  cursor_blink_timer_.Start(FROM_HERE, Textfield::GetCaretBlinkInterval(), this,
                            &Textfield::OnCursorBlinkTimerFired);
}

void Textfield::StopBlinkingCursor() {
  cursor_blink_timer_.Stop();
}

void Textfield::OnCursorBlinkTimerFired() {
  DCHECK(ShouldBlinkCursor());
  // TODO(crbug.com/40820702): The cursor position is not updated appropriately
  // when locale changes from a left-to-right script to a right-to-left script.
  // Thus the cursor is displayed at a wrong position immediately after the
  // locale change. As a band-aid solution, we update the cursor here, so that
  // the cursor can be at the wrong position only up until the next blink. It
  // would be better to detect locale change explicitly (how?) and update
  // there.
  UpdateCursorViewPosition();

#if BUILDFLAG(IS_MAC)
  // https://crbug.com/1311416 . The cursor is a solid color Layer which we make
  // flash by toggling the Layer's visibility. A bug in the CARendererLayerTree
  // causes all Layers that come after the cursor in the Layer tree (the
  // bookmarks bar, for example) to be pushed to the screen with each
  // appearance and disappearance of the cursor, consuming unnecessary CPU in
  // the window server. The same also happens if we leave the cursor visible
  // but make its color or layer 100% transparent (there's an optimization
  // somewhere that removes completely transparent layers from the hierarchy).
  // Until this bug is fixed, flash the cursor by alternating the cursor
  // layer's opacity between opaque and "almost" transparent. Once
  // https://crbug.com/1313999 is fixed, this Mac-specific code and its tests
  // can be removed.
  float new_opacity = cursor_view_->layer()->opacity() != kOpaque
                          ? kOpaque
                          : kAlmostTransparent;
  cursor_view_->layer()->SetOpacity(new_opacity);
#else
  cursor_view_->SetVisible(!cursor_view_->GetVisible());
#endif
}

void Textfield::OnEnabledChanged() {
  if (GetInputMethod())
    GetInputMethod()->OnTextInputTypeChanged(this);
  UpdateDefaultBorder();

  // Only expose readonly if enabled. Don't overwrite the disabled restriction.
  // However, if we re-enable a textfield that was already set to readonly,
  // we need to update the readonly state, since the disabled restriction would
  // have overwritten it.
  if (GetEnabled() && GetReadOnly()) {
    GetViewAccessibility().SetReadOnly(true);
  }
  UpdateAccessibleDefaultActionVerb();
}

void Textfield::DropDraggedText(
    const ui::DropTargetEvent& event,
    ui::mojom::DragOperation& output_drag_op,
    std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
  DCHECK(CanDrop(event.data()));

  gfx::RenderText* render_text = GetRenderText();

  OnBeforeUserAction();
  skip_input_method_cancel_composition_ = true;

  gfx::SelectionModel drop_destination_model =
      render_text->FindCursorPosition(event.location());
  std::u16string new_text = event.data().GetString().value_or(std::u16string());

  // Delete the current selection for a drag and drop within this view.
  const bool move = initiating_drag_ && !event.IsControlDown() &&
                    event.source_operations() & ui::DragDropTypes::DRAG_MOVE;
  if (move) {
    // Adjust the drop destination if it is on or after the current selection.
    size_t pos = drop_destination_model.caret_pos();
    pos -= render_text->selection().Intersect(gfx::Range(0, pos)).length();
    model_->DeletePrimarySelectionAndInsertTextAt(new_text, pos);
  } else {
    model_->MoveCursorTo(drop_destination_model);
    // Drop always inserts text even if the textfield is not in insert mode.
    model_->InsertText(new_text);
  }
  skip_input_method_cancel_composition_ = false;
  UpdateAfterChange(TextChangeType::kUserTriggered, true);
  OnAfterUserAction();
  output_drag_op = move ? DragOperation::kMove : DragOperation::kCopy;
}

float Textfield::GetCornerRadius() {
  return LayoutProvider::Get()->GetCornerRadiusMetric(
      ShapeContextTokens::kTextfieldRadius, size());
}

void Textfield::OnGestureScrollBegin(int drag_start_location_x) {
  drag_start_location_x_ = drag_start_location_x;
  drag_start_display_offset_ = GetRenderText()->GetUpdatedDisplayOffset().x();
  show_touch_handles_after_scroll_ = touch_selection_controller_ != nullptr;
  DestroyTouchSelection();
}

void Textfield::GestureScroll(int drag_location_x) {
  int new_display_offset =
      drag_start_display_offset_ + drag_location_x - drag_start_location_x_;
  GetRenderText()->SetDisplayOffset(new_display_offset);
  SchedulePaint();
}

bool Textfield::HandleGestureForSelectionDragging(ui::GestureEvent* event) {
  if (!::features::IsTouchTextEditingRedesignEnabled()) {
    return false;
  }

  switch (event->type()) {
    case ui::EventType::kGestureTap:
      if (selection_dragging_state_ != SelectionDraggingState::kNone) {
        // Selection has already been set in preceding events, so we can just
        // cancel selection dragging and show touch handles without changing the
        // selection.
        StopSelectionDragging();
        CreateTouchSelectionControllerAndNotifyIt();
        event->SetHandled();
        return true;
      }
      return false;
    case ui::EventType::kGestureTapDown:
      if (event->details().tap_down_count() == 1) {
        selection_dragging_state_ = SelectionDraggingState::kNone;
        return false;
      } else if (event->details().tap_down_count() == 2) {
        OnBeforeUserAction();
        SelectWordAt(event->location());
        OnAfterUserAction();
        selection_dragging_state_ = SelectionDraggingState::kSelectedWord;
        selection_drag_type_ = ui::TouchSelectionDragType::kDoublePressDrag;
      } else if (event->details().tap_down_count() == 3) {
        OnBeforeUserAction();
        SelectAll(false);
        OnAfterUserAction();
        selection_dragging_state_ = SelectionDraggingState::kSelectedAll;
      }
      DestroyTouchSelection();
      event->SetHandled();
      return true;
    case ui::EventType::kGestureLongPress:
      selection_dragging_state_ = SelectionDraggingState::kSelectedWord;
      selection_drag_type_ = ui::TouchSelectionDragType::kLongPressDrag;
      DestroyTouchSelection();
      event->SetHandled();
      return true;
    case ui::EventType::kGestureLongTap:
      if (selection_dragging_state_ != SelectionDraggingState::kNone) {
        StopSelectionDragging();
        CreateTouchSelectionControllerAndNotifyIt();
        event->SetHandled();
        return true;
      }
      return false;
    case ui::EventType::kGestureScrollBegin:
      // Only start selection dragging if scrolling with one touch point.
      if (event->details().touch_points() == 1 &&
          StartSelectionDragging(*event)) {
        CreateTouchSelectionControllerAndNotifyIt();
        show_touch_handles_after_scroll_ = true;
        event->SetHandled();
        return true;
      }
      StopSelectionDragging();
      return false;
    case ui::EventType::kGestureScrollUpdate:
      // Switch from selection dragging to default scrolling behaviour if scroll
      // update has multiple touch points.
      if (IsSelectionDragging() && event->details().touch_points() > 1) {
        StopSelectionDragging();
        OnGestureScrollBegin(event->location().x());
        return false;
      } else if (selection_dragging_state_ ==
                 SelectionDraggingState::kDraggingSelectionExtent) {
        MoveRangeSelectionExtent(event->location() +
                                 selection_dragging_offset_);
        event->SetHandled();
        return true;
      } else if (selection_dragging_state_ ==
                 SelectionDraggingState::kDraggingCursor) {
        MoveCursorTo(event->location(), false);
        event->SetHandled();
        return true;
      }
      return false;
    case ui::EventType::kGestureScrollEnd:
    case ui::EventType::kScrollFlingStart:
    case ui::EventType::kGestureEnd:
      StopSelectionDragging();
      return false;
    default:
      return false;
  }
}

bool Textfield::StartSelectionDragging(const ui::GestureEvent& event) {
  DCHECK_EQ(event.type(), ui::EventType::kGestureScrollBegin);

  const float delta_x = event.details().scroll_x_hint();
  const float delta_y = event.details().scroll_y_hint();
  if (selection_dragging_state_ == SelectionDraggingState::kSelectedWord) {
    gfx::RenderText* render_text = GetRenderText();
    gfx::SelectionModel start_sel =
        render_text->GetSelectionModelForSelectionStart();
    const gfx::SelectionModel& sel = render_text->selection_model();
    gfx::Point selection_start =
        render_text->GetCursorBounds(start_sel, true).CenterPoint();
    gfx::Point selection_end =
        render_text->GetCursorBounds(sel, true).CenterPoint();

    gfx::LogicalCursorDirection drag_direction = gfx::CURSOR_FORWARD;
    if (std::fabs(delta_y) > std::fabs(delta_x)) {
      // If the initial dragging motion is up/down, extend the selection
      // backwards/forwards.
      drag_direction = delta_y < 0 ? gfx::CURSOR_BACKWARD : gfx::CURSOR_FORWARD;
    } else {
      // Otherwise, extend the selection in the direction of horizontal
      // movement.
      drag_direction = delta_x * (selection_end.x() - selection_start.x()) < 0
                           ? gfx::CURSOR_BACKWARD
                           : gfx::CURSOR_FORWARD;
    }

    gfx::Point base =
        drag_direction == gfx::CURSOR_FORWARD ? selection_start : selection_end;
    gfx::Point extent =
        drag_direction == gfx::CURSOR_FORWARD ? selection_end : selection_start;
    SelectBetweenCoordinates(base, extent);

    selection_dragging_offset_ = extent - event.location();
    selection_dragging_state_ =
        SelectionDraggingState::kDraggingSelectionExtent;
    return true;
  } else if (selection_dragging_state_ == SelectionDraggingState::kNone &&
             std::fabs(delta_x) >= std::fabs(delta_y)) {
    // If a horizontal dragging gesture begins while the cursor is present (i.e.
    // empty selection), use the gesture to move the cursor. Temporarily destroy
    // the touch selection controller so that the touch handles don't appear in
    // the wrong spot before the cursor is moved.
    DestroyTouchSelection();
    MoveCursorTo(event.location(), false);
    selection_dragging_state_ = SelectionDraggingState::kDraggingCursor;
    selection_drag_type_ = ui::TouchSelectionDragType::kCursorDrag;
    return true;
  }
  return false;
}

void Textfield::StopSelectionDragging() {
  if (IsSelectionDragging() && selection_drag_type_.has_value()) {
    ui::RecordTouchSelectionDrag(selection_drag_type_.value());
  }
  selection_dragging_state_ = SelectionDraggingState::kNone;
  selection_drag_type_ = std::nullopt;
}

void Textfield::UpdateAccessibleDefaultActionVerb() {
  if (GetEnabled()) {
    GetViewAccessibility().SetDefaultActionVerb(
        ax::mojom::DefaultActionVerb::kActivate);
  } else {
    GetViewAccessibility().RemoveDefaultActionVerb();
  }
}

BEGIN_METADATA(Textfield)
ADD_PROPERTY_METADATA(bool, ReadOnly)
ADD_PROPERTY_METADATA(std::u16string, Text)
ADD_PROPERTY_METADATA(ui::TextInputType, TextInputType)
ADD_PROPERTY_METADATA(int, TextInputFlags)
ADD_PROPERTY_METADATA(SkColor, TextColor, ui::metadata::SkColorConverter)
ADD_PROPERTY_METADATA(SkColor,
                      SelectionTextColor,
                      ui::metadata::SkColorConverter)
ADD_PROPERTY_METADATA(SkColor, BackgroundColor, ui::metadata::SkColorConverter)
ADD_PROPERTY_METADATA(bool, BackgroundEnabled)
ADD_PROPERTY_METADATA(SkColor,
                      SelectionBackgroundColor,
                      ui::metadata::SkColorConverter)
ADD_PROPERTY_METADATA(bool, CursorEnabled)
ADD_PROPERTY_METADATA(std::u16string, PlaceholderText)
ADD_PROPERTY_METADATA(bool, Invalid)
ADD_PROPERTY_METADATA(gfx::HorizontalAlignment, HorizontalAlignment)
ADD_PROPERTY_METADATA(gfx::Range, SelectedRange)
END_METADATA

}  // namespace views
