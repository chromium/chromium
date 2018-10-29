// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_LOCAL_WINDOW_PORT_LOCAL_H_
#define UI_AURA_LOCAL_WINDOW_PORT_LOCAL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/host/host_frame_sink_client.h"
#include "ui/aura/window_port.h"
#include "ui/base/property_data.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class Size;
}

namespace aura {

class Window;

// WindowPort implementation for classic aura, e.g. not mus.
class AURA_EXPORT WindowPortLocal : public WindowPort,
                                    public viz::HostFrameSinkClient {
 public:
  explicit WindowPortLocal(Window* window);
  ~WindowPortLocal() override;

  // WindowPort:
  void OnPreInit(Window* window) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;
  void OnWillAddChild(Window* child) override;
  void OnWillRemoveChild(Window* child) override;
  void OnWillMoveChild(size_t current_index, size_t dest_index) override;
  void OnVisibilityChanged(bool visible) override;
  void OnDidChangeBounds(const gfx::Rect& old_bounds,
                         const gfx::Rect& new_bounds) override;
  void OnDidChangeTransform(const gfx::Transform& old_transform,
                            const gfx::Transform& new_transform) override;
  std::unique_ptr<ui::PropertyData> OnWillChangeProperty(
      const void* key) override;
  void OnPropertyChanged(const void* key,
                         int64_t old_value,
                         std::unique_ptr<ui::PropertyData> data) override;
  std::unique_ptr<cc::LayerTreeFrameSink> CreateLayerTreeFrameSink() override;
  void AllocateLocalSurfaceId() override;
  viz::ScopedSurfaceIdAllocator GetSurfaceIdAllocator(
      base::OnceCallback<void()> allocation_task) override;
  void UpdateLocalSurfaceIdFromEmbeddedClient(
      const viz::LocalSurfaceId& embedded_client_local_surface_id,
      base::TimeTicks embedded_client_local_surface_id_allocation_time)
      override;
  const viz::LocalSurfaceId& GetLocalSurfaceId() override;
  base::TimeTicks GetLocalSurfaceIdAllocationTime() const override;
  void OnEventTargetingPolicyChanged() override;
  bool ShouldRestackTransientChildren() override;
  void TrackOcclusionState() override;

  // viz::HostFrameSinkClient:
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override;
  void OnFrameTokenChanged(uint32_t frame_token) override;

 private:
  void UpdateLocalSurfaceId();
  const viz::LocalSurfaceId& GetCurrentLocalSurfaceId() const;
  bool IsEmbeddingExternalContent() const;

  Window* const window_;
  gfx::Size last_size_;
  float last_device_scale_factor_ = 1.0f;
  base::Optional<viz::ParentLocalSurfaceIdAllocator>
      parent_local_surface_id_allocator_;
  base::WeakPtr<cc::LayerTreeFrameSink> frame_sink_;
  viz::FrameSinkId frame_sink_id_;

  base::WeakPtrFactory<WindowPortLocal> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WindowPortLocal);
};

}  // namespace aura

#endif  // UI_AURA_LOCAL_WINDOW_PORT_LOCAL_H_
