// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/in_process_context_factory.h"

#include <limits>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "cc/trees/raster_context_provider_wrapper.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_sinks/delay_based_time_source.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/display/display_scheduler.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/overlay_processor_stub.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency_impl.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "components/viz/test/test_in_process_context_provider.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "services/viz/privileged/mojom/compositing/display_private.mojom.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test/direct_layer_tree_frame_sink.h"
#include "ui/display/display_switches.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/test/gl_surface_test_support.h"

#if BUILDFLAG(IS_MAC)
#include "ui/accelerated_widget_mac/ca_transaction_observer.h"
#endif

namespace ui {
namespace {

// This should not conflict with ids from RenderWidgetHostImpl or WindowService.
constexpr uint32_t kDefaultClientId = std::numeric_limits<uint32_t>::max() / 2;

class StandaloneBeginFrameObserver : public viz::BeginFrameObserverBase {
 public:
  StandaloneBeginFrameObserver() = default;
  StandaloneBeginFrameObserver(const StandaloneBeginFrameObserver&) = delete;
  StandaloneBeginFrameObserver& operator=(const StandaloneBeginFrameObserver&) =
      delete;
  ~StandaloneBeginFrameObserver() override { SetBeginFrameSource(nullptr); }

  // BeginFrameObserverBase:
  bool OnBeginFrameDerivedImpl(const viz::BeginFrameArgs& args) override {
    if (remote_observer_.is_bound()) {
      remote_observer_->OnStandaloneBeginFrame(args);
    }
    return true;
  }
  void OnBeginFrameSourcePausedChanged(bool paused) override {}
  bool IsRoot() const override { return true; }

  void SetBeginFrameSource(viz::BeginFrameSource* begin_frame_source) {
    TearDownObservation();
    begin_frame_source_ = begin_frame_source;
    SetUpObservation();
  }

  void SetStandaloneObserver(
      mojo::PendingRemote<viz::mojom::BeginFrameObserver> observer) {
    TearDownObservation();
    remote_observer_.reset();
    remote_observer_.Bind(std::move(observer));
    SetUpObservation();
  }

 private:
  void SetUpObservation() {
    if (begin_frame_source_ && remote_observer_.is_bound() &&
        !is_observing_begin_frame_source_) {
      is_observing_begin_frame_source_ = true;
      begin_frame_source_->AddObserver(this);
    }
  }

  void TearDownObservation() {
    if (!is_observing_begin_frame_source_) {
      return;
    }
    begin_frame_source_->RemoveObserver(this);
    begin_frame_source_ = nullptr;
    is_observing_begin_frame_source_ = false;
  }

  mojo::Remote<viz::mojom::BeginFrameObserver> remote_observer_;
  raw_ptr<viz::BeginFrameSource> begin_frame_source_ = nullptr;
  bool is_observing_begin_frame_source_ = false;
};

}  // namespace

// TODO(rivr): This class is managed heavily by InProcessTransportFactory.
// Move some of the logic in here and simplify the interface.
class InProcessContextFactory::PerCompositorData
    : public viz::mojom::DisplayPrivate {
 public:
  // viz::mojom::DisplayPrivate implementation.
  void SetDisplayVisible(bool visible) override {
    display_->SetVisible(visible);
  }
  void Resize(const gfx::Size& size) override { display_->Resize(size); }
#if BUILDFLAG(IS_WIN)
  bool DisableSwapUntilResize() override {
    display_->DisableSwapUntilResize(base::OnceClosure());
    return true;
  }
  void DisableSwapUntilResize(
      DisableSwapUntilResizeCallback callback) override {
    display_->DisableSwapUntilResize(std::move(callback));
  }
#endif
  void SetDisplayColorMatrix(const gfx::Transform& matrix) override {
    output_color_matrix_ = gfx::TransformToSkM44(matrix);
  }
  void SetDisplayColorSpaces(
      const gfx::DisplayColorSpaces& color_spaces) override {
    display_color_spaces_ = color_spaces;
  }
  void SetDisplayVSyncParameters(base::TimeTicks timebase,
                                 base::TimeDelta interval) override {
    vsync_timebase_ = timebase;
    vsync_interval_ = interval;
  }
  void SetOutputIsSecure(bool secure) override {}
#if BUILDFLAG(IS_MAC)
  void SetVSyncDisplayID(int64_t display_id) override {}
#endif
  void ForceImmediateDrawAndSwapIfPossible() override {}
  void AddVSyncParameterObserver(
      mojo::PendingRemote<viz::mojom::VSyncParameterObserver> observer)
      override {}
#if BUILDFLAG(IS_ANDROID)
  void SetVSyncPaused(bool paused) override {}
  void UpdateRefreshRate(float refresh_rate) override {}
  void PreserveChildSurfaceControls() override {}
  void SetSwapCompletionCallbackEnabled(bool enabled) override {}
#endif  // BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
  void SetSupportedRefreshRates(
      const std::vector<float>& refresh_rates) override {}
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
  void SetDelegatedInkPointRenderer(
      mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer> receiver)
      override {}
  void SetStandaloneBeginFrameObserver(
      mojo::PendingRemote<viz::mojom::BeginFrameObserver> observer) override {
    standalone_begin_frame_observer_.SetStandaloneObserver(std::move(observer));
  }

  void SetSurfaceHandle(gpu::SurfaceHandle surface_handle) {
    surface_handle_ = surface_handle;
  }
  void SetBeginFrameSource(
      std::unique_ptr<viz::BeginFrameSource> begin_frame_source) {
    begin_frame_source_ = std::move(begin_frame_source);
    standalone_begin_frame_observer_.SetBeginFrameSource(
        begin_frame_source_.get());
  }
  void SetDisplay(std::unique_ptr<viz::Display> display) {
    display_ = std::move(display);
  }
  void SetMaxVSyncAndVrr(std::optional<base::TimeDelta> max_vsync_interval,
                         display::VariableRefreshRateState vrr_state) override {
    max_vsync_interval_ = max_vsync_interval;
    vrr_state_ = vrr_state;
  }

  void ResetDisplayOutputParameters() {
    output_color_matrix_ = SkM44();
    display_color_spaces_ = gfx::DisplayColorSpaces();
    vsync_timebase_ = base::TimeTicks();
    vsync_interval_ = base::TimeDelta();
    max_vsync_interval_ = std::nullopt;
    vrr_state_ = display::VariableRefreshRateState::kVrrNotCapable;
  }

  void Bind(
      mojo::PendingAssociatedReceiver<viz::mojom::DisplayPrivate> remote) {
    receiver_.reset();
    receiver_.Bind(std::move(remote));
  }

  gpu::SurfaceHandle surface_handle() { return surface_handle_; }
  viz::BeginFrameSource* begin_frame_source() {
    return begin_frame_source_.get();
  }
  viz::Display* display() { return display_.get(); }

  SkM44 output_color_matrix() { return output_color_matrix_; }
  gfx::DisplayColorSpaces display_color_spaces() {
    return display_color_spaces_;
  }
  base::TimeTicks vsync_timebase() { return vsync_timebase_; }
  base::TimeDelta vsync_interval() { return vsync_interval_; }
  std::optional<base::TimeDelta> max_vsync_interval() const {
    return max_vsync_interval_;
  }
  display::VariableRefreshRateState vrr_state() const { return vrr_state_; }

 private:
  gpu::SurfaceHandle surface_handle_ = gpu::kNullSurfaceHandle;
  std::unique_ptr<viz::BeginFrameSource> begin_frame_source_;
  std::unique_ptr<viz::Display> display_;
  StandaloneBeginFrameObserver standalone_begin_frame_observer_;

  SkM44 output_color_matrix_;
  gfx::DisplayColorSpaces display_color_spaces_;
  base::TimeTicks vsync_timebase_;
  base::TimeDelta vsync_interval_;
  std::optional<base::TimeDelta> max_vsync_interval_;
  display::VariableRefreshRateState vrr_state_ =
      display::VariableRefreshRateState::kVrrNotCapable;

  mojo::AssociatedReceiver<viz::mojom::DisplayPrivate> receiver_{this};
};

InProcessContextFactory::InProcessContextFactory(
    viz::HostFrameSinkManager* host_frame_sink_manager,
    viz::FrameSinkManagerImpl* frame_sink_manager,
    bool output_to_window)
    : frame_sink_id_allocator_(kDefaultClientId),
      output_to_window_(output_to_window),
      disable_vsync_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableVsyncForTests)),
      host_frame_sink_manager_(host_frame_sink_manager),
      frame_sink_manager_(frame_sink_manager) {
  DCHECK(host_frame_sink_manager);
  DCHECK_NE(gl::GetGLImplementation(), gl::kGLImplementationNone)
      << "If running tests, ensure that main() is calling "
      << "gl::GLSurfaceTestSupport::InitializeOneOff()";
#if BUILDFLAG(IS_MAC)
  renderer_settings_.release_overlay_resources_after_gpu_query = true;
  // Ensure that tests don't wait for frames that will never come.
  ui::CATransactionCoordinator::Get().DisableForTesting();
#endif
}

InProcessContextFactory::~InProcessContextFactory() {
  DCHECK(per_compositor_data_.empty());
}

void InProcessContextFactory::SetRefreshRateForTests(double refresh_rate) {
  refresh_rate_ = refresh_rate;
}

void InProcessContextFactory::CreateLayerTreeFrameSink(
    base::WeakPtr<Compositor> compositor) {
  // Try to reuse existing shared worker context provider.
  bool shared_worker_context_provider_lost = false;
  if (shared_worker_context_provider_wrapper_) {
    // Note: If context is lost, delete reference after releasing the lock.
    const scoped_refptr<viz::RasterContextProvider>& worker_context =
        shared_worker_context_provider_wrapper_->GetContext();
    base::AutoLock lock(*worker_context->GetLock());
    if (worker_context->RasterInterface()->GetGraphicsResetStatusKHR() !=
        GL_NO_ERROR) {
      shared_worker_context_provider_lost = true;
    }
  }
  if (!shared_worker_context_provider_wrapper_ ||
      shared_worker_context_provider_lost) {
    auto shared_worker_context_provider =
        base::MakeRefCounted<viz::TestInProcessContextProvider>(
            viz::TestContextType::kGpuRaster, /*support_locking=*/true);
    auto result = shared_worker_context_provider->BindToCurrentSequence();
    if (result != gpu::ContextResult::kSuccess) {
      shared_worker_context_provider_wrapper_ = nullptr;
    } else {
      shared_worker_context_provider_wrapper_ =
          base::MakeRefCounted<cc::RasterContextProviderWrapper>(
              std::move(shared_worker_context_provider), nullptr,
              cc::ImageDecodeCacheUtils::GetWorkingSetBytesForImageDecode(
                  /*for_renderer=*/false));
    }
  }

  PerCompositorData* data = per_compositor_data_[compositor.get()].get();
  mojo::AssociatedRemote<viz::mojom::DisplayPrivate> display_private;
  if (!data)
    data = CreatePerCompositorData(compositor.get());
  data->Bind(display_private.BindNewEndpointAndPassDedicatedReceiver());

  auto skia_deps = std::make_unique<viz::SkiaOutputSurfaceDependencyImpl>(
      viz::TestGpuServiceHolder::GetInstance()->gpu_service(),
      output_to_window_ ? data->surface_handle() : gpu::kNullSurfaceHandle);
  auto display_dependency =
      std::make_unique<viz::DisplayCompositorMemoryAndTaskController>(
          std::move(skia_deps));
  std::unique_ptr<viz::OutputSurface> output_surface =
      viz::SkiaOutputSurfaceImpl::Create(display_dependency.get(),
                                         renderer_settings_, &debug_settings_);

  auto overlay_processor = std::make_unique<viz::OverlayProcessorStub>();

  std::unique_ptr<viz::BeginFrameSource> begin_frame_source;
  if (disable_vsync_) {
    begin_frame_source = std::make_unique<viz::BackToBackBeginFrameSource>(
        std::make_unique<viz::DelayBasedTimeSource>(
            compositor->task_runner().get()));
  } else {
    auto time_source = std::make_unique<viz::DelayBasedTimeSource>(
        compositor->task_runner().get());
    time_source->SetTimebaseAndInterval(
        base::TimeTicks(),
        base::Microseconds(base::Time::kMicrosecondsPerSecond / refresh_rate_));
    begin_frame_source = std::make_unique<viz::DelayBasedBeginFrameSource>(
        std::move(time_source), viz::BeginFrameSource::kNotRestartableId);
  }
  auto scheduler = std::make_unique<viz::DisplayScheduler>(
      begin_frame_source.get(), compositor->task_runner().get(),
      output_surface->capabilities().pending_swap_params,
      /*hint_session_factory=*/nullptr);

  data->SetDisplay(std::make_unique<viz::Display>(
      &shared_bitmap_manager_, &shared_image_manager_, &sync_point_manager_,
      &gpu_scheduler_, renderer_settings_, &debug_settings_,
      compositor->frame_sink_id(), std::move(display_dependency),
      std::move(output_surface), std::move(overlay_processor),
      std::move(scheduler), compositor->task_runner()));
  frame_sink_manager_->RegisterBeginFrameSource(begin_frame_source.get(),
                                                compositor->frame_sink_id());
  // Note that we are careful not to destroy a prior |data->begin_frame_source|
  // until we have reset |data->display|.
  data->SetBeginFrameSource(std::move(begin_frame_source));

  auto layer_tree_frame_sink = std::make_unique<DirectLayerTreeFrameSink>(
      compositor->frame_sink_id(), frame_sink_manager_, data->display(),
      SharedMainThreadRasterContextProvider(),
      shared_worker_context_provider_wrapper_, compositor->task_runner(),
      &gpu_memory_buffer_manager_);
  compositor->SetLayerTreeFrameSink(std::move(layer_tree_frame_sink),
                                    std::move(display_private));

  data->Resize(compositor->size());
}

scoped_refptr<viz::RasterContextProvider>
InProcessContextFactory::SharedMainThreadRasterContextProvider() {
  if (shared_main_thread_contexts_ &&
      shared_main_thread_contexts_->RasterInterface()
              ->GetGraphicsResetStatusKHR() == GL_NO_ERROR) {
    return shared_main_thread_contexts_;
  }

  shared_main_thread_contexts_ =
      base::MakeRefCounted<viz::TestInProcessContextProvider>(
          viz::TestContextType::kSoftwareRaster, /*support_locking=*/false);

  auto result = shared_main_thread_contexts_->BindToCurrentSequence();
  if (result != gpu::ContextResult::kSuccess) {
    shared_main_thread_contexts_.reset();
  }

  return shared_main_thread_contexts_;
}

void InProcessContextFactory::RemoveCompositor(Compositor* compositor) {
  auto it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second.get();
  frame_sink_manager_->UnregisterBeginFrameSource(data->begin_frame_source());
  DCHECK(data);
  per_compositor_data_.erase(it);
}

gpu::GpuMemoryBufferManager*
InProcessContextFactory::GetGpuMemoryBufferManager() {
  return &gpu_memory_buffer_manager_;
}

cc::TaskGraphRunner* InProcessContextFactory::GetTaskGraphRunner() {
  return &task_graph_runner_;
}

viz::FrameSinkId InProcessContextFactory::AllocateFrameSinkId() {
  return frame_sink_id_allocator_.NextFrameSinkId();
}

viz::SubtreeCaptureId InProcessContextFactory::AllocateSubtreeCaptureId() {
  return subtree_capture_id_allocator_.NextSubtreeCaptureId();
}

viz::HostFrameSinkManager* InProcessContextFactory::GetHostFrameSinkManager() {
  return host_frame_sink_manager_;
}

SkM44 InProcessContextFactory::GetOutputColorMatrix(
    Compositor* compositor) const {
  auto iter = per_compositor_data_.find(compositor);
  if (iter == per_compositor_data_.end())
    return SkM44();

  return iter->second->output_color_matrix();
}

gfx::DisplayColorSpaces InProcessContextFactory::GetDisplayColorSpaces(
    Compositor* compositor) const {
  auto iter = per_compositor_data_.find(compositor);
  if (iter == per_compositor_data_.end())
    return gfx::DisplayColorSpaces();
  return iter->second->display_color_spaces();
}

base::TimeTicks InProcessContextFactory::GetDisplayVSyncTimeBase(
    Compositor* compositor) const {
  auto iter = per_compositor_data_.find(compositor);
  if (iter == per_compositor_data_.end())
    return base::TimeTicks();
  return iter->second->vsync_timebase();
}

base::TimeDelta InProcessContextFactory::GetDisplayVSyncTimeInterval(
    Compositor* compositor) const {
  auto iter = per_compositor_data_.find(compositor);
  if (iter == per_compositor_data_.end())
    return viz::BeginFrameArgs::DefaultInterval();
  return iter->second->vsync_interval();
}

std::optional<base::TimeDelta> InProcessContextFactory::GetMaxVSyncInterval(
    Compositor* compositor) const {
  auto iter = per_compositor_data_.find(compositor);
  if (iter == per_compositor_data_.end()) {
    return std::nullopt;
  }
  return iter->second->max_vsync_interval();
}

display::VariableRefreshRateState InProcessContextFactory::GetVrrState(
    Compositor* compositor) const {
  auto iter = per_compositor_data_.find(compositor);
  if (iter == per_compositor_data_.end()) {
    return display::VariableRefreshRateState::kVrrNotCapable;
  }
  return iter->second->vrr_state();
}

void InProcessContextFactory::ResetDisplayOutputParameters(
    Compositor* compositor) {
  auto iter = per_compositor_data_.find(compositor);
  if (iter == per_compositor_data_.end())
    return;
  iter->second->ResetDisplayOutputParameters();
}

InProcessContextFactory::PerCompositorData*
InProcessContextFactory::CreatePerCompositorData(Compositor* compositor) {
  DCHECK(!per_compositor_data_[compositor]);

  gfx::AcceleratedWidget widget = compositor->widget();

  auto data = std::make_unique<PerCompositorData>();
  if (widget == gfx::kNullAcceleratedWidget) {
    data->SetSurfaceHandle(gpu::kNullSurfaceHandle);
  } else {
#if defined(GPU_SURFACE_HANDLE_IS_ACCELERATED_WINDOW)
    data->SetSurfaceHandle(widget);
#endif
  }

  PerCompositorData* return_ptr = data.get();
  per_compositor_data_[compositor] = std::move(data);
  return return_ptr;
}

}  // namespace ui
