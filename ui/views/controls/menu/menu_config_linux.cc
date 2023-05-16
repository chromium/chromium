// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

#include "ui/base/ui_base_features.h"

namespace views {

void MenuConfig::Init() {
  if (!features::IsChromeRefresh2023()) {
    arrow_to_edge_padding = 6;
  }
}

}  // namespace views
