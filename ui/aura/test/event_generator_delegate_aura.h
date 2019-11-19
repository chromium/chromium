// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_EVENT_GENERATOR_DELEGATE_AURA_H_
#define UI_AURA_TEST_EVENT_GENERATOR_DELEGATE_AURA_H_

#include "base/macros.h"
#include "ui/events/test/event_generator.h"

#include <memory>

namespace aura {
class Window;

namespace client {
class ScreenPositionClient;
}

namespace test {

// Implementation of ui::test::EventGeneratorDelegate for Aura.
class EventGeneratorDelegateAura : public ui::test::EventGeneratorDelegate {
 public:
  EventGeneratorDelegateAura();
  ~EventGeneratorDelegateAura() override;

  // Creates a new EventGeneratorDelegateAura.
  static std::unique_ptr<ui::test::EventGeneratorDelegate> Create(
      ui::test::EventGenerator* owner,
      gfx::NativeWindow root_window,
      gfx::NativeWindow window);

  // Returns the screen position client that determines the
  // coordinates used in EventGenerator. EventGenerator uses
  // root Window's coordinate if this returns NULL.
  virtual client::ScreenPositionClient* GetScreenPositionClient(
      const Window* window) const;

  // Overridden from ui::test::EventGeneratorDelegate:
  ui::EventSource* GetEventSource(ui::EventTarget* target) override;
  gfx::Point CenterOfTarget(const ui::EventTarget* target) const override;
  gfx::Point CenterOfWindow(gfx::NativeWindow window) const override;
  void ConvertPointFromTarget(const ui::EventTarget* target,
                              gfx::Point* point) const override;
  void ConvertPointToTarget(const ui::EventTarget* target,
                            gfx::Point* point) const override;
  void ConvertPointFromWindow(gfx::NativeWindow window,
                              gfx::Point* point) const override;
  void ConvertPointFromHost(const ui::EventTarget* hosted_target,
                            gfx::Point* point) const override;
  ui::EventDispatchDetails DispatchKeyEventToIME(ui::EventTarget* target,
                                                 ui::KeyEvent* event) override;

 private:
  gfx::Point CenterOfWindow(const Window* window) const;
  void ConvertPointFromWindow(const Window* window, gfx::Point* point) const;

  DISALLOW_COPY_AND_ASSIGN(EventGeneratorDelegateAura);
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_EVENT_GENERATOR_DELEGATE_AURA_H_
