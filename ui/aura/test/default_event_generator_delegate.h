// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_DEFAULT_EVENT_GENERATOR_DELEGATE_H_
#define UI_AURA_TEST_DEFAULT_EVENT_GENERATOR_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "ui/aura/test/event_generator_delegate_aura.h"

namespace aura {
namespace test {

class DefaultEventGeneratorDelegate : public EventGeneratorDelegateAura {
 public:
  explicit DefaultEventGeneratorDelegate(gfx::NativeWindow root_window);

  DefaultEventGeneratorDelegate(const DefaultEventGeneratorDelegate&) = delete;
  DefaultEventGeneratorDelegate& operator=(
      const DefaultEventGeneratorDelegate&) = delete;

  ~DefaultEventGeneratorDelegate() override = default;

  // EventGeneratorDelegateAura:
  void SetTargetWindow(gfx::NativeWindow target_window) override;
  ui::EventTarget* GetTargetAt(const gfx::Point& location) override;
  client::ScreenPositionClient* GetScreenPositionClient(
      const Window* window) const override;

 private:
  raw_ptr<Window, AcrossTasksDanglingUntriaged> root_window_;
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_DEFAULT_EVENT_GENERATOR_DELEGATE_H_
