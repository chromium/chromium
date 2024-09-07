// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/i18n/icu_util.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/build_config.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "mojo/core/embedder/embedder.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_focus_client.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tree_host.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/base/hit_test.h"
#include "ui/base/ime/init/input_method_initializer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/test/in_process_context_factory.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(IS_WIN)
#include "ui/display/win/dpi.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace {

// Trivial WindowDelegate implementation that draws a colored background.
class DemoWindowDelegate : public aura::WindowDelegate {
 public:
  explicit DemoWindowDelegate(SkColor color) : color_(color) {}

  DemoWindowDelegate(const DemoWindowDelegate&) = delete;
  DemoWindowDelegate& operator=(const DemoWindowDelegate&) = delete;

  // Overridden from WindowDelegate:
  gfx::Size GetMinimumSize() const override { return gfx::Size(); }

  gfx::Size GetMaximumSize() const override { return gfx::Size(); }

  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override {
    window_bounds_ = new_bounds;
  }
  gfx::NativeCursor GetCursor(const gfx::Point& point) override {
    return gfx::NativeCursor{};
  }
  int GetNonClientComponent(const gfx::Point& point) const override {
    return HTCAPTION;
  }
  bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) override {
    return true;
  }
  bool CanFocus() override { return true; }
  void OnCaptureLost() override {}
  void OnPaint(const ui::PaintContext& context) override {
    ui::PaintRecorder recorder(context, window_bounds_.size());
    recorder.canvas()->DrawColor(color_, SkBlendMode::kSrc);
    gfx::Rect r;
    recorder.canvas()->GetClipBounds(&r);
    // Fill with a non-solid color so that the compositor will exercise its
    // texture upload path.
    while (!r.IsEmpty()) {
      r.Inset(2);
      recorder.canvas()->FillRect(r, color_, SkBlendMode::kXor);
    }
  }
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}
  void OnWindowDestroying(aura::Window* window) override {}
  void OnWindowDestroyed(aura::Window* window) override {}
  void OnWindowTargetVisibilityChanged(bool visible) override {}
  bool HasHitTestMask() const override { return false; }
  void GetHitTestMask(SkPath* mask) const override {}

 private:
  SkColor color_;
  gfx::Rect window_bounds_;
};

class DemoWindowParentingClient : public aura::client::WindowParentingClient {
 public:
  explicit DemoWindowParentingClient(aura::Window* window) : window_(window) {
    aura::client::SetWindowParentingClient(window_, this);
  }

  DemoWindowParentingClient(const DemoWindowParentingClient&) = delete;
  DemoWindowParentingClient& operator=(const DemoWindowParentingClient&) =
      delete;

  ~DemoWindowParentingClient() override {
    aura::client::SetWindowParentingClient(window_, nullptr);
  }

  // Overridden from aura::client::WindowParentingClient:
  aura::Window* GetDefaultParent(aura::Window* window,
                                 const gfx::Rect& bounds,
                                 const int64_t display_id) override {
    if (!capture_client_) {
      capture_client_ = std::make_unique<aura::client::DefaultCaptureClient>(
          window_->GetRootWindow());
    }
    return window_;
  }

 private:
  raw_ptr<aura::Window> window_;

  std::unique_ptr<aura::client::DefaultCaptureClient> capture_client_;
};

// Runs a base::RunLoop until receiving OnHostCloseRequested from |host|.
void RunRunLoopUntilOnHostCloseRequested(aura::WindowTreeHost* host) {
  class Observer : public aura::WindowTreeHostObserver {
   public:
    explicit Observer(base::OnceClosure quit_closure)
        : quit_closure_(std::move(quit_closure)) {}

    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    void OnHostCloseRequested(aura::WindowTreeHost* host) override {
      std::move(quit_closure_).Run();
    }

   private:
    base::OnceClosure quit_closure_;
  };

  base::RunLoop run_loop;
  Observer observer(run_loop.QuitClosure());
  host->AddObserver(&observer);
  run_loop.Run();
  host->RemoveObserver(&observer);
}

int DemoMain() {
#if BUILDFLAG(IS_OZONE)
  ui::OzonePlatform::InitParams params;
  params.single_process = true;
  ui::OzonePlatform::InitializeForUI(params);
  ui::OzonePlatform::InitializeForGPU(params);
#endif
  gl::init::InitializeGLOneOff(/*gpu_preference=*/gl::GpuPreference::kDefault);

#if BUILDFLAG(IS_WIN)
  display::win::SetDefaultDeviceScaleFactor(1.0f);
#endif

  // Create the task executor here before creating the root window.
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("demo");
  ui::InitializeInputMethodForTesting();

  // The ContextFactory must exist before any Compositors are created.
  viz::HostFrameSinkManager host_frame_sink_manager;
  viz::ServerSharedBitmapManager server_shared_bitmap_manager;
  viz::FrameSinkManagerImpl frame_sink_manager{
      viz::FrameSinkManagerImpl::InitParams(&server_shared_bitmap_manager)};
  host_frame_sink_manager.SetLocalManager(&frame_sink_manager);
  frame_sink_manager.SetLocalClient(&host_frame_sink_manager);
  auto context_factory = std::make_unique<ui::InProcessContextFactory>(
      &host_frame_sink_manager, &frame_sink_manager, /*output_to_window=*/true);

  base::PowerMonitor::GetInstance()->Initialize(
      std::make_unique<base::PowerMonitorDeviceSource>());

  std::unique_ptr<aura::Env> env = aura::Env::CreateInstance();
  env->set_context_factory(context_factory.get());
  std::unique_ptr<aura::TestScreen> test_screen(
      aura::TestScreen::Create(gfx::Size()));
  display::Screen::SetScreenInstance(test_screen.get());
  std::unique_ptr<aura::WindowTreeHost> host(
      test_screen->CreateHostForPrimaryDisplay());
  DemoWindowParentingClient window_parenting_client(host->window());
  aura::test::TestFocusClient focus_client(host->window());

  // Create a hierarchy of test windows.
  gfx::Rect window1_bounds(100, 100, 400, 400);
  DemoWindowDelegate window_delegate1(SK_ColorBLUE);
  aura::Window window1(&window_delegate1);
  window1.SetId(1);
  window1.Init(ui::LAYER_TEXTURED);
  window1.SetBounds(window1_bounds);
  window1.Show();
  aura::client::ParentWindowWithContext(&window1, host->window(), gfx::Rect(),
                                        display::kInvalidDisplayId);

  gfx::Rect window2_bounds(200, 200, 350, 350);
  DemoWindowDelegate window_delegate2(SK_ColorRED);
  aura::Window window2(&window_delegate2);
  window2.SetId(2);
  window2.Init(ui::LAYER_TEXTURED);
  window2.SetBounds(window2_bounds);
  window2.Show();
  aura::client::ParentWindowWithContext(&window2, host->window(), gfx::Rect(),
                                        display::kInvalidDisplayId);

  gfx::Rect window3_bounds(10, 10, 50, 50);
  DemoWindowDelegate window_delegate3(SK_ColorGREEN);
  aura::Window window3(&window_delegate3);
  window3.SetId(3);
  window3.Init(ui::LAYER_TEXTURED);
  window3.SetBounds(window3_bounds);
  window3.Show();
  window2.AddChild(&window3);

  host->Show();

  RunRunLoopUntilOnHostCloseRequested(host.get());

  // Input method shutdown needs to happen before thread cleanup while the
  // sequence manager is still valid.
  ui::ShutdownInputMethodForTesting();

  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  // Disabling Direct Composition works around the limitation that
  // InProcessContextFactory doesn't work with Direct Composition, causing the
  // window to not render. See http://crbug.com/936249.
  gl::SetGlWorkarounds(gl::GlWorkarounds{.disable_direct_composition = true});

  // The exit manager is in charge of calling the dtors of singleton objects.
  base::AtExitManager exit_manager;

  mojo::core::Init();

  base::i18n::InitializeICU();

  return DemoMain();
}
