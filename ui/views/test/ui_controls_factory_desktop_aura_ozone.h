// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_UI_CONTROLS_FACTORY_DESKTOP_AURA_OZONE_H_
#define UI_VIEWS_TEST_UI_CONTROLS_FACTORY_DESKTOP_AURA_OZONE_H_

namespace ui_controls {
class UIControlsAura;
}

namespace views::test {

ui_controls::UIControlsAura* CreateUIControlsDesktopAuraOzone();

}  // namespace views::test

#endif  // UI_VIEWS_TEST_UI_CONTROLS_FACTORY_DESKTOP_AURA_OZONE_H_
