// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_LABEL_H_
#define UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_LABEL_H_

#include <string>

#include "base/bind.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/controls/label.h"
#include "ui/views/views_export.h"

namespace views::internal {

// A Label subclass that can be disabled. This is only used internally for
// views::LabelButton.
class VIEWS_EXPORT LabelButtonLabel : public Label {
 public:
  METADATA_HEADER(LabelButtonLabel);
  LabelButtonLabel(const std::u16string& text, int text_context);
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

  absl::optional<SkColor> requested_disabled_color_;
  absl::optional<SkColor> requested_enabled_color_;
  base::CallbackListSubscription enabled_changed_subscription_ =
      AddEnabledChangedCallback(
          base::BindRepeating(&LabelButtonLabel::OnEnabledChanged,
                              base::Unretained(this)));
};

}  // namespace views::internal

#endif  // UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_LABEL_H_
