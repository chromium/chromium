// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

namespace views {

void MenuConfig::Init() {}

void MenuConfig::InitPlatformCR2023() {
  context_menu_font_list = font_list;
}

}  // namespace views
