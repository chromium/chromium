// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_HOST_HOST_CONTEXT_FACTORY_PRIVATE_H_
#define UI_COMPOSITOR_HOST_HOST_CONTEXT_FACTORY_PRIVATE_H_

#include <stdint.h>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/surfaces/frame_sink_id_allocator.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/viz/privileged/mojom/compositing/display_private.mojom.h"
#include "services/viz/privileged/mojom/compositing/external_begin_frame_controller.mojom.h"
#include "ui/compositor/compositor.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace viz {
class ContextProvider;
class HostDisplayClient;
class RasterContextProvider;
}  // namespace viz

namespace ui {

struct PendingBeginFrameArgs;

class HostContextFactoryPrivate : public ContextFactoryPrivate {
 public:
  HostContextFactoryPrivate(
      uint32_t client_id,
      viz::HostFrameSinkManager* host_frame_sink_manager,
      scoped_refptr<base::SingleThreadTaskRunner> resize_task_runner);
  ~HostContextFactoryPrivate() override;

  // Call this when a compositor is created to ensure a data map entry exists
  // for it, so that the data can be accessed before the compositor is
  // configured. Could be called twice, e.g. if the GPU process crashes.
  void AddCompositor(Compositor* compositor);

  void ConfigureCompositor(
      Compositor* compositor,
      scoped_refptr<viz::ContextProvider> context_provider,
      scoped_refptr<viz::RasterContextProvider> worker_context_provider);

  void UnconfigureCompositor(Compositor* compositor);

  void set_is_gpu_compositing_disabled(bool value) {
    is_gpu_compositing_disabled_ = value;
  }
  bool is_gpu_compositing_disabled() const {
    return is_gpu_compositing_disabled_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> resize_task_runner() {
    return resize_task_runner_;
  }

  base::flat_set<Compositor*> GetAllCompositors();

  // ContextFactoryPrivate implementation.
  std::unique_ptr<Reflector> CreateReflector(Compositor* source,
                                             Layer* target) override;
  void RemoveReflector(Reflector* reflector) override;
  viz::FrameSinkId AllocateFrameSinkId() override;
  viz::HostFrameSinkManager* GetHostFrameSinkManager() override;
  void SetDisplayVisible(Compositor* compositor, bool visible) override;
  void ResizeDisplay(Compositor* compositor, const gfx::Size& size) override;
  void DisableSwapUntilResize(Compositor* compositor) override;
  void SetDisplayColorMatrix(Compositor* compositor,
                             const SkMatrix44& matrix) override;
  void SetDisplayColorSpace(Compositor* compositor,
                            const gfx::ColorSpace& output_color_space,
                            float sdr_white_level) override;
  void SetDisplayVSyncParameters(Compositor* compositor,
                                 base::TimeTicks timebase,
                                 base::TimeDelta interval) override;
  void IssueExternalBeginFrame(
      Compositor* compositor,
      const viz::BeginFrameArgs& args,
      bool force,
      base::OnceCallback<void(const viz::BeginFrameAck&)> callback) override;
  void SetOutputIsSecure(Compositor* compositor, bool secure) override;
  void AddVSyncParameterObserver(
      Compositor* compositor,
      mojo::PendingRemote<viz::mojom::VSyncParameterObserver> observer)
      override;
  void SetDisplayTransformHint(Compositor* compositor,
                               gfx::OverlayTransform transform) override;

 private:
  struct CompositorData {
    CompositorData();
    CompositorData(CompositorData&& other);
    ~CompositorData();
    CompositorData& operator=(CompositorData&& other);

    // Privileged interface that controls the display for a root
    // CompositorFrameSink.
    mojo::AssociatedRemote<viz::mojom::DisplayPrivate> display_private;
    std::unique_ptr<viz::HostDisplayClient> display_client;
    mojo::AssociatedRemote<viz::mojom::ExternalBeginFrameController>
        external_begin_frame_controller;

    std::unique_ptr<PendingBeginFrameArgs> pending_begin_frame_args;

    // SetOutputIsSecure is called before the compositor is ready, so remember
    // the status and apply it during configuration.
    bool output_is_secure = false;

   private:
    DISALLOW_COPY_AND_ASSIGN(CompositorData);
  };

  base::flat_map<Compositor*, CompositorData> compositor_data_map_;

  viz::FrameSinkIdAllocator frame_sink_id_allocator_;
  viz::HostFrameSinkManager* host_frame_sink_manager_;

  bool is_gpu_compositing_disabled_ = false;

  scoped_refptr<base::SingleThreadTaskRunner> const resize_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(HostContextFactoryPrivate);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_HOST_HOST_CONTEXT_FACTORY_PRIVATE_H_
