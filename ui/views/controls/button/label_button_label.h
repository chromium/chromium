// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_LABEL_H_
#define UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_LABEL_H_

#include "base/bind.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/controls/label.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/views_export.h"

namespace views {

namespace internal {

// A Label subclass that can be disabled. This is only used internally for
// views::LabelButton.
class VIEWS_EXPORT LabelButtonLabel : public Label {
 public:
  METADATA_HEADER(LabelButtonLabel);
  LabelButtonLabel(const base::string16& text, int text_context);
  LabelButtonLabel(const LabelButtonLabel&) = delete;
  LabelButtonLabel& operator=(const LabelButtonLabel&) = delete;
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

  base::Optional<SkColor> requested_disabled_color_;
  base::Optional<SkColor> requested_enabled_color_;
  base::CallbackListSubscription enabled_changed_subscription_ =
      AddEnabledChangedCallback(
          base::BindRepeating(&LabelButtonLabel::OnEnabledChanged,
                              base::Unretained(this)));
};

}  // namespace internal

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_LABEL_H_
