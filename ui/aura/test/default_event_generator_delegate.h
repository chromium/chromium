// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_DEFAULT_EVENT_GENERATOR_DELEGATE_H_
#define UI_AURA_TEST_DEFAULT_EVENT_GENERATOR_DELEGATE_H_

#include "ui/aura/test/event_generator_delegate_aura.h"

namespace aura {
namespace test {

class DefaultEventGeneratorDelegate : public EventGeneratorDelegateAura {
 public:
  explicit DefaultEventGeneratorDelegate(gfx::NativeWindow root_window);
  ~DefaultEventGeneratorDelegate() override = default;

  // EventGeneratorDelegateAura:
  void SetTargetWindow(gfx::NativeWindow target_window) override;
  ui::EventTarget* GetTargetAt(const gfx::Point& location) override;
  client::ScreenPositionClient* GetScreenPositionClient(
      const Window* window) const override;

 private:
  Window* root_window_;

  DISALLOW_COPY_AND_ASSIGN(DefaultEventGeneratorDelegate);
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_DEFAULT_EVENT_GENERATOR_DELEGATE_H_
