// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/aura_test_helper.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/aura/client/cursor_shape_client.h"
#include "ui/aura/client/default_capture_client.h"
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
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/base/ime/init/input_method_initializer.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/test_context_factories.h"
#include "ui/display/screen.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/wm/core/cursor_loader.h"
#include "ui/wm/core/default_activation_client.h"
#include "ui/wm/core/default_screen_position_client.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "ui/platform_window/common/platform_window_defaults.h"  // nogncheck
#endif

#if BUILDFLAG(IS_WIN)
#include "base/task/sequenced_task_runner.h"
#include "ui/aura/native_window_occlusion_tracker_win.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/events/ozone/events_ozone.h"
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include "ui/platform_window/fuchsia/initialize_presenter_api_view.h"
#endif

namespace aura {
namespace test {
namespace {

AuraTestHelper* g_instance = nullptr;

}  // namespace

AuraTestHelper::AuraTestHelper(ui::ContextFactory* context_factory) {
  DCHECK(!g_instance);
  g_instance = this;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  ui::test::EnableTestConfigForPlatformWindows();
#endif

#if BUILDFLAG(IS_OZONE) && BUILDFLAG(IS_CHROMEOS_ASH) && \
    !BUILDFLAG(IS_CHROMEOS_DEVICE)
  ui::DisableNativeUiEventDispatchForTest();
#endif

#if BUILDFLAG(IS_FUCHSIA)
  ui::fuchsia::IgnorePresentCallsForTest();
#endif

  ui::InitializeInputMethodForTesting();

  ui::test::EventGeneratorDelegate::SetFactoryFunction(
      base::BindRepeating(&EventGeneratorDelegateAura::Create));

  zero_duration_mode_ = std::make_unique<ui::ScopedAnimationDurationScaleMode>(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  // Some tests suites create Env globally.
  if (Env::HasInstance())
    context_factory_to_restore_ = Env::GetInstance()->context_factory();
  else
    env_ = Env::CreateInstance();
  Env* env = GetEnv();
  CHECK(env) << "No Aura env is set - confirm your test system is set up to "
                "display graphics";

  if (!context_factory) {
    context_factories_ = std::make_unique<ui::TestContextFactories>(false);
    context_factory = context_factories_->GetContextFactory();
  }
  env->set_context_factory(context_factory);

  // Reset aura::Env to eliminate test dependency (https://crbug.com/586514).
  EnvTestHelper env_helper(env);
  // Unit tests generally don't want to query the system, rather use the state
  // from RootWindow.
  env_helper.SetInputStateLookup(nullptr);
  env_helper.ResetEventState();

  // This must be reset before creating TestScreen, which sets up the display
  // scale factor for this test iteration.
  display::Display::ResetForceDeviceScaleFactorForTesting();

  auto* platform_event_source = ui::PlatformEventSource::GetInstance();
  if (platform_event_source) {
    // The previous test (if any) may have left the Wayland event source in
    // "watching" state even though its message pump was already destroyed.
    // Reset its state now so that when the current test creates the
    // WindowTreeHost, Wayland event processing can restart in the new message
    // pump.
    platform_event_source->ResetStateForTesting();
  }
}

AuraTestHelper::~AuraTestHelper() {
  if (g_instance)
    TearDown();
}

// static
AuraTestHelper* AuraTestHelper::GetInstance() {
  return g_instance;
}

void AuraTestHelper::SetUp() {
  display::Screen* screen = display::Screen::GetScreen();
  gfx::Size host_size(screen ? screen->GetPrimaryDisplay().GetSizeInPixel()
                             : kDefaultHostSize);
  test_screen_.reset(TestScreen::Create(host_size));
  // TODO(pkasting): Seems like we should either always set the screen instance,
  // or not create the screen/host if the test already has one; it doesn't make
  // a lot of sense to me to potentially have multiple screens/hosts/etc. alive
  // and be interacting with both depending on what accessors you use.
  if (!screen)
    display::Screen::SetScreenInstance(test_screen_.get());

  host_.reset(test_screen_->CreateHostForPrimaryDisplay());
  host_->window()->SetEventTargeter(std::make_unique<WindowTargeter>());
  host_->SetBoundsInPixels(gfx::Rect(host_size));

  Window* root_window = GetContext();
  new wm::DefaultActivationClient(root_window);  // Manages own lifetime.
  focus_client_ = std::make_unique<TestFocusClient>(root_window);
  capture_client_ = std::make_unique<client::DefaultCaptureClient>(root_window);
  parenting_client_ = std::make_unique<TestWindowParentingClient>(root_window);
  screen_position_client_ =
      std::make_unique<wm::DefaultScreenPositionClient>(root_window);
  cursor_shape_client_ = std::make_unique<wm::CursorLoader>();
  client::SetCursorShapeClient(cursor_shape_client_.get());

  root_window->Show();
}

void AuraTestHelper::TearDown() {
  g_instance = nullptr;

  if (!env_)
    Env::GetInstance()->set_context_factory(context_factory_to_restore_);

  ui::test::EventGeneratorDelegate::SetFactoryFunction(
      ui::test::EventGeneratorDelegate::FactoryFunction());

  ui::ShutdownInputMethodForTesting();

  // Destroy all owned objects to prevent tests from depending on their state
  // after this returns.
  client::SetCursorShapeClient(nullptr);
  cursor_shape_client_.reset();
  screen_position_client_.reset();
  parenting_client_.reset();
  capture_client_.reset();
  focus_client_.reset();
  host_.reset();

  if (test_screen_ && (display::Screen::GetScreen() == GetTestScreen())) {
    display::Screen::SetScreenInstance(nullptr);
  }
  test_screen_.reset();

  context_factories_.reset();
  env_.reset();
  zero_duration_mode_.reset();
  wm_state_.reset();

#if BUILDFLAG(IS_WIN)
  // TODO(pkasting): This code doesn't really belong here.
  // NativeWindowOcclusionTrackerWin is created on demand by various tests, must
  // be torn down before the TaskEnvironment (which our owner is responsible
  // for), and must be torn down after all Windows (so, after e.g. |host_|).
  // Ideally, some specific class would create it and manage its lifetime,
  // guaranteeing the above.
  NativeWindowOcclusionTrackerWin::DeleteInstanceForTesting();
#endif
}

void AuraTestHelper::RunAllPendingInMessageLoop() {
  // TODO(jbates) crbug.com/134753 Find quitters of this RunLoop and have them
  //              use run_loop.QuitClosure().
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

Window* AuraTestHelper::GetContext() {
  return host_ ? host_->window() : nullptr;
}

WindowTreeHost* AuraTestHelper::GetHost() {
  return host_.get();
}

TestScreen* AuraTestHelper::GetTestScreen() {
  return test_screen_.get();
}

client::FocusClient* AuraTestHelper::GetFocusClient() {
  return focus_client_.get();
}

client::CaptureClient* AuraTestHelper::GetCaptureClient() {
  return capture_client_.get();
}

constexpr gfx::Size AuraTestHelper::kDefaultHostSize;

Env* AuraTestHelper::GetEnv() {
  if (env_)
    return env_.get();
  return Env::HasInstance() ? Env::GetInstance() : nullptr;
}

ui::ContextFactory* AuraTestHelper::GetContextFactory() {
  return GetEnv()->context_factory();
}

}  // namespace test
}  // namespace aura
