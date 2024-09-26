// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_tree_host.h"

#include <optional>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/features.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/env.h"
#include "ui/aura/host_frame_rate_throttler.h"
#include "ui/aura/native_window_occlusion_tracker.h"
#include "ui/aura/scoped_keyboard_hook.h"
#include "ui/aura/scoped_simple_keyboard_hook.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/view_prop.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/switches.h"
#include "ui/platform_window/platform_window_init_properties.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif  // BUILDFLAG(IS_WIN)

namespace aura {

namespace {

const char kWindowTreeHostForAcceleratedWidget[] =
    "__AURA_WINDOW_TREE_HOST_ACCELERATED_WIDGET__";

#if DCHECK_IS_ON()
class ScopedLocalSurfaceIdValidator {
 public:
  explicit ScopedLocalSurfaceIdValidator(Window* window)
      : window_(window),
        local_surface_id_(window ? window->GetLocalSurfaceId()
                                 : viz::LocalSurfaceId()) {}

  ScopedLocalSurfaceIdValidator(const ScopedLocalSurfaceIdValidator&) = delete;
  ScopedLocalSurfaceIdValidator& operator=(
      const ScopedLocalSurfaceIdValidator&) = delete;

  ~ScopedLocalSurfaceIdValidator() {
    if (window_) {
      DCHECK_EQ(local_surface_id_, window_->GetLocalSurfaceId());
    }
  }

 private:
  const raw_ptr<Window> window_;
  const viz::LocalSurfaceId local_surface_id_;
};
#else
class ScopedLocalSurfaceIdValidator {
 public:
  explicit ScopedLocalSurfaceIdValidator(Window* window) {}
  ~ScopedLocalSurfaceIdValidator() {}
};
#endif

}  // namespace

WindowTreeHost::VideoCaptureLock::~VideoCaptureLock() {
  if (host_) {
    if (NativeWindowOcclusionTracker::
            IsNativeWindowOcclusionTrackingAlwaysEnabled(host_.get())) {
      host_->DecrementVideoCaptureCountForOcclusionTracking();
    }
    host_->OnVideoCaptureLockDestroyed();
  }
}

WindowTreeHost::VideoCaptureLock::VideoCaptureLock(WindowTreeHost* host)
    : host_(host->GetWeakPtr()) {
  host_->OnVideoCaptureLockCreated();
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHost, public:

const char WindowTreeHost::kWindowTreeHostUsesParent[] =
    "__AURA_WINDOW_TREE_HOST_USE_PARENT_OF_ACCELERATED_WIDGET__";

WindowTreeHost::~WindowTreeHost() {
  DCHECK(!compositor_) << "compositor must be destroyed before root window";
  DCHECK(!base::Contains(HostFrameRateThrottler::GetInstance().hosts(), this));
}

// static
WindowTreeHost* WindowTreeHost::GetForAcceleratedWidget(
    gfx::AcceleratedWidget widget) {
#if BUILDFLAG(IS_WIN)
  if (ui::ViewProp::GetValue(widget, kWindowTreeHostUsesParent)) {
    widget = ::GetParent(widget);
  }
#endif  // BUILDFLAG(IS_WIN)
  return reinterpret_cast<WindowTreeHost*>(
      ui::ViewProp::GetValue(widget, kWindowTreeHostForAcceleratedWidget));
}

void WindowTreeHost::InitHost() {
  device_scale_factor_ = display::Screen::GetScreen()
                             ->GetPreferredScaleFactorForWindow(window())
                             .value_or(1.f);

  UpdateRootWindowSize();
  InitCompositor();
  Env::GetInstance()->NotifyHostInitialized(this);
}

void WindowTreeHost::AddObserver(WindowTreeHostObserver* observer) {
  observers_.AddObserver(observer);
}

void WindowTreeHost::RemoveObserver(WindowTreeHostObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool WindowTreeHost::HasObserver(const WindowTreeHostObserver* observer) const {
  return observers_.HasObserver(observer);
}

base::WeakPtr<WindowTreeHost> WindowTreeHost::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

gfx::Transform WindowTreeHost::GetRootTransform() const {
  gfx::Transform transform;
  transform.Scale(device_scale_factor_, device_scale_factor_);
  transform *= window()->layer()->transform();
  return transform;
}

void WindowTreeHost::SetRootTransform(const gfx::Transform& transform) {
  window()->SetTransform(transform);
  UpdateRootWindowSize();
}

gfx::Transform WindowTreeHost::GetInverseRootTransform() const {
  gfx::Transform invert;
  gfx::Transform transform = GetRootTransform();
  if (!transform.GetInverse(&invert))
    return transform;
  return invert;
}

void WindowTreeHost::SetDisplayTransformHint(gfx::OverlayTransform transform) {
  if (compositor()->display_transform_hint() == transform)
    return;

  compositor()->SetDisplayTransformHint(transform);
  UpdateCompositorScaleAndSize(GetBoundsInPixels().size());
}

gfx::Transform WindowTreeHost::GetRootTransformForLocalEventCoordinates()
    const {
  return GetRootTransform();
}

gfx::Transform WindowTreeHost::GetInverseRootTransformForLocalEventCoordinates()
    const {
  gfx::Transform invert;
  gfx::Transform transform = GetRootTransformForLocalEventCoordinates();
  if (!transform.GetInverse(&invert))
    return transform;
  return invert;
}

void WindowTreeHost::UpdateCompositorScaleAndSize(
    const gfx::Size& new_size_in_pixels) {
  gfx::Rect new_bounds(new_size_in_pixels);
  if (compositor_->display_transform_hint() ==
          gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90 ||
      compositor_->display_transform_hint() ==
          gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270) {
    new_bounds.Transpose();
  }

  // Allocate a new LocalSurfaceId for the new size or scale factor.
  window_->AllocateLocalSurfaceId();
  ScopedLocalSurfaceIdValidator lsi_validator(window());
  compositor_->SetScaleAndSize(device_scale_factor_, new_bounds.size(),
                               window_->GetLocalSurfaceId());
}

void WindowTreeHost::ConvertDIPToScreenInPixels(gfx::Point* point) const {
  ConvertDIPToPixels(point);
  gfx::Point location = GetLocationOnScreenInPixels();
  point->Offset(location.x(), location.y());
}

void WindowTreeHost::ConvertScreenInPixelsToDIP(gfx::Point* point) const {
  gfx::Point location = GetLocationOnScreenInPixels();
  point->Offset(-location.x(), -location.y());
  ConvertPixelsToDIP(point);
}

void WindowTreeHost::ConvertDIPToPixels(gfx::Point* point) const {
  gfx::PointF point_f{*point};
  ConvertDIPToPixels(&point_f);
  *point = gfx::ToFlooredPoint(point_f);
}

void WindowTreeHost::ConvertDIPToPixels(gfx::PointF* point) const {
  *point = GetRootTransform().MapPoint(*point);
}

void WindowTreeHost::ConvertPixelsToDIP(gfx::Point* point) const {
  gfx::PointF point_f{*point};
  ConvertPixelsToDIP(&point_f);
  *point = gfx::ToFlooredPoint(point_f);
}

void WindowTreeHost::ConvertPixelsToDIP(gfx::PointF* point) const {
  *point = GetInverseRootTransform().MapPoint(*point);
}

void WindowTreeHost::SetCursor(gfx::NativeCursor cursor) {
  last_cursor_ = cursor;
  // A lot of code seems to depend on NULL cursors actually showing an arrow,
  // so just pass everything along to the host.
  SetCursorNative(cursor);
}

void WindowTreeHost::OnCursorVisibilityChanged(bool show) {
  // Clear any existing mouse hover effects when the cursor becomes invisible.
  // Note we do not need to dispatch a mouse enter when the cursor becomes
  // visible because that can only happen in response to a mouse event, which
  // will trigger its own mouse enter.
  if (!show) {
    ui::EventDispatchDetails details = dispatcher()->DispatchMouseExitAtPoint(
        nullptr, dispatcher()->GetLastMouseLocationInRoot(),
        ui::EF_CURSOR_HIDE);
    if (details.dispatcher_destroyed)
      return;
  }

  OnCursorVisibilityChangedNative(show);
}

void WindowTreeHost::MoveCursorToLocationInDIP(
    const gfx::Point& location_in_dip) {
  gfx::Point host_location(location_in_dip);
  ConvertDIPToPixels(&host_location);
  MoveCursorToInternal(location_in_dip, host_location);
}

void WindowTreeHost::MoveCursorToLocationInPixels(
    const gfx::Point& location_in_pixels) {
  gfx::Point root_location(location_in_pixels);
  ConvertPixelsToDIP(&root_location);
  MoveCursorToInternal(root_location, location_in_pixels);
}

ui::InputMethod* WindowTreeHost::GetInputMethod() {
  if (!input_method_) {
    input_method_owned_ = ui::CreateInputMethod(this, GetAcceleratedWidget());
    input_method_ = input_method_owned_.get();
  }
  return input_method_;
}

void WindowTreeHost::SetSharedInputMethod(ui::InputMethod* input_method) {
  input_method_ = input_method;
  input_method_owned_.reset();
}

ui::EventDispatchDetails WindowTreeHost::DispatchKeyEventPostIME(
    ui::KeyEvent* event) {
  // If dispatch to IME is already disabled we shouldn't reach here.
  DCHECK(!dispatcher_->should_skip_ime());
  dispatcher_->set_skip_ime(true);

  // InputMethod::DispatchKeyEvent() is called in PRE_DISPATCH phase, so event
  // target is reset here to avoid issues in subsequent processing phases.
  ui::Event::DispatcherApi(event).set_target(nullptr);

  // We should bypass event rewriters here as they've been tried before.
  ui::EventDispatchDetails dispatch_details =
      GetEventSink()->OnEventFromSource(event);
  if (!dispatch_details.dispatcher_destroyed)
    dispatcher_->set_skip_ime(false);
  return dispatch_details;
}

ui::EventSink* WindowTreeHost::GetEventSink() {
  return dispatcher_.get();
}

int64_t WindowTreeHost::GetDisplayId() {
  return display::Screen::GetScreen()->GetDisplayNearestWindow(window()).id();
}

void WindowTreeHost::Show() {
  // Ensure that compositor has been properly initialized, see InitCompositor()
  // and InitHost().
  DCHECK(compositor());
  DCHECK_EQ(compositor()->root_layer(), window()->layer());
  OnAcceleratedWidgetMadeVisible(true);
  ShowImpl();
  window()->Show();
}

void WindowTreeHost::Hide() {
  HideImpl();
  OnAcceleratedWidgetMadeVisible(false);
}

gfx::Rect WindowTreeHost::GetBoundsInDIP() const {
  aura::Window* root_window = const_cast<aura::Window*>(window());
  display::Screen* screen = display::Screen::GetScreen();
  gfx::Rect screen_bounds = GetBoundsInPixels();
  return screen->ScreenToDIPRectInWindow(root_window, screen_bounds);
}

gfx::Rect WindowTreeHost::GetBoundsInAcceleratedWidgetPixelCoordinates() {
  return gfx::Rect(GetBoundsInPixels().size());
}

std::unique_ptr<ScopedKeyboardHook> WindowTreeHost::CaptureSystemKeyEvents(
    std::optional<base::flat_set<ui::DomCode>> dom_codes) {
  // TODO(joedow): Remove the simple hook class/logic once this flag is removed.
  if (!base::FeatureList::IsEnabled(features::kSystemKeyboardLock))
    return std::make_unique<ScopedSimpleKeyboardHook>(std::move(dom_codes));

  if (CaptureSystemKeyEventsImpl(std::move(dom_codes)))
    return std::make_unique<ScopedKeyboardHook>(weak_factory_.GetWeakPtr());
  return nullptr;
}

bool WindowTreeHost::ShouldSendKeyEventToIme() {
  return true;
}

bool WindowTreeHost::IsNativeWindowOcclusionEnabled() const {
  return native_window_occlusion_enabled_;
}

void WindowTreeHost::SetNativeWindowOcclusionState(
    Window::OcclusionState raw_occlusion_state,
    const SkRegion& raw_occluded_region) {
  raw_occlusion_state_ = raw_occlusion_state;
  raw_occluded_region_ = raw_occluded_region;

  auto state = video_capture_count_for_occlusion_tracking_ > 0
                   ? Window::OcclusionState::VISIBLE
                   : raw_occlusion_state;
  auto occluded_region = video_capture_count_for_occlusion_tracking_ > 0
                             ? SkRegion()
                             : raw_occluded_region;

  if (occlusion_state_ == state && occluded_region_ == occluded_region) {
    return;
  }

  occlusion_state_ = state;
  occluded_region_ = occluded_region;
  MaybeUpdateCompositorVisibilityForNativeOcclusion();

  observers_.Notify(&WindowTreeHostObserver::OnOcclusionStateChanged, this,
                    state, occluded_region);
}

void WindowTreeHost::UpdateRootWindowSize() {
  // Validate that the LocalSurfaceId does not change.
  bool compositor_inited = !!compositor()->root_layer();
  ScopedLocalSurfaceIdValidator lsi_validator(compositor_inited ? window()
                                                                : nullptr);
  window()->SetBounds(CalculateRootWindowBounds());
}

gfx::Rect WindowTreeHost::CalculateRootWindowBounds() const {
  return GetTransformedRootWindowBoundsFromPixelSize(
      GetBoundsInPixels().size());
}

void WindowTreeHost::OnVideoCaptureLockCreated() {}

void WindowTreeHost::OnVideoCaptureLockDestroyed() {}

std::unique_ptr<ScopedEnableUnadjustedMouseEvents>
WindowTreeHost::RequestUnadjustedMovement() {
  NOTIMPLEMENTED();
  return nullptr;
}

bool WindowTreeHost::SupportsMouseLock() {
  return false;
}

void WindowTreeHost::LockMouse(Window* window) {
  Window* root_window = window->GetRootWindow();
  DCHECK(root_window);

  auto* cursor_client = client::GetCursorClient(root_window);
  if (cursor_client) {
    cursor_client->HideCursor();
    cursor_client->LockCursor();
  }
}

void WindowTreeHost::UnlockMouse(Window* window) {
  Window* root_window = window->GetRootWindow();
  DCHECK(root_window);

  if (window->HasCapture())
    window->ReleaseCapture();

  auto* cursor_client = client::GetCursorClient(root_window);
  if (cursor_client) {
    cursor_client->UnlockCursor();
    cursor_client->ShowCursor();
  }
}

std::unique_ptr<WindowTreeHost::VideoCaptureLock>
WindowTreeHost::CreateVideoCaptureLock() {
  if (NativeWindowOcclusionTracker::
          IsNativeWindowOcclusionTrackingAlwaysEnabled(this)) {
    ++video_capture_count_for_occlusion_tracking_;
    MaybeUpdateComposibleVisibilityForVideoLockCountChange();
  }

  // WrapUnique() is used as constructor is private.
  return base::WrapUnique(new VideoCaptureLock(this));
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHost, protected:

WindowTreeHost::WindowTreeHost(std::unique_ptr<Window> window)
    : window_(window.release()) {  // See header for details on ownership.
  if (!window_) {
    window_ = new Window(nullptr);
  }
  device_scale_factor_ = display::Screen::GetScreen()
                             ->GetPreferredScaleFactorForWindow(window_)
                             .value_or(1.f);
#if BUILDFLAG(IS_WIN)
  // The feature state is necessary but not sufficient for checking if
  // occlusion is enabled. It may be disabled by other means (e.g., policy).
  native_window_occlusion_enabled_ =
      !base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kHeadless) &&
      base::FeatureList::IsEnabled(features::kCalculateNativeWinOcclusion);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  native_window_occlusion_enabled_ = true;
#endif
}

void WindowTreeHost::UpdateCompositorVisibility(bool visible) {
  if (!compositor())
    return;

  if (NativeOcclusionAffectsThrottle()) {
    // If NativeOcclusionAffectsThrottle() is true, then this function should
    // only be called if visibility is changed externally. In this case, assume
    // `occlusion_state_` is being ignored, and that throttling should be
    // disabled. For the most part, if NativeOcclusionAffectsThrottle() is true,
    // the handling of occlusion changing is done in
    // SetNativeWindowOcclusionState().
    HostFrameRateThrottler::GetInstance().RemoveHost(this);
  }

  compositor()->SetVisible(visible);
}

void WindowTreeHost::DestroyCompositor() {
  if (!compositor_)
    return;

  HostFrameRateThrottler::GetInstance().RemoveHost(this);

  compositor_->RemoveObserver(this);
  compositor_.reset();
  if (NativeWindowOcclusionTracker::
          IsNativeWindowOcclusionTrackingAlwaysEnabled(this)) {
    NativeWindowOcclusionTracker::DisableNativeWindowOcclusionTracking(this);
  }
}

void WindowTreeHost::DestroyDispatcher() {
  Env::GetInstance()->NotifyHostDestroyed(this);
  delete window_;
  window_ = nullptr;
  dispatcher_.reset();

  // TODO(beng): this comment is no longer quite valid since this function
  // isn't called from WED, and WED isn't a subclass of Window. So it seems
  // like we could just rely on ~Window now.
  // Destroy child windows while we're still valid. This is also done by
  // ~Window, but by that time any calls to virtual methods overridden here
  // (such as GetRootWindow()) result in Window's implementation. By destroying
  // here we ensure GetRootWindow() still returns this.
  // window()->RemoveOrDestroyChildren();
}

void WindowTreeHost::OnAcceleratedWidgetMadeVisible(bool value) {
  if (accelerated_widget_made_visible_ == value)
    return;

  accelerated_widget_made_visible_ = value;
  // Always update the compositor (ignoring occlusion-state) as it is entirely
  // possible the occlusion-state is out of date at this point. It is expected
  // that the proper occlusion state is provided soon after this.
  UpdateCompositorVisibility(value);
}

void WindowTreeHost::CreateCompositor(bool force_software_compositor,
                                      bool use_external_begin_frame_control,
                                      bool enable_compositing_based_throttling,
                                      size_t memory_limit_when_visible_mb) {
  Env* env = Env::GetInstance();
  ui::ContextFactory* context_factory = env->context_factory();
  DCHECK(context_factory);
  compositor_ = std::make_unique<ui::Compositor>(
      context_factory->AllocateFrameSinkId(), context_factory,
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      ui::IsPixelCanvasRecordingEnabled(), use_external_begin_frame_control,
      force_software_compositor, enable_compositing_based_throttling,
      memory_limit_when_visible_mb);
  compositor_->AddObserver(this);
  if (!dispatcher()) {
    window()->Init(ui::LAYER_NOT_DRAWN);
    window()->set_host(this);
    window()->SetName("RootWindow");
    dispatcher_ = std::make_unique<WindowEventDispatcher>(this);
  }
}

void WindowTreeHost::InitCompositor() {
  DCHECK(!compositor_->root_layer());
  compositor_->SetScaleAndSize(device_scale_factor_, GetBoundsInPixels().size(),
                               window()->GetLocalSurfaceId());
  compositor_->SetRootLayer(window()->layer());

  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window());
  compositor_->SetDisplayColorSpaces(display.GetColorSpaces());
}

void WindowTreeHost::OnAcceleratedWidgetAvailable() {
  compositor_->SetAcceleratedWidget(GetAcceleratedWidget());
  prop_ = std::make_unique<ui::ViewProp>(
      GetAcceleratedWidget(), kWindowTreeHostForAcceleratedWidget, this);
  if (NativeWindowOcclusionTracker::
          IsNativeWindowOcclusionTrackingAlwaysEnabled(this)) {
    NativeWindowOcclusionTracker::EnableNativeWindowOcclusionTracking(this);
  }
}

void WindowTreeHost::OnHostMovedInPixels() {
  TRACE_EVENT0("ui", "WindowTreeHost::OnHostMovedInPixels");

  observers_.Notify(&WindowTreeHostObserver::OnHostMovedInPixels, this);
}

void WindowTreeHost::OnHostResizedInPixels(
    const gfx::Size& new_size_in_pixels) {
  // The compositor is deleted from WM_DESTROY, but we don't delete things until
  // WM_NCDESTROY, and it must be possible to still get some messages between
  // these two.
  if (!compositor_)
    return;

  // If we don't have the actual preferred scale, don't overwrite the scale
  // factor with the default value. See https://crbug.com/1285476 for details.
  auto* screen = display::Screen::GetScreen();
  if (auto scale = screen->GetPreferredScaleFactorForWindow(window())) {
    device_scale_factor_ = scale.value();
  }

  UpdateRootWindowSize();

  // Passing |new_size_in_pixels| to set compositor size. It could be different
  // from GetBoundsInPixels() on Windows to contain extra space for window
  // transition animations and should be used to set compositor size instead of
  // GetBoundsInPixels() in such case.
  UpdateCompositorScaleAndSize(new_size_in_pixels);

  observers_.Notify(&WindowTreeHostObserver::OnHostResized, this);
}

void WindowTreeHost::OnHostWorkspaceChanged() {
  observers_.Notify(&WindowTreeHostObserver::OnHostWorkspaceChanged, this);
}

void WindowTreeHost::OnHostDisplayChanged() {
  if (!compositor_)
    return;
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window());
  compositor_->SetDisplayColorSpaces(display.GetColorSpaces());
}

void WindowTreeHost::OnHostCloseRequested() {
  observers_.Notify(&WindowTreeHostObserver::OnHostCloseRequested, this);
}

void WindowTreeHost::OnHostLostWindowCapture() {
  // It is possible for this function to be called during destruction, after the
  // root window has already been destroyed (e.g. when the ui::PlatformWindow is
  // destroyed, and during destruction, it loses capture. See more details in
  // http://crbug.com/770670)
  if (!window())
    return;
  Window* capture_window = client::GetCaptureWindow(window());
  if (capture_window && capture_window->GetRootWindow() == window())
    capture_window->ReleaseCapture();
}

void WindowTreeHost::OnDisplayMetricsChanged(const display::Display& display,
                                             uint32_t metrics) {
  if (metrics & DisplayObserver::DISPLAY_METRIC_COLOR_SPACE && compositor_ &&
      display.id() == GetDisplayId())
    compositor_->SetDisplayColorSpaces(display.GetColorSpaces());

// Chrome OS is handled in WindowTreeHostManager::OnDisplayMetricsChanged.
// Chrome OS requires additional handling for the bounds that we do not need to
// do for other OSes.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if (metrics & DISPLAY_METRIC_DEVICE_SCALE_FACTOR &&
      display.id() == GetDisplayId())
    OnHostResizedInPixels(GetBoundsInPixels().size());
#endif
}

gfx::Rect WindowTreeHost::GetTransformedRootWindowBoundsFromPixelSize(
    const gfx::Size& size_in_pixels) const {
  return GetInverseRootTransform().MapRect(gfx::Rect(size_in_pixels));
}

void WindowTreeHost::SetNativeWindowOcclusionEnabled(bool enable) {
  native_window_occlusion_enabled_ = enable;
  // TODO(crbug.com/40118412) If enabled is false, make this
  // turn off native window occlusion on this window.
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHost, private:

void WindowTreeHost::DecrementVideoCaptureCountForOcclusionTracking() {
  DCHECK_GT(video_capture_count_for_occlusion_tracking_, 0);
  --video_capture_count_for_occlusion_tracking_;
  MaybeUpdateComposibleVisibilityForVideoLockCountChange();
}

void WindowTreeHost::MaybeUpdateComposibleVisibilityForVideoLockCountChange() {
  // Only need to check for changes when transitioning between lock and no lock.
  if (video_capture_count_for_occlusion_tracking_ > 1) {
    return;
  }
  // If we no longer have video capture locks, update the occlusion state to
  // what the platform last sent us.
  SetNativeWindowOcclusionState(raw_occlusion_state_, raw_occluded_region_);
  MaybeUpdateCompositorVisibilityForNativeOcclusion();
}

void WindowTreeHost::MaybeUpdateCompositorVisibilityForNativeOcclusion() {
  if (!compositor() || !accelerated_widget_made_visible_ ||
      !NativeWindowOcclusionTracker::
          IsNativeWindowOcclusionTrackingAlwaysEnabled(this)) {
    return;
  }
  const bool visible = CalculateCompositorVisibilityFromOcclusionState();
  if (visible != compositor()->IsVisible()) {
    UpdateCompositorVisibility(visible);
  }

  if (NativeOcclusionAffectsThrottle()) {
    if (ShouldThrottle()) {
      HostFrameRateThrottler::GetInstance().AddHost(this);
    } else {
      HostFrameRateThrottler::GetInstance().RemoveHost(this);
    }
  }
}

bool WindowTreeHost::CalculateCompositorVisibilityFromOcclusionState() const {
  // For example, visibility should not be changed if we should only change
  // throttle status.
  if (!NativeOcclusionAffectsVisibility()) {
    return true;
  }
  switch (occlusion_state_) {
    case Window::OcclusionState::UNKNOWN:
      return true;
    case Window::OcclusionState::VISIBLE:
      return true;
    case Window::OcclusionState::OCCLUDED: {
      // TODO(crbug.com/40208263): For lacros, make sure non-maximized but
      // occluded windows are visible.
      // The compositor needs to be visible when capturing video.
      return video_capture_count_for_occlusion_tracking_ != 0;
    }
    case Window::OcclusionState::HIDDEN:
      // TODO: On windows, this likely needs other changes to really work
      // (such as when an HWND is iconified it is sized to 0x0).
      return video_capture_count_for_occlusion_tracking_ != 0;
  }
}

bool WindowTreeHost::NativeOcclusionAffectsThrottle() const {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!base::FeatureList::IsEnabled(
          features::kApplyNativeOcclusionToCompositor) ||
      !IsNativeWindowOcclusionEnabled()) {
    return false;
  }

  const std::string type =
      features::kApplyNativeOcclusionToCompositorType.Get();
  return type == features::kApplyNativeOcclusionToCompositorTypeThrottle ||
         type ==
             features::kApplyNativeOcclusionToCompositorTypeThrottleAndRelease;
#else
  return false;
#endif
}

bool WindowTreeHost::NativeOcclusionAffectsVisibility() const {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!base::FeatureList::IsEnabled(
          features::kApplyNativeOcclusionToCompositor) ||
      !IsNativeWindowOcclusionEnabled()) {
    return false;
  }

  const std::string type =
      features::kApplyNativeOcclusionToCompositorType.Get();
  return type == features::kApplyNativeOcclusionToCompositorTypeRelease ||
         type ==
             features::kApplyNativeOcclusionToCompositorTypeThrottleAndRelease;
#else
  return false;
#endif
}

bool WindowTreeHost::ShouldThrottle() const {
  // Only throttle if allowed and there are no video captures and we are
  // occluded.
  DCHECK(NativeOcclusionAffectsThrottle());
  return video_capture_count_for_occlusion_tracking_ == 0 &&
         occlusion_state_ == Window::OcclusionState::OCCLUDED;
}

// static
const base::flat_set<raw_ptr<WindowTreeHost, CtnExperimental>>&
WindowTreeHost::GetThrottledHostsForTesting() {
  return HostFrameRateThrottler::GetInstance().hosts();
}

void WindowTreeHost::MoveCursorToInternal(const gfx::Point& root_location,
                                          const gfx::Point& host_location) {
  last_cursor_request_position_in_host_ = host_location;
  MoveCursorToScreenLocationInPixels(host_location);
  client::CursorClient* cursor_client = client::GetCursorClient(window());
  if (cursor_client) {
    const display::Display& display =
        display::Screen::GetScreen()->GetDisplayNearestWindow(window());
    cursor_client->SetDisplay(display);
  }
  dispatcher()->OnCursorMovedToRootLocation(root_location);
}

void WindowTreeHost::OnCompositingAckDeprecated(ui::Compositor* compositor) {
  // Currently, input is only throttled on ash and is not well supported on
  // other platforms. See crbug.com/41359082.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!holding_pointer_moves_)
    return;

  dispatcher_->ReleasePointerMoves();
  holding_pointer_moves_ = false;
#endif
}

void WindowTreeHost::OnCompositingChildResizing(ui::Compositor* compositor) {
  // Currently, input is only throttled on ash and is not well supported on
  // other platforms. See crbug.com/41359082.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!Env::GetInstance()->throttle_input_on_resize() || holding_pointer_moves_)
    return;
  dispatcher_->HoldPointerMoves();
  holding_pointer_moves_ = true;
#endif
}

void WindowTreeHost::OnFrameSinksToThrottleUpdated(
    const base::flat_set<viz::FrameSinkId>& ids) {
  observers_.Notify(
      &WindowTreeHostObserver::OnCompositingFrameSinksToThrottleUpdated, this,
      ids);
}

void WindowTreeHost::OnSetPreferredRefreshRate(ui::Compositor*,
                                               float preferred_refresh_rate) {
  observers_.Notify(&WindowTreeHostObserver::OnSetPreferredRefreshRate, this,
                    preferred_refresh_rate);
}

}  // namespace aura
