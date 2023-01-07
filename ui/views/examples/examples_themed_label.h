// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_EXAMPLES_THEMED_LABEL_H_
#define UI_VIEWS_EXAMPLES_EXAMPLES_THEMED_LABEL_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/label.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

namespace views::examples {

class ThemedLabel : public Label {
 public:
  METADATA_HEADER(ThemedLabel);
  ThemedLabel();
  ThemedLabel(const ThemedLabel&) = delete;
  ThemedLabel& operator=(const ThemedLabel&) = delete;
  ~ThemedLabel() override;

  absl::optional<ui::ColorId> GetEnabledColorId() const;
  void SetEnabledColorId(absl::optional<ui::ColorId> enabled_color_id);

  // View:
  void OnThemeChanged() override;

 private:
  absl::optional<ui::ColorId> enabled_color_id_;
};

BEGIN_VIEW_BUILDER(, ThemedLabel, Label)
VIEW_BUILDER_PROPERTY(ui::ColorId, EnabledColorId)
END_VIEW_BUILDER

}  // namespace views::examples

DEFINE_VIEW_BUILDER(, views::examples::ThemedLabel)

#endif  // UI_VIEWS_EXAMPLES_EXAMPLES_THEMED_LABEL_H_
