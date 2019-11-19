// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/aura_test_helper.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/env.h"
#include "ui/aura/input_state_lookup.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/aura/test/event_generator_delegate_aura.h"
#include "ui/aura/test/test_focus_client.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/test/test_window_parenting_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_targeter.h"
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/base/ime/init/input_method_initializer.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/screen.h"
#include "ui/wm/core/wm_state.h"

#if defined(OS_LINUX)
#include "ui/platform_window/common/platform_window_defaults.h"  // nogncheck
#endif

#if defined(OS_WIN)
#include "base/sequenced_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind_test_util.h"
#include "ui/aura/native_window_occlusion_tracker_win.h"
#endif

#if defined(USE_X11)
#include "ui/base/x/x11_util.h"  // nogncheck
#endif

namespace aura {
namespace test {
namespace {

AuraTestHelper* g_instance = nullptr;

}  // namespace

AuraTestHelper::AuraTestHelper() : AuraTestHelper(nullptr) {}

AuraTestHelper::AuraTestHelper(std::unique_ptr<Env> env)
    : env_(std::move(env)) {
  // Disable animations during tests.
  zero_duration_mode_ = std::make_unique<ui::ScopedAnimationDurationScaleMode>(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
#if defined(OS_LINUX)
  ui::test::EnableTestConfigForPlatformWindows();
#endif
}

AuraTestHelper::~AuraTestHelper() {
  CHECK(setup_called_) << "AuraTestHelper::SetUp() never called.";
  CHECK(teardown_called_) << "AuraTestHelper::TearDown() never called.";
}

// static
AuraTestHelper* AuraTestHelper::GetInstance() {
  return g_instance;
}

void AuraTestHelper::SetUp(ui::ContextFactory* context_factory,
                           ui::ContextFactoryPrivate* context_factory_private) {
  ui::test::EventGeneratorDelegate::SetFactoryFunction(
      base::BindRepeating(&EventGeneratorDelegateAura::Create));

  Env* env = GetEnv();

  setup_called_ = true;

  wm_state_ = std::make_unique<wm::WMState>();
  // Needs to be before creating WindowTreeClient.
  focus_client_ = std::make_unique<TestFocusClient>();
  capture_client_ = std::make_unique<client::DefaultCaptureClient>();

  if (!env) {
    // Some tests suites create Env globally rather than per test.
    env_ = Env::CreateInstance();
    env = env_.get();
  }

  EnvTestHelper env_helper(env);

  // Reset aura::Env to eliminate test dependency (https://crbug.com/586514).
  env_helper.ResetEnvForTesting();

  context_factory_to_restore_ = env->context_factory();
  context_factory_private_to_restore_ = env->context_factory_private();
  env->set_context_factory(context_factory);
  env->set_context_factory_private(context_factory_private);
  // Unit tests generally don't want to query the system, rather use the state
  // from RootWindow.
  env_helper.SetInputStateLookup(nullptr);
  env_helper.ResetEventState();

  ui::InitializeInputMethodForTesting();

  display::Screen* screen = display::Screen::GetScreen();
  gfx::Size host_size(screen ? screen->GetPrimaryDisplay().GetSizeInPixel()
                             : gfx::Size(800, 600));

  // This must be reset before creating TestScreen, which sets up the display
  // scale factor for this test iteration.
  display::Display::ResetForceDeviceScaleFactorForTesting();
  test_screen_.reset(TestScreen::Create(host_size));
  if (!screen)
    display::Screen::SetScreenInstance(test_screen_.get());
  host_.reset(test_screen_->CreateHostForPrimaryDisplay());
  host_->window()->SetEventTargeter(std::make_unique<WindowTargeter>());

  client::SetFocusClient(root_window(), focus_client_.get());
  client::SetCaptureClient(root_window(), capture_client());
  parenting_client_ =
      std::make_unique<TestWindowParentingClient>(root_window());

  root_window()->Show();
  // Ensure width != height so tests won't confuse them.
  host()->SetBoundsInPixels(gfx::Rect(host_size));

  g_instance = this;
}

void AuraTestHelper::TearDown() {
  g_instance = nullptr;
  teardown_called_ = true;
  parenting_client_.reset();
  client::SetFocusClient(root_window(), nullptr);
  client::SetCaptureClient(root_window(), nullptr);
  host_.reset();

  if (display::Screen::GetScreen() == test_screen_.get())
    display::Screen::SetScreenInstance(nullptr);
  test_screen_.reset();

  focus_client_.reset();
  capture_client_.reset();

  ui::ShutdownInputMethodForTesting();

  if (env_) {
    env_.reset();
  } else {
    Env* env = GetEnv();
    env->set_context_factory(context_factory_to_restore_);
    env->set_context_factory_private(context_factory_private_to_restore_);
  }
  wm_state_.reset();

  ui::test::EventGeneratorDelegate::SetFactoryFunction(
      ui::test::EventGeneratorDelegate::FactoryFunction());

#if defined(OS_WIN)
  // NativeWindowOcclusionTrackerWin is a global instance which creates its own
  // task runner. Since ThreadPool is destroyed together with TaskEnvironment,
  // NativeWindowOcclusionTrackerWin instance must be deleted as well and
  // recreated on demand in other test.
  DeleteNativeWindowOcclusionTrackerWin();
#endif
}

void AuraTestHelper::RunAllPendingInMessageLoop() {
  // TODO(jbates) crbug.com/134753 Find quitters of this RunLoop and have them
  //              use run_loop.QuitClosure().
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

client::CaptureClient* AuraTestHelper::capture_client() {
  return capture_client_.get();
}

Env* AuraTestHelper::GetEnv() {
  return env_ ? env_.get() : Env::HasInstance() ? Env::GetInstance() : nullptr;
}

#if defined(OS_WIN)
void AuraTestHelper::DeleteNativeWindowOcclusionTrackerWin() {
  NativeWindowOcclusionTrackerWin** global_ptr =
      NativeWindowOcclusionTrackerWin::GetInstanceForTesting();
  if (NativeWindowOcclusionTrackerWin* tracker = *global_ptr) {
    // WindowOcclusionCalculator must be deleted on its sequence. Wait until
    // it's deleted and then delete the tracker.
    base::WaitableEvent waitable_event;
    DCHECK(
        !tracker->update_occlusion_task_runner_->RunsTasksInCurrentSequence());
    tracker->update_occlusion_task_runner_->PostTask(
        FROM_HERE, base::BindLambdaForTesting([tracker, &waitable_event]() {
          if (tracker->occlusion_calculator_) {
            tracker->occlusion_calculator_->root_window_hwnds_occlusion_state_
                .clear();
            tracker->occlusion_calculator_->UnregisterEventHooks();
            tracker->occlusion_calculator_.reset();
          }
          waitable_event.Signal();
        }));
    waitable_event.Wait();
    delete tracker;
    *global_ptr = nullptr;
  }
}
#endif  // defined(OS_WIN)

}  // namespace test
}  // namespace aura
