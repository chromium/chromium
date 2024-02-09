// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_LINK_H_
#define UI_VIEWS_CONTROLS_LINK_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/controls/label.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/style/typography.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
//
// Link class
//
// A Link is a label subclass that looks like an HTML link. It has a
// controller which is notified when a click occurs.
//
////////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT Link : public Label {
  METADATA_HEADER(Link, Label)

 public:
  // A callback to be called when the link is clicked.  Closures are also
  // accepted; see below.
  using ClickedCallback = base::RepeatingCallback<void(const ui::Event& event)>;

  explicit Link(const std::u16string& title = std::u16string(),
                int text_context = style::CONTEXT_LABEL,
                int text_style = style::STYLE_LINK);

  Link(const Link&) = delete;
  Link& operator=(const Link&) = delete;

  ~Link() override;

  // Allow providing callbacks that expect either zero or one args, since many
  // callers don't care about the argument and can avoid adapter functions this
  // way.
  void SetCallback(base::RepeatingClosure callback) {
    // Adapt this closure to a ClickedCallback by discarding the extra arg.
    callback_ =
        base::BindRepeating([](base::RepeatingClosure closure,
                               const ui::Event& event) { closure.Run(); },
                            std::move(callback));
  }
  void SetCallback(ClickedCallback callback) {
    callback_ = std::move(callback);
  }

  SkColor GetColor() const;

  void SetForceUnderline(bool force_underline);
  bool GetForceUnderline() const;

  // Label:
  ui::Cursor GetCursor(const ui::MouseEvent& event) override;
  bool GetCanProcessEventsWithinSubtree() const override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override;
  void OnFocus() override;
  void OnBlur() override;
  void SetFontList(const gfx::FontList& font_list) override;
  void SetText(const std::u16string& text) override;
  void OnThemeChanged() override;
  void SetEnabledColor(SkColor color) override;
  bool IsSelectionSupported() const override;

 private:
  virtual void RecalculateFont();

  void SetPressed(bool pressed);

  void OnClick(const ui::Event& event);

  void ConfigureFocus();

  ClickedCallback callback_;

  // Whether the link is currently pressed.
  bool pressed_ = false;

  // The color when the link is neither pressed nor disabled.
  std::optional<SkColor> requested_enabled_color_;

  base::CallbackListSubscription enabled_changed_subscription_;

  // Whether the link text should use underline style regardless of enabled or
  // focused state.
  bool force_underline_ = true;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, Link, Label)
VIEW_BUILDER_OVERLOAD_METHOD(SetCallback, base::RepeatingClosure)
VIEW_BUILDER_OVERLOAD_METHOD(SetCallback, Link::ClickedCallback)
VIEW_BUILDER_PROPERTY(bool, ForceUnderline)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, Link)

#endif  // UI_VIEWS_CONTROLS_LINK_H_
