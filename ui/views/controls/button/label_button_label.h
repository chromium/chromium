// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_LABEL_H_
#define UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_LABEL_H_

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/controls/label.h"
#include "ui/views/views_export.h"

namespace views {

// A Label subclass that can be disabled. This is only used internally for
// views::LabelButton.
class VIEWS_EXPORT LabelButtonLabel : public Label {
 public:
  LabelButtonLabel(const base::string16& text, int text_context);
  ~LabelButtonLabel() override;

  // Set an explicit disabled color. This will stop the Label responding to
  // changes in the native theme for disabled colors.
  void SetDisabledColor(SkColor color);

  // Label:
  void SetEnabledColor(SkColor color) override;

 protected:
  // Label:
  void OnThemeChanged() override;

 private:
  void OnEnabledChanged();
  void SetColorForEnableState();

  SkColor requested_disabled_color_ = SK_ColorRED;
  SkColor requested_enabled_color_ = SK_ColorRED;
  bool disabled_color_set_ = false;
  bool enabled_color_set_ = false;
  PropertyChangedSubscription enabled_changed_subscription_ =
      AddEnabledChangedCallback(
          base::BindRepeating(&LabelButtonLabel::OnEnabledChanged,
                              base::Unretained(this)));

  DISALLOW_COPY_AND_ASSIGN(LabelButtonLabel);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_LABEL_H_
