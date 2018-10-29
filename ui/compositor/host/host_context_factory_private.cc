// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/host/host_context_factory_private.h"

#include "base/command_line.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"
#include "components/viz/client/hit_test_data_provider_draw_quad.h"
#include "components/viz/client/local_surface_id_provider.h"
#include "components/viz/common/switches.h"
#include "components/viz/host/host_display_client.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/host/renderer_settings_creation.h"
#include "services/viz/privileged/interfaces/compositing/frame_sink_manager.mojom.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom.h"
#include "ui/compositor/host/external_begin_frame_controller_client_impl.h"
#include "ui/compositor/reflector.h"

#if defined(OS_WIN)
#include "ui/gfx/win/rendering_window_manager.h"
#endif

namespace ui {

namespace {
static const char* kBrowser = "Browser";
}  // namespace

HostContextFactoryPrivate::HostContextFactoryPrivate(
    uint32_t client_id,
    viz::HostFrameSinkManager* host_frame_sink_manager,
    scoped_refptr<base::SingleThreadTaskRunner> resize_task_runner)
    : frame_sink_id_allocator_(client_id),
      host_frame_sink_manager_(host_frame_sink_manager),
      renderer_settings_(viz::CreateRendererSettings()),
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
  viz::mojom::CompositorFrameSinkAssociatedPtrInfo sink_info;
  root_params->compositor_frame_sink = mojo::MakeRequest(&sink_info);
  viz::mojom::CompositorFrameSinkClientRequest client_request =
      mojo::MakeRequest(&root_params->compositor_frame_sink_client);
  root_params->display_private =
      mojo::MakeRequest(&compositor_data.display_private);
  compositor_data.display_client =
      std::make_unique<viz::HostDisplayClient>(compositor->widget());
  root_params->display_client =
      compositor_data.display_client->GetBoundPtr(resize_task_runner_)
          .PassInterface();

  // Initialize ExternalBeginFrameController client if enabled.
  compositor_data.external_begin_frame_controller_client.reset();
  if (compositor->external_begin_frames_enabled()) {
    compositor_data.external_begin_frame_controller_client =
        std::make_unique<ExternalBeginFrameControllerClientImpl>(compositor);
    root_params->external_begin_frame_controller =
        compositor_data.external_begin_frame_controller_client
            ->GetControllerRequest();
    root_params->external_begin_frame_controller_client =
        compositor_data.external_begin_frame_controller_client->GetBoundPtr()
            .PassInterface();
  }

  root_params->frame_sink_id = compositor->frame_sink_id();
#if defined(GPU_SURFACE_HANDLE_IS_ACCELERATED_WINDOW)
  root_params->widget = compositor->widget();
#endif
  root_params->gpu_compositing = gpu_compositing;
  root_params->renderer_settings = renderer_settings_;

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

  // Create LayerTreeFrameSink with the browser end of CompositorFrameSink.
  cc::mojo_embedder::AsyncLayerTreeFrameSink::InitParams params;
  params.compositor_task_runner = compositor->task_runner();
  params.gpu_memory_buffer_manager =
      compositor->context_factory()->GetGpuMemoryBufferManager();
  params.pipes.compositor_frame_sink_associated_info = std::move(sink_info);
  params.pipes.client_request = std::move(client_request);
  params.local_surface_id_provider =
      std::make_unique<viz::DefaultLocalSurfaceIdProvider>();
  params.enable_surface_synchronization = true;
  params.hit_test_data_provider =
      std::make_unique<viz::HitTestDataProviderDrawQuad>(
          false /* should_ask_for_child_region */,
          true /* root_accepts_events */);
  params.client_name = kBrowser;
  compositor->SetLayerTreeFrameSink(
      std::make_unique<cc::mojo_embedder::AsyncLayerTreeFrameSink>(
          std::move(context_provider), std::move(worker_context_provider),
          &params));
}

void HostContextFactoryPrivate::UnconfigureCompositor(Compositor* compositor) {
#if defined(OS_WIN)
  gfx::RenderingWindowManager::GetInstance()->UnregisterParent(
      compositor->widget());
#endif

  compositor_data_map_.erase(compositor);
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
    const gfx::ColorSpace& blending_color_space,
    const gfx::ColorSpace& output_color_space) {
  auto iter = compositor_data_map_.find(compositor);
  if (iter == compositor_data_map_.end() || !iter->second.display_private)
    return;
  iter->second.display_private->SetDisplayColorSpace(blending_color_space,
                                                     output_color_space);
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
    const viz::BeginFrameArgs& args) {
  auto iter = compositor_data_map_.find(compositor);
  if (iter == compositor_data_map_.end() || !iter->second.display_private)
    return;

  DCHECK(iter->second.external_begin_frame_controller_client);
  iter->second.external_begin_frame_controller_client->GetController()
      ->IssueExternalBeginFrame(args);
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

viz::FrameSinkManagerImpl* HostContextFactoryPrivate::GetFrameSinkManager() {
  // When running with viz there is no FrameSinkManagerImpl in the browser
  // process. FrameSinkManagerImpl runs in the GPU process instead. Anything in
  // the browser process that relies FrameSinkManagerImpl or SurfaceManager
  // internal state needs to change. See https://crbug.com/787097 and
  // https://crbug.com/760181 for more context.
  NOTREACHED();
  return nullptr;
}

base::flat_set<Compositor*> HostContextFactoryPrivate::GetAllCompositors() {
  base::flat_set<Compositor*> all_compositors;
  all_compositors.reserve(compositor_data_map_.size());
  for (auto& pair : compositor_data_map_)
    all_compositors.insert(pair.first);
  return all_compositors;
}

HostContextFactoryPrivate::CompositorData::CompositorData() = default;
HostContextFactoryPrivate::CompositorData::CompositorData(
    CompositorData&& other) = default;
HostContextFactoryPrivate::CompositorData::~CompositorData() = default;
HostContextFactoryPrivate::CompositorData&
HostContextFactoryPrivate::CompositorData::operator=(CompositorData&& other) =
    default;

}  // namespace ui
