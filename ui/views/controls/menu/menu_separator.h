// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_SEPARATOR_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_SEPARATOR_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace views {

class VIEWS_EXPORT MenuSeparator : public View {
 public:
  METADATA_HEADER(MenuSeparator);

  explicit MenuSeparator(ui::MenuSeparatorType type) : type_(type) {}

  // View overrides.
  void OnPaint(gfx::Canvas* canvas) override;
  gfx::Size CalculatePreferredSize() const override;

 private:
  // The type of the separator.
  const ui::MenuSeparatorType type_;

  DISALLOW_COPY_AND_ASSIGN(MenuSeparator);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_SEPARATOR_H_
