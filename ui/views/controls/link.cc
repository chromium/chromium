// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/link.h"

#include "build/build_config.h"

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/style/platform_style.h"

namespace views {

Link::Link(const std::u16string& title, int text_context, int text_style)
    : Label(title, text_context, text_style) {
  RecalculateFont();

  enabled_changed_subscription_ = AddEnabledChangedCallback(
      base::BindRepeating(&Link::RecalculateFont, base::Unretained(this)));

  GetViewAccessibility().SetRole(ax::mojom::Role::kLink);
  GetViewAccessibility().SetName(title);
  // Prevent invisible links from being announced by screen reader.
  GetViewAccessibility().SetIsIgnored(title.empty());

  // Label() indirectly calls SetText(), but at that point our virtual override
  // will not be reached.  Call it explicitly here to configure focus.
  SetText(GetText());

  views::FocusRing::Install(this);
}

Link::~Link() = default;

SkColor Link::GetColor() const {
  // TODO(crbug.com/40268779): Use TypographyProvider::GetColorId().
  const ui::ColorProvider* color_provider = GetColorProvider();
  DCHECK(color_provider);
  if (!GetEnabled())
    return color_provider->GetColor(ui::kColorLinkForegroundDisabled);

  if (requested_enabled_color_.has_value())
    return requested_enabled_color_.value();

  if (GetTextContext() == style::CONTEXT_BUBBLE_FOOTER) {
    return color_provider->GetColor(
        pressed_ ? ui::kColorLinkForegroundPressedOnBubbleFooter
                 : ui::kColorLinkForegroundOnBubbleFooter);
  }

  return color_provider->GetColor(pressed_ ? ui::kColorLinkForegroundPressed
                                           : ui::kColorLinkForeground);
}

void Link::SetForceUnderline(bool force_underline) {
  if (force_underline_ == force_underline)
    return;

  force_underline_ = force_underline;
  RecalculateFont();
}

bool Link::GetForceUnderline() const {
  return force_underline_;
}

ui::Cursor Link::GetCursor(const ui::MouseEvent& event) {
  if (!GetEnabled())
    return ui::Cursor();
  return ui::mojom::CursorType::kHand;
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

  if (event->type() == ui::EventType::kGestureTapDown) {
    SetPressed(true);
  } else if (event->type() == ui::EventType::kGestureTap) {
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

void Link::SetText(const std::u16string& text) {
  Label::SetText(text);
  // Prevent invisible links from being announced by screen reader.
  GetViewAccessibility().SetIsIgnored(text.empty());
  ConfigureFocus();
}

void Link::OnThemeChanged() {
  Label::OnThemeChanged();
  Label::SetEnabledColor(GetColor());
}

void Link::SetEnabledColor(SkColor color) {
  requested_enabled_color_ = color;
  if (GetWidget())
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
#if BUILDFLAG(IS_MAC)
    SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
#else
    SetFocusBehavior(FocusBehavior::ALWAYS);
#endif
  }
}

BEGIN_METADATA(Link)
ADD_READONLY_PROPERTY_METADATA(SkColor, Color, ui::metadata::SkColorConverter)
ADD_PROPERTY_METADATA(bool, ForceUnderline)
END_METADATA

}  // namespace views
