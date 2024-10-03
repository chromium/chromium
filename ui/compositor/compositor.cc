// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/observer_list.h"
#include "base/power_monitor/power_monitor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/animation_timeline.h"
#include "cc/base/features.h"
#include "cc/base/switches.h"
#include "cc/input/input_handler.h"
#include "cc/layers/layer.h"
#include "cc/metrics/begin_main_frame_metrics.h"
#include "cc/metrics/custom_metrics_recorder.h"
#include "cc/metrics/frame_sequence_tracker.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/switches.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/host/renderer_settings_creation.h"
#include "services/viz/privileged/mojom/compositing/display_private.mojom.h"
#include "services/viz/privileged/mojom/compositing/external_begin_frame_controller.mojom.h"
#include "services/viz/privileged/mojom/compositing/vsync_parameter_observer.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator_collection.h"
#include "ui/compositor/overscroll/scroll_input_handler.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(IS_WIN)
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#endif

namespace ui {

// Used to hold on to IssueExternalBeginFrame arguments if
// |external_begin_frame_controller_| isn't ready yet.
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

Compositor::Compositor(const viz::FrameSinkId& frame_sink_id,
                       ui::ContextFactory* context_factory,
                       scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                       bool enable_pixel_canvas,
                       bool use_external_begin_frame_control,
                       bool force_software_compositor,
                       bool enable_compositing_based_throttling,
                       size_t memory_limit_when_visible_mb)
    : context_factory_(context_factory),
      frame_sink_id_(frame_sink_id),
      task_runner_(task_runner),
      use_external_begin_frame_control_(use_external_begin_frame_control),
      force_software_compositor_(force_software_compositor),
      layer_animator_collection_(this),
      is_pixel_canvas_(enable_pixel_canvas),
      lock_manager_(task_runner) {
  DCHECK(context_factory_);
  auto* host_frame_sink_manager = context_factory_->GetHostFrameSinkManager();
  host_frame_sink_manager->RegisterFrameSinkId(
      frame_sink_id_, this, viz::ReportFirstSurfaceActivation::kNo);
  host_frame_sink_manager->SetFrameSinkDebugLabel(frame_sink_id_, "Compositor");
  root_web_layer_ = cc::Layer::Create();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  cc::LayerTreeSettings settings;

  // This will ensure PictureLayers always can have LCD text, to match the
  // previous behaviour with ContentLayers, where LCD-not-allowed notifications
  // were ignored.
  settings.layers_always_allowed_lcd_text = true;
  // Use occlusion to allow more overlapping windows to take less memory.
  settings.use_occlusion_for_tile_prioritization = true;
  settings.main_frame_before_activation_enabled = false;

  // Browser UI generally doesn't get gains from keeping around hidden layers.
  // Better to release the resources and save memory.
  settings.release_tile_resources_for_hidden_layers = true;

  // Disable edge anti-aliasing in order to increase support for HW overlays.
  settings.enable_edge_anti_aliasing = false;

  // GPU rasterization in the UI compositor is controlled by a feature.
  settings.gpu_rasterization_disabled =
      !features::IsUiGpuRasterizationEnabled();

  if (command_line->HasSwitch(cc::switches::kUIShowCompositedLayerBorders)) {
    std::string layer_borders_string = command_line->GetSwitchValueASCII(
        cc::switches::kUIShowCompositedLayerBorders);
    std::vector<std::string_view> entries = base::SplitStringPiece(
        layer_borders_string, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (entries.empty()) {
      settings.initial_debug_state.show_debug_borders.set();
    } else {
      for (const auto& entry : entries) {
        const struct {
          const char* name;
          cc::DebugBorderType type;
        } kBorders[] = {{cc::switches::kCompositedRenderPassBorders,
                         cc::DebugBorderType::RENDERPASS},
                        {cc::switches::kCompositedSurfaceBorders,
                         cc::DebugBorderType::SURFACE},
                        {cc::switches::kCompositedLayerBorders,
                         cc::DebugBorderType::LAYER}};
        for (const auto& border : kBorders) {
          if (border.name == entry) {
            settings.initial_debug_state.show_debug_borders.set(border.type);
            break;
          }
        }
      }
    }
  }
  settings.initial_debug_state.show_fps_counter =
      command_line->HasSwitch(cc::switches::kUIShowFPSCounter);
  settings.initial_debug_state.show_layer_animation_bounds_rects =
      command_line->HasSwitch(cc::switches::kUIShowLayerAnimationBounds);
  settings.initial_debug_state.show_paint_rects =
      command_line->HasSwitch(switches::kUIShowPaintRects);
  settings.initial_debug_state.show_property_changed_rects =
      command_line->HasSwitch(cc::switches::kUIShowPropertyChangedRects);
  settings.initial_debug_state.show_surface_damage_rects =
      command_line->HasSwitch(cc::switches::kUIShowSurfaceDamageRects);
  settings.initial_debug_state.show_screen_space_rects =
      command_line->HasSwitch(cc::switches::kUIShowScreenSpaceRects);

  settings.initial_debug_state.SetRecordRenderingStats(
      command_line->HasSwitch(cc::switches::kEnableGpuBenchmarking));

  settings.use_zero_copy = IsUIZeroCopyEnabled() && !features::IsUsingRawDraw();

  // UI compositor always uses partial raster if not using zero-copy. Zero copy
  // doesn't currently support partial raster.
  // RawDraw doesn't support partial raster.
  settings.use_partial_raster =
      !(settings.use_zero_copy || features::IsUsingRawDraw());

  settings.use_rgba_4444 =
      command_line->HasSwitch(switches::kUIEnableRGBA4444Textures);

#if BUILDFLAG(IS_APPLE)
  // Using CoreAnimation to composite requires using GpuMemoryBuffers, which
  // require zero copy.
  settings.use_gpu_memory_buffer_resources = settings.use_zero_copy;
  settings.enable_elastic_overscroll = true;
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_WIN)
  // Rasterized tiles must be overlay candidates to be forwarded.
  // This is very similar to the line above for Apple.
  settings.use_gpu_memory_buffer_resources =
      features::IsDelegatedCompositingEnabled();
#endif

  // Set use_gpu_memory_buffer_resources to false to disable delegated
  // compositing, if RawDraw is enabled.
  if (settings.use_gpu_memory_buffer_resources && features::IsUsingRawDraw()) {
    settings.use_gpu_memory_buffer_resources = false;
  }

  settings.memory_policy.bytes_limit_when_visible =
      (memory_limit_when_visible_mb > 0 ? memory_limit_when_visible_mb : 512) *
      1024 * 1024;

  settings.memory_policy.priority_cutoff_when_visible =
      gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE;

  settings.disallow_non_exact_resource_reuse =
      command_line->HasSwitch(switches::kDisallowNonExactResourceReuse);

  settings.wait_for_all_pipeline_stages_before_draw =
      command_line->HasSwitch(switches::kRunAllCompositorStagesBeforeDraw);

  if (features::IsPercentBasedScrollingEnabled()) {
    settings.percent_based_scrolling = true;
  }

  settings.enable_compositing_based_throttling =
      enable_compositing_based_throttling;

  settings.is_layer_tree_for_ui = true;

#if DCHECK_IS_ON()
  if (command_line->HasSwitch(cc::switches::kLogOnUIDoubleBackgroundBlur))
    settings.log_on_ui_double_background_blur = true;
#endif

  settings.enable_shared_image_cache_for_gpu =
      base::FeatureList::IsEnabled(features::kUIEnableSharedImageCacheForGpu);

  animation_host_ = cc::AnimationHost::CreateMainInstance();

  cc::LayerTreeHost::InitParams params;
  params.client = this;
  params.task_graph_runner = context_factory_->GetTaskGraphRunner();
  params.settings = &settings;
  params.main_task_runner = task_runner_;
  params.mutator_host = animation_host_.get();
  host_ = cc::LayerTreeHost::CreateSingleThreaded(this, std::move(params));

  const base::WeakPtr<cc::CompositorDelegateForInput>& compositor_delegate =
      host_->GetDelegateForInput();
  if (base::FeatureList::IsEnabled(features::kUiCompositorScrollWithLayers) &&
      compositor_delegate) {
    input_handler_weak_ = cc::InputHandler::Create(*compositor_delegate);
    scroll_input_handler_ =
        std::make_unique<ScrollInputHandler>(input_handler_weak_);
  }

  animation_timeline_ =
      cc::AnimationTimeline::Create(cc::AnimationIdProvider::NextTimelineId());
  animation_host_->AddAnimationTimeline(animation_timeline_.get());

  host_->SetRootLayer(root_web_layer_);

  // This shouldn't be done in the constructor in order to match Widget.
  // See: http://crbug.com/956264.
  host_->SetVisible(true);

  if (auto* power_monitor = base::PowerMonitor::GetInstance();
      power_monitor->IsInitialized()) {
    power_monitor->AddPowerSuspendObserver(this);
  }

  if (command_line->HasSwitch(switches::kUISlowAnimations)) {
    slow_animations_ = std::make_unique<ScopedAnimationDurationScaleMode>(
        ScopedAnimationDurationScaleMode::SLOW_DURATION);
  }

  settings.disable_frame_rate_limit =
      command_line->HasSwitch(switches::kDisableFrameRateLimit);
}

Compositor::~Compositor() {
  TRACE_EVENT0("shutdown,viz", "Compositor::destructor");
  if (auto* power_monitor = base::PowerMonitor::GetInstance();
      power_monitor->IsInitialized()) {
    power_monitor->RemovePowerSuspendObserver(this);
  }

  observer_list_.Notify(&CompositorObserver::OnCompositingShuttingDown, this);

  animation_observer_list_.Notify(
      &CompositorAnimationObserver::OnCompositingShuttingDown, this);

  simple_begin_frame_observers_.Notify(
      &ui::HostBeginFrameObserver::SimpleBeginFrameObserver::
          OnBeginFrameSourceShuttingDown);

  if (root_layer_)
    root_layer_->ResetCompositor();

  if (animation_timeline_)
    animation_host_->RemoveAnimationTimeline(animation_timeline_.get());

  // Stop all outstanding draws before telling the ContextFactory to tear
  // down any contexts that the |host_| may rely upon.
  host_.reset();

  context_factory_->RemoveCompositor(this);
  auto* host_frame_sink_manager = context_factory_->GetHostFrameSinkManager();
  for (auto& client : child_frame_sinks_) {
    DCHECK(client.is_valid());
    host_frame_sink_manager->UnregisterFrameSinkHierarchy(frame_sink_id_,
                                                          client);
  }
  host_frame_sink_manager->InvalidateFrameSinkId(frame_sink_id_, this);
}

void Compositor::AddChildFrameSink(const viz::FrameSinkId& frame_sink_id) {
  context_factory_->GetHostFrameSinkManager()->RegisterFrameSinkHierarchy(
      frame_sink_id_, frame_sink_id);

  auto result = child_frame_sinks_.insert(frame_sink_id);
  DCHECK(result.second);
}

void Compositor::RemoveChildFrameSink(const viz::FrameSinkId& frame_sink_id) {
  auto it = child_frame_sinks_.find(frame_sink_id);
  CHECK(it != child_frame_sinks_.end());
  DCHECK(it->is_valid());
  context_factory_->GetHostFrameSinkManager()->UnregisterFrameSinkHierarchy(
      frame_sink_id_, *it);
  child_frame_sinks_.erase(it);
}

void Compositor::SetLayerTreeFrameSink(
    std::unique_ptr<cc::LayerTreeFrameSink> layer_tree_frame_sink,
    mojo::AssociatedRemote<viz::mojom::DisplayPrivate> display_private) {
  layer_tree_frame_sink_requested_ = false;
  display_private_ = std::move(display_private);
  host_->SetLayerTreeFrameSink(std::move(layer_tree_frame_sink));
  // Display properties are reset when the output surface is lost, so update it
  // to match the Compositor's.
  if (display_private_) {
    disabled_swap_until_resize_ = false;
    display_private_->Resize(size());
    display_private_->SetDisplayVisible(host_->IsVisible());
    display_private_->SetDisplayColorSpaces(display_color_spaces_);
    display_private_->SetDisplayColorMatrix(
        gfx::SkM44ToTransform(display_color_matrix_));
    display_private_->SetOutputIsSecure(output_is_secure_);
#if BUILDFLAG(IS_MAC)
    display_private_->SetVSyncDisplayID(display_id_);
#endif
    if (has_vsync_params_) {
      display_private_->SetDisplayVSyncParameters(vsync_timebase_,
                                                  vsync_interval_);
    }
    display_private_->SetMaxVSyncAndVrr(max_vsync_interval_, vrr_state_);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    display_private_->SetSupportedRefreshRates(seamless_refresh_rates_);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  MaybeUpdateObserveBeginFrame();
}

void Compositor::SetExternalBeginFrameController(
    mojo::AssociatedRemote<viz::mojom::ExternalBeginFrameController>
        external_begin_frame_controller) {
  DCHECK(use_external_begin_frame_control());
  external_begin_frame_controller_ = std::move(external_begin_frame_controller);
  if (pending_begin_frame_args_) {
    external_begin_frame_controller_->IssueExternalBeginFrame(
        pending_begin_frame_args_->args, pending_begin_frame_args_->force,
        std::move(pending_begin_frame_args_->callback));
    pending_begin_frame_args_.reset();
  }
}

void Compositor::OnChildResizing() {
  observer_list_.Notify(&CompositorObserver::OnCompositingChildResizing, this);
}

void Compositor::ScheduleDraw() {
  host_->SetNeedsCommit();
}

void Compositor::SetRootLayer(Layer* root_layer) {
  if (root_layer_ == root_layer)
    return;
  if (root_layer_)
    root_layer_->ResetCompositor();
  root_layer_ = root_layer;
  root_web_layer_->RemoveAllChildren();
  if (root_layer_)
    root_layer_->SetCompositor(this, root_web_layer_);
}

void Compositor::DisableAnimations() {
  DCHECK(animations_are_enabled_);
  animations_are_enabled_ = false;
  root_layer_->ResetCompositorForAnimatorsInTree(this);
}

void Compositor::EnableAnimations() {
  DCHECK(!animations_are_enabled_);
  animations_are_enabled_ = true;
  root_layer_->SetCompositorForAnimatorsInTree(this);
}

cc::AnimationTimeline* Compositor::GetAnimationTimeline() const {
  return animation_timeline_.get();
}

void Compositor::SetDisplayColorMatrix(const SkM44& matrix) {
  display_color_matrix_ = matrix;
  if (display_private_)
    display_private_->SetDisplayColorMatrix(gfx::SkM44ToTransform(matrix));
}

void Compositor::ScheduleFullRedraw() {
  // TODO(enne): Some callers (mac) call this function expecting that it
  // will also commit.  This should probably just redraw the screen
  // from damage and not commit.  ScheduleDraw/ScheduleRedraw need
  // better names.
  host_->SetNeedsRedrawRect(host_->device_viewport_rect());
  host_->SetNeedsCommit();
}

void Compositor::ScheduleRedrawRect(const gfx::Rect& damage_rect) {
  // TODO(enne): Make this not commit.  See ScheduleFullRedraw.
  host_->SetNeedsRedrawRect(damage_rect);
  host_->SetNeedsCommit();
}

#if BUILDFLAG(IS_WIN)
void Compositor::SetShouldDisableSwapUntilResize(bool should) {
  should_disable_swap_until_resize_ = should;
}

void Compositor::DisableSwapUntilResize() {
  if (should_disable_swap_until_resize_ && display_private_) {
    // Browser needs to block for Viz to receive and process this message.
    // Otherwise when we return from WM_WINDOWPOSCHANGING message handler and
    // receive a WM_WINDOWPOSCHANGED the resize is finalized and any swaps of
    // wrong size by Viz can cause the swapped content to get scaled.
    // TODO(crbug.com/40583169): Investigate nonblocking ways for solving.
    TRACE_EVENT0("viz", "Blocked UI for DisableSwapUntilResize");
    mojo::SyncCallRestrictions::ScopedAllowSyncCall scoped_allow_sync_call;
    display_private_->DisableSwapUntilResize();
    disabled_swap_until_resize_ = true;
  }
}

void Compositor::ReenableSwap() {
  if (should_disable_swap_until_resize_ && display_private_)
    display_private_->Resize(size_);
}
#endif

void Compositor::SetScaleAndSize(float scale,
                                 const gfx::Size& size_in_pixel,
                                 const viz::LocalSurfaceId& local_surface_id) {
  DCHECK_GT(scale, 0);
  bool device_scale_factor_changed = device_scale_factor_ != scale;
  device_scale_factor_ = scale;

  // cc requires the size to be non-empty (meaning DCHECKs if size is empty).
  if (!size_in_pixel.IsEmpty()) {
    bool size_changed = size_ != size_in_pixel;
    size_ = size_in_pixel;
    host_->SetViewportRectAndScale(gfx::Rect(size_in_pixel), scale,
                                   local_surface_id);
    root_web_layer_->SetBounds(size_in_pixel);
    if (display_private_ && (size_changed || disabled_swap_until_resize_)) {
      display_private_->Resize(size_in_pixel);
      disabled_swap_until_resize_ = false;
    }
  }
  if (device_scale_factor_changed) {
    if (is_pixel_canvas())
      host_->SetRecordingScaleFactor(scale);
    if (root_layer_)
      root_layer_->OnDeviceScaleFactorChanged(scale);
  }
}

void Compositor::SetDisplayColorSpaces(
    const gfx::DisplayColorSpaces& display_color_spaces) {
  if (display_color_spaces_ == display_color_spaces)
    return;

  bool only_hdr_headroom_changed =
      gfx::DisplayColorSpaces::EqualExceptForHdrHeadroom(display_color_spaces_,
                                                         display_color_spaces);
  display_color_spaces_ = display_color_spaces;

  host_->SetDisplayColorSpaces(display_color_spaces_);

  // Always force the ui::Compositor to re-draw all layers, because damage
  // tracking bugs result in black flashes.
  // https://crbug.com/804430
  // TODO(ccameron): Remove this when the above bug is fixed.
  // b/329479347: This severely impacts performance when HDR capability is
  // ramped in and out. Restrict this to changes that would result in backbuffer
  // reallocation.
  if (!only_hdr_headroom_changed) {
    host_->SetNeedsDisplayOnAllLayers();
  }

  // Color space is reset when the output surface is lost, so this must also be
  // updated then.
  if (display_private_)
    display_private_->SetDisplayColorSpaces(display_color_spaces_);
}

#if BUILDFLAG(IS_MAC)
void Compositor::SetVSyncDisplayID(const int64_t display_id) {
  if (display_id_ == display_id) {
    return;
  }

  display_id_ = display_id;

  if (display_private_) {
    display_private_->SetVSyncDisplayID(display_id);
  }
}

int64_t Compositor::display_id() const {
  return display_id_;
}
#endif

void Compositor::SetDisplayTransformHint(gfx::OverlayTransform hint) {
  host_->set_display_transform_hint(hint);
}

void Compositor::SetBackgroundColor(SkColor color) {
  // TODO(crbug.com/40219248): Remove FromColor and make all SkColor4f.
  host_->set_background_color(SkColor4f::FromColor(color));
  ScheduleDraw();
}

void Compositor::SetVisible(bool visible) {
  const bool changed = visible != IsVisible();
  if (changed) {
    observer_list_.Notify(&CompositorObserver::OnCompositorVisibilityChanging,
                          this, visible);
  }

  host_->SetVisible(visible);
  // Visibility is reset when the output surface is lost, so this must also be
  // updated then. We need to call this even if the visibility hasn't changed,
  // for the same reason.
  if (display_private_)
    display_private_->SetDisplayVisible(visible);

  if (changed) {
    observer_list_.Notify(&CompositorObserver::OnCompositorVisibilityChanged,
                          this, visible);
  }
}

bool Compositor::IsVisible() {
  return host_->IsVisible();
}

// TODO(bokan): These calls should be delegated through the
// scroll_input_handler_ so that we don't have to keep a pointer to the
// cc::InputHandler in this class.
bool Compositor::ScrollLayerTo(cc::ElementId element_id,
                               const gfx::PointF& offset) {
  return input_handler_weak_ &&
         input_handler_weak_->ScrollLayerTo(element_id, offset);
}

bool Compositor::GetScrollOffsetForLayer(cc::ElementId element_id,
                                         gfx::PointF* offset) const {
  return input_handler_weak_ &&
         input_handler_weak_->GetScrollOffsetForLayer(element_id, offset);
}

void Compositor::SetDisplayVSyncParameters(base::TimeTicks timebase,
                                           base::TimeDelta interval) {
  static bool is_frame_rate_limit_disabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableFrameRateLimit);
  if (is_frame_rate_limit_disabled)
    return;

  if (interval.is_zero()) {
    // TODO(brianderson): We should not be receiving 0 intervals.
    interval = viz::BeginFrameArgs::DefaultInterval();
  }
  DCHECK_GT(interval.InMillisecondsF(), 0);

  // This is called at high frequency on macOS, so early-out of redundant
  // updates here.
  if (vsync_timebase_ == timebase && vsync_interval_ == interval)
    return;

  if (interval != vsync_interval_)
    has_vsync_params_ = true;

  vsync_timebase_ = timebase;
  vsync_interval_ = interval;
  if (display_private_)
    display_private_->SetDisplayVSyncParameters(timebase, interval);
}

void Compositor::AddVSyncParameterObserver(
    mojo::PendingRemote<viz::mojom::VSyncParameterObserver> observer) {
  if (display_private_)
    display_private_->AddVSyncParameterObserver(std::move(observer));
}

void Compositor::SetMaxVSyncAndVrr(
    const std::optional<base::TimeDelta>& max_vsync_interval,
    display::VariableRefreshRateState vrr_state) {
  max_vsync_interval_ = max_vsync_interval;
  vrr_state_ = vrr_state;

  if (display_private_) {
    display_private_->SetMaxVSyncAndVrr(max_vsync_interval, vrr_state);
  }
}

void Compositor::SetAcceleratedWidget(gfx::AcceleratedWidget widget) {
  // This function should only get called once.
  DCHECK(!widget_valid_);
  widget_ = widget;
  widget_valid_ = true;
  if (layer_tree_frame_sink_requested_) {
    context_factory_->CreateLayerTreeFrameSink(
        context_creation_weak_ptr_factory_.GetWeakPtr());
  }
}

gfx::AcceleratedWidget Compositor::ReleaseAcceleratedWidget() {
  DCHECK(!IsVisible());
  host_->ReleaseLayerTreeFrameSink();
  display_private_.reset();
  external_begin_frame_controller_.reset();
  context_factory_->RemoveCompositor(this);
  context_creation_weak_ptr_factory_.InvalidateWeakPtrs();
  widget_valid_ = false;
  gfx::AcceleratedWidget widget = widget_;
  widget_ = gfx::kNullAcceleratedWidget;
  return widget;
}

gfx::AcceleratedWidget Compositor::widget() const {
  DCHECK(widget_valid_);
  return widget_;
}

void Compositor::AddObserver(CompositorObserver* observer) {
  observer_list_.AddObserver(observer);
}

void Compositor::RemoveObserver(CompositorObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

bool Compositor::HasObserver(const CompositorObserver* observer) const {
  return observer_list_.HasObserver(observer);
}

void Compositor::AddAnimationObserver(CompositorAnimationObserver* observer) {
  animation_started_ = true;
  if (animation_observer_list_.empty()) {
    observer_list_.Notify(&CompositorObserver::OnFirstAnimationStarted, this);
  }
  observer->Start();
  animation_observer_list_.AddObserver(observer);
  host_->SetNeedsAnimate();
}

void Compositor::RemoveAnimationObserver(
    CompositorAnimationObserver* observer) {
  if (!animation_observer_list_.HasObserver(observer))
    return;

  animation_observer_list_.Notify(&CompositorAnimationObserver::Check);

  animation_observer_list_.RemoveObserver(observer);
  if (animation_observer_list_.empty()) {
    // The only way to get here should be through the AddAnimationObserver.
    DCHECK(animation_started_);

    // Request one more frame so that BeginMainFrame could notify the observers.
    host_->SetNeedsAnimate();
  }
}

bool Compositor::HasAnimationObserver(
    const CompositorAnimationObserver* observer) const {
  return animation_observer_list_.HasObserver(observer);
}

void Compositor::IssueExternalBeginFrame(
    const viz::BeginFrameArgs& args,
    bool force,
    base::OnceCallback<void(const viz::BeginFrameAck&)> callback) {
  if (!external_begin_frame_controller_) {
    // IssueExternalBeginFrame() shouldn't be called again before the previous
    // begin frame is acknowledged.
    DCHECK(!pending_begin_frame_args_);
    pending_begin_frame_args_ = std::make_unique<PendingBeginFrameArgs>(
        args, force, std::move(callback));
    return;
  }
  external_begin_frame_controller_->IssueExternalBeginFrame(
      args, force, std::move(callback));
}

ThroughputTracker Compositor::RequestNewThroughputTracker() {
  return ThroughputTracker(next_throughput_tracker_id_++,
                           weak_ptr_factory_.GetWeakPtr());
}

double Compositor::GetPercentDroppedFrames() const {
  return host_->GetPercentDroppedFrames();
}

std::unique_ptr<cc::EventsMetricsManager::ScopedMonitor>
Compositor::GetScopedEventMetricsMonitor(
    cc::EventsMetricsManager::ScopedMonitor::DoneCallback done_callback) {
  return host_->GetScopedEventMetricsMonitor(std::move(done_callback));
}

void Compositor::DidBeginMainFrame() {
  observer_list_.Notify(&CompositorObserver::OnDidBeginMainFrame, this);
}

void Compositor::DidUpdateLayers() {
  // Dump property trees and layers if run with:
  //   --vmodule=*ui/compositor*=3
  VLOG(3) << "After updating layers:\n"
          << "property trees:\n"
          << host_->property_trees()->ToString() << "\n"
          << "cc::Layers:\n"
          << host_->LayersAsString();
}

void Compositor::BeginMainFrame(const viz::BeginFrameArgs& args) {
  DCHECK(!IsLocked());
  animation_observer_list_.Notify(&CompositorAnimationObserver::OnAnimationStep,
                                  args.frame_time);
  if (!animation_observer_list_.empty()) {
    host_->SetNeedsAnimate();
  } else if (animation_started_) {
    // When |animation_started_| is true but there are no animations observers
    // notify the compositor observers.
    animation_started_ = false;
    observer_list_.Notify(&CompositorObserver::OnFirstNonAnimatedFrameStarted,
                          this);
  }
}

void Compositor::BeginMainFrameNotExpectedSoon() {}

void Compositor::BeginMainFrameNotExpectedUntil(base::TimeTicks time) {}

// static
void Compositor::SendDamagedRectsRecursive(Layer* layer) {
  layer->SendDamagedRects();
  // Iterate using the size for the case of mutation during sending damaged
  // regions. https://crbug.com/1242257.
  base::AutoReset<bool> setter(&(layer->sending_damaged_rects_for_descendants_),
                               true);
  for (size_t i = 0; i < layer->children().size(); ++i)
    SendDamagedRectsRecursive(layer->children()[i]);
}

void Compositor::UpdateLayerTreeHost() {
  if (!root_layer())
    return;
  SendDamagedRectsRecursive(root_layer());
}

void Compositor::RequestNewLayerTreeFrameSink() {
  DCHECK(!layer_tree_frame_sink_requested_);
  layer_tree_frame_sink_requested_ = true;
  if (widget_valid_) {
    context_factory_->CreateLayerTreeFrameSink(
        context_creation_weak_ptr_factory_.GetWeakPtr());
  }
}

void Compositor::DidFailToInitializeLayerTreeFrameSink() {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Compositor::RequestNewLayerTreeFrameSink,
                     context_creation_weak_ptr_factory_.GetWeakPtr()));
}

void Compositor::DidCommit(int source_frame_number,
                           base::TimeTicks,
                           base::TimeTicks) {
  DCHECK(!IsLocked());
  observer_list_.Notify(&CompositorObserver::OnCompositingDidCommit, this);
}

std::unique_ptr<cc::BeginMainFrameMetrics>
Compositor::GetBeginMainFrameMetrics() {
#if BUILDFLAG(IS_CHROMEOS)
  auto metrics_data = std::make_unique<cc::BeginMainFrameMetrics>();
  metrics_data->should_measure_smoothness = true;
  return metrics_data;
#else
  return nullptr;
#endif
}

void Compositor::NotifyThroughputTrackerResults(
    cc::CustomTrackerResults results) {
  for (auto& pair : results)
    ReportMetricsForTracker(pair.first, std::move(pair.second));
}

void Compositor::DidReceiveCompositorFrameAckDeprecatedForCompositor() {
  observer_list_.Notify(&CompositorObserver::OnCompositingAckDeprecated, this);
}

void Compositor::DidPresentCompositorFrame(
    uint32_t frame_token,
    const viz::FrameTimingDetails& frame_timing_details) {
  TRACE_EVENT_MARK_WITH_TIMESTAMP1(
      "cc,benchmark", "FramePresented",
      frame_timing_details.presentation_feedback.timestamp, "environment",
      "browser");
  observer_list_.Notify(&CompositorObserver::OnDidPresentCompositorFrame,
                        frame_token,
                        frame_timing_details.presentation_feedback);
}

void Compositor::DidSubmitCompositorFrame() {
  base::TimeTicks start_time = base::TimeTicks::Now();
  observer_list_.Notify(&CompositorObserver::OnCompositingStarted, this,
                        start_time);
}

void Compositor::FrameIntervalUpdated(base::TimeDelta interval) {
  refresh_rate_ = interval.ToHz();
}

void Compositor::FrameSinksToThrottleUpdated(
    const base::flat_set<viz::FrameSinkId>& ids) {
  observer_list_.Notify(&CompositorObserver::OnFrameSinksToThrottleUpdated,
                        ids);
}

void Compositor::OnFirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
  NOTREACHED_IN_MIGRATION();
}

void Compositor::OnFrameTokenChanged(uint32_t frame_token,
                                     base::TimeTicks activation_time) {
  // TODO(yiyix, fsamuel): Implement frame token propagation for Compositor.
  NOTREACHED_IN_MIGRATION();
}

Compositor::TrackerState::TrackerState() = default;
Compositor::TrackerState::TrackerState(TrackerState&&) = default;
Compositor::TrackerState& Compositor::TrackerState::operator=(TrackerState&&) =
    default;
Compositor::TrackerState::~TrackerState() = default;

void Compositor::StartThroughputTracker(
    TrackerId tracker_id,
    ThroughputTrackerHost::ReportCallback callback) {
  DCHECK(!base::Contains(throughput_tracker_map_, tracker_id));

  auto& tracker_state = throughput_tracker_map_[tracker_id];
  tracker_state.report_callback = std::move(callback);

  animation_host_->StartThroughputTracking(tracker_id);
}

bool Compositor::StopThroughputTracker(TrackerId tracker_id) {
  auto it = throughput_tracker_map_.find(tracker_id);
  CHECK(it != throughput_tracker_map_.end(), base::NotFatalUntil::M130);

  // Clean up if report has happened since StopThroughputTracking would
  // not trigger report in this case.
  if (it->second.report_attempted) {
    throughput_tracker_map_.erase(it);
    return false;
  }

  it->second.should_report = true;
  animation_host_->StopThroughputTracking(tracker_id);
  return true;
}

void Compositor::CancelThroughputTracker(TrackerId tracker_id) {
  auto it = throughput_tracker_map_.find(tracker_id);
  CHECK(it != throughput_tracker_map_.end(), base::NotFatalUntil::M130);

  const bool should_stop = !it->second.report_attempted;

  throughput_tracker_map_.erase(it);

  if (should_stop)
    animation_host_->StopThroughputTracking(tracker_id);
}

void Compositor::OnResume() {
  // Restart the time upon resume.
  for (auto& obs : animation_observer_list_)
    obs.ResetIfActive();
}

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE_X11)
void Compositor::OnCompleteSwapWithNewSize(const gfx::Size& size) {
  observer_list_.Notify(
      &CompositorObserver::OnCompositingCompleteSwapWithNewSize, this, size);
}
#endif  // BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE_X11)

void Compositor::SetOutputIsSecure(bool output_is_secure) {
  output_is_secure_ = output_is_secure;
  if (display_private_)
    display_private_->SetOutputIsSecure(output_is_secure);
}

const cc::LayerTreeDebugState& Compositor::GetLayerTreeDebugState() const {
  return host_->GetDebugState();
}

void Compositor::SetLayerTreeDebugState(
    const cc::LayerTreeDebugState& debug_state) {
  host_->SetDebugState(debug_state);
}

void Compositor::RequestPresentationTimeForNextFrame(
    PresentationTimeCallback callback) {
  host_->RequestPresentationTimeForNextFrame(std::move(callback));
}

void Compositor::RequestSuccessfulPresentationTimeForNextFrame(
    SuccessfulPresentationTimeCallback callback) {
  host_->RequestSuccessfulPresentationTimeForNextFrame(std::move(callback));
}

void Compositor::ReportMetricsForTracker(
    int tracker_id,
    const cc::FrameSequenceMetrics::CustomReportData& data) {
  auto it = throughput_tracker_map_.find(tracker_id);
  if (it == throughput_tracker_map_.end())
    return;

  // Set `report_attempted` but not reporting if relevant ThroughputTrackers
  // are not stopped and waiting for reports.
  if (!it->second.should_report) {
    it->second.report_attempted = true;
    return;
  }

  // Callback may modify `throughput_tracker_map_` so update the map first.
  // See https://crbug.com/1193382.
  auto callback = std::move(it->second.report_callback);
  throughput_tracker_map_.erase(it);
  std::move(callback).Run(data);
}

void Compositor::SetDelegatedInkPointRenderer(
    mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer> receiver) {
  if (display_private_)
    display_private_->SetDelegatedInkPointRenderer(std::move(receiver));
}

const cc::LayerTreeSettings& Compositor::GetLayerTreeSettings() const {
  return host_->GetSettings();
}

void Compositor::AddSimpleBeginFrameObserver(
    ui::HostBeginFrameObserver::SimpleBeginFrameObserver* obs) {
  DCHECK(obs);
  simple_begin_frame_observers_.AddObserver(obs);
  MaybeUpdateObserveBeginFrame();
}

void Compositor::RemoveSimpleBeginFrameObserver(
    ui::HostBeginFrameObserver::SimpleBeginFrameObserver* obs) {
  DCHECK(obs);
  simple_begin_frame_observers_.RemoveObserver(obs);
  MaybeUpdateObserveBeginFrame();
}

void Compositor::MaybeUpdateObserveBeginFrame() {
  if (simple_begin_frame_observers_.empty() || !display_private_) {
    host_begin_frame_observer_.reset();
    return;
  }

  if (host_begin_frame_observer_) {
    return;
  }

  host_begin_frame_observer_ = std::make_unique<ui::HostBeginFrameObserver>(
      simple_begin_frame_observers_, task_runner_);
  display_private_->SetStandaloneBeginFrameObserver(
      host_begin_frame_observer_->GetBoundRemote());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void Compositor::SetSeamlessRefreshRates(
    const std::vector<float>& seamless_refresh_rates) {
  seamless_refresh_rates_ = seamless_refresh_rates;

  if (display_private_) {
    display_private_->SetSupportedRefreshRates(seamless_refresh_rates);
  }
}

void Compositor::OnSetPreferredRefreshRate(float refresh_rate) {
  observer_list_.Notify(&CompositorObserver::OnSetPreferredRefreshRate, this,
                        refresh_rate);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace ui
