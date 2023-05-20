// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_UI_CONTROLS_FACTORY_AURA_H_
#define UI_AURA_TEST_UI_CONTROLS_FACTORY_AURA_H_

#include "build/build_config.h"

namespace ui_controls {
class UIControlsAura;
}

namespace aura {
class WindowTreeHost;

namespace test {

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_WIN)
ui_controls::UIControlsAura* CreateUIControlsAura(WindowTreeHost* host);
#endif

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_UI_CONTROLS_FACTORY_AURA_H_
