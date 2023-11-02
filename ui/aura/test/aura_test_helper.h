// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_AURA_TEST_HELPER_H_
#define UI_AURA_TEST_AURA_TEST_HELPER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/wm/core/wm_state.h"

namespace ui {
class ContextFactory;
class ScopedAnimationDurationScaleMode;
class TestContextFactories;
}

namespace aura {
class Env;
class TestScreen;
class Window;
class WindowTreeHost;

namespace client {
class CaptureClient;
class CursorShapeClient;
class DefaultCaptureClient;
class FocusClient;
class ScreenPositionClient;
}

namespace test {
class TestWindowParentingClient;

// A helper class owned by tests that does common initialization required for
// Aura use. This class creates a root window with clients and other objects
// that are necessary to run test on Aura.
class AuraTestHelper {
 public:
  // Instantiates/destroys an AuraTestHelper. This can happen in a
  // single-threaded phase without a backing task environment, and must not
  // create one lest the caller wish to do so.
  explicit AuraTestHelper(ui::ContextFactory* context_factory = nullptr);

  AuraTestHelper(const AuraTestHelper&) = delete;
  AuraTestHelper& operator=(const AuraTestHelper&) = delete;

  virtual ~AuraTestHelper();

  // Returns the current AuraTestHelper, or nullptr if it's not alive.
  static AuraTestHelper* GetInstance();

  // Creates and initializes (shows and sizes) the RootWindow for use in tests.
  // This implementation does not create a task environment, but subclasses may
  // choose to do so.
  virtual void SetUp();

  // Destroys the window, Env, and most other objects.  This will be called
  // automatically on destruction if it is not called manually earlier.
  virtual void TearDown();

  // Flushes message loop.
  void RunAllPendingInMessageLoop();

  virtual Window* GetContext();
  virtual WindowTreeHost* GetHost();
  virtual TestScreen* GetTestScreen();
  virtual client::FocusClient* GetFocusClient();
  virtual client::CaptureClient* GetCaptureClient();

  static constexpr gfx::Size kDefaultHostSize{800, 600};

  Env* GetEnv();

 protected:
  // May only be called between SetUp() and TearDown().
  ui::ContextFactory* GetContextFactory();

 private:
  std::unique_ptr<wm::WMState> wm_state_ = std::make_unique<wm::WMState>();
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> zero_duration_mode_;
  std::unique_ptr<Env> env_;
  raw_ptr<ui::ContextFactory> context_factory_to_restore_ = nullptr;
  std::unique_ptr<ui::TestContextFactories> context_factories_;
  std::unique_ptr<TestScreen> test_screen_;
  std::unique_ptr<WindowTreeHost> host_;
  std::unique_ptr<client::FocusClient> focus_client_;
  std::unique_ptr<client::DefaultCaptureClient> capture_client_;
  std::unique_ptr<TestWindowParentingClient> parenting_client_;
  std::unique_ptr<client::ScreenPositionClient> screen_position_client_;
  std::unique_ptr<client::CursorShapeClient> cursor_shape_client_;
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_AURA_TEST_HELPER_H_
