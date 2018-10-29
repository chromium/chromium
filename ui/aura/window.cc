// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "services/ws/public/mojom/window_tree_constants.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/event_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/client/visibility_client.h"
#include "ui/aura/client/window_stacking_client.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/scoped_keyboard_hook.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/aura/window_port.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tracker.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event_target_iterator.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/path.h"
#include "ui/gfx/scoped_canvas.h"

namespace aura {
namespace {
#if DCHECK_IS_ON()
const char* g_env_arg_required_string = nullptr;
#endif

}  // namespace

Window::Window(WindowDelegate* delegate, client::WindowType type, Env* env)
    : Window(delegate, nullptr, type, env) {}

Window::Window(WindowDelegate* delegate,
               std::unique_ptr<WindowPort> port,
               client::WindowType type,
               Env* env)
    : env_(env ? env : Env::GetInstance()),
      port_owner_(std::move(port)),
      port_(port_owner_.get()),
      host_(nullptr),
      type_(type),
      owned_by_parent_(true),
      delegate_(delegate),
      parent_(nullptr),
      visible_(false),
      occlusion_state_(OcclusionState::UNKNOWN),
      id_(kInitialId),
      transparent_(false),
      event_targeting_policy_(
          ws::mojom::EventTargetingPolicy::TARGET_AND_DESCENDANTS),
      // Don't notify newly added observers during notification. This causes
      // problems for code that adds an observer as part of an observer
      // notification (such as the workspace code).
      observers_(base::ObserverListPolicy::EXISTING_ONLY) {
  SetTargetHandler(delegate_);
#if DCHECK_IS_ON()
  // NOTE: at the time of adding this this function is only called from ash.
  DCHECK(env || !g_env_arg_required_string) << g_env_arg_required_string;
#endif
}

Window::~Window() {
  WindowOcclusionTracker::ScopedPause pause_occlusion_tracking(env_);

  if (layer()->owner() == this)
    layer()->CompleteAllAnimations();
  layer()->SuppressPaint();

  // Let the delegate know we're in the processing of destroying.
  if (delegate_)
    delegate_->OnWindowDestroying(this);
  for (WindowObserver& observer : observers_)
    observer.OnWindowDestroying(this);

  // While we are being destroyed, our target handler may also be in the
  // process of destruction or already destroyed, so do not forward any
  // input events at the ui::EP_TARGET phase.
  SetTargetHandler(nullptr);

  // TODO(beng): See comment in window_event_dispatcher.h. This shouldn't be
  //             necessary but unfortunately is right now due to ordering
  //             peculiarities. WED must be notified _after_ other observers
  //             are notified of pending teardown but before the hierarchy
  //             is actually torn down.
  WindowTreeHost* host = GetHost();
  if (host)
    host->dispatcher()->OnPostNotifiedWindowDestroying(this);

  // The window should have already had its state cleaned up in
  // WindowEventDispatcher::OnWindowHidden(), but there have been some crashes
  // involving windows being destroyed without being hidden first. See
  // crbug.com/342040. This should help us debug the issue. TODO(tdresser):
  // remove this once we determine why we have windows that are destroyed
  // without being hidden.
  bool window_incorrectly_cleaned_up = CleanupGestureState();
  CHECK(!window_incorrectly_cleaned_up);

  // Then destroy the children.
  RemoveOrDestroyChildren();

  // The window needs to be removed from the parent before calling the
  // WindowDestroyed callbacks of delegate and the observers.
  if (parent_)
    parent_->RemoveChild(this);

  if (delegate_)
    delegate_->OnWindowDestroyed(this);
  for (WindowObserver& observer : observers_) {
    RemoveObserver(&observer);
    observer.OnWindowDestroyed(this);
  }

  // Delete the LayoutManager before properties. This way if the LayoutManager
  // depends upon properties existing the properties are still valid.
  layout_manager_.reset();

  ClearProperties();

  // The layer will either be destroyed by |layer_owner_|'s dtor, or by whoever
  // acquired it.
  layer()->set_delegate(NULL);
  DestroyLayer();

  // Delete the WindowPort now, in case it needs to reach back into the Window
  // during destruction.
  port_owner_.reset();
  port_ = nullptr;
}

void Window::Init(ui::LayerType layer_type) {
  WindowOcclusionTracker::ScopedPause pause_occlusion_tracking(env_);

  if (!port_owner_) {
    port_owner_ = env_->CreateWindowPort(this);
    port_ = port_owner_.get();
  }
  SetLayer(std::make_unique<ui::Layer>(layer_type));
  port_->OnPreInit(this);
  layer()->SetVisible(false);
  layer()->set_delegate(this);
  UpdateLayerName();
  layer()->SetFillsBoundsOpaquely(!transparent_);
  env_->NotifyWindowInitialized(this);
}

void Window::SetType(client::WindowType type) {
  // Cannot change type after the window is initialized.
  DCHECK(!layer());
  type_ = type;
}

const std::string& Window::GetName() const {
  std::string* name = GetProperty(client::kNameKey);
  return name ? *name : base::EmptyString();
}

void Window::SetName(const std::string& name) {
  if (name == GetName())
    return;
  SetProperty(client::kNameKey, new std::string(name));
  if (layer())
    UpdateLayerName();
}

const base::string16& Window::GetTitle() const {
  base::string16* title = GetProperty(client::kTitleKey);
  return title ? *title : base::EmptyString16();
}

void Window::SetTitle(const base::string16& title) {
  if (title == GetTitle())
    return;
  SetProperty(client::kTitleKey, new base::string16(title));
  for (WindowObserver& observer : observers_)
    observer.OnWindowTitleChanged(this);
}

void Window::SetTransparent(bool transparent) {
  transparent_ = transparent;
  if (layer())
    layer()->SetFillsBoundsOpaquely(!transparent_);
}

void Window::SetFillsBoundsCompletely(bool fills_bounds) {
  layer()->SetFillsBoundsCompletely(fills_bounds);
}

Window* Window::GetRootWindow() {
  return const_cast<Window*>(
      static_cast<const Window*>(this)->GetRootWindow());
}

const Window* Window::GetRootWindow() const {
  return IsRootWindow() ? this : parent_ ? parent_->GetRootWindow() : NULL;
}

WindowTreeHost* Window::GetHost() {
  return const_cast<WindowTreeHost*>(const_cast<const Window*>(this)->
      GetHost());
}

const WindowTreeHost* Window::GetHost() const {
  const Window* root_window = GetRootWindow();
  return root_window ? root_window->host_ : NULL;
}

void Window::Show() {
  DCHECK_EQ(visible_, layer()->GetTargetVisibility());
  // It is not allowed that a window is visible but the layers alpha is fully
  // transparent since the window would still be considered to be active but
  // could not be seen.
  DCHECK(!visible_ || layer()->GetTargetOpacity() > 0.0f);
  SetVisible(true);
}

void Window::Hide() {
  // RootWindow::OnVisibilityChanged will call ReleaseCapture.
  SetVisible(false);
}

bool Window::IsVisible() const {
  // Layer visibility can be inconsistent with window visibility, for example
  // when a Window is hidden, we want this function to return false immediately
  // after, even though the client may decide to animate the hide effect (and
  // so the layer will be visible for some time after Hide() is called).
  return visible_ ? layer()->IsDrawn() : false;
}

gfx::Rect Window::GetBoundsInRootWindow() const {
  // TODO(beng): There may be a better way to handle this, and the existing code
  //             is likely wrong anyway in a multi-display world, but this will
  //             do for now.
  if (!GetRootWindow())
    return bounds();
  gfx::Rect bounds_in_root(bounds().size());
  ConvertRectToTarget(this, GetRootWindow(), &bounds_in_root);
  return bounds_in_root;
}

gfx::Rect Window::GetBoundsInScreen() const {
  gfx::Rect bounds(GetBoundsInRootWindow());
  const Window* root = GetRootWindow();
  if (root) {
    aura::client::ScreenPositionClient* screen_position_client =
        aura::client::GetScreenPositionClient(root);
    if (screen_position_client) {
      gfx::Point origin = bounds.origin();
      screen_position_client->ConvertPointToScreen(root, &origin);
      bounds.set_origin(origin);
    }
  }
  return bounds;
}

void Window::SetTransform(const gfx::Transform& transform) {
  WindowOcclusionTracker::ScopedPause pause_occlusion_tracking(env_);
  for (WindowObserver& observer : observers_)
    observer.OnWindowTargetTransformChanging(this, transform);
  layer()->SetTransform(transform);
}

void Window::SetLayoutManager(LayoutManager* layout_manager) {
  if (layout_manager == layout_manager_.get())
    return;
  layout_manager_.reset(layout_manager);
  if (!layout_manager)
    return;
  // If we're changing to a new layout manager, ensure it is aware of all the
  // existing child windows.
  for (Windows::const_iterator it = children_.begin();
       it != children_.end();
       ++it)
    layout_manager_->OnWindowAddedToLayout(*it);
}

std::unique_ptr<WindowTargeter> Window::SetEventTargeter(
    std::unique_ptr<WindowTargeter> targeter) {
  std::unique_ptr<WindowTargeter> old_targeter = std::move(targeter_);
  if (old_targeter)
    old_targeter->OnInstalled(nullptr);
  targeter_ = std::move(targeter);
  if (targeter_)
    targeter_->OnInstalled(this);
  return old_targeter;
}

void Window::SetBounds(const gfx::Rect& new_bounds) {
  if (parent_ && parent_->layout_manager())
    parent_->layout_manager()->SetChildBounds(this, new_bounds);
  else {
    // Ensure we don't go smaller than our minimum bounds.
    gfx::Rect final_bounds(new_bounds);
    if (delegate_) {
      const gfx::Size& min_size = delegate_->GetMinimumSize();
      final_bounds.set_width(std::max(min_size.width(), final_bounds.width()));
      final_bounds.set_height(std::max(min_size.height(),
                                       final_bounds.height()));
    }
    SetBoundsInternal(final_bounds);
  }
}

void Window::SetBoundsInScreen(const gfx::Rect& new_bounds_in_screen,
                               const display::Display& dst_display) {
  WindowTreeHost* host = GetHost();
  bool is_moving = false;
  if (host && host->GetDisplayId() != dst_display.id()) {
    is_moving = true;
    for (auto& observer : observers_)
      observer.OnWillMoveWindowToDisplay(this, dst_display.id());
  }

  aura::client::ScreenPositionClient* screen_position_client = nullptr;
  Window* root = GetRootWindow();
  if (root)
    screen_position_client = aura::client::GetScreenPositionClient(root);
  if (screen_position_client)
    screen_position_client->SetBounds(this, new_bounds_in_screen, dst_display);
  else
    SetBounds(new_bounds_in_screen);

  if (is_moving) {
    for (auto& observer : observers_)
      observer.OnDidMoveWindowToDisplay(this);
  }
}

gfx::Rect Window::GetTargetBounds() const {
  return layer() ? layer()->GetTargetBounds() : bounds();
}

void Window::SchedulePaintInRect(const gfx::Rect& rect) {
  layer()->SchedulePaint(rect);
}

void Window::StackChildAtTop(Window* child) {
  if (children_.size() <= 1 || child == children_.back())
    return;  // In the front already.
  StackChildAbove(child, children_.back());
}

void Window::StackChildAbove(Window* child, Window* target) {
  StackChildRelativeTo(child, target, STACK_ABOVE);
}

void Window::StackChildAtBottom(Window* child) {
  if (children_.size() <= 1 || child == children_.front())
    return;  // At the bottom already.
  StackChildBelow(child, children_.front());
}

void Window::StackChildBelow(Window* child, Window* target) {
  StackChildRelativeTo(child, target, STACK_BELOW);
}

void Window::AddChild(Window* child) {
  WindowOcclusionTracker::ScopedPause pause_occlusion_tracking(env_);

  DCHECK(layer()) << "Parent has not been Init()ed yet.";
  DCHECK(child->layer()) << "Child has not been Init()ed yt.";
  DCHECK_EQ(env_, child->env_) << "All windows in a hierarchy must share the "
                                  " same Env.";
  WindowObserver::HierarchyChangeParams params;
  params.target = child;
  params.new_parent = this;
  params.old_parent = child->parent();
  params.phase = WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGING;
  NotifyWindowHierarchyChange(params);

  Window* old_root = child->GetRootWindow();

  port_->OnWillAddChild(child);

  DCHECK(!base::ContainsValue(children_, child));
  if (child->parent())
    child->parent()->RemoveChildImpl(child, this);

  child->parent_ = this;
  layer()->Add(child->layer());

  children_.push_back(child);
  if (layout_manager_)
    layout_manager_->OnWindowAddedToLayout(child);
  for (WindowObserver& observer : observers_)
    observer.OnWindowAdded(child);
  child->OnParentChanged();

  Window* root_window = GetRootWindow();
  if (root_window && old_root != root_window) {
    root_window->GetHost()->dispatcher()->OnWindowAddedToRootWindow(child);
    child->NotifyAddedToRootWindow();
  }

  params.phase = WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGED;
  NotifyWindowHierarchyChange(params);
}

void Window::RemoveChild(Window* child) {
  WindowOcclusionTracker::ScopedPause pause_occlusion_tracking(env_);

  WindowObserver::HierarchyChangeParams params;
  params.target = child;
  params.new_parent = NULL;
  params.old_parent = this;
  params.phase = WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGING;
  NotifyWindowHierarchyChange(params);

  port_->OnWillRemoveChild(child);
  RemoveChildImpl(child, NULL);

  params.phase = WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGED;
  NotifyWindowHierarchyChange(params);
}

bool Window::Contains(const Window* other) const {
  for (const Window* parent = other; parent; parent = parent->parent_) {
    if (parent == this)
      return true;
  }
  return false;
}

Window* Window::GetChildById(int id) {
  return const_cast<Window*>(const_cast<const Window*>(this)->GetChildById(id));
}

const Window* Window::GetChildById(int id) const {
  Windows::const_iterator i;
  for (i = children_.begin(); i != children_.end(); ++i) {
    if ((*i)->id() == id)
      return *i;
    const Window* result = (*i)->GetChildById(id);
    if (result)
      return result;
  }
  return NULL;
}

// static
void Window::ConvertPointToTarget(const Window* source,
                                  const Window* target,
                                  gfx::PointF* point) {
  if (!source)
    return;
  if (source->GetRootWindow() != target->GetRootWindow()) {
    client::ScreenPositionClient* source_client =
        client::GetScreenPositionClient(source->GetRootWindow());
    // |source_client| can be NULL in tests.
    if (source_client)
      source_client->ConvertPointToScreen(source, point);

    client::ScreenPositionClient* target_client =
        client::GetScreenPositionClient(target->GetRootWindow());
    // |target_client| can be NULL in tests.
    if (target_client)
      target_client->ConvertPointFromScreen(target, point);
  } else {
    ui::Layer::ConvertPointToLayer(source->layer(), target->layer(), point);
  }
}

// static
void Window::ConvertPointToTarget(const Window* source,
                                  const Window* target,
                                  gfx::Point* point) {
  gfx::PointF point_float(*point);
  ConvertPointToTarget(source, target, &point_float);
  *point = gfx::ToFlooredPoint(point_float);
}

// static
void Window::ConvertRectToTarget(const Window* source,
                                 const Window* target,
                                 gfx::Rect* rect) {
  DCHECK(rect);
  gfx::Point origin = rect->origin();
  ConvertPointToTarget(source, target, &origin);
  rect->set_origin(origin);
}

// static
void Window::ConvertNativePointToTargetHost(const Window* source,
                                            const Window* target,
                                            gfx::PointF* point) {
  if (!source || !target)
    return;

  if (source->GetHost() == target->GetHost())
    return;

  point->Offset(-target->GetHost()->GetBoundsInPixels().x(),
                -target->GetHost()->GetBoundsInPixels().y());
}

// static
void Window::ConvertNativePointToTargetHost(const Window* source,
                                            const Window* target,
                                            gfx::Point* point) {
  gfx::PointF point_float(*point);
  ConvertNativePointToTargetHost(source, target, &point_float);
  *point = gfx::ToFlooredPoint(point_float);
}

void Window::MoveCursorTo(const gfx::Point& point_in_window) {
  Window* root_window = GetRootWindow();
  DCHECK(root_window);
  gfx::Point point_in_root(point_in_window);
  ConvertPointToTarget(this, root_window, &point_in_root);
  root_window->GetHost()->MoveCursorToLocationInDIP(point_in_root);
}

gfx::NativeCursor Window::GetCursor(const gfx::Point& point) const {
  return delegate_ ? delegate_->GetCursor(point) : gfx::kNullCursor;
}

bool Window::ShouldRestackTransientChildren() {
  return port_->ShouldRestackTransientChildren();
}

void Window::AddObserver(WindowObserver* observer) {
  observer->OnObservingWindow(this);
  observers_.AddObserver(observer);
}

void Window::RemoveObserver(WindowObserver* observer) {
  observer->OnUnobservingWindow(this);
  observers_.RemoveObserver(observer);
}

bool Window::HasObserver(const WindowObserver* observer) const {
  return observers_.HasObserver(observer);
}

void Window::SetEventTargetingPolicy(ws::mojom::EventTargetingPolicy policy) {
#if DCHECK_IS_ON()
  const bool old_window_accepts_events =
      (event_targeting_policy_ ==
       ws::mojom::EventTargetingPolicy::TARGET_ONLY) ||
      (event_targeting_policy_ ==
       ws::mojom::EventTargetingPolicy::TARGET_AND_DESCENDANTS);
  const bool new_window_accepts_events =
      (policy == ws::mojom::EventTargetingPolicy::TARGET_ONLY) ||
      (policy == ws::mojom::EventTargetingPolicy::TARGET_AND_DESCENDANTS);
  if (new_window_accepts_events != old_window_accepts_events) {
    DCHECK(!created_layer_tree_frame_sink_);
  }
#endif

  if (event_targeting_policy_ == policy)
    return;

  event_targeting_policy_ = policy;
  if (port_)
    port_->OnEventTargetingPolicyChanged();
}

bool Window::ContainsPointInRoot(const gfx::Point& point_in_root) const {
  const Window* root_window = GetRootWindow();
  if (!root_window)
    return false;
  gfx::Point local_point(point_in_root);
  ConvertPointToTarget(root_window, this, &local_point);
  return gfx::Rect(GetTargetBounds().size()).Contains(local_point);
}

bool Window::ContainsPoint(const gfx::Point& local_point) const {
  return gfx::Rect(bounds().size()).Contains(local_point);
}

Window* Window::GetEventHandlerForPoint(const gfx::Point& local_point) {
  if (!IsVisible())
    return nullptr;

  if (!HitTest(local_point))
    return nullptr;

  for (Windows::const_reverse_iterator it = children_.rbegin(),
                                       rend = children_.rend();
       it != rend; ++it) {
    Window* child = *it;

    if (child->event_targeting_policy_ ==
        ws::mojom::EventTargetingPolicy::NONE) {
      continue;
    }

    // The client may not allow events to be processed by certain subtrees.
    client::EventClient* client = client::GetEventClient(GetRootWindow());
    if (client && !client->CanProcessEventsWithinSubtree(child))
      continue;

    if (delegate_ && !delegate_->ShouldDescendIntoChildForEventHandling(
                         child, local_point)) {
      continue;
    }

    gfx::Point point_in_child_coords(local_point);
    ConvertPointToTarget(this, child, &point_in_child_coords);
    Window* match = child->GetEventHandlerForPoint(point_in_child_coords);
    if (!match)
      continue;

    switch (child->event_targeting_policy_) {
      case ws::mojom::EventTargetingPolicy::TARGET_ONLY:
        if (child->delegate_)
          return child;
        break;
      case ws::mojom::EventTargetingPolicy::TARGET_AND_DESCENDANTS:
        return match;
      case ws::mojom::EventTargetingPolicy::DESCENDANTS_ONLY:
        if (match != child)
          return match;
        break;
      case ws::mojom::EventTargetingPolicy::NONE:
        NOTREACHED();  // This case is handled early on.
    }
  }

  return delegate_ ? this : nullptr;
}

Window* Window::GetToplevelWindow() {
  // TODO: this may need to call to the WindowPort. For mus this may need to
  // return for any top level.
  Window* topmost_window_with_delegate = NULL;
  for (aura::Window* window = this; window != NULL; window = window->parent()) {
    if (window->delegate())
      topmost_window_with_delegate = window;
  }
  return topmost_window_with_delegate;
}

void Window::Focus() {
  client::FocusClient* client = client::GetFocusClient(this);
  DCHECK(client);
  client->FocusWindow(this);
}

bool Window::HasFocus() const {
  client::FocusClient* client = client::GetFocusClient(this);
  return client && client->GetFocusedWindow() == this;
}

bool Window::CanFocus() const {
  if (IsRootWindow())
    return IsVisible();

  // NOTE: as part of focusing the window the ActivationClient may make the
  // window visible (by way of making a hidden ancestor visible). For this
  // reason we can't check visibility here and assume the client is doing it.
  if (!parent_ || (delegate_ && !delegate_->CanFocus()))
    return false;

  // The client may forbid certain windows from receiving focus at a given point
  // in time.
  client::EventClient* client = client::GetEventClient(GetRootWindow());
  if (client && !client->CanProcessEventsWithinSubtree(this))
    return false;

  return parent_->CanFocus();
}

bool Window::CanReceiveEvents() const {
  // TODO(sky): this may want to delegate to the WindowPort as for mus there
  // isn't a point in descending into windows owned by the client.
  if (IsRootWindow())
    return IsVisible();

  // The client may forbid certain windows from receiving events at a given
  // point in time.
  client::EventClient* client = client::GetEventClient(GetRootWindow());
  if (client && !client->CanProcessEventsWithinSubtree(this))
    return false;

  return parent_ && IsVisible() && parent_->CanReceiveEvents();
}

void Window::SetCapture() {
  if (!IsVisible())
    return;

  Window* root_window = GetRootWindow();
  if (!root_window)
    return;
  client::CaptureClient* capture_client = client::GetCaptureClient(root_window);
  if (!capture_client)
    return;
  capture_client->SetCapture(this);
}

void Window::ReleaseCapture() {
  Window* root_window = GetRootWindow();
  if (!root_window)
    return;
  client::CaptureClient* capture_client = client::GetCaptureClient(root_window);
  if (!capture_client)
    return;
  capture_client->ReleaseCapture(this);
}

bool Window::HasCapture() {
  Window* root_window = GetRootWindow();
  if (!root_window)
    return false;
  client::CaptureClient* capture_client = client::GetCaptureClient(root_window);
  return capture_client && capture_client->GetCaptureWindow() == this;
}

std::unique_ptr<ScopedKeyboardHook> Window::CaptureSystemKeyEvents(
    base::Optional<base::flat_set<ui::DomCode>> dom_codes) {
  Window* root_window = GetRootWindow();
  if (!root_window)
    return nullptr;

  WindowTreeHost* host = root_window->GetHost();
  if (!host)
    return nullptr;

  return host->CaptureSystemKeyEvents(std::move(dom_codes));
}

void Window::SuppressPaint() {
  layer()->SuppressPaint();
}

// {Set,Get,Clear}Property are implemented in class_property.h.

void Window::SetNativeWindowProperty(const char* key, void* value) {
  SetPropertyInternal(key, key, NULL, reinterpret_cast<int64_t>(value), 0);
}

void* Window::GetNativeWindowProperty(const char* key) const {
  return reinterpret_cast<void*>(GetPropertyInternal(key, 0));
}

void Window::OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                        float new_device_scale_factor) {
  port_->OnDeviceScaleFactorChanged(old_device_scale_factor,
                                    new_device_scale_factor);
}

#if !defined(NDEBUG)
std::string Window::GetDebugInfo() const {
  return base::StringPrintf(
      "%s<%d> bounds(%d, %d, %d, %d) %s %s opacity=%.1f",
      GetName().empty() ? "Unknown" : GetName().c_str(), id(), bounds().x(),
      bounds().y(), bounds().width(), bounds().height(),
      visible_ ? "WindowVisible" : "WindowHidden",
      layer()
          ? (layer()->GetTargetVisibility() ? "LayerVisible" : "LayerHidden")
          : "NoLayer",
      layer() ? layer()->opacity() : 1.0f);
}

void Window::PrintWindowHierarchy(int depth) const {
  VLOG(0) << base::StringPrintf(
      "%*s%s", depth * 2, "", GetDebugInfo().c_str());
  for (Windows::const_iterator it = children_.begin();
       it != children_.end(); ++it) {
    Window* child = *it;
    child->PrintWindowHierarchy(depth + 1);
  }
}
#endif

void Window::RemoveOrDestroyChildren() {
  while (!children_.empty()) {
    Window* child = children_[0];
    if (child->owned_by_parent_) {
      delete child;
      // Deleting the child so remove it from out children_ list.
      DCHECK(!base::ContainsValue(children_, child));
    } else {
      // Even if we can't delete the child, we still need to remove it from the
      // parent so that relevant bookkeeping (parent_ back-pointers etc) are
      // updated.
      RemoveChild(child);
    }
  }
}

std::unique_ptr<ui::PropertyData> Window::BeforePropertyChange(
    const void* key) {
  return port_ ? port_->OnWillChangeProperty(key) : nullptr;
}

void Window::AfterPropertyChange(const void* key,
                                 int64_t old_value,
                                 std::unique_ptr<ui::PropertyData> data) {
  if (port_)
    port_->OnPropertyChanged(key, old_value, std::move(data));
  for (WindowObserver& observer : observers_)
    observer.OnWindowPropertyChanged(this, key, old_value);
}

///////////////////////////////////////////////////////////////////////////////
// Window, private:

bool Window::HitTest(const gfx::Point& local_point) {
  gfx::Rect local_bounds(bounds().size());
  if (!delegate_ || !delegate_->HasHitTestMask())
    return local_bounds.Contains(local_point);

  gfx::Path mask;
  delegate_->GetHitTestMask(&mask);

  SkRegion clip_region;
  clip_region.setRect(local_bounds.x(), local_bounds.y(),
                      local_bounds.width(), local_bounds.height());
  SkRegion mask_region;
  return mask_region.setPath(mask, clip_region) &&
      mask_region.contains(local_point.x(), local_point.y());
}

void Window::SetBoundsInternal(const gfx::Rect& new_bounds) {
  gfx::Rect old_bounds = GetTargetBounds();

  // Always need to set the layer's bounds -- even if it is to the same thing.
  // This may cause important side effects such as stopping animation.
  layer()->SetBounds(new_bounds);

  // If we are currently not the layer's delegate, we will not get bounds
  // changed notification from the layer (this typically happens after animating
  // hidden). We must notify ourselves.
  if (layer()->delegate() != this) {
    OnLayerBoundsChanged(old_bounds,
                         ui::PropertyChangeReason::NOT_FROM_ANIMATION);
  }
}

void Window::SetVisible(bool visible) {
  if (visible == layer()->GetTargetVisibility())
    return;  // No change.

  WindowOcclusionTracker::ScopedPause pause_occlusion_tracking(env_);

  for (WindowObserver& observer : observers_)
    observer.OnWindowVisibilityChanging(this, visible);

  client::VisibilityClient* visibility_client =
      client::GetVisibilityClient(this);
  if (visibility_client)
    visibility_client->UpdateLayerVisibility(this, visible);
  else
    layer()->SetVisible(visible);
  visible_ = visible;
  port_->OnVisibilityChanged(visible);
  SchedulePaint();
  if (parent_ && parent_->layout_manager_)
    parent_->layout_manager_->OnChildWindowVisibilityChanged(this, visible);

  if (delegate_)
    delegate_->OnWindowTargetVisibilityChanged(visible);

  NotifyWindowVisibilityChanged(this, visible);
}

void Window::SetOcclusionState(OcclusionState occlusion_state) {
  if (occlusion_state != occlusion_state_) {
    occlusion_state_ = occlusion_state;
    if (delegate_)
      delegate_->OnWindowOcclusionChanged(occlusion_state);
  }
}

void Window::SchedulePaint() {
  SchedulePaintInRect(gfx::Rect(0, 0, bounds().width(), bounds().height()));
}

void Window::Paint(const ui::PaintContext& context) {
  if (delegate_)
    delegate_->OnPaint(context);
}

void Window::RemoveChildImpl(Window* child, Window* new_parent) {
  if (layout_manager_)
    layout_manager_->OnWillRemoveWindowFromLayout(child);
  for (WindowObserver& observer : observers_)
    observer.OnWillRemoveWindow(child);
  Window* root_window = child->GetRootWindow();
  Window* new_root_window = new_parent ? new_parent->GetRootWindow() : NULL;
  if (root_window && root_window != new_root_window)
    child->NotifyRemovingFromRootWindow(new_root_window);

  if (child->OwnsLayer())
    layer()->Remove(child->layer());
  child->parent_ = NULL;
  auto i = std::find(children_.begin(), children_.end(), child);
  DCHECK(i != children_.end());
  children_.erase(i);
  child->OnParentChanged();
  if (layout_manager_)
    layout_manager_->OnWindowRemovedFromLayout(child);
}

void Window::OnParentChanged() {
  for (WindowObserver& observer : observers_)
    observer.OnWindowParentChanged(this, parent_);
}

void Window::StackChildRelativeTo(Window* child,
                                  Window* target,
                                  StackDirection direction) {
  DCHECK_NE(child, target);
  DCHECK(child);
  DCHECK(target);
  DCHECK_EQ(this, child->parent());
  DCHECK_EQ(this, target->parent());

  WindowOcclusionTracker::ScopedPause pause_occlusion_tracking(env_);

  client::WindowStackingClient* stacking_client =
      client::GetWindowStackingClient();
  if (stacking_client &&
      !stacking_client->AdjustStacking(&child, &target, &direction))
    return;

  const size_t child_i =
      std::find(children_.begin(), children_.end(), child) - children_.begin();
  const size_t target_i =
      std::find(children_.begin(), children_.end(), target) - children_.begin();

  // Don't move the child if it is already in the right place.
  if ((direction == STACK_ABOVE && child_i == target_i + 1) ||
      (direction == STACK_BELOW && child_i + 1 == target_i))
    return;

  const size_t dest_i =
      direction == STACK_ABOVE ?
      (child_i < target_i ? target_i : target_i + 1) :
      (child_i < target_i ? target_i - 1 : target_i);
  port_->OnWillMoveChild(child_i, dest_i);
  children_.erase(children_.begin() + child_i);
  children_.insert(children_.begin() + dest_i, child);

  StackChildLayerRelativeTo(child, target, direction);

  child->OnStackingChanged();
}

void Window::StackChildLayerRelativeTo(Window* child,
                                       Window* target,
                                       StackDirection direction) {
  DCHECK(layer() && child->layer() && target->layer());
  if (direction == STACK_ABOVE)
    layer()->StackAbove(child->layer(), target->layer());
  else
    layer()->StackBelow(child->layer(), target->layer());
}

void Window::OnStackingChanged() {
  for (WindowObserver& observer : observers_)
    observer.OnWindowStackingChanged(this);
}

void Window::NotifyRemovingFromRootWindow(Window* new_root) {
  if (IsEmbeddingClient())
    UnregisterFrameSinkId();
  for (WindowObserver& observer : observers_)
    observer.OnWindowRemovingFromRootWindow(this, new_root);
  for (Window::Windows::const_iterator it = children_.begin();
       it != children_.end(); ++it) {
    (*it)->NotifyRemovingFromRootWindow(new_root);
  }
}

void Window::NotifyAddedToRootWindow() {
  if (IsEmbeddingClient())
    RegisterFrameSinkId();
  for (WindowObserver& observer : observers_)
    observer.OnWindowAddedToRootWindow(this);
  for (Window::Windows::const_iterator it = children_.begin();
       it != children_.end(); ++it) {
    (*it)->NotifyAddedToRootWindow();
  }
}

void Window::NotifyWindowHierarchyChange(
    const WindowObserver::HierarchyChangeParams& params) {
  params.target->NotifyWindowHierarchyChangeDown(params);
  switch (params.phase) {
    case WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGING:
      if (params.old_parent)
        params.old_parent->NotifyWindowHierarchyChangeUp(params);
      break;
    case WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGED:
      if (params.new_parent)
        params.new_parent->NotifyWindowHierarchyChangeUp(params);
      break;
  }
}

void Window::NotifyWindowHierarchyChangeDown(
    const WindowObserver::HierarchyChangeParams& params) {
  NotifyWindowHierarchyChangeAtReceiver(params);
  for (Window::Windows::const_iterator it = children_.begin();
       it != children_.end(); ++it) {
    (*it)->NotifyWindowHierarchyChangeDown(params);
  }
}

void Window::NotifyWindowHierarchyChangeUp(
    const WindowObserver::HierarchyChangeParams& params) {
  for (Window* window = this; window; window = window->parent())
    window->NotifyWindowHierarchyChangeAtReceiver(params);
}

void Window::NotifyWindowHierarchyChangeAtReceiver(
    const WindowObserver::HierarchyChangeParams& params) {
  WindowObserver::HierarchyChangeParams local_params = params;
  local_params.receiver = this;

  switch (params.phase) {
    case WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGING:
      for (WindowObserver& observer : observers_)
        observer.OnWindowHierarchyChanging(local_params);
      break;
    case WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGED:
      for (WindowObserver& observer : observers_)
        observer.OnWindowHierarchyChanged(local_params);
      break;
  }
}

void Window::NotifyWindowVisibilityChanged(aura::Window* target,
                                           bool visible) {
  if (!NotifyWindowVisibilityChangedDown(target, visible))
    return; // |this| has been deleted.
  NotifyWindowVisibilityChangedUp(target, visible);
}

bool Window::NotifyWindowVisibilityChangedAtReceiver(aura::Window* target,
                                                     bool visible) {
  // |this| may be deleted during a call to OnWindowVisibilityChanged() on one
  // of the observers. We create an local observer for that. In that case we
  // exit without further access to any members.
  WindowTracker tracker;
  tracker.Add(this);
  for (WindowObserver& observer : observers_)
    observer.OnWindowVisibilityChanged(target, visible);
  return tracker.Contains(this);
}

bool Window::NotifyWindowVisibilityChangedDown(aura::Window* target,
                                               bool visible) {
  if (!NotifyWindowVisibilityChangedAtReceiver(target, visible))
    return false; // |this| was deleted.

  WindowTracker this_tracker;
  this_tracker.Add(this);
  // Copy |children_| in case iterating mutates |children_|, or destroys an
  // existing child.
  WindowTracker children(children_);

  while (!this_tracker.windows().empty() && !children.windows().empty())
    children.Pop()->NotifyWindowVisibilityChangedDown(target, visible);

  const bool this_still_valid = !this_tracker.windows().empty();
  return this_still_valid;
}

void Window::NotifyWindowVisibilityChangedUp(aura::Window* target,
                                             bool visible) {
  // Start with the parent as we already notified |this|
  // in NotifyWindowVisibilityChangedDown.
  for (Window* window = parent(); window; window = window->parent()) {
    bool ret = window->NotifyWindowVisibilityChangedAtReceiver(target, visible);
    DCHECK(ret);
  }
}

bool Window::CleanupGestureState() {
  bool state_modified = false;
  state_modified |= env_->gesture_recognizer()->CancelActiveTouches(this);
  state_modified |= env_->gesture_recognizer()->CleanupStateForConsumer(this);
  for (auto iter = children_.begin(); iter != children_.end(); ++iter) {
    state_modified |= (*iter)->CleanupGestureState();
  }
  return state_modified;
}

std::unique_ptr<cc::LayerTreeFrameSink> Window::CreateLayerTreeFrameSink() {
  auto sink = port_->CreateLayerTreeFrameSink();
  DCHECK(frame_sink_id_.is_valid());
  DCHECK(embeds_external_client_);
  DCHECK(GetLocalSurfaceId().is_valid());
  created_layer_tree_frame_sink_ = true;
  return sink;
}

viz::SurfaceId Window::GetSurfaceId() const {
  return viz::SurfaceId(GetFrameSinkId(), port_->GetLocalSurfaceId());
}

base::TimeTicks Window::GetLocalSurfaceIdAllocationTime() const {
  return port_->GetLocalSurfaceIdAllocationTime();
}

void Window::AllocateLocalSurfaceId() {
  port_->AllocateLocalSurfaceId();
}

viz::ScopedSurfaceIdAllocator Window::GetSurfaceIdAllocator(
    base::OnceCallback<void()> allocation_task) {
  return port_->GetSurfaceIdAllocator(std::move(allocation_task));
}

const viz::LocalSurfaceId& Window::GetLocalSurfaceId() const {
  return port_->GetLocalSurfaceId();
}

void Window::UpdateLocalSurfaceIdFromEmbeddedClient(
    const base::Optional<viz::LocalSurfaceId>& embedded_client_local_surface_id,
    const base::Optional<base::TimeTicks>&
        embedded_client_local_surface_id_allocation_time) {
  if (embedded_client_local_surface_id) {
    port_->UpdateLocalSurfaceIdFromEmbeddedClient(
        *embedded_client_local_surface_id,
        embedded_client_local_surface_id_allocation_time.value_or(
            base::TimeTicks()));
  } else {
    port_->AllocateLocalSurfaceId();
  }
}

const viz::FrameSinkId& Window::GetFrameSinkId() const {
  if (IsRootWindow()) {
    DCHECK(host_);
    auto* compositor = host_->compositor();
    DCHECK(compositor);
    return compositor->frame_sink_id();
  }
  return frame_sink_id_;
}

void Window::SetEmbedFrameSinkId(const viz::FrameSinkId& frame_sink_id) {
  UnregisterFrameSinkId();

  DCHECK(frame_sink_id.is_valid());
  frame_sink_id_ = frame_sink_id;
  embeds_external_client_ = true;
  RegisterFrameSinkId();
}

bool Window::IsEmbeddingClient() const {
  return embeds_external_client_;
}

void Window::TrackOcclusionState() {
  port_->TrackOcclusionState();
}

bool Window::RequiresDoubleTapGestureEvents() const {
  return delegate_ && delegate_->RequiresDoubleTapGestureEvents();
}

#if DCHECK_IS_ON()
// static
void Window::SetEnvArgRequired(const char* error_string) {
  g_env_arg_required_string = error_string;
}
#endif

void Window::OnPaintLayer(const ui::PaintContext& context) {
  Paint(context);
}

void Window::OnLayerBoundsChanged(const gfx::Rect& old_bounds,
                                  ui::PropertyChangeReason reason) {
  WindowOcclusionTracker::ScopedPause pause_occlusion_tracking(env_);

  bounds_ = layer()->bounds();

  // Use |bounds_| as that is the bounds before any animations, which is what
  // mus wants.
  port_->OnDidChangeBounds(old_bounds, bounds_);

  if (layout_manager_)
    layout_manager_->OnWindowResized();
  if (delegate_)
    delegate_->OnBoundsChanged(old_bounds, bounds_);
  for (auto& observer : observers_)
    observer.OnWindowBoundsChanged(this, old_bounds, bounds_, reason);
}

void Window::OnLayerOpacityChanged(ui::PropertyChangeReason reason) {
  WindowOcclusionTracker::ScopedPause pause_occlusion_tracking(env_);
  for (WindowObserver& observer : observers_)
    observer.OnWindowOpacitySet(this, reason);
}

void Window::OnLayerAlphaShapeChanged() {
  WindowOcclusionTracker::ScopedPause pause_occlusion_tracking(env_);
  for (WindowObserver& observer : observers_)
    observer.OnWindowAlphaShapeSet(this);
}

void Window::OnLayerTransformed(const gfx::Transform& old_transform,
                                ui::PropertyChangeReason reason) {
  port_->OnDidChangeTransform(old_transform, layer()->transform());
  WindowOcclusionTracker::ScopedPause pause_occlusion_tracking(env_);
  for (WindowObserver& observer : observers_)
    observer.OnWindowTransformed(this, reason);
}

bool Window::CanAcceptEvent(const ui::Event& event) {
  // The client may forbid certain windows from receiving events at a given
  // point in time.
  client::EventClient* client = client::GetEventClient(GetRootWindow());
  if (client && !client->CanProcessEventsWithinSubtree(this))
    return false;

  // We need to make sure that a touch cancel event and any gesture events it
  // creates can always reach the window. This ensures that we receive a valid
  // touch / gesture stream.
  if (event.IsEndingEvent())
    return true;

  if (!IsVisible())
    return false;

  // The top-most window can always process an event.
  if (!parent_)
    return true;

  // For located events (i.e. mouse, touch etc.), an assumption is made that
  // windows that don't have a default event-handler cannot process the event
  // (see more in GetWindowForPoint()). This assumption is not made for key
  // events.
  return event.IsKeyEvent() || target_handler();
}

ui::EventTarget* Window::GetParentTarget() {
  if (IsRootWindow()) {
    return client::GetEventClient(this)
               ? client::GetEventClient(this)->GetToplevelEventTarget()
               : env_;
  }
  return parent_;
}

std::unique_ptr<ui::EventTargetIterator> Window::GetChildIterator() const {
  return std::make_unique<ui::EventTargetIteratorPtrImpl<Window>>(children());
}

ui::EventTargeter* Window::GetEventTargeter() {
  return targeter_.get();
}

void Window::ConvertEventToTarget(ui::EventTarget* target,
                                  ui::LocatedEvent* event) {
  event->ConvertLocationToTarget(this,
                                 static_cast<Window*>(target));
}

gfx::PointF Window::GetScreenLocationF(const ui::LocatedEvent& event) const {
  DCHECK_EQ(this, event.target());
  gfx::PointF screen_location(event.root_location_f());
  const Window* root = GetRootWindow();
  auto* screen_position_client = aura::client::GetScreenPositionClient(root);
  if (screen_position_client)
    screen_position_client->ConvertPointToScreen(root, &screen_location);
  return screen_location;
}

std::unique_ptr<ui::Layer> Window::RecreateLayer() {
  WindowOcclusionTracker::ScopedPause pause_occlusion_tracking(env_);

  ui::LayerAnimator* const animator = layer()->GetAnimator();
  const bool was_animating_opacity =
      animator->IsAnimatingProperty(ui::LayerAnimationElement::OPACITY);
  const bool was_animating_transform =
      animator->IsAnimatingProperty(ui::LayerAnimationElement::TRANSFORM);

  std::unique_ptr<ui::Layer> old_layer = LayerOwner::RecreateLayer();

  // If a frame sink is attached to the window, then allocate a new surface
  // id when layers are recreated, so the old layer contents are not affected
  // by a frame sent to the frame sink.
  if (GetFrameSinkId().is_valid() && old_layer)
    AllocateLocalSurfaceId();

  // Observers are guaranteed to be notified when an opacity or transform
  // animation ends.
  if (was_animating_opacity) {
    for (WindowObserver& observer : observers_) {
      observer.OnWindowOpacitySet(this,
                                  ui::PropertyChangeReason::FROM_ANIMATION);
    }
  }
  if (was_animating_transform) {
    for (WindowObserver& observer : observers_) {
      observer.OnWindowTransformed(this,
                                   ui::PropertyChangeReason::FROM_ANIMATION);
    }
  }

  for (WindowObserver& observer : observers_)
    observer.OnWindowLayerRecreated(this);

  return old_layer;
}

void Window::UpdateLayerName() {
#if !defined(NDEBUG)
  DCHECK(layer());

  std::string layer_name(GetName());
  if (layer_name.empty())
    layer_name = "Unnamed Window";

  if (id_ != -1)
    layer_name += " " + base::IntToString(id_);

  layer()->set_name(layer_name);
#endif
}

void Window::RegisterFrameSinkId() {
  DCHECK(frame_sink_id_.is_valid());
  DCHECK(IsEmbeddingClient());
  if (registered_frame_sink_id_ || disable_frame_sink_id_registration_)
    return;
  if (auto* compositor = layer()->GetCompositor()) {
    compositor->AddChildFrameSink(frame_sink_id_);
    registered_frame_sink_id_ = true;
    port_->RegisterFrameSinkId(frame_sink_id_);
  }
}

void Window::UnregisterFrameSinkId() {
  if (!registered_frame_sink_id_)
    return;
  registered_frame_sink_id_ = false;
  port_->UnregisterFrameSinkId(frame_sink_id_);
  if (auto* compositor = layer()->GetCompositor())
    compositor->RemoveChildFrameSink(frame_sink_id_);
}

}  // namespace aura
