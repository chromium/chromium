// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/in_process_context_factory.h"

#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "cc/test/pixel_test_output_surface.h"
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
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "services/viz/privileged/mojom/compositing/display_private.mojom.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test/direct_layer_tree_frame_sink.h"
#include "ui/compositor/test/in_process_context_provider.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/test/gl_surface_test_support.h"

#if defined(OS_APPLE)
#include "ui/accelerated_widget_mac/ca_transaction_observer.h"
#endif

#if !defined(GPU_SURFACE_HANDLE_IS_ACCELERATED_WINDOW)
#include "gpu/ipc/common/gpu_surface_tracker.h"
#endif

namespace ui {
namespace {

// This should not conflict with ids from RenderWidgetHostImpl or WindowService.
constexpr uint32_t kDefaultClientId = std::numeric_limits<uint32_t>::max() / 2;

// An OutputSurface implementation that directly draws and swaps to an actual
// GL surface.
class DirectOutputSurface : public viz::OutputSurface {
 public:
  explicit DirectOutputSurface(
      scoped_refptr<InProcessContextProvider> context_provider)
      : viz::OutputSurface(context_provider) {
    capabilities_.output_surface_origin =
        context_provider->ContextCapabilities().surface_origin;
  }

  ~DirectOutputSurface() override {}

  // viz::OutputSurface implementation.
  void BindToClient(viz::OutputSurfaceClient* client) override {
    client_ = client;
  }
  void EnsureBackbuffer() override {}
  void DiscardBackbuffer() override {}
  void BindFramebuffer() override {
    context_provider()->ContextGL()->BindFramebuffer(GL_FRAMEBUFFER, 0);
  }
  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               gfx::BufferFormat format,
               bool use_stencil) override {
    context_provider()->ContextGL()->ResizeCHROMIUM(
        size.width(), size.height(), device_scale_factor,
        color_space.AsGLColorSpace(), gfx::AlphaBitsForBufferFormat(format));
  }
  void SwapBuffers(viz::OutputSurfaceFrame frame) override {
    DCHECK(context_provider_.get());
    if (frame.sub_buffer_rect) {
      context_provider_->ContextSupport()->PartialSwapBuffers(
          *frame.sub_buffer_rect, 0 /* flags */, base::DoNothing(),
          base::DoNothing());
    } else {
      context_provider_->ContextSupport()->Swap(
          0 /* flags */, base::DoNothing(), base::DoNothing());
    }
    gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
    gpu::SyncToken sync_token;
    gl->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());

    context_provider_->ContextSupport()->SignalSyncToken(
        sync_token, base::BindOnce(&DirectOutputSurface::OnSwapBuffersComplete,
                                   weak_ptr_factory_.GetWeakPtr()));
  }
  uint32_t GetFramebufferCopyTextureFormat() override {
    auto* gl = static_cast<InProcessContextProvider*>(context_provider());
    return gl->GetCopyTextureInternalFormat();
  }
  bool IsDisplayedAsOverlayPlane() const override { return false; }
  unsigned GetOverlayTextureId() const override { return 0; }
  bool HasExternalStencilTest() const override { return false; }
  void ApplyExternalStencil() override {}
  unsigned UpdateGpuFence() override { return 0; }
  void SetUpdateVSyncParametersCallback(
      viz::UpdateVSyncParametersCallback callback) override {}
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override {}
  gfx::OverlayTransform GetDisplayTransform() override {
    return gfx::OVERLAY_TRANSFORM_NONE;
  }

 private:
  void OnSwapBuffersComplete() {
    // Metrics tracking in OutputSurfaceClient expects non-null SwapTimings
    // so we provide dummy values here.
    base::TimeTicks now = base::TimeTicks::Now();
    gfx::SwapTimings timings = {now, now};
    client_->DidReceiveSwapBuffersAck(timings);
    client_->DidReceivePresentationFeedback(gfx::PresentationFeedback());
  }

  viz::OutputSurfaceClient* client_ = nullptr;
  base::WeakPtrFactory<DirectOutputSurface> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DirectOutputSurface);
};

}  // namespace

// TODO(sgilhuly): This class is managed heavily by InProcessTransportFactory.
// Move some of the logic in here and simplify the interface.
class InProcessContextFactory::PerCompositorData
    : public viz::mojom::DisplayPrivate {
 public:
  // viz::mojom::DisplayPrivate implementation.
  void SetDisplayVisible(bool visible) override {
    display_->SetVisible(visible);
  }
  void Resize(const gfx::Size& size) override { display_->Resize(size); }
#if defined(OS_WIN)
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
    output_color_matrix_ = matrix.matrix();
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
  void ForceImmediateDrawAndSwapIfPossible() override {}
  void AddVSyncParameterObserver(
      mojo::PendingRemote<viz::mojom::VSyncParameterObserver> observer)
      override {}
#if defined(OS_ANDROID)
  void SetVSyncPaused(bool paused) override {}
  void UpdateRefreshRate(float refresh_rate) override {}
  void SetSupportedRefreshRates(
      const std::vector<float>& refresh_rates) override {}
  void PreserveChildSurfaceControls() override {}
#endif

  void SetDelegatedInkPointRenderer(
      mojo::PendingReceiver<viz::mojom::DelegatedInkPointRenderer> receiver)
      override {}

  void SetSurfaceHandle(gpu::SurfaceHandle surface_handle) {
    surface_handle_ = surface_handle;
  }
  void SetBeginFrameSource(
      std::unique_ptr<viz::BeginFrameSource> begin_frame_source) {
    begin_frame_source_ = std::move(begin_frame_source);
  }
  void SetDisplay(std::unique_ptr<viz::Display> display) {
    display_ = std::move(display);
  }

  void ResetDisplayOutputParameters() {
    output_color_matrix_.setIdentity();
    display_color_spaces_ = gfx::DisplayColorSpaces();
    vsync_timebase_ = base::TimeTicks();
    vsync_interval_ = base::TimeDelta();
  }

  gpu::SurfaceHandle surface_handle() { return surface_handle_; }
  viz::BeginFrameSource* begin_frame_source() {
    return begin_frame_source_.get();
  }
  viz::Display* display() { return display_.get(); }

  SkMatrix44 output_color_matrix() { return output_color_matrix_; }
  gfx::DisplayColorSpaces display_color_spaces() {
    return display_color_spaces_;
  }
  base::TimeTicks vsync_timebase() { return vsync_timebase_; }
  base::TimeDelta vsync_interval() { return vsync_interval_; }

 private:
  gpu::SurfaceHandle surface_handle_ = gpu::kNullSurfaceHandle;
  std::unique_ptr<viz::BeginFrameSource> begin_frame_source_;
  std::unique_ptr<viz::Display> display_;

  SkMatrix44 output_color_matrix_;
  gfx::DisplayColorSpaces display_color_spaces_;
  base::TimeTicks vsync_timebase_;
  base::TimeDelta vsync_interval_;
};

InProcessContextFactory::InProcessContextFactory(
    viz::HostFrameSinkManager* host_frame_sink_manager,
    viz::FrameSinkManagerImpl* frame_sink_manager)
    : InProcessContextFactory(host_frame_sink_manager,
                              frame_sink_manager,
                              features::IsUsingSkiaRenderer()) {}

InProcessContextFactory::InProcessContextFactory(
    viz::HostFrameSinkManager* host_frame_sink_manager,
    viz::FrameSinkManagerImpl* frame_sink_manager,
    bool use_skia_renderer)
    : frame_sink_id_allocator_(kDefaultClientId),
      use_test_surface_(true),
      disable_vsync_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableVsyncForTests)),
      host_frame_sink_manager_(host_frame_sink_manager),
      frame_sink_manager_(frame_sink_manager) {
  DCHECK(host_frame_sink_manager);
  DCHECK_NE(gl::GetGLImplementation(), gl::kGLImplementationNone)
      << "If running tests, ensure that main() is calling "
      << "gl::GLSurfaceTestSupport::InitializeOneOff()";
  if (use_skia_renderer)
    renderer_settings_.use_skia_renderer = true;
#if defined(OS_APPLE)
  renderer_settings_.release_overlay_resources_after_gpu_query = true;
  // Ensure that tests don't wait for frames that will never come.
  ui::CATransactionCoordinator::Get().DisableForTesting();
#endif
}

InProcessContextFactory::~InProcessContextFactory() {
  DCHECK(per_compositor_data_.empty());
}

void InProcessContextFactory::SetUseFastRefreshRateForTests() {
  refresh_rate_ = 200.0;
}

void InProcessContextFactory::CreateLayerTreeFrameSink(
    base::WeakPtr<Compositor> compositor) {
  // Try to reuse existing shared worker context provider.
  bool shared_worker_context_provider_lost = false;
  if (shared_worker_context_provider_) {
    // Note: If context is lost, delete reference after releasing the lock.
    base::AutoLock lock(*shared_worker_context_provider_->GetLock());
    if (shared_worker_context_provider_->ContextGL()
            ->GetGraphicsResetStatusKHR() != GL_NO_ERROR) {
      shared_worker_context_provider_lost = true;
    }
  }
  if (!shared_worker_context_provider_ || shared_worker_context_provider_lost) {
    constexpr bool support_locking = true;
    shared_worker_context_provider_ = InProcessContextProvider::CreateOffscreen(
        &gpu_memory_buffer_manager_, &image_factory_, support_locking);
    auto result = shared_worker_context_provider_->BindToCurrentThread();
    if (result != gpu::ContextResult::kSuccess)
      shared_worker_context_provider_ = nullptr;
  }

  gpu::ContextCreationAttribs attribs;
  attribs.alpha_size = 8;
  attribs.blue_size = 8;
  attribs.green_size = 8;
  attribs.red_size = 8;
  attribs.depth_size = 0;
  attribs.stencil_size = 0;
  attribs.samples = 0;
  attribs.sample_buffers = 0;
  attribs.fail_if_major_perf_caveat = false;
  attribs.bind_generates_resource = false;
  PerCompositorData* data = per_compositor_data_[compositor.get()].get();
  if (!data)
    data = CreatePerCompositorData(compositor.get());

  constexpr bool support_locking = false;
  scoped_refptr<InProcessContextProvider> context_provider =
      InProcessContextProvider::Create(attribs, &gpu_memory_buffer_manager_,
                                       &image_factory_, data->surface_handle(),
                                       "UICompositor", support_locking);

  auto context_result = context_provider->BindToCurrentThread();
  DCHECK_EQ(context_result, gpu::ContextResult::kSuccess);

  std::unique_ptr<viz::OutputSurface> display_output_surface;
  std::unique_ptr<viz::DisplayCompositorMemoryAndTaskController>
      display_dependency;

  if (renderer_settings_.use_skia_renderer) {
    auto skia_deps = std::make_unique<viz::SkiaOutputSurfaceDependencyImpl>(
        viz::TestGpuServiceHolder::GetInstance()->gpu_service(),
        gpu::kNullSurfaceHandle);
    display_dependency =
        std::make_unique<viz::DisplayCompositorMemoryAndTaskController>(
            std::move(skia_deps));
    display_output_surface = viz::SkiaOutputSurfaceImpl::Create(
        display_dependency.get(), renderer_settings_, &debug_settings_);
  } else if (use_test_surface_) {
    // The |context_provider| will contain an InProcessCommandBuffer, which will
    // make a gpu::GpuTaskSchedulerHelper if one is not provided.
    gfx::SurfaceOrigin surface_origin = gfx::SurfaceOrigin::kBottomLeft;
    display_output_surface = std::make_unique<cc::PixelTestOutputSurface>(
        context_provider, surface_origin);
  } else {
    // The |context_provider| will contain an InProcessCommandBuffer, which will
    // make a gpu::GpuTaskSchedulerHelper if one is not provided.
    display_output_surface =
        std::make_unique<DirectOutputSurface>(context_provider);
  }

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
        base::TimeDelta::FromMicroseconds(base::Time::kMicrosecondsPerSecond /
                                          refresh_rate_));
    begin_frame_source = std::make_unique<viz::DelayBasedBeginFrameSource>(
        std::move(time_source), viz::BeginFrameSource::kNotRestartableId);
  }
  auto scheduler = std::make_unique<viz::DisplayScheduler>(
      begin_frame_source.get(), compositor->task_runner().get(),
      display_output_surface->capabilities().max_frames_pending);

  data->SetDisplay(std::make_unique<viz::Display>(
      &shared_bitmap_manager_, renderer_settings_, &debug_settings_,
      compositor->frame_sink_id(), std::move(display_dependency),
      std::move(display_output_surface), std::move(overlay_processor),
      std::move(scheduler), compositor->task_runner()));
  frame_sink_manager_->RegisterBeginFrameSource(begin_frame_source.get(),
                                                compositor->frame_sink_id());
  // Note that we are careful not to destroy a prior |data->begin_frame_source|
  // until we have reset |data->display|.
  data->SetBeginFrameSource(std::move(begin_frame_source));

  auto layer_tree_frame_sink = std::make_unique<DirectLayerTreeFrameSink>(
      compositor->frame_sink_id(), frame_sink_manager_, data->display(),
      context_provider, shared_worker_context_provider_,
      compositor->task_runner(), &gpu_memory_buffer_manager_);
  compositor->SetLayerTreeFrameSink(std::move(layer_tree_frame_sink), data);

  data->Resize(compositor->size());
}

scoped_refptr<viz::ContextProvider>
InProcessContextFactory::SharedMainThreadContextProvider() {
  if (shared_main_thread_contexts_ &&
      shared_main_thread_contexts_->ContextGL()->GetGraphicsResetStatusKHR() ==
          GL_NO_ERROR)
    return shared_main_thread_contexts_;

  constexpr bool support_locking = false;
  shared_main_thread_contexts_ = InProcessContextProvider::CreateOffscreen(
      &gpu_memory_buffer_manager_, &image_factory_, support_locking);
  auto result = shared_main_thread_contexts_->BindToCurrentThread();
  if (result != gpu::ContextResult::kSuccess)
    shared_main_thread_contexts_.reset();

  return shared_main_thread_contexts_;
}

scoped_refptr<viz::RasterContextProvider>
InProcessContextFactory::SharedMainThreadRasterContextProvider() {
  SharedMainThreadContextProvider();
  DCHECK(!shared_main_thread_contexts_ ||
         shared_main_thread_contexts_->RasterInterface());
  return shared_main_thread_contexts_;
}

void InProcessContextFactory::RemoveCompositor(Compositor* compositor) {
  auto it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second.get();
  frame_sink_manager_->UnregisterBeginFrameSource(data->begin_frame_source());
  DCHECK(data);
#if !defined(GPU_SURFACE_HANDLE_IS_ACCELERATED_WINDOW)
  if (data->surface_handle())
    gpu::GpuSurfaceTracker::Get()->RemoveSurface(data->surface_handle());
#endif
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

SkMatrix44 InProcessContextFactory::GetOutputColorMatrix(
    Compositor* compositor) const {
  auto iter = per_compositor_data_.find(compositor);
  if (iter == per_compositor_data_.end())
    return SkMatrix44(SkMatrix44::kIdentity_Constructor);

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
#else
    gpu::GpuSurfaceTracker* tracker = gpu::GpuSurfaceTracker::Get();
    data->SetSurfaceHandle(tracker->AddSurfaceForNativeWidget(
        gpu::GpuSurfaceTracker::SurfaceRecord(
            widget
#if defined(OS_ANDROID)
            // We have to provide a surface too, but we don't have one.  For
            // now, we don't proide it, since nobody should ask anyway.
            // If we ever provide a valid surface here, then GpuSurfaceTracker
            // can be more strict about enforcing it.
            ,
            nullptr, false /* can_be_used_with_surface_control */
#endif
            )));
#endif
  }

  PerCompositorData* return_ptr = data.get();
  per_compositor_data_[compositor] = std::move(data);
  return return_ptr;
}

}  // namespace ui
