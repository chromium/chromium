// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/link.h"

#include "build/build_config.h"

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/cursor/cursor.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/native_cursor.h"
#include "ui/views/style/platform_style.h"

namespace views {

Link::Link(const base::string16& title, int text_context, int text_style)
    : Label(title, text_context, text_style) {
  RecalculateFont();

  enabled_changed_subscription_ = AddEnabledChangedCallback(
      base::BindRepeating(&Link::RecalculateFont, base::Unretained(this)));

  // Label() indirectly calls SetText(), but at that point our virtual override
  // will not be reached.  Call it explicitly here to configure focus.
  SetText(GetText());
}

Link::~Link() = default;

SkColor Link::GetColor() const {
  // TODO(tapted): Use style::GetColor().
  const ui::NativeTheme* theme = GetNativeTheme();
  DCHECK(theme);
  if (!GetEnabled())
    return theme->GetSystemColor(ui::NativeTheme::kColorId_LinkDisabled);

  if (requested_enabled_color_.has_value())
    return requested_enabled_color_.value();

  return GetNativeTheme()->GetSystemColor(
      pressed_ ? ui::NativeTheme::kColorId_LinkPressed
               : ui::NativeTheme::kColorId_LinkEnabled);
}

void Link::SetForceUnderline(bool force_underline) {
  if (force_underline_ == force_underline)
    return;

  force_underline_ = force_underline;
  RecalculateFont();
}

gfx::NativeCursor Link::GetCursor(const ui::MouseEvent& event) {
  if (!GetEnabled())
    return gfx::kNullCursor;
  return GetNativeHandCursor();
}

bool Link::GetCanProcessEventsWithinSubtree() const {
  // Links need to be able to accept events (e.g., clicking) even though
  // in general Labels do not.
  return View::GetCanProcessEventsWithinSubtree();
}

void Link::OnMouseEntered(const ui::MouseEvent& event) {
  RecalculateFont();
}

void Link::OnMouseExited(const ui::MouseEvent& event) {
  RecalculateFont();
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
      HitTestPoint(event.location()))
    OnClick(event);
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
  OnClick(event);
  return true;
}

void Link::OnGestureEvent(ui::GestureEvent* event) {
  if (!GetEnabled())
    return;

  if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
    SetPressed(true);
  } else if (event->type() == ui::ET_GESTURE_TAP) {
    OnClick(*event);
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
  requested_enabled_color_ = color;
  Label::SetEnabledColor(GetColor());
}

bool Link::IsSelectionSupported() const {
  return false;
}

void Link::SetPressed(bool pressed) {
  if (pressed_ != pressed) {
    pressed_ = pressed;
    Label::SetEnabledColor(GetColor());
    RecalculateFont();
    SchedulePaint();
  }
}

void Link::OnClick(const ui::Event& event) {
  RequestFocus();
  if (callback_)
    callback_.Run(event);
}

void Link::RecalculateFont() {
  const int style = font_list().GetFontStyle();
  const int intended_style =
      ((GetEnabled() && (HasFocus() || IsMouseHovered())) || force_underline_)
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
#if defined(OS_APPLE)
    SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
#else
    SetFocusBehavior(FocusBehavior::ALWAYS);
#endif
  }
}

BEGIN_METADATA(Link, Label)
ADD_READONLY_PROPERTY_METADATA(SkColor,
                               Color,
                               views::metadata::SkColorConverter)
END_METADATA

}  // namespace views
