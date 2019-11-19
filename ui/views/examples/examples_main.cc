// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/i18n/icu_util.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "mojo/core/embedder/embedder.h"
#include "ui/base/ime/init/input_method_initializer.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/compositor/test/in_process_context_factory.h"
#include "ui/display/screen.h"
#include "ui/gfx/font_util.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/views/buildflags.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/test/desktop_test_views_delegate.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#include "ui/wm/core/wm_state.h"
#endif

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#endif

#if defined(OS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#endif

#if defined(USE_X11)
#include "ui/gfx/x/x11_connection.h"  // nogncheck
#endif

base::LazyInstance<base::TestDiscardableMemoryAllocator>::DestructorAtExit
    g_discardable_memory_allocator = LAZY_INSTANCE_INITIALIZER;

int main(int argc, char** argv) {
#if defined(OS_WIN)
  ui::ScopedOleInitializer ole_initializer;
#endif

  base::CommandLine::Init(argc, argv);

  // Disabling Direct Composition works around the limitation that
  // InProcessContextFactory doesn't work with Direct Composition, causing the
  // window to not render. See http://crbug.com/936249.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableDirectComposition);

  base::AtExitManager at_exit;

  mojo::core::Init();

#if defined(USE_X11)
  // This demo uses InProcessContextFactory which uses X on a separate Gpu
  // thread.
  gfx::InitializeThreadedX11();
#endif

  gl::init::InitializeGLOneOff();

  // The use of base::test::TaskEnvironment below relies on the timeout
  // values from TestTimeouts. This ensures they're properly initialized.
  TestTimeouts::Initialize();

  // The ContextFactory must exist before any Compositors are created.
  viz::HostFrameSinkManager host_frame_sink_manager;
  viz::ServerSharedBitmapManager shared_bitmap_manager;
  viz::FrameSinkManagerImpl frame_sink_manager(&shared_bitmap_manager);
  host_frame_sink_manager.SetLocalManager(&frame_sink_manager);
  frame_sink_manager.SetLocalClient(&host_frame_sink_manager);
  auto context_factory = std::make_unique<ui::InProcessContextFactory>(
      &host_frame_sink_manager, &frame_sink_manager);
  context_factory->set_use_test_surface(false);

  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::UI);

  base::i18n::InitializeICU();

  ui::RegisterPathProvider();

  base::FilePath ui_test_pak_path;
  CHECK(base::PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
  ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);

  base::DiscardableMemoryAllocator::SetInstance(
      g_discardable_memory_allocator.Pointer());

  base::PowerMonitor::Initialize(
      std::make_unique<base::PowerMonitorDeviceSource>());

  gfx::InitializeFonts();

#if defined(USE_AURA)
  std::unique_ptr<aura::Env> env = aura::Env::CreateInstance();
  aura::Env::GetInstance()->set_context_factory(context_factory.get());
  aura::Env::GetInstance()->set_context_factory_private(context_factory.get());
#endif
  ui::InitializeInputMethodForTesting();
  ui::MaterialDesignController::Initialize();

  {
    views::DesktopTestViewsDelegate views_delegate;
#if defined(USE_AURA)
    wm::WMState wm_state;
#endif
#if BUILDFLAG(ENABLE_DESKTOP_AURA)
    std::unique_ptr<display::Screen> desktop_screen(
        views::CreateDesktopScreen());
    display::Screen::SetScreenInstance(desktop_screen.get());
#endif

    // This app isn't a test and shouldn't timeout.
    base::RunLoop::ScopedDisableRunTimeoutForTest disable_timeout;

    base::RunLoop run_loop;
    views::examples::ShowExamplesWindow(run_loop.QuitClosure());

    run_loop.Run();

    ui::ResourceBundle::CleanupSharedInstance();
  }

  ui::ShutdownInputMethod();

#if defined(USE_AURA)
  env.reset();
#endif

  return 0;
}
