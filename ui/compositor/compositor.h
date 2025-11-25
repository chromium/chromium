// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_COMPOSITOR_H_
#define UI_COMPOSITOR_COMPOSITOR_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <unordered_set>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/power_monitor/power_observer.h"
#include "base/scoped_observation_traits.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/metrics/events_metrics_manager.h"
#include "cc/metrics/frame_sequence_tracker.h"
#include "cc/paint/element_id.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"
#include "cc/trees/paint_holding_reason.h"
#include "cc/trees/property_tree.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"
#include "components/viz/host/host_frame_sink_client.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/viz/privileged/mojom/compositing/display_private.mojom.h"
#include "services/viz/privileged/mojom/compositing/external_begin_frame_controller.mojom.h"
#include "services/viz/privileged/mojom/compositing/vsync_parameter_observer.mojom-forward.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkM44.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/compositor/compositor_export.h"
#include "ui/compositor/compositor_lock.h"
#include "ui/compositor/compositor_metrics_tracker_host.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/compositor_property_tree_delegate.h"
#include "ui/compositor/host_begin_frame_observer.h"
#include "ui/compositor/layer_animator_collection.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/gfx/overlay_transform.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace cc {
class AnimationHost;
class AnimationTimeline;
class Layer;
class LayerTreeDebugState;
class LayerTreeFrameSink;
class LayerTreeSettings;
class TaskGraphRunner;
}  // namespace cc

namespace gfx {
namespace mojom {
class DelegatedInkPointRenderer;
}  // namespace mojom
struct PresentationFeedback;
class Rect;
class Size;
class ScopedAnimationDurationScaleMode;
}  // namespace gfx

namespace viz {
namespace mojom {
class DisplayPrivate;
class ExternalBeginFrameController;
}  // namespace mojom

class HostFrameSinkManager;
class LocalSurfaceId;
class RasterContextProvider;
}  // namespace viz

namespace ui {
class Compositor;
class Layer;
class ScrollInputHandler;
class CompositorMetricsTracker;
class CompositorPropertyTreeDelegate;

constexpr int kCompositorLockTimeoutMs = 67;

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
  virtual scoped_refptr<viz::RasterContextProvider>
  SharedMainThreadRasterContextProvider() = 0;

  // Destroys per-compositor data.
  virtual void RemoveCompositor(Compositor* compositor) = 0;

  // Gets the task graph runner.
  virtual cc::TaskGraphRunner* GetTaskGraphRunner() = 0;

  // Allocate a new client ID for the display compositor.
  virtual viz::FrameSinkId AllocateFrameSinkId() = 0;

  // Allocates a new capture ID for a layer subtree within a frame sink.
  virtual viz::SubtreeCaptureId AllocateSubtreeCaptureId() = 0;

  // Gets the frame sink manager host instance.
  virtual viz::HostFrameSinkManager* GetHostFrameSinkManager() = 0;
};

// Factory object to create a ExternalBeginFrameControllerClient on demand.
class COMPOSITOR_EXPORT ExternalBeginFrameControllerClientFactory {
 public:
  virtual ~ExternalBeginFrameControllerClientFactory() = default;

  // Create a new client.
  virtual mojo::PendingAssociatedRemote<
      viz::mojom::ExternalBeginFrameControllerClient>
  CreateExternalBeginFrameControllerClient() = 0;
};

// Compositor object to take care of GPU painting.
// A Browser compositor object is responsible for generating the final
// displayable form of pixels comprising a single widget's contents. It draws an
// appropriately transformed texture for each transformed view in the widget's
// view hierarchy.
class COMPOSITOR_EXPORT Compositor : public base::PowerSuspendObserver,
                                     public cc::LayerTreeHostClient,
                                     public cc::LayerTreeHostSingleThreadClient,
                                     public viz::HostFrameSinkClient,
                                     public CompositorMetricsTrackerHost {
 public:
  Compositor(const viz::FrameSinkId& frame_sink_id,
             ui::ContextFactory* context_factory,
             scoped_refptr<base::SingleThreadTaskRunner> task_runner,
             bool enable_pixel_canvas,
             bool use_external_begin_frame_control = false,
             bool force_software_compositor = false,
             bool enable_compositing_based_throttling = false,
             size_t memory_limit_when_visible_mb = 0);

  Compositor(const Compositor&) = delete;
  Compositor& operator=(const Compositor&) = delete;

  ~Compositor() override;

  ui::ContextFactory* context_factory() { return context_factory_; }

  void AddChildFrameSink(const viz::FrameSinkId& frame_sink_id);
  void RemoveChildFrameSink(const viz::FrameSinkId& frame_sink_id);

  void SetLayerTreeFrameSink(
      std::unique_ptr<cc::LayerTreeFrameSink> surface,
      mojo::AssociatedRemote<viz::mojom::DisplayPrivate> display_private);
  void SetExternalBeginFrameController(
      mojo::AssociatedRemote<viz::mojom::ExternalBeginFrameController>
          external_begin_frame_controller);

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

  // HideHelper temporarily hides the root layer and replaces it with a
  // temporary layer, without calling SetRootLayer (but doing much of the work
  // that SetRootLayer does).  During that time we must disable ticking
  // of animations, since animations that animate layers that are not in
  // cc's layer tree must not tick.  These methods make those changes
  // and record/reflect that state.
  void DisableAnimations();
  void EnableAnimations();
  bool animations_are_enabled() const { return animations_are_enabled_; }
  bool IsAnimating() const { return animation_started_; }

  cc::AnimationTimeline* GetAnimationTimeline() const;

  // The scale factor of the device that this compositor is
  // compositing layers on.
  float device_scale_factor() const { return device_scale_factor_; }

  // Gets and sets the color matrix used to transform the output colors of what
  // this compositor renders.
  const SkM44& display_color_matrix() const { return display_color_matrix_; }
  void SetDisplayColorMatrix(const SkM44& matrix);

  // Where possible, draws are scissored to a damage region calculated from
  // changes to layer properties.  This bypasses that and indicates that
  // the whole frame needs to be drawn.
  void ScheduleFullRedraw();

  // Schedule redraw and append damage_rect to the damage region calculated
  // from changes to layer properties.
  void ScheduleRedrawRect(const gfx::Rect& damage_rect);

#if BUILDFLAG(IS_WIN)
  // Until this is called with |should| true then both DisableSwapUntilResize()
  // and ReenableSwap() do nothing.
  void SetShouldDisableSwapUntilResize(bool should);

  // Attempts to immediately swap a frame with the current size if possible,
  // then disables swapping on this surface until it is resized.
  void DisableSwapUntilResize();
  void ReenableSwap();
#endif

  // Sets the compositor's device scale factor and size.
  void SetScaleAndSize(float scale,
                       const gfx::Size& size_in_pixel,
                       const viz::LocalSurfaceId& local_surface_id);

  // Set the output color profile into which this compositor should render. Also
  // sets the SDR white level (in nits) used to scale HDR color space primaries.
  void SetDisplayColorSpaces(
      const gfx::DisplayColorSpaces& display_color_spaces);

#if BUILDFLAG(IS_MAC)
  // Set the current CGDirectDisplayID and update the private client.
  void SetVSyncDisplayID(const int64_t display_id);
  int64_t display_id() const;
#endif

  const gfx::DisplayColorSpaces& display_color_spaces() const {
    return display_color_spaces_;
  }

  // Set the transform/rotation info for the display output surface.
  void SetDisplayTransformHint(gfx::OverlayTransform hint);
  gfx::OverlayTransform display_transform_hint() const {
    return host_->display_transform_hint();
  }

  const viz::LocalSurfaceId& local_surface_id_from_parent() const {
    return host_->local_surface_id_from_parent();
  }

  void SetLocalSurfaceIdFromParent(
      const viz::LocalSurfaceId& local_surface_id_from_parent) {
    host_->SetLocalSurfaceIdFromParent(local_surface_id_from_parent);
  }

  // Returns the size of the widget that is being drawn to in pixel coordinates.
  const gfx::Size& size() const { return size_; }

  // Sets the background color used for areas that aren't covered by
  // the |root_layer|.
  void SetBackgroundColor(SkColor color);

  // Inform the display corresponding to this compositor if it is visible. When
  // false it does not need to produce any frames. Visibility is reset for each
  // call to CreateLayerTreeFrameSink.
  void SetVisible(bool visible);

  // Gets the visibility of the underlying compositor.
  bool IsVisible();

  // Gets or sets the scroll offset for the given layer in step with the
  // cc::InputHandler. Returns true if the layer is active on the impl side.
  bool GetScrollOffsetForLayer(cc::ElementId element_id,
                               gfx::PointF* offset) const;
  bool ScrollLayerTo(cc::ElementId element_id, const gfx::PointF& offset);

  // Mac path for transporting vsync parameters to the display. Other platforms
  // update it via the BrowserCompositorLayerTreeFrameSink directly.
  void SetDisplayVSyncParameters(base::TimeTicks timebase,
                                 base::TimeDelta interval);
  void AddVSyncParameterObserver(
      mojo::PendingRemote<viz::mojom::VSyncParameterObserver> observer);

  // Sets and caches the |max_vsync_interval| and |vrr_state|, to be applied to
  // the |display_private_| when possible, for use with variable refresh rates
  // and/or virtual modes. An absent |max_vsync_interval| value indicates that
  // the display is not capable of utilizing such features.
  void SetMaxVSyncAndVrr(
      const std::optional<base::TimeDelta>& max_vsync_interval,
      display::VariableRefreshRateState vrr_state);

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
      base::TimeDelta timeout = base::Milliseconds(kCompositorLockTimeoutMs)) {
    return lock_manager_.GetCompositorLock(
        client, timeout,
        base::DoNothingWithBoundArgs(host_->DeferMainFrameUpdate()));
  }

  // Registers a callback that is run when the presentation feedback for the
  // next submitted frame is received (it's entirely possible some frames may be
  // dropped between the time this is called and the callback is run).
  // See ui/gfx/presentation_feedback.h for details on the args (TimeTicks is
  // always non-zero).
  // Note that since this might be called on failed presentations, it is
  // deprecated in favor of `RequestSuccessfulPresentationTimeForNextFrame()`
  // which will be called only after a successful presentation.
  using PresentationTimeCallback =
      base::OnceCallback<void(const gfx::PresentationFeedback&)>;
  void RequestPresentationTimeForNextFrame(PresentationTimeCallback callback);

  // Registers a callback that is run when the next frame successfully makes it
  // to the screen (it's entirely possible some frames may be dropped between
  // the time this is called and the callback is run).
  using SuccessfulPresentationTimeCallback = base::OnceCallback<void(
      const viz::FrameTimingDetails& frame_timing_details)>;
  void RequestSuccessfulPresentationTimeForNextFrame(
      SuccessfulPresentationTimeCallback callback);

#if BUILDFLAG(IS_IOS)
  void IssueExternalBeginFrameNoAck(const viz::BeginFrameArgs& args);
#else
  void IssueExternalBeginFrame(
      const viz::BeginFrameArgs& args,
      bool force,
      base::OnceCallback<void(const viz::BeginFrameAck&)> callback);
#endif

  // Creates a CompositorMetricsTracker for tracking this Compositor.
  CompositorMetricsTracker RequestNewCompositorMetricsTracker();

  // Returns average throughput as measured by the FrameSorter.
  double GetAverageThroughput() const;

  // Activates a scoped monitor for the current event to track its metrics.
  // `done_callback` is called when the monitor goes out of scope.
  std::unique_ptr<cc::EventsMetricsManager::ScopedMonitor>
  GetScopedEventMetricsMonitor(
      cc::EventsMetricsManager::ScopedMonitor::DoneCallback done_callback);

  // LayerTreeHostClient implementation.
  void WillBeginMainFrame() override {}
  void DidBeginMainFrame() override;
  void OnDeferMainFrameUpdatesChanged(bool) override {}
  void OnDeferCommitsChanged(
      bool,
      cc::PaintHoldingReason,
      std::optional<cc::PaintHoldingCommitTrigger>) override {}
  void OnCommitRequested() override {}
  void WillUpdateLayers() override {}
  void DidUpdateLayers() override;
  void BeginMainFrame(const viz::BeginFrameArgs& args) override;
  void BeginMainFrameNotExpectedSoon() override;
  void BeginMainFrameNotExpectedUntil(base::TimeTicks time) override;
  void UpdateLayerTreeHost() override;
  void ApplyViewportChanges(const cc::ApplyViewportChangesArgs& args) override {
  }
  void UpdateCompositorScrollState(
      const cc::CompositorCommitData& commit_data) override {}
  void RequestNewLayerTreeFrameSink() override;
  void DidInitializeLayerTreeFrameSink() override {}
  void DidFailToInitializeLayerTreeFrameSink() override;
  void WillCommit(const cc::CommitState&) override {}
  void DidCommit(int source_frame_number,
                 base::TimeTicks,
                 base::TimeTicks) override;
  void DidCommitAndDrawFrame(int source_frame_number) override {}
  void DidReceiveCompositorFrameAckDeprecatedForCompositor() override;
  void DidCompletePageScaleAnimation(int source_frame_number) override {}
  void DidPresentCompositorFrame(
      uint32_t frame_token,
      const viz::FrameTimingDetails& frame_timing_details) override;
  void RecordStartOfFrameMetrics() override {}
  void RecordEndOfFrameMetrics(
      base::TimeTicks frame_begin_time,
      cc::ActiveFrameSequenceTrackers trackers) override {}
  std::unique_ptr<cc::BeginMainFrameMetrics> GetBeginMainFrameMetrics()
      override;
  void NotifyCompositorMetricsTrackerResults(
      cc::CustomTrackerResults results) override;
  void DidObserveFirstScrollDelay(
      int source_frame_number,
      base::TimeDelta first_scroll_delay,
      base::TimeTicks first_scroll_timestamp) override {}

  // cc::LayerTreeHostSingleThreadClient implementation.
  void DidSubmitCompositorFrame() override;
  void DidLoseLayerTreeFrameSink() override {}
  void FrameIntervalUpdated(base::TimeDelta interval) override;
  void FrameSinksToThrottleUpdated(
      const base::flat_set<viz::FrameSinkId>& ids) override;

  // viz::HostFrameSinkClient implementation.
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override;
  void OnFrameTokenChanged(uint32_t frame_token,
                           base::TimeTicks activation_time) override;

  // CompositorMetricsTrackerHost implementation.
  void StartMetricsTracker(
      TrackerId tracker_id,
      CompositorMetricsTrackerHost::ReportCallback callback) override;
  bool StopMetricsTracker(TrackerId tracker_id) override;
  void CancelMetricsTracker(TrackerId tracker_id) override;

  // base::PowerSuspendObserver:
  void OnResume() override;

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE_X11)
  void OnCompleteSwapWithNewSize(const gfx::Size& size);
#endif  // BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE_X11)

  bool IsLocked() { return lock_manager_.IsLocked(); }

  bool output_is_secure() const { return output_is_secure_; }
  void SetOutputIsSecure(bool output_is_secure);

  const cc::LayerTreeDebugState& GetLayerTreeDebugState() const;
  void SetLayerTreeDebugState(const cc::LayerTreeDebugState& debug_state);

  LayerAnimatorCollection* layer_animator_collection() {
    return &layer_animator_collection_;
  }

  const viz::FrameSinkId& frame_sink_id() const { return frame_sink_id_; }
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

  virtual void SetDelegatedInkPointRenderer(
      mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer> receiver);

  const cc::LayerTreeSettings& GetLayerTreeSettings() const;

  size_t saved_events_metrics_count_for_testing() const {
    return host_->saved_events_metrics_count_for_testing();
  }

  // Returns true if there are compositor metrics trackers.
  bool has_compositor_metrics_trackers_for_testing() const {
    return !compositor_metrics_tracker_map_.empty();
  }

  const cc::LayerTreeHost* host_for_testing() const { return host_.get(); }

  void AddSimpleBeginFrameObserver(
      ui::HostBeginFrameObserver::SimpleBeginFrameObserver* obs);
  void RemoveSimpleBeginFrameObserver(
      ui::HostBeginFrameObserver::SimpleBeginFrameObserver* obs);

  const std::optional<base::TimeDelta>& max_vsync_interval_for_testing() const {
    return max_vsync_interval_;
  }

  display::VariableRefreshRateState vrr_state_for_testing() const {
    return vrr_state_;
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Sets the list of refresh rates that the compositor may request to use.
  void SetSeamlessRefreshRates(
      const std::vector<float>& seamless_refresh_rates);

  // Notifies observers of a new refresh rate preference.
  void OnSetPreferredRefreshRate(float refresh_rate);
#endif  // BUILDFLAG(IS_CHROMEOS)

  // While there are outstanding `ScopedKeepSurfaceAlive`, Compositor will
  // attempt to ensure any pending `viz::CopyOutputRequest` in any part of the
  // compositor surface tree are fulfilled in a timely manner. `surface_id`
  // corresponds to the `Surface` being copied. The GPU contents of this
  // `surface_id` are kept alive as long as there is an outstanding
  // `ScopedKeepSurfaceAlive` for it.
  using ScopedKeepSurfaceAliveCallback = base::ScopedClosureRunner;
  ScopedKeepSurfaceAliveCallback TakeScopedKeepSurfaceAliveCallback(
      const viz::SurfaceId& surface_id);

  CompositorPropertyTreeDelegate* property_tree_delegate() {
    return property_tree_delegate_.get();
  }

  const cc::PropertyTrees* property_trees() const;

  ExternalBeginFrameControllerClientFactory*
  external_begin_frame_controler_client_factory() {
    return external_begin_frame_controler_client_factory_.get();
  }

  void SetExternalBeginFrameControllerClientFactory(
      ExternalBeginFrameControllerClientFactory* factory) {
    external_begin_frame_controler_client_factory_ = factory;
  }

  // TODO(crbug.com/389771428) - Right now the local property tree is
  // an incomplete thing that only partially matches the one the LayerTreeHost
  // actually uses. Eventually we want to make it completely match and then
  // switch the LayerTreeHost to using it directly.
  void CheckPropertyTrees() const;

 private:
  friend class base::RefCounted<Compositor>;
  friend class TotalAnimationThroughputReporter;
  friend class TestCompositorHost;

  static void SendDamagedRectsRecursive(Layer* layer);

  // Called when collected metrics for the tracker of |tracker_id| is ready.
  void ReportMetricsForTracker(
      int tracker_id,
      const cc::FrameSequenceMetrics::CustomReportData& data);

  void MaybeUpdateObserveBeginFrame();

  // Tracks a list of pending `viz::CopyOutputRequest`s.
  using PendingSurfaceCopyId =
      base::StrongAlias<struct PendingSurfaceCopyIdTag, uint32_t>;
  void RemoveScopedKeepSurfaceAlive(
      const PendingSurfaceCopyId& scoped_keep_surface_alive_id);

  PendingSurfaceCopyId pending_surface_copy_id_ = PendingSurfaceCopyId(0u);
  base::flat_map<PendingSurfaceCopyId,
                 std::unique_ptr<cc::ScopedKeepSurfaceAlive>>
      pending_surface_copies_;

  gfx::Size size_;

  raw_ptr<ui::ContextFactory> context_factory_;

  // |display_private_| can be unbound for:
  // 1. Tests that don't set |display_private_|.
  // 2. Intermittently on creation or if there is some kind of error (GPU crash,
  //    GL context loss, etc.) that triggers reinitializing message pipes to the
  //    GPU process RootCompositorFrameSinkImpl.
  // Therefore, it should always be checked for safety before use.
  mojo::AssociatedRemote<viz::mojom::DisplayPrivate> display_private_;
  mojo::AssociatedRemote<viz::mojom::ExternalBeginFrameController>
      external_begin_frame_controller_;
  raw_ptr<ExternalBeginFrameControllerClientFactory, DanglingUntriaged>
      external_begin_frame_controler_client_factory_;

  // Used to hold on to IssueExternalBeginFrame(NoAck) arguments if
  // |external_begin_frame_controller_| isn't ready yet.
#if BUILDFLAG(IS_IOS)
  using PendingBeginFrameArgs = viz::BeginFrameArgs;
#else
  struct PendingBeginFrameArgs {
    PendingBeginFrameArgs(
        const viz::BeginFrameArgs& args,
        bool force,
        base::OnceCallback<void(const viz::BeginFrameAck&)> callback);
    ~PendingBeginFrameArgs();

    const viz::BeginFrameArgs args;
    const bool force;
    base::OnceCallback<void(const viz::BeginFrameAck&)> callback;
  };
#endif
  std::optional<PendingBeginFrameArgs> pending_begin_frame_args_;

  ui::HostBeginFrameObserver::SimpleBeginFrameObserverList
      simple_begin_frame_observers_;
  std::unique_ptr<ui::HostBeginFrameObserver> host_begin_frame_observer_;

  // The root of the Layer tree drawn by this compositor.
  raw_ptr<Layer> root_layer_ = nullptr;

  base::ObserverList<CompositorObserver, true>::Unchecked observer_list_;
  base::ObserverList<CompositorAnimationObserver>::Unchecked
      animation_observer_list_;

  gfx::AcceleratedWidget widget_ = gfx::kNullAcceleratedWidget;

#if BUILDFLAG(IS_MAC)
  // Current CGDirectDisplayID for the screen.
  int64_t display_id_ = display::kInvalidDisplayId;
#endif

  // Current vsync refresh rate per second. Initialized to 60hz as a reasonable
  // value until first begin frame arrives with the real refresh rate.
  float refresh_rate_ = 60.f;

  // A map from child id to parent id.
  std::unordered_set<viz::FrameSinkId, viz::FrameSinkIdHash> child_frame_sinks_;
  bool widget_valid_ = false;
  bool layer_tree_frame_sink_requested_ = false;
  const viz::FrameSinkId frame_sink_id_;
  scoped_refptr<cc::Layer> root_cc_layer_;
  std::unique_ptr<cc::AnimationHost> animation_host_;
  std::unique_ptr<cc::LayerTreeHost> host_;
  base::WeakPtr<cc::InputHandler> input_handler_weak_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Snapshot of last set vsync parameters, to avoid redundant IPCs.
  base::TimeTicks vsync_timebase_;
  base::TimeDelta vsync_interval_ = viz::BeginFrameArgs::DefaultInterval();
  bool has_vsync_params_ = false;
  std::optional<base::TimeDelta> max_vsync_interval_ = std::nullopt;
  display::VariableRefreshRateState vrr_state_ =
      display::VariableRefreshRateState::kVrrNotCapable;
#if BUILDFLAG(IS_CHROMEOS)
  std::vector<float> seamless_refresh_rates_;
#endif  // BUILDFLAG(IS_CHROMEOS)

  const bool use_external_begin_frame_control_;
  const bool force_software_compositor_;

  // The device scale factor of the monitor that this compositor is compositing
  // layers on.
  float device_scale_factor_ = 0.f;

  LayerAnimatorCollection layer_animator_collection_;
  scoped_refptr<cc::AnimationTimeline> animation_timeline_;
  std::unique_ptr<gfx::ScopedAnimationDurationScaleMode> slow_animations_;

  SkM44 display_color_matrix_;
  gfx::DisplayColorSpaces display_color_spaces_;

  bool output_is_secure_ = false;

  // If true, all paint commands are recorded at pixel size instead of DIP.
  const bool is_pixel_canvas_;

  CompositorLockManager lock_manager_;

  std::unique_ptr<ScrollInputHandler> scroll_input_handler_;

#if BUILDFLAG(IS_WIN)
  bool should_disable_swap_until_resize_ = false;
#endif

  // Set in DisableSwapUntilResize and reset when a resize happens.
  bool disabled_swap_until_resize_ = false;

  bool animations_are_enabled_ = true;

  // This together with the animations observer list carries the "last
  // animation finished" state to the next BeginMainFrame so that it could
  // notify observers if needed. It is set in AddAnimationObserver and
  // Cleared in BeginMainFrame when there are no animation observers.
  // See go/report-ux-metrics-at-painting for details.
  bool animation_started_ = false;

  TrackerId next_compositor_metrics_tracker_id_ = 1u;
  struct TrackerState {
    TrackerState();
    TrackerState(TrackerState&&);
    TrackerState& operator=(TrackerState&&);
    ~TrackerState();

    // Whether a tracker is waiting for report and `report_callback` should be
    // invoked. This is set to true when a tracker is stopped.
    bool should_report = false;

    // Whether the report for a tracker has happened. This is set when an
    // involuntary report happens before the tracker is stopped and set
    // `should_report` field above.
    bool report_attempted = false;

    // Invoked to send report to the owner of a tracker.
    CompositorMetricsTrackerHost::ReportCallback report_callback;
  };
  using CompositorMetricsTrackerMap = base::flat_map<TrackerId, TrackerState>;
  CompositorMetricsTrackerMap compositor_metrics_tracker_map_;

  // TODO(crbug.com/389771428): This holds a transitional object that
  // will be used to migrate from using layer trees to property trees and
  // layer lists. We can remove this once the code has been
  // fully converted to using property trees and layer lists and then
  // go back to using the cc::Compositor's default logic for that mode.
  bool uses_layer_lists_ = false;
  std::unique_ptr<CompositorPropertyTreeDelegate> property_tree_delegate_;
  std::optional<cc::PropertyTrees> property_trees_;
  int viewport_clip_id_ = cc::kInvalidPropertyNodeId;

  base::WeakPtrFactory<Compositor> context_creation_weak_ptr_factory_{this};
  base::WeakPtrFactory<Compositor> weak_ptr_factory_{this};
};

}  // namespace ui

namespace base {

template <>
struct ScopedObservationTraits<ui::Compositor,
                               ui::CompositorAnimationObserver> {
  static void AddObserver(ui::Compositor* source,
                          ui::CompositorAnimationObserver* observer) {
    source->AddAnimationObserver(observer);
  }
  static void RemoveObserver(ui::Compositor* source,
                             ui::CompositorAnimationObserver* observer) {
    source->RemoveAnimationObserver(observer);
  }
};

}  // namespace base

#endif  // UI_COMPOSITOR_COMPOSITOR_H_
