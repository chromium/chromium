// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/examples_themed_label.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"

namespace views::examples {

ThemedLabel::ThemedLabel() = default;

ThemedLabel::~ThemedLabel() = default;

std::optional<ui::ColorId> ThemedLabel::GetEnabledColorId() const {
  return enabled_color_id_;
}

void ThemedLabel::SetEnabledColorId(
    std::optional<ui::ColorId> enabled_color_id) {
  if (enabled_color_id == enabled_color_id_)
    return;
  enabled_color_id_ = enabled_color_id;
  OnPropertyChanged(&enabled_color_id_, kPropertyEffectsPaint);
}

// View:
void ThemedLabel::OnThemeChanged() {
  Label::OnThemeChanged();
  if (enabled_color_id_)
    SetEnabledColor(GetColorProvider()->GetColor(enabled_color_id_.value()));
}

BEGIN_METADATA(ThemedLabel)
ADD_PROPERTY_METADATA(std::optional<ui::ColorId>, EnabledColorId)
END_METADATA

}  // namespace views::examples
