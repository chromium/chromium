// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_LINK_H_
#define UI_VIEWS_CONTROLS_LINK_H_

#include <string>

#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/controls/label.h"
#include "ui/views/style/typography.h"

namespace views {

class LinkListener;

////////////////////////////////////////////////////////////////////////////////
//
// Link class
//
// A Link is a label subclass that looks like an HTML link. It has a
// controller which is notified when a click occurs.
//
////////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT Link : public Label {
 public:
  METADATA_HEADER(Link);

  // The padding for the focus ring border when rendering a focused Link with
  // FocusStyle::kRing.
  static constexpr gfx::Insets kFocusBorderPadding = gfx::Insets(1);

  // How the Link is styled when focused.
  enum class FocusStyle {
    kUnderline,  // An underline style is added to the text only when focused.
    kRing,       // A focus ring is drawn around the View.
  };

  explicit Link(const base::string16& title,
                int text_context = style::CONTEXT_LABEL,
                int text_style = style::STYLE_LINK);
  ~Link() override;

  // Returns the default FocusStyle for a views::Link. Calling SetUnderline()
  // may change it: E.g. SetUnderline(true) forces FocusStyle::kRing.
  static FocusStyle GetDefaultFocusStyle();

  // Returns the current FocusStyle of this Link.
  FocusStyle GetFocusStyle() const;

  const LinkListener* listener() { return listener_; }
  void set_listener(LinkListener* listener) { listener_ = listener; }

  SkColor GetColor() const;

  // Label:
  void PaintFocusRing(gfx::Canvas* canvas) const override;
  gfx::Insets GetInsets() const override;
  gfx::NativeCursor GetCursor(const ui::MouseEvent& event) override;
  bool CanProcessEventsWithinSubtree() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnFocus() override;
  void OnBlur() override;
  void SetFontList(const gfx::FontList& font_list) override;
  void SetText(const base::string16& text) override;
  void OnThemeChanged() override;
  void SetEnabledColor(SkColor color) override;
  bool IsSelectionSupported() const override;

  bool GetUnderline() const;
  // TODO(estade): almost all the places that call this pass false. With
  // Harmony, false is already the default so those callsites can be removed.
  // TODO(tapted): Then remove all callsites when client code sets a correct
  // typography style and derives this from style::GetFont(STYLE_LINK).
  void SetUnderline(bool underline);

 private:
  void Init();

  void SetPressed(bool pressed);

  void RecalculateFont();

  void ConfigureFocus();

  LinkListener* listener_;

  // Whether the link should be underlined when enabled.
  bool underline_;

  // Whether the link is currently pressed.
  bool pressed_;

  // The color when the link is neither pressed nor disabled.
  SkColor requested_enabled_color_;
  bool requested_enabled_color_set_;

  PropertyChangedSubscription enabled_changed_subscription_;

  DISALLOW_COPY_AND_ASSIGN(Link);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_LINK_H_
