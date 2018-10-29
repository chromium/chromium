// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_tree_host.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/features.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/env.h"
#include "ui/aura/scoped_keyboard_hook.h"
#include "ui/aura/scoped_simple_keyboard_hook.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/base/layout.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/view_prop.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/dip_util.h"
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
#include "ui/platform_window/platform_window_init_properties.h"

namespace aura {

namespace {

const char kWindowTreeHostForAcceleratedWidget[] =
    "__AURA_WINDOW_TREE_HOST_ACCELERATED_WIDGET__";

bool ShouldAllocateLocalSurfaceId(Window* window) {
  // When running with the window service (either in 'mus' or 'mash' mode), the
  // LocalSurfaceId allocation for the WindowTreeHost is managed by the
  // WindowTreeClient and WindowTreeHostMus.
  return window->env()->mode() == Env::Mode::LOCAL;
}

#if DCHECK_IS_ON()
class ScopedLocalSurfaceIdValidator {
 public:
  explicit ScopedLocalSurfaceIdValidator(Window* window)
      : window_(window),
        local_surface_id_(window ? window->GetLocalSurfaceId()
                                 : viz::LocalSurfaceId()) {}
  ~ScopedLocalSurfaceIdValidator() {
    if (window_ && ShouldAllocateLocalSurfaceId(window_))
      DCHECK_EQ(local_surface_id_, window_->GetLocalSurfaceId());
  }

 private:
  Window* const window_;
  const viz::LocalSurfaceId local_surface_id_;
  DISALLOW_COPY_AND_ASSIGN(ScopedLocalSurfaceIdValidator);
};
#else
class ScopedLocalSurfaceIdValidator {
 public:
  explicit ScopedLocalSurfaceIdValidator(Window* window) {}
  ~ScopedLocalSurfaceIdValidator() {}
};
#endif

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHost, public:

WindowTreeHost::~WindowTreeHost() {
  if (display::Screen::GetScreen())
    display::Screen::GetScreen()->RemoveObserver(this);
  DCHECK(!compositor_) << "compositor must be destroyed before root window";
  if (owned_input_method_) {
    delete input_method_;
    input_method_ = nullptr;
  }
}

// static
WindowTreeHost* WindowTreeHost::GetForAcceleratedWidget(
    gfx::AcceleratedWidget widget) {
  return reinterpret_cast<WindowTreeHost*>(
      ui::ViewProp::GetValue(widget, kWindowTreeHostForAcceleratedWidget));
}

void WindowTreeHost::InitHost() {
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window());
  device_scale_factor_ = display.device_scale_factor();

  UpdateRootWindowSizeInPixels();
  InitCompositor();
  window()->env()->NotifyHostInitialized(this);
}

void WindowTreeHost::AddObserver(WindowTreeHostObserver* observer) {
  observers_.AddObserver(observer);
}

void WindowTreeHost::RemoveObserver(WindowTreeHostObserver* observer) {
  observers_.RemoveObserver(observer);
}

ui::EventSink* WindowTreeHost::event_sink() {
  return dispatcher_.get();
}

gfx::Transform WindowTreeHost::GetRootTransform() const {
  gfx::Transform transform;
  transform.Scale(device_scale_factor_, device_scale_factor_);
  transform *= window()->layer()->transform();
  return transform;
}

void WindowTreeHost::SetRootTransform(const gfx::Transform& transform) {
  window()->SetTransform(transform);
  UpdateRootWindowSizeInPixels();
}

gfx::Transform WindowTreeHost::GetInverseRootTransform() const {
  gfx::Transform invert;
  gfx::Transform transform = GetRootTransform();
  if (!transform.GetInverse(&invert))
    return transform;
  return invert;
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

void WindowTreeHost::UpdateRootWindowSizeInPixels() {
  // Validate that the LocalSurfaceId does not change.
  bool compositor_inited = !!compositor()->root_layer();
  ScopedLocalSurfaceIdValidator lsi_validator(compositor_inited ? window()
                                                                : nullptr);
  gfx::Rect transformed_bounds_in_pixels =
      GetTransformedRootWindowBoundsInPixels(GetBoundsInPixels().size());
  window()->SetBounds(transformed_bounds_in_pixels);
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
  auto point_3f = gfx::Point3F(gfx::PointF(*point));
  GetRootTransform().TransformPoint(&point_3f);
  *point = gfx::ToFlooredPoint(point_3f.AsPointF());
}

void WindowTreeHost::ConvertPixelsToDIP(gfx::Point* point) const {
  auto point_3f = gfx::Point3F(gfx::PointF(*point));
  GetInverseRootTransform().TransformPoint(&point_3f);
  *point = gfx::ToFlooredPoint(point_3f.AsPointF());
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
    input_method_ =
        ui::CreateInputMethod(this, GetAcceleratedWidget()).release();
    owned_input_method_ = true;
  }
  return input_method_;
}

void WindowTreeHost::SetSharedInputMethod(ui::InputMethod* input_method) {
  DCHECK(!input_method_);
  input_method_ = input_method;
  owned_input_method_ = false;
}

ui::EventDispatchDetails WindowTreeHost::DispatchKeyEventPostIME(
    ui::KeyEvent* event,
    base::OnceCallback<void(bool)> ack_callback) {
  // If dispatch to IME is already disabled we shouldn't reach here.
  DCHECK(!dispatcher_->should_skip_ime());
  dispatcher_->set_skip_ime(true);
  // We should bypass event rewriters here as they've been tried before.
  ui::EventDispatchDetails dispatch_details =
      event_sink()->OnEventFromSource(event);
  if (!dispatch_details.dispatcher_destroyed)
    dispatcher_->set_skip_ime(false);
  CallDispatchKeyEventPostIMEAck(event, std::move(ack_callback));
  return dispatch_details;
}

int64_t WindowTreeHost::GetDisplayId() {
  return display::Screen::GetScreen()->GetDisplayNearestWindow(window()).id();
}

void WindowTreeHost::Show() {
  // Ensure that compositor has been properly initialized, see InitCompositor()
  // and InitHost().
  DCHECK(compositor());
  DCHECK_EQ(compositor()->root_layer(), window()->layer());
  compositor()->SetVisible(true);
  ShowImpl();
  window()->Show();
}

void WindowTreeHost::Hide() {
  HideImpl();
  if (compositor())
    compositor()->SetVisible(false);
}

std::unique_ptr<ScopedKeyboardHook> WindowTreeHost::CaptureSystemKeyEvents(
    base::Optional<base::flat_set<ui::DomCode>> dom_codes) {
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

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHost, protected:

WindowTreeHost::WindowTreeHost(std::unique_ptr<Window> window)
    : window_(window.release()),  // See header for details on ownership.
      last_cursor_(ui::CursorType::kNull),
      input_method_(nullptr),
      owned_input_method_(false),
      weak_factory_(this) {
  if (!window_)
    window_ = new Window(nullptr);
  display::Screen::GetScreen()->AddObserver(this);
}

void WindowTreeHost::DestroyCompositor() {
  if (compositor_) {
    compositor_->RemoveObserver(this);
    compositor_.reset();
  }
}

void WindowTreeHost::DestroyDispatcher() {
  delete window_;
  window_ = nullptr;
  dispatcher_.reset();

  // TODO(beng): this comment is no longer quite valid since this function
  // isn't called from WED, and WED isn't a subclass of Window. So it seems
  // like we could just rely on ~Window now.
  // Destroy child windows while we're still valid. This is also done by
  // ~Window, but by that time any calls to virtual methods overriden here (such
  // as GetRootWindow()) result in Window's implementation. By destroying here
  // we ensure GetRootWindow() still returns this.
  //window()->RemoveOrDestroyChildren();
}

void WindowTreeHost::CreateCompositor(const viz::FrameSinkId& frame_sink_id,
                                      bool force_software_compositor,
                                      bool external_begin_frames_enabled,
                                      bool are_events_in_pixels,
                                      const char* trace_environment_name) {
  DCHECK(window()->env());
  Env* env = window()->env();
  ui::ContextFactory* context_factory = env->context_factory();
  DCHECK(context_factory);
  ui::ContextFactoryPrivate* context_factory_private =
      env->context_factory_private();
  bool enable_surface_synchronization =
      env->mode() == aura::Env::Mode::MUS ||
      features::IsSurfaceSynchronizationEnabled();
  compositor_ = std::make_unique<ui::Compositor>(
      (!context_factory_private || frame_sink_id.is_valid())
          ? frame_sink_id
          : context_factory_private->AllocateFrameSinkId(),
      context_factory, context_factory_private,
      base::ThreadTaskRunnerHandle::Get(), enable_surface_synchronization,
      ui::IsPixelCanvasRecordingEnabled(), external_begin_frames_enabled,
      force_software_compositor, trace_environment_name);
#if defined(OS_CHROMEOS)
  compositor_->AddObserver(this);
#endif
  if (!dispatcher()) {
    window()->Init(ui::LAYER_NOT_DRAWN);
    window()->set_host(this);
    window()->SetName("RootWindow");
    dispatcher_ =
        std::make_unique<WindowEventDispatcher>(this, are_events_in_pixels);
  }
}

void WindowTreeHost::InitCompositor() {
  DCHECK(!compositor_->root_layer());
  compositor_->SetScaleAndSize(device_scale_factor_, GetBoundsInPixels().size(),
                               window()->GetLocalSurfaceId(),
                               window()->GetLocalSurfaceIdAllocationTime());
  compositor_->SetRootLayer(window()->layer());

  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window());
  compositor_->SetDisplayColorSpace(display.color_space());
}

void WindowTreeHost::OnAcceleratedWidgetAvailable() {
  compositor_->SetAcceleratedWidget(GetAcceleratedWidget());
  prop_.reset(new ui::ViewProp(GetAcceleratedWidget(),
                               kWindowTreeHostForAcceleratedWidget, this));
}

void WindowTreeHost::OnHostMovedInPixels(
    const gfx::Point& new_location_in_pixels) {
  TRACE_EVENT1("ui", "WindowTreeHost::OnHostMovedInPixels", "origin",
               new_location_in_pixels.ToString());

  for (WindowTreeHostObserver& observer : observers_)
    observer.OnHostMovedInPixels(this, new_location_in_pixels);
}

void WindowTreeHost::OnHostResizedInPixels(
    const gfx::Size& new_size_in_pixels,
    const viz::LocalSurfaceId& new_local_surface_id,
    base::TimeTicks new_allocation_time) {
  // TODO(jonross) Unify all OnHostResizedInPixels to have both
  // viz::LocalSurfaceId and allocation time as optional parameters.
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window());
  device_scale_factor_ = display.device_scale_factor();
  UpdateRootWindowSizeInPixels();

  // Allocate a new LocalSurfaceId for the new state.
  auto local_surface_id = new_local_surface_id;
  auto allocation_time = new_allocation_time;
  if (ShouldAllocateLocalSurfaceId(window()) &&
      !new_local_surface_id.is_valid()) {
    window_->AllocateLocalSurfaceId();
    local_surface_id = window_->GetLocalSurfaceId();
    allocation_time = window_->GetLocalSurfaceIdAllocationTime();
  }
  ScopedLocalSurfaceIdValidator lsi_validator(window());
  compositor_->SetScaleAndSize(device_scale_factor_, new_size_in_pixels,
                               local_surface_id, allocation_time);

  for (WindowTreeHostObserver& observer : observers_)
    observer.OnHostResized(this);
}

void WindowTreeHost::OnHostWorkspaceChanged() {
  for (WindowTreeHostObserver& observer : observers_)
    observer.OnHostWorkspaceChanged(this);
}

void WindowTreeHost::OnHostDisplayChanged() {
  if (!compositor_)
    return;
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window());
  compositor_->SetDisplayColorSpace(display.color_space());
}

void WindowTreeHost::OnHostCloseRequested() {
  for (WindowTreeHostObserver& observer : observers_)
    observer.OnHostCloseRequested(this);
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

ui::EventSink* WindowTreeHost::GetEventSink() {
  return dispatcher_.get();
}

void WindowTreeHost::OnDisplayMetricsChanged(const display::Display& display,
                                             uint32_t metrics) {
  if (metrics & DisplayObserver::DISPLAY_METRIC_COLOR_SPACE) {
    display::Screen* screen = display::Screen::GetScreen();
    if (compositor_ &&
        display.id() == screen->GetDisplayNearestView(window()).id()) {
      compositor_->SetDisplayColorSpace(display.color_space());
    }
  }
}

gfx::Rect WindowTreeHost::GetTransformedRootWindowBoundsInPixels(
    const gfx::Size& size_in_pixels) const {
  gfx::Rect bounds(size_in_pixels);
  gfx::RectF new_bounds =
      gfx::ScaleRect(gfx::RectF(bounds), 1.0f / device_scale_factor_);
  window()->layer()->transform().TransformRect(&new_bounds);
  return gfx::ToEnclosingRect(new_bounds);
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHost, private:

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

void WindowTreeHost::OnCompositingDidCommit(ui::Compositor* compositor) {
  if (!holding_pointer_moves_)
    return;

  dispatcher_->ReleasePointerMoves();
  holding_pointer_moves_ = false;
  DCHECK(!synchronization_start_time_.is_null());
  UMA_HISTOGRAM_TIMES("UI.WindowTreeHost.SurfaceSynchronizationDuration",
                      base::TimeTicks::Now() - synchronization_start_time_);
}

void WindowTreeHost::OnCompositingStarted(ui::Compositor* compositor,
                                          base::TimeTicks start_time) {}

void WindowTreeHost::OnCompositingEnded(ui::Compositor* compositor) {}

void WindowTreeHost::OnCompositingChildResizing(ui::Compositor* compositor) {
  if (!window()->env()->throttle_input_on_resize() || holding_pointer_moves_)
    return;
  synchronization_start_time_ = base::TimeTicks::Now();
  dispatcher_->HoldPointerMoves();
  holding_pointer_moves_ = true;
}

void WindowTreeHost::OnCompositingShuttingDown(ui::Compositor* compositor) {
  compositor->RemoveObserver(this);
}

}  // namespace aura
