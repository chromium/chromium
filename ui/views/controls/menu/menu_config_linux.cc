// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

#include "ui/ozone/public/ozone_platform.h"

namespace views {

void MenuConfig::InitPlatform() {
  use_bubble_border = true;
}

}  // namespace views
