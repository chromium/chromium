// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/animation_timeline.h"
#include "cc/base/switches.h"
#include "cc/input/input_handler.h"
#include "cc/layers/layer.h"
#include "cc/trees/latency_info_swap_promise.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_settings.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/common/switches.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/host/renderer_settings_creation.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/compositor_vsync_manager.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/external_begin_frame_client.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator_collection.h"
#include "ui/compositor/overscroll/scroll_input_handler.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"

namespace ui {
namespace {

const char* kDefaultTraceEnvironmentName = "browser";

}  // namespace

Compositor::Compositor(const viz::FrameSinkId& frame_sink_id,
                       ui::ContextFactory* context_factory,
                       ui::ContextFactoryPrivate* context_factory_private,
                       scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                       bool enable_surface_synchronization,
                       bool enable_pixel_canvas,
                       bool external_begin_frames_enabled,
                       bool force_software_compositor,
                       const char* trace_environment_name)
    : context_factory_(context_factory),
      context_factory_private_(context_factory_private),
      frame_sink_id_(frame_sink_id),
      task_runner_(task_runner),
      vsync_manager_(new CompositorVSyncManager()),
      external_begin_frames_enabled_(external_begin_frames_enabled),
      force_software_compositor_(force_software_compositor),
      layer_animator_collection_(this),
      is_pixel_canvas_(enable_pixel_canvas),
      lock_manager_(task_runner),
      trace_environment_name_(trace_environment_name
                                  ? trace_environment_name
                                  : kDefaultTraceEnvironmentName),
      context_creation_weak_ptr_factory_(this) {
  if (context_factory_private) {
    auto* host_frame_sink_manager =
        context_factory_private_->GetHostFrameSinkManager();
    host_frame_sink_manager->RegisterFrameSinkId(
        frame_sink_id_, this, viz::ReportFirstSurfaceActivation::kNo);
    host_frame_sink_manager->SetFrameSinkDebugLabel(frame_sink_id_,
                                                    "Compositor");
  }
  root_web_layer_ = cc::Layer::Create();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  cc::LayerTreeSettings settings;

  // This will ensure PictureLayers always can have LCD text, to match the
  // previous behaviour with ContentLayers, where LCD-not-allowed notifications
  // were ignored.
  settings.layers_always_allowed_lcd_text = true;
  // Use occlusion to allow more overlapping windows to take less memory.
  settings.use_occlusion_for_tile_prioritization = true;
  refresh_rate_ = context_factory_->GetRefreshRate();
  settings.main_frame_before_activation_enabled = false;
  settings.delegated_sync_points_required =
      context_factory_->SyncTokensRequiredForDisplayCompositor();

  // Disable edge anti-aliasing in order to increase support for HW overlays.
  settings.enable_edge_anti_aliasing = false;

  if (command_line->HasSwitch(switches::kLimitFps)) {
    std::string fps_str =
        command_line->GetSwitchValueASCII(switches::kLimitFps);
    double fps;
    if (base::StringToDouble(fps_str, &fps) && fps > 0) {
      forced_refresh_rate_ = fps;
    }
  }

  if (command_line->HasSwitch(cc::switches::kUIShowCompositedLayerBorders)) {
    std::string layer_borders_string = command_line->GetSwitchValueASCII(
        cc::switches::kUIShowCompositedLayerBorders);
    std::vector<base::StringPiece> entries = base::SplitStringPiece(
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
  settings.enable_surface_synchronization = enable_surface_synchronization;
  settings.build_hit_test_data = features::IsVizHitTestingSurfaceLayerEnabled();

  settings.use_zero_copy = IsUIZeroCopyEnabled();

  settings.use_layer_lists =
      command_line->HasSwitch(cc::switches::kUIEnableLayerLists);

  // UI compositor always uses partial raster if not using zero-copy. Zero copy
  // doesn't currently support partial raster.
  settings.use_partial_raster = !settings.use_zero_copy;

  settings.use_rgba_4444 =
      command_line->HasSwitch(switches::kUIEnableRGBA4444Textures);

#if defined(OS_MACOSX)
  // Using CoreAnimation to composite requires using GpuMemoryBuffers, which
  // require zero copy.
  settings.resource_settings.use_gpu_memory_buffer_resources =
      settings.use_zero_copy;
  settings.enable_elastic_overscroll = true;
#endif

  settings.memory_policy.bytes_limit_when_visible = 512 * 1024 * 1024;
  settings.memory_policy.priority_cutoff_when_visible =
      gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE;

  settings.disallow_non_exact_resource_reuse =
      command_line->HasSwitch(switches::kDisallowNonExactResourceReuse);

  if (command_line->HasSwitch(switches::kRunAllCompositorStagesBeforeDraw)) {
    settings.wait_for_all_pipeline_stages_before_draw = true;
    settings.enable_latency_recovery = false;
  }

  settings.always_request_presentation_time =
      command_line->HasSwitch(cc::switches::kAlwaysRequestPresentationTime);

  animation_host_ = cc::AnimationHost::CreateMainInstance();

  cc::LayerTreeHost::InitParams params;
  params.client = this;
  params.task_graph_runner = context_factory_->GetTaskGraphRunner();
  params.settings = &settings;
  params.main_task_runner = task_runner_;
  params.mutator_host = animation_host_.get();
  host_ = cc::LayerTreeHost::CreateSingleThreaded(this, std::move(params));

  if (base::FeatureList::IsEnabled(features::kUiCompositorScrollWithLayers) &&
      host_->GetInputHandler()) {
    scroll_input_handler_.reset(
        new ScrollInputHandler(host_->GetInputHandler()));
  }

  animation_timeline_ =
      cc::AnimationTimeline::Create(cc::AnimationIdProvider::NextTimelineId());
  animation_host_->AddAnimationTimeline(animation_timeline_.get());

  host_->SetHasGpuRasterizationTrigger(features::IsUiGpuRasterizationEnabled());
  host_->SetRootLayer(root_web_layer_);
  host_->SetVisible(true);

  if (command_line->HasSwitch(switches::kUISlowAnimations)) {
    slow_animations_ = std::make_unique<ScopedAnimationDurationScaleMode>(
        ScopedAnimationDurationScaleMode::SLOW_DURATION);
  }
}

Compositor::~Compositor() {
  TRACE_EVENT0("shutdown", "Compositor::destructor");

  for (auto& observer : observer_list_)
    observer.OnCompositingShuttingDown(this);

  for (auto& observer : animation_observer_list_)
    observer.OnCompositingShuttingDown(this);

  if (root_layer_)
    root_layer_->ResetCompositor();

  if (animation_timeline_)
    animation_host_->RemoveAnimationTimeline(animation_timeline_.get());

  // Stop all outstanding draws before telling the ContextFactory to tear
  // down any contexts that the |host_| may rely upon.
  host_.reset();

  context_factory_->RemoveCompositor(this);
  if (context_factory_private_) {
    auto* host_frame_sink_manager =
        context_factory_private_->GetHostFrameSinkManager();
    for (auto& client : child_frame_sinks_) {
      DCHECK(client.is_valid());
      host_frame_sink_manager->UnregisterFrameSinkHierarchy(frame_sink_id_,
                                                            client);
    }
    host_frame_sink_manager->InvalidateFrameSinkId(frame_sink_id_);
  }
}

void Compositor::AddChildFrameSink(const viz::FrameSinkId& frame_sink_id) {
  if (!context_factory_private_)
    return;
  context_factory_private_->GetHostFrameSinkManager()
      ->RegisterFrameSinkHierarchy(frame_sink_id_, frame_sink_id);

  child_frame_sinks_.insert(frame_sink_id);
}

void Compositor::RemoveChildFrameSink(const viz::FrameSinkId& frame_sink_id) {
  if (!context_factory_private_)
    return;
  auto it = child_frame_sinks_.find(frame_sink_id);
  DCHECK(it != child_frame_sinks_.end());
  DCHECK(it->is_valid());
  context_factory_private_->GetHostFrameSinkManager()
      ->UnregisterFrameSinkHierarchy(frame_sink_id_, *it);
  child_frame_sinks_.erase(it);
}

void Compositor::SetLocalSurfaceId(
    const viz::LocalSurfaceId& local_surface_id,
    base::TimeTicks local_surface_id_allocation_time) {
  host_->SetLocalSurfaceIdFromParent(local_surface_id,
                                     local_surface_id_allocation_time);
}

void Compositor::SetLayerTreeFrameSink(
    std::unique_ptr<cc::LayerTreeFrameSink> layer_tree_frame_sink) {
  layer_tree_frame_sink_requested_ = false;
  host_->SetLayerTreeFrameSink(std::move(layer_tree_frame_sink));
  // Display properties are reset when the output surface is lost, so update it
  // to match the Compositor's.
  if (context_factory_private_) {
    context_factory_private_->SetDisplayVisible(this, host_->IsVisible());
    context_factory_private_->SetDisplayColorSpace(this, blending_color_space_,
                                                   output_color_space_);
    context_factory_private_->SetDisplayColorMatrix(this,
                                                    display_color_matrix_);
  }
}

void Compositor::OnChildResizing() {
  for (auto& observer : observer_list_)
    observer.OnCompositingChildResizing(this);
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

cc::AnimationTimeline* Compositor::GetAnimationTimeline() const {
  return animation_timeline_.get();
}

void Compositor::SetDisplayColorMatrix(const SkMatrix44& matrix) {
  display_color_matrix_ = matrix;
  if (context_factory_private_)
    context_factory_private_->SetDisplayColorMatrix(this, matrix);
}

void Compositor::ScheduleFullRedraw() {
  // TODO(enne): Some callers (mac) call this function expecting that it
  // will also commit.  This should probably just redraw the screen
  // from damage and not commit.  ScheduleDraw/ScheduleRedraw need
  // better names.
  host_->SetNeedsRedrawRect(gfx::Rect(host_->device_viewport_size()));
  host_->SetNeedsCommit();
}

void Compositor::ScheduleRedrawRect(const gfx::Rect& damage_rect) {
  // TODO(enne): Make this not commit.  See ScheduleFullRedraw.
  host_->SetNeedsRedrawRect(damage_rect);
  host_->SetNeedsCommit();
}

void Compositor::DisableSwapUntilResize() {
  DCHECK(context_factory_private_);
  context_factory_private_->DisableSwapUntilResize(this);
  disabled_swap_until_resize_ = true;
}

void Compositor::ReenableSwap() {
  DCHECK(context_factory_private_);
  context_factory_private_->ResizeDisplay(this, size_);
}

void Compositor::SetLatencyInfo(const ui::LatencyInfo& latency_info) {
  std::unique_ptr<cc::SwapPromise> swap_promise(
      new cc::LatencyInfoSwapPromise(latency_info));
  host_->QueueSwapPromise(std::move(swap_promise));
}

void Compositor::SetScaleAndSize(
    float scale,
    const gfx::Size& size_in_pixel,
    const viz::LocalSurfaceId& local_surface_id,
    base::TimeTicks local_surface_id_allocation_time) {
  DCHECK_GT(scale, 0);
  bool device_scale_factor_changed = device_scale_factor_ != scale;
  device_scale_factor_ = scale;

  if (size_ != size_in_pixel && local_surface_id.is_valid()) {
    // A new LocalSurfaceId must be set when the compositor size changes.
    DCHECK_NE(local_surface_id, host_->local_surface_id_from_parent());
  }

  if (!size_in_pixel.IsEmpty()) {
    bool size_changed = size_ != size_in_pixel;
    size_ = size_in_pixel;
    host_->SetViewportSizeAndScale(size_in_pixel, scale, local_surface_id,
                                   local_surface_id_allocation_time);
    root_web_layer_->SetBounds(size_in_pixel);
    // TODO(fsamuel): Get rid of ContextFactoryPrivate.
    if (context_factory_private_ &&
        (size_changed || disabled_swap_until_resize_)) {
      context_factory_private_->ResizeDisplay(this, size_in_pixel);
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

void Compositor::SetDisplayColorSpace(const gfx::ColorSpace& color_space) {
  if (output_color_space_ == color_space)
    return;
  output_color_space_ = color_space;
  blending_color_space_ = output_color_space_.GetBlendingColorSpace();
  // Do all ui::Compositor rasterization to sRGB because UI resources will not
  // have their color conversion results cached, and will suffer repeated
  // image color conversions.
  // https://crbug.com/769677
  host_->SetRasterColorSpace(gfx::ColorSpace::CreateSRGB());
  // Always force the ui::Compositor to re-draw all layers, because damage
  // tracking bugs result in black flashes.
  // https://crbug.com/804430
  // TODO(ccameron): Remove this when the above bug is fixed.
  host_->SetNeedsDisplayOnAllLayers();

  // Color space is reset when the output surface is lost, so this must also be
  // updated then.
  // TODO(fsamuel): Get rid of this.
  if (context_factory_private_) {
    context_factory_private_->SetDisplayColorSpace(this, blending_color_space_,
                                                   output_color_space_);
  }
}

void Compositor::SetBackgroundColor(SkColor color) {
  host_->set_background_color(color);
  ScheduleDraw();
}

void Compositor::SetVisible(bool visible) {
  host_->SetVisible(visible);
  // Visibility is reset when the output surface is lost, so this must also be
  // updated then.
  // TODO(fsamuel): Eliminate this call.
  if (context_factory_private_)
    context_factory_private_->SetDisplayVisible(this, visible);
}

bool Compositor::IsVisible() {
  return host_->IsVisible();
}

bool Compositor::ScrollLayerTo(cc::ElementId element_id,
                               const gfx::ScrollOffset& offset) {
  auto input_handler = host_->GetInputHandler();
  return input_handler && input_handler->ScrollLayerTo(element_id, offset);
}

bool Compositor::GetScrollOffsetForLayer(cc::ElementId element_id,
                                         gfx::ScrollOffset* offset) const {
  auto input_handler = host_->GetInputHandler();
  return input_handler &&
         input_handler->GetScrollOffsetForLayer(element_id, offset);
}

void Compositor::SetDisplayVSyncParameters(base::TimeTicks timebase,
                                           base::TimeDelta interval) {
  static bool is_frame_rate_limit_disabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableFrameRateLimit);
  if (is_frame_rate_limit_disabled)
    return;

  if (forced_refresh_rate_) {
    timebase = base::TimeTicks();
    interval = base::TimeDelta::FromSeconds(1) / forced_refresh_rate_;
  }
  if (interval.is_zero()) {
    // TODO(brianderson): We should not be receiving 0 intervals.
    interval = viz::BeginFrameArgs::DefaultInterval();
  }
  DCHECK_GT(interval.InMillisecondsF(), 0);

  // This is called at high frequenty on macOS, so early-out of redundant
  // updates here.
  if (vsync_timebase_ == timebase && vsync_interval_ == interval)
    return;

  vsync_timebase_ = timebase;
  vsync_interval_ = interval;
  refresh_rate_ =
      base::Time::kMillisecondsPerSecond / interval.InMillisecondsF();
  if (context_factory_private_) {
    context_factory_private_->SetDisplayVSyncParameters(this, timebase,
                                                        interval);
  }
  vsync_manager_->UpdateVSyncParameters(timebase, interval);
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

scoped_refptr<CompositorVSyncManager> Compositor::vsync_manager() const {
  return vsync_manager_;
}

void Compositor::IssueExternalBeginFrame(const viz::BeginFrameArgs& args) {
  TRACE_EVENT1("ui", "Compositor::IssueExternalBeginFrame", "args",
               args.AsValue());
  DCHECK(external_begin_frames_enabled_);
  if (context_factory_private_)
    context_factory_private_->IssueExternalBeginFrame(this, args);
}

void Compositor::SetExternalBeginFrameClient(ExternalBeginFrameClient* client) {
  DCHECK(external_begin_frames_enabled_);
  external_begin_frame_client_ = client;
  if (needs_external_begin_frames_ && external_begin_frame_client_)
    external_begin_frame_client_->OnNeedsExternalBeginFrames(true);
}

void Compositor::OnDisplayDidFinishFrame(const viz::BeginFrameAck& ack) {
  DCHECK(external_begin_frames_enabled_);
  if (external_begin_frame_client_)
    external_begin_frame_client_->OnDisplayDidFinishFrame(ack);
}

void Compositor::OnNeedsExternalBeginFrames(bool needs_begin_frames) {
  DCHECK(external_begin_frames_enabled_);
  if (external_begin_frame_client_) {
    external_begin_frame_client_->OnNeedsExternalBeginFrames(
        needs_begin_frames);
  }
  needs_external_begin_frames_ = needs_begin_frames;
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
  animation_observer_list_.AddObserver(observer);
  host_->SetNeedsAnimate();
}

void Compositor::RemoveAnimationObserver(
    CompositorAnimationObserver* observer) {
  animation_observer_list_.RemoveObserver(observer);
}

bool Compositor::HasAnimationObserver(
    const CompositorAnimationObserver* observer) const {
  return animation_observer_list_.HasObserver(observer);
}

void Compositor::BeginMainFrame(const viz::BeginFrameArgs& args) {
  DCHECK(!IsLocked());
  for (auto& observer : animation_observer_list_)
    observer.OnAnimationStep(args.frame_time);
  if (animation_observer_list_.might_have_observers())
    host_->SetNeedsAnimate();
}

void Compositor::BeginMainFrameNotExpectedSoon() {
}

void Compositor::BeginMainFrameNotExpectedUntil(base::TimeTicks time) {}

static void SendDamagedRectsRecursive(ui::Layer* layer) {
  layer->SendDamagedRects();
  for (auto* child : layer->children())
    SendDamagedRectsRecursive(child);
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
  // The LayerTreeFrameSink should already be bound/initialized before being
  // given to
  // the Compositor.
  NOTREACHED();
}

void Compositor::DidCommit() {
  DCHECK(!IsLocked());
  for (auto& observer : observer_list_)
    observer.OnCompositingDidCommit(this);
}

void Compositor::DidReceiveCompositorFrameAck() {
  ++activated_frame_count_;
  for (auto& observer : observer_list_)
    observer.OnCompositingEnded(this);
}

void Compositor::DidPresentCompositorFrame(
    uint32_t frame_token,
    const gfx::PresentationFeedback& feedback) {
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("cc,benchmark", "FramePresented",
                                   feedback.timestamp, "environment",
                                   trace_environment_name_);
}

void Compositor::DidSubmitCompositorFrame() {
  base::TimeTicks start_time = base::TimeTicks::Now();
  for (auto& observer : observer_list_)
    observer.OnCompositingStarted(this, start_time);
}

void Compositor::OnFirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
  NOTREACHED();
}

void Compositor::OnFrameTokenChanged(uint32_t frame_token) {
  // TODO(yiyix, fsamuel): Implement frame token propagation for Compositor.
  NOTREACHED();
}

void Compositor::SetOutputIsSecure(bool output_is_secure) {
  if (context_factory_private_)
    context_factory_private_->SetOutputIsSecure(this, output_is_secure);
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

}  // namespace ui
