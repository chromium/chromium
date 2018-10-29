// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/local/window_port_local.h"

#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"
#include "components/viz/client/hit_test_data_provider_draw_quad.h"
#include "components/viz/client/local_surface_id_provider.h"
#include "components/viz/common/features.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "services/ws/public/mojom/window_tree_constants.mojom.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/env.h"
#include "ui/aura/hit_test_data_provider_aura.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/base/layout.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace aura {

namespace {
static const char* kExo = "Exo";

class ScopedCursorHider {
 public:
  explicit ScopedCursorHider(Window* window)
      : window_(window), hid_cursor_(false) {
    if (!window_->IsRootWindow())
      return;
    const bool cursor_is_in_bounds = window_->GetBoundsInScreen().Contains(
        window->env()->last_mouse_location());
    client::CursorClient* cursor_client = client::GetCursorClient(window_);
    if (cursor_is_in_bounds && cursor_client &&
        cursor_client->IsCursorVisible()) {
      cursor_client->HideCursor();
      hid_cursor_ = true;
    }
  }
  ~ScopedCursorHider() {
    if (!window_->IsRootWindow())
      return;

    // Update the device scale factor of the cursor client only when the last
    // mouse location is on this root window.
    if (hid_cursor_) {
      client::CursorClient* cursor_client = client::GetCursorClient(window_);
      if (cursor_client) {
        const display::Display& display =
            display::Screen::GetScreen()->GetDisplayNearestWindow(window_);
        cursor_client->SetDisplay(display);
        cursor_client->ShowCursor();
      }
    }
  }

 private:
  Window* window_;
  bool hid_cursor_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCursorHider);
};

}  // namespace

WindowPortLocal::WindowPortLocal(Window* window)
    : WindowPort(WindowPort::Type::kLocal),
      window_(window),
      weak_factory_(this) {}

WindowPortLocal::~WindowPortLocal() {
  if (frame_sink_id_.is_valid()) {
    auto* context_factory_private = window_->env()->context_factory_private();
    auto* host_frame_sink_manager =
        context_factory_private->GetHostFrameSinkManager();
    host_frame_sink_manager->InvalidateFrameSinkId(frame_sink_id_);
  }
}

void WindowPortLocal::OnPreInit(Window* window) {}

void WindowPortLocal::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {
  if (!window_->IsRootWindow() &&
      last_device_scale_factor_ != new_device_scale_factor &&
      IsEmbeddingExternalContent()) {
    last_device_scale_factor_ = new_device_scale_factor;
    parent_local_surface_id_allocator_->GenerateId();
    if (frame_sink_)
      frame_sink_->SetLocalSurfaceId(GetCurrentLocalSurfaceId());
  }

  ScopedCursorHider hider(window_);
  if (window_->delegate()) {
    window_->delegate()->OnDeviceScaleFactorChanged(old_device_scale_factor,
                                                    new_device_scale_factor);
  }
}

void WindowPortLocal::OnWillAddChild(Window* child) {}

void WindowPortLocal::OnWillRemoveChild(Window* child) {}

void WindowPortLocal::OnWillMoveChild(size_t current_index, size_t dest_index) {
}

void WindowPortLocal::OnVisibilityChanged(bool visible) {}

void WindowPortLocal::OnDidChangeBounds(const gfx::Rect& old_bounds,
                                        const gfx::Rect& new_bounds) {
  if (!window_->IsRootWindow() && last_size_ != new_bounds.size() &&
      IsEmbeddingExternalContent()) {
    last_size_ = new_bounds.size();
    parent_local_surface_id_allocator_->GenerateId();
    if (frame_sink_)
      frame_sink_->SetLocalSurfaceId(GetCurrentLocalSurfaceId());
  }
}

void WindowPortLocal::OnDidChangeTransform(
    const gfx::Transform& old_transform,
    const gfx::Transform& new_transform) {}

std::unique_ptr<ui::PropertyData> WindowPortLocal::OnWillChangeProperty(
    const void* key) {
  return nullptr;
}

void WindowPortLocal::OnPropertyChanged(
    const void* key,
    int64_t old_value,
    std::unique_ptr<ui::PropertyData> data) {}

std::unique_ptr<cc::LayerTreeFrameSink>
WindowPortLocal::CreateLayerTreeFrameSink() {
  DCHECK(!frame_sink_id_.is_valid());
  auto* context_factory_private = window_->env()->context_factory_private();
  auto* host_frame_sink_manager =
      context_factory_private->GetHostFrameSinkManager();
  frame_sink_id_ = context_factory_private->AllocateFrameSinkId();

  // For creating a async frame sink which connects to the viz display
  // compositor.
  viz::mojom::CompositorFrameSinkPtrInfo sink_info;
  viz::mojom::CompositorFrameSinkRequest sink_request =
      mojo::MakeRequest(&sink_info);
  viz::mojom::CompositorFrameSinkClientPtr client;
  viz::mojom::CompositorFrameSinkClientRequest client_request =
      mojo::MakeRequest(&client);
  host_frame_sink_manager->RegisterFrameSinkId(
      frame_sink_id_, this, viz::ReportFirstSurfaceActivation::kYes);
  window_->SetEmbedFrameSinkId(frame_sink_id_);
  host_frame_sink_manager->CreateCompositorFrameSink(
      frame_sink_id_, std::move(sink_request), std::move(client));

  cc::mojo_embedder::AsyncLayerTreeFrameSink::InitParams params;
  params.gpu_memory_buffer_manager =
      window_->env()->context_factory()->GetGpuMemoryBufferManager();
  params.pipes.compositor_frame_sink_info = std::move(sink_info);
  params.pipes.client_request = std::move(client_request);
  params.enable_surface_synchronization = true;
  params.client_name = kExo;
  if (features::IsVizHitTestingDrawQuadEnabled()) {
    bool root_accepts_events =
        (window_->event_targeting_policy() ==
         ws::mojom::EventTargetingPolicy::TARGET_ONLY) ||
        (window_->event_targeting_policy() ==
         ws::mojom::EventTargetingPolicy::TARGET_AND_DESCENDANTS);
    params.hit_test_data_provider =
        std::make_unique<viz::HitTestDataProviderDrawQuad>(
            true /* should_ask_for_child_region */, root_accepts_events);
  } else {
    params.hit_test_data_provider =
        std::make_unique<HitTestDataProviderAura>(window_);
  }
  auto frame_sink =
      std::make_unique<cc::mojo_embedder::AsyncLayerTreeFrameSink>(
          nullptr /* context_provider */, nullptr /* worker_context_provider */,
          &params);
  frame_sink_ = frame_sink->GetWeakPtr();
  AllocateLocalSurfaceId();
  return std::move(frame_sink);
}

void WindowPortLocal::AllocateLocalSurfaceId() {
  if (!parent_local_surface_id_allocator_)
    parent_local_surface_id_allocator_.emplace();
  else
    parent_local_surface_id_allocator_->GenerateId();
  UpdateLocalSurfaceId();
}

viz::ScopedSurfaceIdAllocator WindowPortLocal::GetSurfaceIdAllocator(
    base::OnceCallback<void()> allocation_task) {
  return viz::ScopedSurfaceIdAllocator(
      &parent_local_surface_id_allocator_.value(), std::move(allocation_task));
}

void WindowPortLocal::UpdateLocalSurfaceIdFromEmbeddedClient(
    const viz::LocalSurfaceId& embedded_client_local_surface_id,
    base::TimeTicks embedded_client_local_surface_id_allocation_time) {
  parent_local_surface_id_allocator_->UpdateFromChild(
      embedded_client_local_surface_id,
      embedded_client_local_surface_id_allocation_time);
  UpdateLocalSurfaceId();
}

const viz::LocalSurfaceId& WindowPortLocal::GetLocalSurfaceId() {
  if (!parent_local_surface_id_allocator_)
    AllocateLocalSurfaceId();
  return GetCurrentLocalSurfaceId();
}

base::TimeTicks WindowPortLocal::GetLocalSurfaceIdAllocationTime() const {
  if (!parent_local_surface_id_allocator_)
    return base::TimeTicks();
  return parent_local_surface_id_allocator_->allocation_time();
}

void WindowPortLocal::OnEventTargetingPolicyChanged() {}

bool WindowPortLocal::ShouldRestackTransientChildren() {
  return true;
}

void WindowPortLocal::TrackOcclusionState() {
  window_->env()->GetWindowOcclusionTracker()->Track(window_);
}

void WindowPortLocal::OnFirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
  DCHECK_EQ(surface_info.id().frame_sink_id(), window_->GetFrameSinkId());
  window_->layer()->SetShowSurface(surface_info.id(), window_->bounds().size(),
                                   SK_ColorWHITE,
                                   cc::DeadlinePolicy::UseDefaultDeadline(),
                                   false /* stretch_content_to_fill_bounds */);
  window_->layer()->SetOldestAcceptableFallback(surface_info.id());
}

void WindowPortLocal::OnFrameTokenChanged(uint32_t frame_token) {}

void WindowPortLocal::UpdateLocalSurfaceId() {
  last_device_scale_factor_ = ui::GetScaleFactorForNativeView(window_);
  last_size_ = window_->bounds().size();
  if (frame_sink_)
    frame_sink_->SetLocalSurfaceId(GetCurrentLocalSurfaceId());
}

const viz::LocalSurfaceId& WindowPortLocal::GetCurrentLocalSurfaceId() const {
  return parent_local_surface_id_allocator_->GetCurrentLocalSurfaceId();
}

bool WindowPortLocal::IsEmbeddingExternalContent() const {
  return parent_local_surface_id_allocator_.has_value();
}

}  // namespace aura
