// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_KEYBOARD_LAYOUT_ENGINE_H_
#define UI_OZONE_PLATFORM_X11_X11_KEYBOARD_LAYOUT_ENGINE_H_

#include <cstdint>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event_observer.h"
#include "ui/gfx/x/xkb.h"

namespace ui {

// Provides live X11 keyboard layout map lookups across configured XKB groups
// (ordered with the active group first), while keeping `Lookup()` stubbed for
// synthetic `KeyEvent` layout resolution (see crbug.com/41472924).
//
// The active XKB group and the number of groups are tracked from XKB
// `StateNotify`, `ControlsNotify`, `MapNotify` and `NewKeyboardNotify` events.
class X11KeyboardLayoutEngine : public StubKeyboardLayoutEngine,
                                public x11::EventObserver {
 public:
  X11KeyboardLayoutEngine();
  X11KeyboardLayoutEngine(const X11KeyboardLayoutEngine&) = delete;
  X11KeyboardLayoutEngine& operator=(const X11KeyboardLayoutEngine&) = delete;
  ~X11KeyboardLayoutEngine() override;

  // KeyboardLayoutEngine:
  uint32_t GetKeyboardLayoutCount() const override;
  DomKey LookupForLayout(DomCode dom_code,
                         uint32_t layout_index) const override;

  // x11::EventObserver:
  void OnEvent(const x11::Event& event) override;

 private:
  // Asynchronously refreshes `active_group_` and `num_groups_`.
  void FetchGroupState();
  void OnGetState(x11::Xkb::GetStateResponse response);
  void OnGetControls(x11::Xkb::GetControlsResponse response);

  // Null when there is no usable X11 connection with XKB (e.g. `--headless`
  // without `$DISPLAY`); the stub single-layout behavior is used then.
  const raw_ptr<x11::Connection> connection_;

  uint8_t active_group_ = 0;
  uint8_t num_groups_ = 1;

  base::ScopedObservation<x11::Connection, x11::EventObserver>
      connection_observation_{this};

  base::WeakPtrFactory<X11KeyboardLayoutEngine> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_KEYBOARD_LAYOUT_ENGINE_H_
