// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/link.h"

#include "build/build_config.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/cursor/cursor.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/native_cursor.h"
#include "ui/views/style/platform_style.h"

namespace views {

// static
constexpr gfx::Insets Link::kFocusBorderPadding;

Link::Link(const base::string16& title, int text_context, int text_style)
    : Label(title, text_context, text_style),
      requested_enabled_color_(gfx::kPlaceholderColor),
      requested_enabled_color_set_(false) {
  Init();
}

Link::~Link() = default;

// static
Link::FocusStyle Link::GetDefaultFocusStyle() {
  return FocusStyle::kUnderline;
}

Link::FocusStyle Link::GetFocusStyle() const {
  // Use the default, unless the link would "always" be underlined.
  if (underline_ && GetDefaultFocusStyle() == FocusStyle::kUnderline)
    return FocusStyle::kRing;

  return GetDefaultFocusStyle();
}

SkColor Link::GetColor() const {
  // TODO(tapted): Use style::GetColor().
  const ui::NativeTheme* theme = GetNativeTheme();
  DCHECK(theme);
  if (!GetEnabled())
    return theme->GetSystemColor(ui::NativeTheme::kColorId_LinkDisabled);

  if (requested_enabled_color_set_)
    return requested_enabled_color_;

  return GetNativeTheme()->GetSystemColor(
      pressed_ ? ui::NativeTheme::kColorId_LinkPressed
               : ui::NativeTheme::kColorId_LinkEnabled);
}

void Link::PaintFocusRing(gfx::Canvas* canvas) const {
  if (GetFocusStyle() == FocusStyle::kRing) {
    gfx::Rect focus_ring_bounds = GetTextBounds();
    focus_ring_bounds.Inset(-kFocusBorderPadding);
    focus_ring_bounds.Intersect(GetLocalBounds());
    canvas->DrawFocusRect(focus_ring_bounds);
  }
}

gfx::Insets Link::GetInsets() const {
  gfx::Insets insets = Label::GetInsets();
  if (GetFocusStyle() == FocusStyle::kRing &&
      GetFocusBehavior() != FocusBehavior::NEVER) {
    DCHECK(!GetText().empty());
    insets += kFocusBorderPadding;
  }
  return insets;
}

gfx::NativeCursor Link::GetCursor(const ui::MouseEvent& event) {
  if (!GetEnabled())
    return gfx::kNullCursor;
  return GetNativeHandCursor();
}

bool Link::CanProcessEventsWithinSubtree() const {
  // Links need to be able to accept events (e.g., clicking) even though
  // in general Labels do not.
  return View::CanProcessEventsWithinSubtree();
}

bool Link::OnMousePressed(const ui::MouseEvent& event) {
  if (!GetEnabled() ||
      (!event.IsLeftMouseButton() && !event.IsMiddleMouseButton()))
    return false;
  SetPressed(true);
  return true;
}

bool Link::OnMouseDragged(const ui::MouseEvent& event) {
  SetPressed(GetEnabled() &&
             (event.IsLeftMouseButton() || event.IsMiddleMouseButton()) &&
             HitTestPoint(event.location()));
  return true;
}

void Link::OnMouseReleased(const ui::MouseEvent& event) {
  // Change the highlight first just in case this instance is deleted
  // while calling the controller
  OnMouseCaptureLost();
  if (GetEnabled() &&
      (event.IsLeftMouseButton() || event.IsMiddleMouseButton()) &&
      HitTestPoint(event.location())) {
    // Focus the link on click.
    RequestFocus();

    if (listener_)
      listener_->LinkClicked(this, event.flags());
  }
}

void Link::OnMouseCaptureLost() {
  SetPressed(false);
}

bool Link::OnKeyPressed(const ui::KeyEvent& event) {
  bool activate = (((event.key_code() == ui::VKEY_SPACE) &&
                    (event.flags() & ui::EF_ALT_DOWN) == 0) ||
                   (event.key_code() == ui::VKEY_RETURN &&
                    PlatformStyle::kReturnClicksFocusedControl));
  if (!activate)
    return false;

  SetPressed(false);

  // Focus the link on key pressed.
  RequestFocus();

  if (listener_)
    listener_->LinkClicked(this, event.flags());

  return true;
}

void Link::OnGestureEvent(ui::GestureEvent* event) {
  if (!GetEnabled())
    return;

  if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
    SetPressed(true);
  } else if (event->type() == ui::ET_GESTURE_TAP) {
    RequestFocus();
    if (listener_)
      listener_->LinkClicked(this, event->flags());
  } else {
    SetPressed(false);
    return;
  }
  event->SetHandled();
}

bool Link::SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) {
  // Don't process Space and Return (depending on the platform) as an
  // accelerator.
  return event.key_code() == ui::VKEY_SPACE ||
         (event.key_code() == ui::VKEY_RETURN &&
          PlatformStyle::kReturnClicksFocusedControl);
}

void Link::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  Label::GetAccessibleNodeData(node_data);
  // Prevent invisible links from being announced by screen reader.
  node_data->role =
      GetText().empty() ? ax::mojom::Role::kIgnored : ax::mojom::Role::kLink;
}

void Link::OnFocus() {
  Label::OnFocus();
  RecalculateFont();
  // We render differently focused.
  SchedulePaint();
}

void Link::OnBlur() {
  Label::OnBlur();
  RecalculateFont();
  // We render differently focused.
  SchedulePaint();
}

void Link::SetFontList(const gfx::FontList& font_list) {
  Label::SetFontList(font_list);
  RecalculateFont();
}

void Link::SetText(const base::string16& text) {
  Label::SetText(text);
  ConfigureFocus();
}

void Link::OnThemeChanged() {
  Label::OnThemeChanged();
  Label::SetEnabledColor(GetColor());
}

void Link::SetEnabledColor(SkColor color) {
  requested_enabled_color_set_ = true;
  requested_enabled_color_ = color;
  Label::SetEnabledColor(GetColor());
}

bool Link::IsSelectionSupported() const {
  return false;
}

bool Link::GetUnderline() const {
  return underline_;
}

void Link::SetUnderline(bool underline) {
  if (underline_ == underline)
    return;
  underline_ = underline;
  RecalculateFont();
  OnPropertyChanged(&underline_, kPropertyEffectsPreferredSizeChanged);
}

void Link::Init() {
  listener_ = nullptr;
  pressed_ = false;
  underline_ = GetDefaultFocusStyle() != FocusStyle::kUnderline;
  RecalculateFont();

  enabled_changed_subscription_ = AddEnabledChangedCallback(
      base::BindRepeating(&Link::RecalculateFont, base::Unretained(this)));

  // Label::Init() calls SetText(), but if that's being called from Label(), our
  // SetText() override will not be reached (because the constructed class is
  // only a Label at the moment, not yet a Link).  So explicitly configure focus
  // here.
  ConfigureFocus();
}

void Link::SetPressed(bool pressed) {
  if (pressed_ != pressed) {
    pressed_ = pressed;
    Label::SetEnabledColor(GetColor());
    RecalculateFont();
    SchedulePaint();
  }
}

void Link::RecalculateFont() {
  // Underline the link if it is enabled and |underline_| is true. Also
  // underline to indicate focus when that's the style.
  const int style = font_list().GetFontStyle();
  const bool underline =
      underline_ || (HasFocus() && GetFocusStyle() == FocusStyle::kUnderline);
  const int intended_style = (GetEnabled() && underline)
                                 ? (style | gfx::Font::UNDERLINE)
                                 : (style & ~gfx::Font::UNDERLINE);

  if (style != intended_style)
    Label::SetFontList(font_list().DeriveWithStyle(intended_style));
}

void Link::ConfigureFocus() {
  // Disable focusability for empty links.
  if (GetText().empty()) {
    SetFocusBehavior(FocusBehavior::NEVER);
  } else {
#if defined(OS_MACOSX)
    SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
#else
    SetFocusBehavior(FocusBehavior::ALWAYS);
#endif
  }
}

DEFINE_ENUM_CONVERTERS(Link::FocusStyle,
                       {Link::FocusStyle::kUnderline,
                        base::ASCIIToUTF16("UNDERLINE")},
                       {Link::FocusStyle::kRing, base::ASCIIToUTF16("RING")})
BEGIN_METADATA(Link)
METADATA_PARENT_CLASS(Label)
ADD_READONLY_PROPERTY_METADATA(Link, SkColor, Color)
ADD_READONLY_PROPERTY_METADATA(Link, Link::FocusStyle, FocusStyle)
ADD_PROPERTY_METADATA(Link, bool, Underline)
END_METADATA()

}  // namespace views
