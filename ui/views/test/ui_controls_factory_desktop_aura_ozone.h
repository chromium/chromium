// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_UI_CONTROLS_FACTORY_DESKTOP_AURA_OZONE_H_
#define UI_VIEWS_TEST_UI_CONTROLS_FACTORY_DESKTOP_AURA_OZONE_H_

namespace ui_controls {
class UIControlsAura;
}

namespace views {
namespace test {

ui_controls::UIControlsAura* CreateUIControlsDesktopAuraOzone();

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_TEST_UI_CONTROLS_FACTORY_DESKTOP_AURA_OZONE_H_
