// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_COMPOSITOR_H_
#define UI_COMPOSITOR_COMPOSITOR_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/paint/element_id.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/host/host_frame_sink_client.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/viz/privileged/mojom/compositing/vsync_parameter_observer.mojom-forward.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkMatrix44.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/compositor/compositor_export.h"
#include "ui/compositor/compositor_lock.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/layer_animator_collection.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_transform.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace cc {
class AnimationHost;
class AnimationTimeline;
class Layer;
class LayerTreeDebugState;
class LayerTreeFrameSink;
class TaskGraphRunner;
}

namespace gfx {
struct PresentationFeedback;
class Rect;
class ScrollOffset;
class Size;
}

namespace gpu {
class GpuMemoryBufferManager;
}

namespace viz {
class ContextProvider;
class HostFrameSinkManager;
class LocalSurfaceIdAllocation;
class RasterContextProvider;
}

namespace ui {

class Compositor;
class LatencyInfo;
class Layer;
class Reflector;
class ScopedAnimationDurationScaleMode;
class ScrollInputHandler;

constexpr int kCompositorLockTimeoutMs = 67;

class COMPOSITOR_EXPORT ContextFactoryObserver {
 public:
  virtual ~ContextFactoryObserver() {}

  // Notifies that the viz::ContextProviders returned from
  // ui::ContextFactory::SharedMainThreadContextProvider and/or
  // ui::ContextFactory::SharedMainThreadRasterContextProvider were lost.
  // When this is called, the old resources (e.g. shared context, GL helper)
  // still exist, but are about to be destroyed. Getting a reference to those
  // resources from the ContextFactory (e.g. through
  // SharedMainThreadContextProvider()) will return newly recreated, valid
  // resources.
  virtual void OnLostSharedContext() = 0;
};

// This is privileged interface to the compositor. It is a global object.
class COMPOSITOR_EXPORT ContextFactoryPrivate {
 public:
  virtual ~ContextFactoryPrivate() {}

  // Creates a reflector that copies the content of the |mirrored_compositor|
  // onto |mirroring_layer|.
  virtual std::unique_ptr<Reflector> CreateReflector(
      Compositor* mirrored_compositor,
      Layer* mirroring_layer) = 0;

  // Removes the reflector, which stops the mirroring.
  virtual void RemoveReflector(Reflector* reflector) = 0;

  // Allocate a new client ID for the display compositor.
  virtual viz::FrameSinkId AllocateFrameSinkId() = 0;

  // Gets the frame sink manager host instance.
  virtual viz::HostFrameSinkManager* GetHostFrameSinkManager() = 0;

  // Inform the display corresponding to this compositor if it is visible. When
  // false it does not need to produce any frames. Visibility is reset for each
  // call to CreateLayerTreeFrameSink.
  virtual void SetDisplayVisible(ui::Compositor* compositor, bool visible) = 0;

  // Resize the display corresponding to this compositor to a particular size.
  virtual void ResizeDisplay(ui::Compositor* compositor,
                             const gfx::Size& size) = 0;

  // Attempts to immediately swap a frame with the current size if possible,
  // then will no longer swap until ResizeDisplay() is called.
  virtual void DisableSwapUntilResize(ui::Compositor* compositor) = 0;

  // Sets the color matrix used to transform how all output is drawn to the
  // display underlying this ui::Compositor.
  virtual void SetDisplayColorMatrix(ui::Compositor* compositor,
                                     const SkMatrix44& matrix) = 0;

  // Set the output color profile into which this compositor should render.
  virtual void SetDisplayColorSpace(ui::Compositor* compositor,
                                    const gfx::ColorSpace& output_color_space,
                                    float sdr_white_level) = 0;

  // Mac path for transporting vsync parameters to the display.  Other platforms
  // update it via the BrowserCompositorLayerTreeFrameSink directly.
  virtual void SetDisplayVSyncParameters(ui::Compositor* compositor,
                                         base::TimeTicks timebase,
                                         base::TimeDelta interval) = 0;

  virtual void IssueExternalBeginFrame(
      ui::Compositor* compositor,
      const viz::BeginFrameArgs& args,
      bool force,
      base::OnceCallback<void(const viz::BeginFrameAck&)> callback) = 0;

  virtual void SetOutputIsSecure(Compositor* compositor, bool secure) = 0;

  // Adds an observer for vsync parameter changes.
  virtual void AddVSyncParameterObserver(
      Compositor* compositor,
      mojo::PendingRemote<viz::mojom::VSyncParameterObserver> observer) = 0;

  // Set the transform/rotation info for the display output surface that this
  // compositor represents.
  virtual void SetDisplayTransformHint(Compositor* compositor,
                                       gfx::OverlayTransform transform) = 0;
};

// This class abstracts the creation of the 3D context for the compositor. It is
// a global object.
class COMPOSITOR_EXPORT ContextFactory {
 public:
  virtual ~ContextFactory() {}

  // Creates an output surface for the given compositor. The factory may keep
  // per-compositor data (e.g. a shared context), that needs to be cleaned up
  // by calling RemoveCompositor when the compositor gets destroyed.
  virtual void CreateLayerTreeFrameSink(
      base::WeakPtr<Compositor> compositor) = 0;

  // Return a reference to a shared offscreen context provider usable from the
  // main thread.
  virtual scoped_refptr<viz::ContextProvider>
  SharedMainThreadContextProvider() = 0;

  // Return a reference to a shared offscreen context provider usable from the
  // main thread.
  virtual scoped_refptr<viz::RasterContextProvider>
  SharedMainThreadRasterContextProvider() = 0;

  // Destroys per-compositor data.
  virtual void RemoveCompositor(Compositor* compositor) = 0;

  // Gets the GPU memory buffer manager.
  virtual gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() = 0;

  // Gets the task graph runner.
  virtual cc::TaskGraphRunner* GetTaskGraphRunner() = 0;

  virtual void AddObserver(ContextFactoryObserver* observer) = 0;

  virtual void RemoveObserver(ContextFactoryObserver* observer) = 0;

  virtual bool SyncTokensRequiredForDisplayCompositor() = 0;
};

// Compositor object to take care of GPU painting.
// A Browser compositor object is responsible for generating the final
// displayable form of pixels comprising a single widget's contents. It draws an
// appropriately transformed texture for each transformed view in the widget's
// view hierarchy.
class COMPOSITOR_EXPORT Compositor : public cc::LayerTreeHostClient,
                                     public cc::LayerTreeHostSingleThreadClient,
                                     public viz::HostFrameSinkClient {
 public:
  // |trace_environment_name| is passed to trace events so that tracing
  // can identify the environment the trace events are from. Examples are,
  // "ash", and "browser". If no value is supplied, "browser" is used.
  Compositor(const viz::FrameSinkId& frame_sink_id,
             ui::ContextFactory* context_factory,
             ui::ContextFactoryPrivate* context_factory_private,
             scoped_refptr<base::SingleThreadTaskRunner> task_runner,
             bool enable_pixel_canvas,
             bool use_external_begin_frame_control = false,
             bool force_software_compositor = false,
             const char* trace_environment_name = nullptr);
  ~Compositor() override;

  ui::ContextFactory* context_factory() { return context_factory_; }

  ui::ContextFactoryPrivate* context_factory_private() {
    return context_factory_private_;
  }

  void AddChildFrameSink(const viz::FrameSinkId& frame_sink_id);
  void RemoveChildFrameSink(const viz::FrameSinkId& frame_sink_id);

  void SetLayerTreeFrameSink(std::unique_ptr<cc::LayerTreeFrameSink> surface);

  // Called when a child surface is about to resize.
  void OnChildResizing();

  // Schedules a redraw of the layer tree associated with this compositor.
  void ScheduleDraw();

  // Sets the root of the layer tree drawn by this Compositor. The root layer
  // must have no parent. The compositor's root layer is reset if the root layer
  // is destroyed. NULL can be passed to reset the root layer, in which case the
  // compositor will stop drawing anything.
  // The Compositor does not own the root layer.
  const Layer* root_layer() const { return root_layer_; }
  Layer* root_layer() { return root_layer_; }
  void SetRootLayer(Layer* root_layer);

  cc::AnimationTimeline* GetAnimationTimeline() const;

  // The scale factor of the device that this compositor is
  // compositing layers on.
  float device_scale_factor() const { return device_scale_factor_; }

  // The color space of the device that this compositor is being displayed on.
  const gfx::ColorSpace& output_color_space() const {
    return output_color_space_;
  }

  // Gets and sets the color matrix used to transform the output colors of what
  // this compositor renders.
  const SkMatrix44& display_color_matrix() const {
    return display_color_matrix_;
  }
  void SetDisplayColorMatrix(const SkMatrix44& matrix);

  // Where possible, draws are scissored to a damage region calculated from
  // changes to layer properties.  This bypasses that and indicates that
  // the whole frame needs to be drawn.
  void ScheduleFullRedraw();

  // Schedule redraw and append damage_rect to the damage region calculated
  // from changes to layer properties.
  void ScheduleRedrawRect(const gfx::Rect& damage_rect);

  // Finishes all outstanding rendering and disables swapping on this surface
  // until it is resized.
  void DisableSwapUntilResize();
  void ReenableSwap();

  void SetLatencyInfo(const LatencyInfo& latency_info);

  // Sets the compositor's device scale factor and size.
  void SetScaleAndSize(
      float scale,
      const gfx::Size& size_in_pixel,
      const viz::LocalSurfaceIdAllocation& local_surface_id_allocation);

  // Set the output color profile into which this compositor should render. Also
  // sets the SDR white level (in nits) used to scale HDR color space primaries.
  void SetDisplayColorSpace(
      const gfx::ColorSpace& color_space,
      float sdr_white_level = gfx::ColorSpace::kDefaultSDRWhiteLevel);

  // Set the transform/rotation info for the display output surface.
  void SetDisplayTransformHint(gfx::OverlayTransform transform);
  gfx::OverlayTransform display_transform() const { return display_transform_; }

  // Returns the size of the widget that is being drawn to in pixel coordinates.
  const gfx::Size& size() const { return size_; }

  // Sets the background color used for areas that aren't covered by
  // the |root_layer|.
  void SetBackgroundColor(SkColor color);

  // Sets the visibility of the underlying compositor.
  void SetVisible(bool visible);

  // Gets the visibility of the underlying compositor.
  bool IsVisible();

  // Gets or sets the scroll offset for the given layer in step with the
  // cc::InputHandler. Returns true if the layer is active on the impl side.
  bool GetScrollOffsetForLayer(cc::ElementId element_id,
                               gfx::ScrollOffset* offset) const;
  bool ScrollLayerTo(cc::ElementId element_id, const gfx::ScrollOffset& offset);

  // Mac sets vsync parameters through the browser compositor rather than from
  // the GPU.
  void SetDisplayVSyncParameters(base::TimeTicks timebase,
                                 base::TimeDelta interval);
  void AddVSyncParameterObserver(
      mojo::PendingRemote<viz::mojom::VSyncParameterObserver> observer);

  // Sets the widget for the compositor to render into.
  void SetAcceleratedWidget(gfx::AcceleratedWidget widget);
  // Releases the widget previously set through SetAcceleratedWidget().
  // After returning it will not be used for rendering anymore.
  // The compositor must be set to invisible when taking away a widget.
  gfx::AcceleratedWidget ReleaseAcceleratedWidget();
  gfx::AcceleratedWidget widget() const;

  // This flag is used to force a compositor into software compositing even tho
  // in general chrome is using gpu compositing. This allows the compositor to
  // be created without a gpu context, and does not go through the gpu path at
  // all. This flag can not be used with a compositor that embeds any external
  // content via a SurfaceLayer, as they would not agree on what compositing
  // mode to use for resources, but may be used eg for tooltip windows.
  bool force_software_compositor() { return force_software_compositor_; }

  // Returns the main thread task runner this compositor uses. Users of the
  // compositor generally shouldn't use this.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const {
    return task_runner_;
  }

  // Compositor does not own observers. It is the responsibility of the
  // observer to remove itself when it is done observing.
  void AddObserver(CompositorObserver* observer);
  void RemoveObserver(CompositorObserver* observer);
  bool HasObserver(const CompositorObserver* observer) const;

  void AddAnimationObserver(CompositorAnimationObserver* observer);
  void RemoveAnimationObserver(CompositorAnimationObserver* observer);
  bool HasAnimationObserver(const CompositorAnimationObserver* observer) const;

  // Creates a compositor lock. Returns NULL if it is not possible to lock at
  // this time (i.e. we're waiting to complete a previous unlock). If the
  // timeout is null, then no timeout is used.
  std::unique_ptr<CompositorLock> GetCompositorLock(
      CompositorLockClient* client,
      base::TimeDelta timeout =
          base::TimeDelta::FromMilliseconds(kCompositorLockTimeoutMs)) {
    return lock_manager_.GetCompositorLock(client, timeout,
                                           host_->DeferMainFrameUpdate());
  }

  // Registers a callback that is run when the next frame successfully makes it
  // to the screen (it's entirely possible some frames may be dropped between
  // the time this is called and the callback is run).
  // See ui/gfx/presentation_feedback.h for details on the args (TimeTicks is
  // always non-zero).
  using PresentationTimeCallback =
      base::OnceCallback<void(const gfx::PresentationFeedback&)>;
  void RequestPresentationTimeForNextFrame(PresentationTimeCallback callback);

  // LayerTreeHostClient implementation.
  void WillBeginMainFrame() override {}
  void DidBeginMainFrame() override {}
  void OnDeferMainFrameUpdatesChanged(bool) override {}
  void OnDeferCommitsChanged(bool) override {}
  void WillUpdateLayers() override {}
  void DidUpdateLayers() override;
  void BeginMainFrame(const viz::BeginFrameArgs& args) override;
  void BeginMainFrameNotExpectedSoon() override;
  void BeginMainFrameNotExpectedUntil(base::TimeTicks time) override;
  void UpdateLayerTreeHost() override;
  void ApplyViewportChanges(const cc::ApplyViewportChangesArgs& args) override {
  }
  void RecordManipulationTypeCounts(cc::ManipulationInfo info) override {}
  void SendOverscrollEventFromImplSide(
      const gfx::Vector2dF& overscroll_delta,
      cc::ElementId scroll_latched_element_id) override {}
  void SendScrollEndEventFromImplSide(
      cc::ElementId scroll_latched_element_id) override {}
  void RequestNewLayerTreeFrameSink() override;
  void DidInitializeLayerTreeFrameSink() override {}
  void DidFailToInitializeLayerTreeFrameSink() override;
  void WillCommit() override {}
  void DidCommit() override;
  void DidCommitAndDrawFrame() override {}
  void DidReceiveCompositorFrameAck() override;
  void DidCompletePageScaleAnimation() override {}
  void DidPresentCompositorFrame(
      uint32_t frame_token,
      const gfx::PresentationFeedback& feedback) override;
  void RecordStartOfFrameMetrics() override {}
  void RecordEndOfFrameMetrics(base::TimeTicks frame_begin_time) override {}
  std::unique_ptr<cc::BeginMainFrameMetrics> GetBeginMainFrameMetrics()
      override;

  // cc::LayerTreeHostSingleThreadClient implementation.
  void DidSubmitCompositorFrame() override;
  void DidLoseLayerTreeFrameSink() override {}
  void FrameIntervalUpdated(base::TimeDelta interval) override;

  // viz::HostFrameSinkClient implementation.
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override;
  void OnFrameTokenChanged(uint32_t frame_token) override;

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  void OnCompleteSwapWithNewSize(const gfx::Size& size);
#endif

  bool IsLocked() { return lock_manager_.IsLocked(); }

  void SetOutputIsSecure(bool output_is_secure);

  const cc::LayerTreeDebugState& GetLayerTreeDebugState() const;
  void SetLayerTreeDebugState(const cc::LayerTreeDebugState& debug_state);

  LayerAnimatorCollection* layer_animator_collection() {
    return &layer_animator_collection_;
  }

  const viz::FrameSinkId& frame_sink_id() const { return frame_sink_id_; }
  int activated_frame_count() const { return activated_frame_count_; }
  float refresh_rate() const { return refresh_rate_; }

  bool use_external_begin_frame_control() const {
    return use_external_begin_frame_control_;
  }

  void SetAllowLocksToExtendTimeout(bool allowed) {
    lock_manager_.set_allow_locks_to_extend_timeout(allowed);
  }

  // If true, all paint commands are recorded at pixel size instead of DIP.
  bool is_pixel_canvas() const { return is_pixel_canvas_; }

  ScrollInputHandler* scroll_input_handler() const {
    return scroll_input_handler_.get();
  }

 private:
  friend class base::RefCounted<Compositor>;

  gfx::Size size_;

  ui::ContextFactory* context_factory_;
  ui::ContextFactoryPrivate* context_factory_private_;

  // The root of the Layer tree drawn by this compositor.
  Layer* root_layer_ = nullptr;

  base::ObserverList<CompositorObserver, true>::Unchecked observer_list_;
  base::ObserverList<CompositorAnimationObserver>::Unchecked
      animation_observer_list_;

  gfx::AcceleratedWidget widget_ = gfx::kNullAcceleratedWidget;
  // A sequence number of a current compositor frame for use with metrics.
  int activated_frame_count_ = 0;

  // Current vsync refresh rate per second. Initialized to 60hz as a reasonable
  // value until first begin frame arrives with the real refresh rate.
  float refresh_rate_ = 60.f;

  // A map from child id to parent id.
  std::unordered_set<viz::FrameSinkId, viz::FrameSinkIdHash> child_frame_sinks_;
  bool widget_valid_ = false;
  bool layer_tree_frame_sink_requested_ = false;
  const viz::FrameSinkId frame_sink_id_;
  scoped_refptr<cc::Layer> root_web_layer_;
  std::unique_ptr<cc::AnimationHost> animation_host_;
  std::unique_ptr<cc::LayerTreeHost> host_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Snapshot of last set vsync parameters, to avoid redundant IPCs.
  base::TimeTicks vsync_timebase_;
  base::TimeDelta vsync_interval_ = viz::BeginFrameArgs::DefaultInterval();
  bool has_vsync_params_ = false;

  const bool use_external_begin_frame_control_;
  const bool force_software_compositor_;

  // The device scale factor of the monitor that this compositor is compositing
  // layers on.
  float device_scale_factor_ = 0.f;

  LayerAnimatorCollection layer_animator_collection_;
  scoped_refptr<cc::AnimationTimeline> animation_timeline_;
  std::unique_ptr<ScopedAnimationDurationScaleMode> slow_animations_;

  SkMatrix44 display_color_matrix_;

  gfx::ColorSpace output_color_space_;

  float sdr_white_level_ = gfx::ColorSpace::kDefaultSDRWhiteLevel;

  // If true, all paint commands are recorded at pixel size instead of DIP.
  const bool is_pixel_canvas_;

  CompositorLockManager lock_manager_;

  std::unique_ptr<ScrollInputHandler> scroll_input_handler_;

  // Set in DisableSwapUntilResize and reset when a resize happens.
  bool disabled_swap_until_resize_ = false;

  const char* trace_environment_name_;

  gfx::OverlayTransform display_transform_ = gfx::OVERLAY_TRANSFORM_NONE;

  base::WeakPtrFactory<Compositor> context_creation_weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Compositor);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_COMPOSITOR_H_
