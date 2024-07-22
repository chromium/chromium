// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/examples_main_proc.h"

#include <memory>
#include <string>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/i18n/icu_util.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "base/run_loop.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "mojo/core/embedder/embedder.h"
#include "ui/accessibility/platform/ax_platform_for_test.h"
#include "ui/base/ime/init/input_method_initializer.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/color/color_provider_manager.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/test/in_process_context_factory.h"
#include "ui/compositor/test/test_context_factories.h"
#include "ui/display/screen.h"
#include "ui/gfx/font_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/views/buildflags.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/examples/examples_color_mixer.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/test/desktop_test_views_delegate.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#include "ui/wm/core/wm_state.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/views/examples/examples_views_delegate_chromeos.h"
#endif

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#include "ui/views/examples/examples_skia_gold_pixel_diff.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace views::examples {

base::LazyInstance<base::TestDiscardableMemoryAllocator>::DestructorAtExit
    g_discardable_memory_allocator = LAZY_INSTANCE_INITIALIZER;

bool g_initialized_once = false;

ExamplesExitCode ExamplesMainProc(bool under_test, ExampleVector examples) {
#if BUILDFLAG(IS_WIN)
  ui::ScopedOleInitializer ole_initializer;
#endif

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (CheckCommandLineUsage())
    return ExamplesExitCode::kSucceeded;

  ui::AXPlatformForTest ax_platform;

  // Disabling Direct Composition works around the limitation that
  // InProcessContextFactory doesn't work with Direct Composition, causing the
  // window to not render. See http://crbug.com/936249.
  gl::SetGlWorkarounds(gl::GlWorkarounds{.disable_direct_composition = true});

  base::FeatureList::InitInstance(
      command_line->GetSwitchValueASCII(switches::kEnableFeatures),
      command_line->GetSwitchValueASCII(switches::kDisableFeatures));

  if (under_test)
    command_line->AppendSwitch(switches::kEnablePixelOutputInTests);

#if BUILDFLAG(IS_OZONE)
  ui::OzonePlatform::InitParams params;
  params.single_process = true;
  ui::OzonePlatform::InitializeForGPU(params);
#endif

  // ExamplesMainProc can be called multiple times in a test suite.
  // These methods should only be initialized once.
  if (!g_initialized_once) {
    mojo::core::Init();

    gl::init::InitializeGLOneOff(
        /*gpu_preference=*/gl::GpuPreference::kDefault);

    base::i18n::InitializeICU();

    ui::RegisterPathProvider();

    base::DiscardableMemoryAllocator::SetInstance(
        g_discardable_memory_allocator.Pointer());

    gfx::InitializeFonts();

    g_initialized_once = true;
  }

  // Viz depends on the task environment to correctly tear down.
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::UI);

  // The ContextFactory must exist before any Compositors are created.
  auto context_factories =
      std::make_unique<ui::TestContextFactories>(under_test,
                                                 /*output_to_window=*/true);

  base::FilePath ui_test_pak_path;
  CHECK(base::PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
  ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);

  base::FilePath views_examples_resources_pak_path;
  CHECK(base::PathService::Get(base::DIR_ASSETS,
                               &views_examples_resources_pak_path));
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      views_examples_resources_pak_path.AppendASCII(
          "views_examples_resources.pak"),
      ui::k100Percent);

  ui::ColorProviderManager::Get().AppendColorProviderInitializer(
      base::BindRepeating(&AddExamplesColorMixers));

#if defined(USE_AURA)
  std::unique_ptr<aura::Env> env = aura::Env::CreateInstance();
  aura::Env::GetInstance()->set_context_factory(
      context_factories->GetContextFactory());
#endif
  ui::InitializeInputMethodForTesting();

  ExamplesExitCode compare_result = ExamplesExitCode::kSucceeded;

  {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    ExamplesViewsDelegateChromeOS views_delegate;
#else  // BUILDFLAG(IS_CHROMEOS_ASH)
    views::DesktopTestViewsDelegate views_delegate;
#if BUILDFLAG(IS_MAC)
    views_delegate.set_context_factory(context_factories->GetContextFactory());
#endif
#if defined(USE_AURA)
    wm::WMState wm_state;
#endif
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_MAC)
    display::ScopedNativeScreen desktop_screen;
#elif BUILDFLAG(ENABLE_DESKTOP_AURA)
    std::unique_ptr<display::Screen> desktop_screen =
        views::CreateDesktopScreen();
#endif

    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);

#if BUILDFLAG(IS_WIN)
    ExamplesSkiaGoldPixelDiff pixel_diff;
    views::AnyWidgetObserver widget_observer{
        views::test::AnyWidgetTestPasskey()};

    // If this app isn't a test, it shouldn't timeout.
    auto disable_timeout =
        std::make_unique<base::test::ScopedDisableRunLoopTimeout>();

    if (under_test) {
      pixel_diff.Init("ViewsExamples");
      widget_observer.set_shown_callback(
          base::BindRepeating(&ExamplesSkiaGoldPixelDiff::OnExamplesWindowShown,
                              base::Unretained(&pixel_diff)));
      // Enable the timeout since we're not running in a test.
      disable_timeout.reset();
    }
#else
    base::test::ScopedDisableRunLoopTimeout disable_timeout;
#endif

    if (examples.empty()) {
      views::examples::ShowExamplesWindow(run_loop.QuitClosure());
    } else {
      views::examples::ShowExamplesWindow(run_loop.QuitClosure(),
                                          std::move(examples));
    }

    run_loop.Run();

#if BUILDFLAG(IS_WIN)
    compare_result = pixel_diff.get_result();
#endif

    ui::ResourceBundle::CleanupSharedInstance();
  }

  ui::ShutdownInputMethod();

#if defined(USE_AURA)
  env.reset();
#endif

  return compare_result;
}

}  // namespace views::examples
