// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_BORDER_H_
#define UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_BORDER_H_

#include <memory>

#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/painter.h"

namespace views {

// An empty Border with customizable insets used by a LabelButton.
class VIEWS_EXPORT LabelButtonBorder : public Border {
 public:
  LabelButtonBorder();

  LabelButtonBorder(const LabelButtonBorder&) = delete;
  LabelButtonBorder& operator=(const LabelButtonBorder&) = delete;

  ~LabelButtonBorder() override;

  void set_insets(const gfx::Insets& insets) { insets_ = insets; }

  // Returns true if |this| is able to paint for the given |focused| and |state|
  // values.
  virtual bool PaintsButtonState(bool focused, Button::ButtonState state);

  // Overridden from Border:
  void Paint(const View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetMinimumSize() const override;

 private:
  gfx::Insets insets_;
};

// A Border that paints a LabelButton's background frame using image assets.
class VIEWS_EXPORT LabelButtonAssetBorder : public LabelButtonBorder {
 public:
  LabelButtonAssetBorder();

  LabelButtonAssetBorder(const LabelButtonAssetBorder&) = delete;
  LabelButtonAssetBorder& operator=(const LabelButtonAssetBorder&) = delete;

  ~LabelButtonAssetBorder() override;

  // Returns the default insets.
  static gfx::Insets GetDefaultInsets();

  // Overridden from LabelButtonBorder:
  bool PaintsButtonState(bool focused, Button::ButtonState state) override;

  // Overridden from Border:
  void Paint(const View& view, gfx::Canvas* canvas) override;
  gfx::Size GetMinimumSize() const override;

  // Get or set the painter used for the specified |focused| button |state|.
  // LabelButtonAssetBorder takes and retains ownership of |painter|.
  Painter* GetPainter(bool focused, Button::ButtonState state);
  void SetPainter(bool focused,
                  Button::ButtonState state,
                  std::unique_ptr<Painter> painter);

 private:
  // The painters used for each unfocused or focused button state.
  std::array<std::array<std::unique_ptr<Painter>, Button::STATE_COUNT>, 2>
      painters_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_BORDER_H_
