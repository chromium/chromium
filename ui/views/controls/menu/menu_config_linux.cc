// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

#include "ui/base/ui_base_features.h"

namespace views {

void MenuConfig::Init() {
  arrow_to_edge_padding = 6;
  // Set Linux specific metrics for CR2023
  if (features::IsChromeRefresh2023()) {
    item_horizontal_border_padding = 12;
  }
}

}  // namespace views
