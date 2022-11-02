// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_UI_CONTROLS_FACTORY_AURA_H_
#define UI_AURA_TEST_UI_CONTROLS_FACTORY_AURA_H_

#include "build/build_config.h"
#include "ui/base/test/ui_controls_aura.h"

namespace aura {
class WindowTreeHost;

namespace test {

ui_controls::UIControlsAura* CreateUIControlsAura(WindowTreeHost* host);

#if BUILDFLAG(IS_OZONE)
// Callback from Window Service with the result of posting an event. |result|
// is true if event successfully processed and |closure| is an optional closure
// to run when done (used in client code to wait for ack).
void OnWindowServiceProcessedEvent(base::OnceClosure closure, bool result);
#endif

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_UI_CONTROLS_FACTORY_AURA_H_
