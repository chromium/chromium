// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_LABEL_H_
#define UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_LABEL_H_

#include <optional>
#include <string_view>

#include "base/functional/bind.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_variant.h"
#include "ui/views/controls/label.h"
#include "ui/views/views_export.h"

namespace views::internal {

// A Label subclass that can be disabled. This is only used internally for
// views::LabelButton.
class VIEWS_EXPORT LabelButtonLabel : public Label {
  METADATA_HEADER(LabelButtonLabel, Label)

 public:
  LabelButtonLabel(std::u16string_view text, int text_context);

  LabelButtonLabel(const LabelButtonLabel&) = delete;
  LabelButtonLabel& operator=(const LabelButtonLabel&) = delete;

  ~LabelButtonLabel() override;

  // Set an explicit disabled color. This will stop the Label responding to
  // changes in the native theme for disabled colors.
  void SetDisabledColor(ui::ColorVariant color);
  std::optional<ui::ColorVariant> GetDisabledColor() const;

  // Label:
  void SetEnabledColor(ui::ColorVariant color) override;
  std::optional<ui::ColorVariant> GetEnabledColor() const;

 protected:
  // Label:
  void OnThemeChanged() override;

 private:
  void OnEnabledChanged();
  void SetColorForEnableState();

  std::optional<ui::ColorVariant> requested_enabled_color_;
  std::optional<ui::ColorVariant> requested_disabled_color_;

  base::CallbackListSubscription enabled_changed_subscription_ =
      AddEnabledChangedCallback(
          base::BindRepeating(&LabelButtonLabel::OnEnabledChanged,
                              base::Unretained(this)));
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, LabelButtonLabel, Label)
VIEW_BUILDER_PROPERTY(std::optional<ui::ColorVariant>, EnabledColor)
VIEW_BUILDER_PROPERTY(std::optional<ui::ColorVariant>, DisabledColor)
END_VIEW_BUILDER

}  // namespace views::internal

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, internal::LabelButtonLabel)

#endif  // UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_LABEL_H_
