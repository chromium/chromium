// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/host/host_context_factory_private.h"

#include "base/command_line.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"
#include "components/viz/client/hit_test_data_provider_draw_quad.h"
#include "components/viz/common/features.h"
#include "components/viz/common/switches.h"
#include "components/viz/host/host_display_client.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/host/renderer_settings_creation.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager.mojom.h"
#include "services/viz/privileged/mojom/compositing/vsync_parameter_observer.mojom.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "ui/compositor/reflector.h"

#if defined(OS_WIN)
#include "ui/gfx/win/rendering_window_manager.h"
#endif

namespace ui {

namespace {

static const char* kBrowser = "Browser";

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
class HostDisplayClient : public viz::HostDisplayClient {
 public:
  explicit HostDisplayClient(ui::Compositor* compositor)
      : viz::HostDisplayClient(compositor->widget()), compositor_(compositor) {}
  ~HostDisplayClient() override = default;

  // viz::HostDisplayClient:
  void DidCompleteSwapWithNewSize(const gfx::Size& size) override {
    compositor_->OnCompleteSwapWithNewSize(size);
  }

 private:
  ui::Compositor* const compositor_;

  DISALLOW_COPY_AND_ASSIGN(HostDisplayClient);
};
#else
class HostDisplayClient : public viz::HostDisplayClient {
 public:
  explicit HostDisplayClient(ui::Compositor* compositor)
      : viz::HostDisplayClient(compositor->widget()) {}
  ~HostDisplayClient() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(HostDisplayClient);
};
#endif

}  // namespace

struct PendingBeginFrameArgs {
  PendingBeginFrameArgs(
      const viz::BeginFrameArgs& args,
      bool force,
      base::OnceCallback<void(const viz::BeginFrameAck&)> callback)
      : args(args), force(force), callback(std::move(callback)) {}

  viz::BeginFrameArgs args;
  bool force;
  base::OnceCallback<void(const viz::BeginFrameAck&)> callback;
};

HostContextFactoryPrivate::HostContextFactoryPrivate(
    uint32_t client_id,
    viz::HostFrameSinkManager* host_frame_sink_manager,
    scoped_refptr<base::SingleThreadTaskRunner> resize_task_runner)
    : frame_sink_id_allocator_(client_id),
      host_frame_sink_manager_(host_frame_sink_manager),
      resize_task_runner_(std::move(resize_task_runner)) {
  DCHECK(host_frame_sink_manager_);
}

HostContextFactoryPrivate::~HostContextFactoryPrivate() = default;

void HostContextFactoryPrivate::AddCompositor(Compositor* compositor) {
  compositor_data_map_.try_emplace(compositor);
}

void HostContextFactoryPrivate::ConfigureCompositor(
    Compositor* compositor,
    scoped_refptr<viz::ContextProvider> context_provider,
    scoped_refptr<viz::RasterContextProvider> worker_context_provider) {
  bool gpu_compositing =
      !is_gpu_compositing_disabled_ && !compositor->force_software_compositor();

#if defined(OS_WIN)
  gfx::RenderingWindowManager::GetInstance()->RegisterParent(
      compositor->widget());
#endif
  auto& compositor_data = compositor_data_map_[compositor];

  auto root_params = viz::mojom::RootCompositorFrameSinkParams::New();
  // Create interfaces for a root CompositorFrameSink.
  mojo::PendingAssociatedRemote<viz::mojom::CompositorFrameSink> sink_remote;
  root_params->compositor_frame_sink =
      sink_remote.InitWithNewEndpointAndPassReceiver();
  mojo::PendingReceiver<viz::mojom::CompositorFrameSinkClient> client_receiver =
      root_params->compositor_frame_sink_client
          .InitWithNewPipeAndPassReceiver();
  compositor_data.display_private.reset();
  root_params->display_private =
      compositor_data.display_private.BindNewEndpointAndPassReceiver();
  compositor_data.display_client =
      std::make_unique<HostDisplayClient>(compositor);
  root_params->display_client =
      compositor_data.display_client->GetBoundRemote(resize_task_runner_);

  if (compositor->use_external_begin_frame_control()) {
    root_params->external_begin_frame_controller =
        compositor_data.external_begin_frame_controller
            .BindNewEndpointAndPassReceiver();
  }

  root_params->frame_sink_id = compositor->frame_sink_id();
#if defined(GPU_SURFACE_HANDLE_IS_ACCELERATED_WINDOW)
  root_params->widget = compositor->widget();
#endif
  root_params->gpu_compositing = gpu_compositing;
  root_params->renderer_settings = viz::CreateRendererSettings();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableFrameRateLimit))
    root_params->disable_frame_rate_limit = true;

  // Connects the viz process end of CompositorFrameSink message pipes. The
  // browser compositor may request a new CompositorFrameSink on context loss,
  // which will destroy the existing CompositorFrameSink.
  GetHostFrameSinkManager()->CreateRootCompositorFrameSink(
      std::move(root_params));
  compositor_data.display_private->Resize(compositor->size());
  compositor_data.display_private->SetOutputIsSecure(
      compositor_data.output_is_secure);
  compositor_data.display_private->SetDisplayTransformHint(
      compositor->display_transform());

  // Create LayerTreeFrameSink with the browser end of CompositorFrameSink.
  cc::mojo_embedder::AsyncLayerTreeFrameSink::InitParams params;
  params.compositor_task_runner = compositor->task_runner();
  params.gpu_memory_buffer_manager =
      compositor->context_factory()->GetGpuMemoryBufferManager();
  params.pipes.compositor_frame_sink_associated_remote = std::move(sink_remote);
  params.pipes.client_receiver = std::move(client_receiver);
  if (!features::IsVizHitTestingSurfaceLayerEnabled()) {
    params.hit_test_data_provider =
        std::make_unique<viz::HitTestDataProviderDrawQuad>(
            false /* should_ask_for_child_region */,
            true /* root_accepts_events */);
  }
  params.client_name = kBrowser;
  compositor->SetLayerTreeFrameSink(
      std::make_unique<cc::mojo_embedder::AsyncLayerTreeFrameSink>(
          std::move(context_provider), std::move(worker_context_provider),
          &params));
  auto* args = compositor_data.pending_begin_frame_args.get();
  if (args && compositor->use_external_begin_frame_control()) {
    compositor_data.external_begin_frame_controller->IssueExternalBeginFrame(
        args->args, args->force, std::move(args->callback));
    compositor_data.pending_begin_frame_args.reset();
  }
}

void HostContextFactoryPrivate::UnconfigureCompositor(Compositor* compositor) {
#if defined(OS_WIN)
  gfx::RenderingWindowManager::GetInstance()->UnregisterParent(
      compositor->widget());
#endif

  compositor_data_map_.erase(compositor);
}

base::flat_set<Compositor*> HostContextFactoryPrivate::GetAllCompositors() {
  base::flat_set<Compositor*> all_compositors;
  all_compositors.reserve(compositor_data_map_.size());
  for (auto& pair : compositor_data_map_)
    all_compositors.insert(pair.first);
  return all_compositors;
}

std::unique_ptr<Reflector> HostContextFactoryPrivate::CreateReflector(
    Compositor* source,
    Layer* target) {
  // TODO(crbug.com/601869): Reflector needs to be rewritten for viz.
  NOTIMPLEMENTED();
  return nullptr;
}

void HostContextFactoryPrivate::RemoveReflector(Reflector* reflector) {
  // TODO(crbug.com/601869): Reflector needs to be rewritten for viz.
  NOTIMPLEMENTED();
}

viz::FrameSinkId HostContextFactoryPrivate::AllocateFrameSinkId() {
  return frame_sink_id_allocator_.NextFrameSinkId();
}

viz::HostFrameSinkManager*
HostContextFactoryPrivate::GetHostFrameSinkManager() {
  return host_frame_sink_manager_;
}

void HostContextFactoryPrivate::SetDisplayVisible(Compositor* compositor,
                                                  bool visible) {
  auto iter = compositor_data_map_.find(compositor);
  if (iter == compositor_data_map_.end() || !iter->second.display_private)
    return;
  iter->second.display_private->SetDisplayVisible(visible);
}

void HostContextFactoryPrivate::ResizeDisplay(Compositor* compositor,
                                              const gfx::Size& size) {
  auto iter = compositor_data_map_.find(compositor);
  if (iter == compositor_data_map_.end() || !iter->second.display_private)
    return;
  iter->second.display_private->Resize(size);
}

void HostContextFactoryPrivate::DisableSwapUntilResize(Compositor* compositor) {
  auto iter = compositor_data_map_.find(compositor);
  if (iter == compositor_data_map_.end() || !iter->second.display_private)
    return;
  {
    // Browser needs to block for Viz to receive and process this message.
    // Otherwise when we return from WM_WINDOWPOSCHANGING message handler and
    // receive a WM_WINDOWPOSCHANGED the resize is finalized and any swaps of
    // wrong size by Viz can cause the swapped content to get scaled.
    // TODO(crbug.com/859168): Investigate nonblocking ways for solving.
    TRACE_EVENT0("viz", "Blocked UI for DisableSwapUntilResize");
    mojo::SyncCallRestrictions::ScopedAllowSyncCall scoped_allow_sync_call;
    iter->second.display_private->DisableSwapUntilResize();
  }
}

void HostContextFactoryPrivate::SetDisplayColorMatrix(
    Compositor* compositor,
    const SkMatrix44& matrix) {
  auto iter = compositor_data_map_.find(compositor);
  if (iter == compositor_data_map_.end() || !iter->second.display_private)
    return;
  iter->second.display_private->SetDisplayColorMatrix(gfx::Transform(matrix));
}

void HostContextFactoryPrivate::SetDisplayColorSpace(
    Compositor* compositor,
    const gfx::ColorSpace& output_color_space,
    float sdr_white_level) {
  auto iter = compositor_data_map_.find(compositor);
  if (iter == compositor_data_map_.end() || !iter->second.display_private)
    return;
  iter->second.display_private->SetDisplayColorSpace(output_color_space,
                                                     sdr_white_level);
}

void HostContextFactoryPrivate::SetDisplayVSyncParameters(
    Compositor* compositor,
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  auto iter = compositor_data_map_.find(compositor);
  if (iter == compositor_data_map_.end() || !iter->second.display_private)
    return;
  iter->second.display_private->SetDisplayVSyncParameters(timebase, interval);
}

void HostContextFactoryPrivate::IssueExternalBeginFrame(
    Compositor* compositor,
    const viz::BeginFrameArgs& args,
    bool force,
    base::OnceCallback<void(const viz::BeginFrameAck&)> callback) {
  auto iter = compositor_data_map_.find(compositor);
  DCHECK(iter != compositor_data_map_.end() && iter->second.display_private);
  if (!iter->second.external_begin_frame_controller.is_bound()) {
    DCHECK(!iter->second.pending_begin_frame_args);
    iter->second.pending_begin_frame_args =
        std::make_unique<PendingBeginFrameArgs>(args, force,
                                                std::move(callback));
    return;
  }
  iter->second.external_begin_frame_controller->IssueExternalBeginFrame(
      args, force, std::move(callback));
}

void HostContextFactoryPrivate::SetOutputIsSecure(Compositor* compositor,
                                                  bool secure) {
  auto iter = compositor_data_map_.find(compositor);
  if (iter == compositor_data_map_.end())
    return;
  iter->second.output_is_secure = secure;

  if (iter->second.display_private)
    iter->second.display_private->SetOutputIsSecure(secure);
}

void HostContextFactoryPrivate::AddVSyncParameterObserver(
    Compositor* compositor,
    mojo::PendingRemote<viz::mojom::VSyncParameterObserver> observer) {
  auto iter = compositor_data_map_.find(compositor);
  if (iter == compositor_data_map_.end())
    return;

  if (iter->second.display_private) {
    iter->second.display_private->AddVSyncParameterObserver(
        std::move(observer));
  }
}

void HostContextFactoryPrivate::SetDisplayTransformHint(
    Compositor* compositor,
    gfx::OverlayTransform transform) {
  auto iter = compositor_data_map_.find(compositor);
  if (iter == compositor_data_map_.end())
    return;

  if (iter->second.display_private)
    iter->second.display_private->SetDisplayTransformHint(transform);
}

HostContextFactoryPrivate::CompositorData::CompositorData() = default;
HostContextFactoryPrivate::CompositorData::CompositorData(
    CompositorData&& other) = default;
HostContextFactoryPrivate::CompositorData::~CompositorData() = default;
HostContextFactoryPrivate::CompositorData&
HostContextFactoryPrivate::CompositorData::operator=(CompositorData&& other) =
    default;

}  // namespace ui
