// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_SEPARATOR_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_SEPARATOR_H_

#include <optional>

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/color/color_id.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace views {

class VIEWS_EXPORT MenuSeparator : public View {
  METADATA_HEADER(MenuSeparator, View)

 public:
  explicit MenuSeparator(
      ui::MenuSeparatorType type = ui::MenuSeparatorType::NORMAL_SEPARATOR,
      std::optional<ui::ColorId> color_id = std::nullopt);
  MenuSeparator(const MenuSeparator&) = delete;
  MenuSeparator& operator=(const MenuSeparator&) = delete;

  // View overrides.
  void OnPaint(gfx::Canvas* canvas) override;
  gfx::Size CalculatePreferredSize(
      const SizeBounds& /*available_size*/) const override;

  ui::MenuSeparatorType GetType() const;
  void SetType(ui::MenuSeparatorType type);

  std::optional<ui::ColorId> GetColorId() const;
  void SetColorId(std::optional<ui::ColorId> color_id);

 private:
  // The type of the separator.
  ui::MenuSeparatorType type_;

  // Optional custom color ID for the separator.
  std::optional<ui::ColorId> color_id_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_SEPARATOR_H_
