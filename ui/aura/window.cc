// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/aura/window.h"

#include <stddef.h>

#include <sstream>
#include <utility>

#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "components/viz/common/features.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/skia/include/core/SkPath.h"
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
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tracker.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/layout.h"
#include "ui/base/metadata/base_type_conversion.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event_target_iterator.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/scoped_canvas.h"

DEFINE_ENUM_CONVERTERS(
    aura::client::WindowType,
    {aura::client::WINDOW_TYPE_UNKNOWN, u"WINDOW_TYPE_UNKNOWN"},
    {aura::client::WINDOW_TYPE_NORMAL, u"WINDOW_TYPE_NORMAL"},
    {aura::client::WINDOW_TYPE_POPUP, u"WINDOW_TYPE_POPUP"},
    {aura::client::WINDOW_TYPE_CONTROL, u"WINDOW_TYPE_CONTROL"},
    {aura::client::WINDOW_TYPE_MENU, u"WINDOW_TYPE_MENU"},
    {aura::client::WINDOW_TYPE_TOOLTIP, u"WINDOW_TYPE_TOOLTIP"})

DEFINE_ENUM_CONVERTERS(aura::Window::OcclusionState,
                       {aura::Window::OcclusionState::UNKNOWN, u"UNKNOWN"},
                       {aura::Window::OcclusionState::VISIBLE, u"VISIBLE"},
                       {aura::Window::OcclusionState::OCCLUDED, u"OCCLUDED"},
                       {aura::Window::OcclusionState::HIDDEN, u"HIDDEN"})

namespace ui {
namespace metadata {

template <>
struct TypeConverter<viz::SurfaceId> : public BaseTypeConverter<true> {
  static std::u16string ToString(const viz::SurfaceId& source_value);
  static std::optional<viz::SurfaceId> FromString(
      const std::u16string& source_value);
  static ValidStrings GetValidStrings();
};

// static
std::u16string TypeConverter<viz::SurfaceId>::ToString(
    const viz::SurfaceId& source_value) {
  return source_value.is_valid() ? base::UTF8ToUTF16(source_value.ToString())
                                 : u"<none>";
}

// static
std::optional<viz::SurfaceId> TypeConverter<viz::SurfaceId>::FromString(
    const std::u16string& source_value) {
  return std::nullopt;
}

// static
ValidStrings TypeConverter<viz::SurfaceId>::GetValidStrings() {
  return ValidStrings();
}

}  // namespace metadata
}  // namespace ui

namespace {

// This enum is used to construct a unique property changed callback key from
// the address of the `bounds_` field by using these values as an offset.
enum BoundsCallbackIndex : int {
  kBoundsX,
  kBoundsY,
  kBoundsWidth,
  kBoundsHeight,
};

}  // namespace

namespace aura {
namespace {

class ScopedCursorHider {
 public:
  explicit ScopedCursorHider(Window* window)
      : window_(window), hid_cursor_(false) {
    if (!window_->IsRootWindow())
      return;
    const bool cursor_is_in_bounds = window_->GetBoundsInScreen().Contains(
        Env::GetInstance()->last_mouse_location());
    client::CursorClient* cursor_client = client::GetCursorClient(window_);
    if (cursor_is_in_bounds && cursor_client &&
        cursor_client->IsCursorVisible()) {
      cursor_client->HideCursor();
      hid_cursor_ = true;
    }
  }

  ScopedCursorHider(const ScopedCursorHider&) = delete;
  ScopedCursorHider& operator=(const ScopedCursorHider&) = delete;

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
  raw_ptr<Window> window_;
  bool hid_cursor_;
};

}  // namespace

Window::Window(WindowDelegate* delegate, client::WindowType type)
    : type_(type),
      delegate_(delegate),
      event_targeting_policy_(
          aura::EventTargetingPolicy::kTargetAndDescendants),
      restore_event_targeting_policy_(
          aura::EventTargetingPolicy::kTargetAndDescendants),
      // Don't notify newly added observers during notification. This causes
      // problems for code that adds an observer as part of an observer
      // notification (such as the workspace code).
      observers_(base::ObserverListPolicy::EXISTING_ONLY) {
  SetTargetHandler(delegate_);
}

Window::~Window() {
  is_destroying_ = true;
  WindowOcclusionTracker::ScopedPause pause_occlusion_tracking;

  if (layer()->owner() == this)
    layer()->CompleteAllAnimations();

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
  layer()->set_delegate(nullptr);
  DestroyLayer();

  // If SetEmbedFrameSinkId() was called by client code, then we assume client
  // code is taking care of invalidating.
  if (frame_sink_id_.is_valid() && !embeds_external_client_) {
    auto* context_factory = Env::GetInstance()->context_factory();
    auto* host_frame_sink_manager = context_factory->GetHostFrameSinkManager();
    host_frame_sink_manager->InvalidateFrameSinkId(frame_sink_id_, this);
  }
}

void Window::Init(ui::LayerType layer_type) {
  WindowOcclusionTracker::ScopedPause pause_occlusion_tracking;

  SetLayer(std::make_unique<ui::Layer>(layer_type));
  layer()->SetVisible(false);
  layer()->set_delegate(this);
  UpdateLayerName();
  layer()->SetFillsBoundsOpaquely(!transparent_);
  Env::GetInstance()->NotifyWindowInitialized(this);
}

int Window::GetId() const {
  return id_;
}

void Window::SetId(int id) {
  if (id == id_)
    return;
  id_ = id;
  TriggerChangedCallback(&id_);
}

client::WindowType Window::GetType() const {
  return type_;
}

void Window::SetType(client::WindowType type) {
  // Cannot change type after the window is initialized.
  DCHECK(!layer());
  if (type == type_)
    return;
  type_ = type;
  TriggerChangedCallback(&type_);
}

const std::string& Window::GetName() const {
  std::string* name = GetProperty(client::kNameKey);
  return name ? *name : base::EmptyString();
}

void Window::SetName(const std::string& name) {
  if (name == GetName())
    return;
  SetProperty(client::kNameKey, name);
  if (layer())
    UpdateLayerName();
  TriggerChangedCallback(client::kNameKey);
}

const std::u16string& Window::GetTitle() const {
  std::u16string* title = GetProperty(client::kTitleKey);
  return title ? *title : base::EmptyString16();
}

void Window::SetTitle(const std::u16string& title) {
  if (title == GetTitle())
    return;
  SetProperty(client::kTitleKey, title);
  for (WindowObserver& observer : observers_)
    observer.OnWindowTitleChanged(this);
}

bool Window::GetTransparent() const {
  return transparent_;
}

void Window::SetTransparent(bool transparent) {
  if (transparent == transparent_)
    return;
  transparent_ = transparent;
  if (layer())
    layer()->SetFillsBoundsOpaquely(!transparent_);
  TriggerChangedCallback(&transparent_);
}

void Window::SetFillsBoundsCompletely(bool fills_bounds) {
  layer()->SetFillsBoundsCompletely(fills_bounds);
}

Window* Window::GetRootWindow() {
  return const_cast<Window*>(
      static_cast<const Window*>(this)->GetRootWindow());
}

const Window* Window::GetRootWindow() const {
  return IsRootWindow() ? this : parent_ ? parent_->GetRootWindow() : nullptr;
}

WindowTreeHost* Window::GetHost() {
  return const_cast<WindowTreeHost*>(const_cast<const Window*>(this)->
      GetHost());
}

const WindowTreeHost* Window::GetHost() const {
  const Window* root_window = GetRootWindow();
  return root_window ? root_window->host_.get() : nullptr;
}

void Window::Show() {
  DCHECK_EQ(visible_, layer()->GetTargetVisibility());
  // It is not allowed that a window is visible but the layers alpha is fully
  // transparent since the window would still be considered to be active but
  // could not be seen.
  DCHECK(!visible_ || layer()->GetTargetOpacity() > 0.0f);
  SetVisibleInternal(true);
}

void Window::Hide() {
  // RootWindow::OnVisibilityChanged will call ReleaseCapture.
  SetVisibleInternal(false);
}

bool Window::IsVisible() const {
  // Layer visibility can be inconsistent with window visibility, for example
  // when a Window is hidden, we want this function to return false immediately
  // after, even though the client may decide to animate the hide effect (and
  // so the layer will be visible for some time after Hide() is called).
  return visible_ ? layer()->IsVisible() : false;
}

Window::OcclusionState Window::GetOcclusionState() const {
  return occlusion_state_;
}

ScopedWindowCaptureRequest Window::MakeWindowCapturable() {
  DCHECK(!IsRootWindow()) << "Root windows can already be captured using their "
                             "FrameSinkId; no need to call this.";

  return ScopedWindowCaptureRequest(this);
}

gfx::Rect Window::GetBoundsInRootWindow() const {
  if (!GetRootWindow())
    return bounds();
  gfx::Rect bounds_in_root(bounds().size());
  ConvertRectToTarget(this, GetRootWindow(), &bounds_in_root);
  return bounds_in_root;
}

gfx::Rect Window::GetActualBoundsInRootWindow() const {
  if (!GetRootWindow())
    return bounds();
  gfx::Rect bounds_in_root(bounds().size());
  gfx::PointF origin_f = gfx::PointF(bounds_in_root.origin());
  ui::Layer::ConvertPointToLayer(layer(), GetRootWindow()->layer(),
                                 /*use_target_transform=*/false, &origin_f);
  bounds_in_root.set_origin(gfx::ToFlooredPoint(origin_f));
  return bounds_in_root;
}

const gfx::Transform& Window::transform() const {
  return layer()->transform();
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

gfx::Rect Window::GetActualBoundsInScreen() const {
  gfx::Rect bounds(GetActualBoundsInRootWindow());
  const Window* root = GetRootWindow();
  if (root) {
    gfx::Point origin_in_screen = root->GetBoundsInScreen().origin();
    origin_in_screen += bounds.OffsetFromOrigin();
    bounds.set_origin(origin_in_screen);
  }
  return bounds;
}

void Window::SetTransform(const gfx::Transform& transform) {
  WindowOcclusionTracker::ScopedPause pause_occlusion_tracking;
  for (WindowObserver& observer : observers_)
    observer.OnWindowTargetTransformChanging(this, transform);
  layer()->SetTransform(transform);
}

void Window::SetLayoutManager(std::nullptr_t) {
  SetLayoutManagerImpl(nullptr);
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
  if (parent_ && parent_->layout_manager()) {
    parent_->layout_manager()->SetChildBounds(this, new_bounds);
  } else {
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
  if (auto* screen_position_client =
          aura::client::GetScreenPositionClient(GetRootWindow())) {
    screen_position_client->SetBounds(this, new_bounds_in_screen, dst_display);
  } else {
    SetBounds(new_bounds_in_screen);
  }
}

gfx::Rect Window::GetTargetBounds() const {
  return layer() ? layer()->GetTargetBounds() : bounds();
}

void Window::ScheduleDraw() {
  layer()->ScheduleDraw();
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
  WindowOcclusionTracker::ScopedPause pause_occlusion_tracking;

  DCHECK(layer()) << "Parent has not been Init()ed yet.";
  DCHECK(child->layer()) << "Child has not been Init()ed yt.";
  WindowObserver::HierarchyChangeParams params;
  params.target = child;
  params.new_parent = this;
  params.old_parent = child->parent();
  params.phase = WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGING;
  NotifyWindowHierarchyChange(params);

  Window* old_root = child->GetRootWindow();

  DCHECK(!base::Contains(children_, child));
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
  WindowOcclusionTracker::ScopedPause pause_occlusion_tracking;

  WindowObserver::HierarchyChangeParams params;
  params.target = child;
  params.new_parent = nullptr;
  params.old_parent = this;
  params.phase = WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGING;
  NotifyWindowHierarchyChange(params);

  RemoveChildImpl(child, nullptr);

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
    if ((*i)->GetId() == id)
      return *i;
    const Window* result = (*i)->GetChildById(id);
    if (result)
      return result;
  }
  return nullptr;
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
    // |source_client| can be nullptr in tests.
    if (source_client)
      source_client->ConvertPointToScreen(source, point);

    client::ScreenPositionClient* target_client =
        client::GetScreenPositionClient(target->GetRootWindow());
    // |target_client| can be nullptr in tests.
    if (target_client)
      target_client->ConvertPointFromScreen(target, point);
  } else {
#if BUILDFLAG(IS_CHROMEOS)
    // TODO(b/319939913): Remove this log when the issue is fixed.
    auto get_root = [](const ui::Layer* layer) {
      const ui::Layer* root = layer;
      while (root->parent()) {
        root = root->parent();
      }
      return root;
    };
    auto chain_name = [](const aura::Window* window) {
      std::ostringstream out;
      out << "[";
      out << window->GetName();
      while (window->parent()) {
        window = window->parent();
        out << "]-[" << window->GetName();
      }
      out << "]";
      return out.str();
    };
    if (get_root(source->layer()) != get_root(target->layer())) {
      LOG(ERROR) << "Root layer in source and target window are different. "
                    "source chain="
                 << chain_name(source)
                 << ", target chain=" << chain_name(target);
    }
#endif
    ui::Layer::ConvertPointToLayer(source->layer(), target->layer(),
                                   /*use_target_transform=*/true, point);
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
  return delegate_ ? delegate_->GetCursor(point) : gfx::NativeCursor{};
}

void Window::AddObserver(WindowObserver* observer) {
  observers_.AddObserver(observer);
}

void Window::RemoveObserver(WindowObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool Window::HasObserver(const WindowObserver* observer) const {
  return observers_.HasObserver(observer);
}

void Window::SetEventTargetingPolicy(EventTargetingPolicy policy) {
  // If the event targeting is blocked on the window, do not allow change event
  // targeting policy until all event targeting blockers are removed from the
  // window.
  if (event_targeting_blocker_count_ > 0) {
    restore_event_targeting_policy_ = policy;
    return;
  }

#if DCHECK_IS_ON()
  const bool old_window_accepts_events =
      (event_targeting_policy_ == EventTargetingPolicy::kTargetOnly) ||
      (event_targeting_policy_ == EventTargetingPolicy::kTargetAndDescendants);
  const bool new_window_accepts_events =
      (policy == EventTargetingPolicy::kTargetOnly) ||
      (policy == EventTargetingPolicy::kTargetAndDescendants);
  if (new_window_accepts_events != old_window_accepts_events)
    DCHECK(!created_layer_tree_frame_sink_);
#endif

  if (event_targeting_policy_ == policy)
    return;

  event_targeting_policy_ = policy;
  layer()->SetAcceptEvents(policy != EventTargetingPolicy::kNone);
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

  for (Window* child : base::Reversed(children_)) {
    if (child->event_targeting_policy_ == EventTargetingPolicy::kNone) {
      continue;
    }

    // The client may not allow events to be processed by certain subtrees.
    client::EventClient* client = client::GetEventClient(GetRootWindow());
    if (client && !client->GetCanProcessEventsWithinSubtree(child))
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
      case EventTargetingPolicy::kTargetOnly:
        if (child->delegate_)
          return child;
        break;
      case EventTargetingPolicy::kTargetAndDescendants:
        return match;
      case EventTargetingPolicy::kDescendantsOnly:
        if (match != child)
          return match;
        break;
      case EventTargetingPolicy::kNone:
        NOTREACHED_IN_MIGRATION();  // This case is handled early on.
    }
  }

  return delegate_ ? this : nullptr;
}

Window* Window::GetToplevelWindow() {
  Window* topmost_window_with_delegate = nullptr;
  for (aura::Window* window = this; window != nullptr;
       window = window->parent()) {
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
  if (client && !client->GetCanProcessEventsWithinSubtree(this))
    return false;

  return parent_->CanFocus();
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
    std::optional<base::flat_set<ui::DomCode>> dom_codes) {
  Window* root_window = GetRootWindow();
  if (!root_window)
    return nullptr;

  WindowTreeHost* host = root_window->GetHost();
  if (!host)
    return nullptr;

  return host->CaptureSystemKeyEvents(std::move(dom_codes));
}

// {Set,Get,Clear}Property are implemented in class_property.h.

void Window::SetNativeWindowProperty(const char* key, void* value) {
  SetPropertyInternal(key, key, nullptr, reinterpret_cast<int64_t>(value), 0);
}

void* Window::GetNativeWindowProperty(const char* key) const {
  return reinterpret_cast<void*>(GetPropertyInternal(key, 0,
                                                     /*search_parent=*/false));
}

void Window::OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                        float new_device_scale_factor) {
  if (!IsRootWindow() && last_device_scale_factor_ != new_device_scale_factor &&
      IsEmbeddingExternalContent()) {
    last_device_scale_factor_ = new_device_scale_factor;
    parent_local_surface_id_allocator_->GenerateId();
    if (frame_sink_)
      frame_sink_->SetLocalSurfaceId(GetCurrentLocalSurfaceId());
  }

  ScopedCursorHider hider(this);
  if (delegate_) {
    delegate_->OnDeviceScaleFactorChanged(old_device_scale_factor,
                                          new_device_scale_factor);
  }
}

void Window::UpdateVisualState() {
  if (delegate_)
    delegate_->UpdateVisualState();
}

void Window::GetDebugInfo(const aura::Window* active_window,
                          const aura::Window* focused_window,
                          const aura::Window* capture_window,
                          std::ostringstream* out) const {
  std::string name(GetName());
  if (name.empty())
    name = "\"\"";
  const gfx::Vector2dF& subpixel_position_offset = layer()->GetSubpixelOffset();
  bool can_occlude_others = aura::Env::GetInstance()
                                ->GetWindowOcclusionTracker()
                                ->VisibleWindowCanOccludeOtherWindows(this);
  bool has_opaque_regions = !opaque_regions_for_occlusion().empty();
  *out << " " << name << "<" << GetId() << ">";
  *out << " (" << this << ")"
       << " type=" << aura::Window::WindowTypeToString(GetType());
  *out << ((this == active_window) ? " [active]" : "")
       << ((this == focused_window) ? " [focused]" : "")
       << ((this == capture_window) ? " [capture]" : "")
       << (GetTransparent() ? " [transparent]" : "")
       << (IsVisible() ? " [visible]" : "")
       << (has_opaque_regions ? " [opaque_regions]" : "")
       << (can_occlude_others ? " [occlude others]" : "")
       << (GetOcclusionState() != aura::Window::OcclusionState::UNKNOWN
               ? (" " + base::UTF16ToUTF8(aura::Window::OcclusionStateToString(
                            GetOcclusionState())))
                     .c_str()
               : "")
       << " " << bounds().ToString()
       << " scale=" + transform().To2dScale().ToString();

  if (!subpixel_position_offset.IsZero()) {
    *out << " subpixel offset=" + subpixel_position_offset.ToString();
  }
  *out << base::StringPrintf(" opacity=%.1f", layer()->opacity());

  switch (layer()->type()) {
    case ui::LAYER_NOT_DRAWN:
      *out << " layer(not_drawn ";
      break;
    case ui::LAYER_TEXTURED:
      *out << " layer(textured ";
      if (layer()->fills_bounds_opaquely()) {
        *out << " opaque ";
      }
      break;
    case ui::LAYER_SOLID_COLOR:
      *out << " layer(solid ";
      break;
    case ui::LAYER_NINE_PATCH:
      *out << " layer(nine_patch ";
      break;
  }

  *out << (layer()->GetTargetVisibility() ? " visible)" : " hidden)");
}

#if DCHECK_IS_ON()
std::string Window::GetWindowHierarchy(int depth) const {
  std::ostringstream out;
  std::string indent_str(depth * 2, ' ');
  out << indent_str;
  GetDebugInfo(nullptr, nullptr, nullptr, &out);
  out << std::endl;
  for (Window* child : children_) {
    out << child->GetWindowHierarchy(depth + 1);
  }
  return out.str();
}

void Window::PrintWindowHierarchy(int depth) const {
  VLOG(0) << GetWindowHierarchy(depth);
}
#endif

void Window::RemoveOrDestroyChildren() {
  while (!children_.empty()) {
    Window* child = children_[0];
    if (child->owned_by_parent_) {
      delete child;
      // Deleting the child so remove it from out children_ list.
      DCHECK(!base::Contains(children_, child));
    } else {
      // Even if we can't delete the child, we still need to remove it from the
      // parent so that relevant bookkeeping (parent_ back-pointers etc) are
      // updated.
      RemoveChild(child);
    }
  }
}

void Window::AfterPropertyChange(const void* key, int64_t old_value) {
  for (WindowObserver& observer : observers_)
    observer.OnWindowPropertyChanged(this, key, old_value);
}

///////////////////////////////////////////////////////////////////////////////
// Window, private:

void Window::SetEmbedFrameSinkIdImpl(const viz::FrameSinkId& frame_sink_id) {
  if (frame_sink_id_ == frame_sink_id) {
    return;
  }

  UnregisterFrameSinkId();

  frame_sink_id_ = frame_sink_id;
  if (!frame_sink_id_.is_valid()) {
    return;
  }

  RegisterFrameSinkId();
}

bool Window::HitTest(const gfx::Point& local_point) {
  gfx::Rect local_bounds(bounds().size());
  if (!delegate_ || !delegate_->HasHitTestMask())
    return local_bounds.Contains(local_point);

  SkPath mask;
  delegate_->GetHitTestMask(&mask);

  SkRegion clip_region;
  clip_region.setRect({local_bounds.x(), local_bounds.y(), local_bounds.width(),
                       local_bounds.height()});
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

void Window::SetVisibleInternal(bool visible) {
  if (visible == layer()->GetTargetVisibility())
    return;  // No change.

  WindowOcclusionTracker::ScopedPause pause_occlusion_tracking;

  for (WindowObserver& observer : observers_)
    observer.OnWindowVisibilityChanging(this, visible);

  client::VisibilityClient* visibility_client =
      client::GetVisibilityClient(this);
  if (visibility_client)
    visibility_client->UpdateLayerVisibility(this, visible);
  else
    layer()->SetVisible(visible);
  visible_ = visible;
  SchedulePaint();
  if (parent_ && parent_->layout_manager_)
    parent_->layout_manager_->OnChildWindowVisibilityChanged(this, visible);

  if (delegate_)
    delegate_->OnWindowTargetVisibilityChanged(visible);

  NotifyWindowVisibilityChanged(this, visible);
}

void Window::SetOcclusionInfo(OcclusionState occlusion_state,
                              const SkRegion& occluded_region) {
  if (occlusion_state == occlusion_state_ &&
      occluded_region_in_root_ == occluded_region) {
    return;
  }
  OcclusionState old_occlusion_state = occlusion_state_;
  occlusion_state_ = occlusion_state;
  occluded_region_in_root_ = occluded_region;
  if (delegate_)
    delegate_->OnWindowOcclusionChanged(old_occlusion_state, occlusion_state);

  for (WindowObserver& observer : observers_)
    observer.OnWindowOcclusionChanged(this);
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
  Window* new_root_window = new_parent ? new_parent->GetRootWindow() : nullptr;
  if (root_window && root_window != new_root_window)
    child->NotifyRemovingFromRootWindow(new_root_window);

  if (child->OwnsLayer())
    layer()->Remove(child->layer());
  child->parent_ = nullptr;
  auto i = base::ranges::find(children_, child);
  CHECK(i != children_.end(), base::NotFatalUntil::M130);
  children_.erase(i);
  child->OnParentChanged();
  if (layout_manager_)
    layout_manager_->OnWindowRemovedFromLayout(child);
  for (WindowObserver& observer : observers_)
    observer.OnWindowRemoved(child);
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

  WindowOcclusionTracker::ScopedPause pause_occlusion_tracking;

  client::WindowStackingClient* stacking_client =
      client::GetWindowStackingClient();
  if (stacking_client &&
      !stacking_client->AdjustStacking(&child, &target, &direction))
    return;

  const size_t child_i =
      base::ranges::find(children_, child) - children_.begin();
  const size_t target_i =
      base::ranges::find(children_, target) - children_.begin();

  DCHECK_LT(child_i, children_.size()) << "Child was not in list of children!";
  DCHECK_LT(target_i, children_.size())
      << "Target was not in list of children!";

  // Don't move the child if it is already in the right place.
  if ((direction == STACK_ABOVE && child_i == target_i + 1) ||
      (direction == STACK_BELOW && child_i + 1 == target_i))
    return;

  const size_t dest_i =
      direction == STACK_ABOVE ?
      (child_i < target_i ? target_i : target_i + 1) :
      (child_i < target_i ? target_i - 1 : target_i);
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
  if (frame_sink_id_.is_valid())
    UnregisterFrameSinkId();
  for (WindowObserver& observer : observers_)
    observer.OnWindowRemovingFromRootWindow(this, new_root);
  for (Window::Windows::const_iterator it = children_.begin();
       it != children_.end(); ++it) {
    (*it)->NotifyRemovingFromRootWindow(new_root);
  }
}

void Window::NotifyAddedToRootWindow() {
  if (frame_sink_id_.is_valid())
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
    return;  // |this| has been deleted.
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
    return false;  // |this| was deleted.

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
  // If it's in the process already, clean up the consumer state. Reentrant can
  // happen through some event handlers for CancelActiveTouches().
  Env* env = Env::GetInstance();
  if (cleaning_up_gesture_state_) {
    return env->gesture_recognizer()->CleanupStateForConsumer(this);
  }
  cleaning_up_gesture_state_ = true;

  // Cancelling active touches may end up destroying this window. We use a
  // tracker to detect this.
  WindowTracker tracking_this({this});

  bool state_modified = false;
  state_modified |= env->gesture_recognizer()->CancelActiveTouches(this);
  if (!tracking_this.Contains(this))
    return state_modified;

  state_modified |= env->gesture_recognizer()->CleanupStateForConsumer(this);
  // Potentially event handlers for CancelActiveTouches() within
  // CleanupGestureState may change the window hierarchy (or reorder the
  // |children_|), and therefore iterating over |children_| is not safe. Use
  // WindowTracker to track the list of children.
  WindowTracker children(children_);
  while (!children.windows().empty()) {
    Window* child = children.Pop();
    state_modified |= child->CleanupGestureState();
  }
  cleaning_up_gesture_state_ = false;
  return state_modified;
}

std::unique_ptr<cc::LayerTreeFrameSink> Window::CreateLayerTreeFrameSink() {
  // Currently we don't have a need for both SetEmbedFrameSinkId() and
  // this function be called.
  DCHECK(!embeds_external_client_);

  auto* context_factory = Env::GetInstance()->context_factory();
  auto* host_frame_sink_manager = context_factory->GetHostFrameSinkManager();

  if (!frame_sink_id_.is_valid()) {
    auto frame_sink_id = context_factory->AllocateFrameSinkId();
    host_frame_sink_manager->RegisterFrameSinkId(
        frame_sink_id, this, viz::ReportFirstSurfaceActivation::kYes);
    SetEmbedFrameSinkIdImpl(frame_sink_id);
  }

  // For creating a async frame sink which connects to the viz display
  // compositor.
  mojo::PendingRemote<viz::mojom::CompositorFrameSink> sink_remote;
  mojo::PendingReceiver<viz::mojom::CompositorFrameSink> sink_receiver =
      sink_remote.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient> client_remote;
  mojo::PendingReceiver<viz::mojom::CompositorFrameSinkClient> client_receiver =
      client_remote.InitWithNewPipeAndPassReceiver();
  host_frame_sink_manager->CreateCompositorFrameSink(
      frame_sink_id_, std::move(sink_receiver), std::move(client_remote));

  cc::mojo_embedder::AsyncLayerTreeFrameSink::InitParams params;
  params.gpu_memory_buffer_manager =
      Env::GetInstance()->context_factory()->GetGpuMemoryBufferManager();
  params.pipes.compositor_frame_sink_remote = std::move(sink_remote);
  params.pipes.client_receiver = std::move(client_receiver);
  auto frame_sink =
      std::make_unique<cc::mojo_embedder::AsyncLayerTreeFrameSink>(
          /*context_provider=*/nullptr, /*worker_context_provider=*/nullptr,
          /*shared_image_interface=*/nullptr, &params);
  frame_sink_ = frame_sink->GetWeakPtr();
  AllocateLocalSurfaceId();
  DCHECK(GetLocalSurfaceId().is_valid());
#if DCHECK_IS_ON()
  created_layer_tree_frame_sink_ = true;
#endif
  return std::move(frame_sink);
}

viz::SurfaceId Window::GetSurfaceId() {
  return viz::SurfaceId(GetFrameSinkId(), GetLocalSurfaceId());
}

viz::SurfaceId Window::GetSurfaceId() const {
  return const_cast<Window*>(this)->GetSurfaceId();
}

void Window::AllocateLocalSurfaceId() {
  if (!parent_local_surface_id_allocator_) {
    parent_local_surface_id_allocator_ =
        std::make_unique<viz::ParentLocalSurfaceIdAllocator>();
  }
  parent_local_surface_id_allocator_->GenerateId();
  UpdateLocalSurfaceId();
}

viz::ScopedSurfaceIdAllocator Window::GetSurfaceIdAllocator(
    base::OnceCallback<void()> allocation_task) {
  return viz::ScopedSurfaceIdAllocator(parent_local_surface_id_allocator_.get(),
                                       std::move(allocation_task));
}

const viz::LocalSurfaceId& Window::GetLocalSurfaceId() {
  if (!parent_local_surface_id_allocator_)
    AllocateLocalSurfaceId();
  return GetCurrentLocalSurfaceId();
}

void Window::InvalidateLocalSurfaceId(bool also_invalidate_allocation_group) {
  if (!parent_local_surface_id_allocator_)
    return;
  parent_local_surface_id_allocator_->Invalidate(
      also_invalidate_allocation_group);
}

void Window::UpdateLocalSurfaceIdFromEmbeddedClient(
    const std::optional<viz::LocalSurfaceId>&
        embedded_client_local_surface_id) {
  if (embedded_client_local_surface_id) {
    parent_local_surface_id_allocator_->UpdateFromChild(
        *embedded_client_local_surface_id);
    UpdateLocalSurfaceId();
  } else {
    AllocateLocalSurfaceId();
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
  SetEmbedFrameSinkIdImpl(frame_sink_id);
  embeds_external_client_ = true;
}

void Window::TrackOcclusionState() {
  Env::GetInstance()->GetWindowOcclusionTracker()->Track(this);
}

bool Window::RequiresDoubleTapGestureEvents() const {
  return delegate_ && delegate_->RequiresDoubleTapGestureEvents();
}

// static
const std::u16string Window::OcclusionStateToString(OcclusionState state) {
  return ui::metadata::TypeConverter<OcclusionState>::ToString(state);
}

// static
std::string_view Window::WindowTypeToString(client::WindowType type) {
  switch (type) {
    case client::WINDOW_TYPE_UNKNOWN:
      return "unknown";
    case client::WINDOW_TYPE_NORMAL:
      return "normal";
    case client::WINDOW_TYPE_POPUP:
      return "popup";
    case client::WINDOW_TYPE_CONTROL:
      return "control";
    case client::WINDOW_TYPE_MENU:
      return "menu";
    case client::WINDOW_TYPE_TOOLTIP:
      return "tooltip";
  }
}

void Window::SetOpaqueRegionsForOcclusion(
    const std::vector<gfx::Rect>& opaque_regions_for_occlusion) {
  // Opaque regions for occlusion do not apply to opaque windows, so only
  // allow opaque regions for occlusion to be set for them if they are the
  // same as the window bounds size.
  DCHECK(GetTransparent() || layer()->type() == ui::LAYER_NOT_DRAWN ||
         opaque_regions_for_occlusion.empty() ||
         (opaque_regions_for_occlusion.size() == 1 &&
          opaque_regions_for_occlusion[0] == gfx::Rect(bounds().size())));
  if (opaque_regions_for_occlusion == opaque_regions_for_occlusion_)
    return;
  opaque_regions_for_occlusion_ = opaque_regions_for_occlusion;
  for (auto& observer : observers_)
    observer.OnWindowOpaqueRegionsForOcclusionChanged(this);
}

void Window::NotifyResizeLoopStarted() {
  for (auto& observer : observers_)
    observer.OnResizeLoopStarted(this);
}

void Window::NotifyResizeLoopEnded() {
  for (auto& observer : observers_)
    observer.OnResizeLoopEnded(this);
}

void Window::OnPaintLayer(const ui::PaintContext& context) {
  Paint(context);
}

void Window::OnLayerBoundsChanged(const gfx::Rect& old_bounds,
                                  ui::PropertyChangeReason reason) {
  WindowOcclusionTracker::ScopedPause pause_occlusion_tracking;

  bounds_ = layer()->bounds();

  if (!IsRootWindow() && old_bounds.size() != bounds_.size() &&
      IsEmbeddingExternalContent()) {
    parent_local_surface_id_allocator_->GenerateId();
    if (frame_sink_) {
      frame_sink_->SetLocalSurfaceId(GetCurrentLocalSurfaceId());
    }
  }

  if (layout_manager_)
    layout_manager_->OnWindowResized();
  if (delegate_)
    delegate_->OnBoundsChanged(old_bounds, bounds_);
  for (auto& observer : observers_)
    observer.OnWindowBoundsChanged(this, old_bounds, bounds_, reason);

  // Trigger the changed notification for each of the bounds "properties".
  if (old_bounds.x() != bounds_.x())
    TriggerChangedCallback(&bounds_ + kBoundsX);
  if (old_bounds.y() != bounds_.y())
    TriggerChangedCallback(&bounds_ + kBoundsY);
  if (old_bounds.width() != bounds_.width())
    TriggerChangedCallback(&bounds_ + kBoundsWidth);
  if (old_bounds.height() != bounds_.height())
    TriggerChangedCallback(&bounds_ + kBoundsHeight);
}

void Window::OnLayerOpacityChanged(ui::PropertyChangeReason reason) {
  WindowOcclusionTracker::ScopedPause pause_occlusion_tracking;
  for (WindowObserver& observer : observers_)
    observer.OnWindowOpacitySet(this, reason);
}

void Window::OnLayerAlphaShapeChanged() {
  WindowOcclusionTracker::ScopedPause pause_occlusion_tracking;
  for (WindowObserver& observer : observers_)
    observer.OnWindowAlphaShapeSet(this);
}

void Window::OnLayerFillsBoundsOpaquelyChanged(
    ui::PropertyChangeReason reason) {
  // Let observers know that this window's transparent status has changed.
  // Transparent status can affect the occlusion computed for windows.
  WindowOcclusionTracker::ScopedPause pause_occlusion_tracking;

  // Non-transparent windows should not have opaque regions for occlusion set.
#if DCHECK_IS_ON()
  if (!GetTransparent() && layer()->type() != ui::LAYER_NOT_DRAWN) {
    DCHECK(opaque_regions_for_occlusion_.empty());
  }
#endif

  for (WindowObserver& observer : observers_)
    observer.OnWindowTransparentChanged(this, reason);
}

void Window::OnLayerTransformed(const gfx::Transform& old_transform,
                                ui::PropertyChangeReason reason) {
  WindowOcclusionTracker::ScopedPause pause_occlusion_tracking;
  for (WindowObserver& observer : observers_)
    observer.OnWindowTransformed(this, reason);
}

bool Window::CanAcceptEvent(const ui::Event& event) {
  // The client may forbid certain windows from receiving events at a given
  // point in time.
  client::EventClient* client = client::GetEventClient(GetRootWindow());
  if (client && !client->GetCanProcessEventsWithinSubtree(this))
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
               : Env::GetInstance();
  }
  return parent_;
}

std::unique_ptr<ui::EventTargetIterator> Window::GetChildIterator() const {
  return std::make_unique<ui::EventTargetIteratorPtrImpl<Window>>(children());
}

ui::EventTargeter* Window::GetEventTargeter() {
  return targeter_.get();
}

void Window::ConvertEventToTarget(const ui::EventTarget* target,
                                  ui::LocatedEvent* event) const {
  event->ConvertLocationToTarget(this, static_cast<const Window*>(target));
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

std::unique_ptr<ui::Layer> Window::ReleaseLayer() {
  if (number_of_capture_requests_) {
    // Before we release our own layer, if this window was marked for capture,
    // we need to reset the SubtreeCaptureId on that layer as it will no longer
    // be associated with us.
    DCHECK(subtree_capture_id_.is_valid());
    if (layer())
      layer()->SetSubtreeCaptureId(viz::SubtreeCaptureId());
  }
  return LayerOwner::ReleaseLayer();
}

std::unique_ptr<ui::Layer> Window::RecreateLayer() {
  WindowOcclusionTracker::ScopedPause pause_occlusion_tracking;

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

  // The old layer subtree must no longer be capturable.
  // Note that we don't need to worry about the newly recreated layer since
  // Window::SetLayer() will have taken care of it already.
  if (number_of_capture_requests_ && old_layer)
    old_layer->SetSubtreeCaptureId(viz::SubtreeCaptureId());

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

void Window::SetLayer(std::unique_ptr<ui::Layer> alayer) {
  LayerOwner::SetLayer(std::move(alayer));
  if (number_of_capture_requests_) {
    // If this window was marked for capture before, then the new layer that we
    // own now should be given the current SubtreeCaptureId that we have.
    DCHECK(subtree_capture_id_.is_valid());
    if (layer())
      layer()->SetSubtreeCaptureId(subtree_capture_id_);
  }
}

void Window::OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) {
  DCHECK_EQ(surface_info.id().frame_sink_id(), GetFrameSinkId());
  layer()->SetShowSurface(surface_info.id(), bounds().size(), SK_ColorWHITE,
                          cc::DeadlinePolicy::UseDefaultDeadline(),
                          false /* stretch_content_to_fill_bounds */);
}

void Window::OnFrameTokenChanged(uint32_t frame_token,
                                 base::TimeTicks activation_time) {}

void Window::UpdateLayerName() {
  DCHECK(layer());

  std::string layer_name(GetName());
  if (layer_name.empty())
    layer_name = "Unnamed Window";

  if (id_ != -1)
    layer_name += " " + base::NumberToString(id_);

  layer()->SetName(layer_name);
}

void Window::RegisterFrameSinkId() {
  DCHECK(frame_sink_id_.is_valid());
  if (registered_frame_sink_id_ || disable_frame_sink_id_registration_)
    return;
  if (auto* compositor = layer()->GetCompositor()) {
    compositor->AddChildFrameSink(frame_sink_id_);
    registered_frame_sink_id_ = true;
  }
}

void Window::UnregisterFrameSinkId() {
  if (!registered_frame_sink_id_)
    return;
  registered_frame_sink_id_ = false;
  if (auto* compositor = layer()->GetCompositor())
    compositor->RemoveChildFrameSink(frame_sink_id_);
}

void Window::UpdateLocalSurfaceId() {
  last_device_scale_factor_ = ui::GetScaleFactorForNativeView(this);
  if (frame_sink_) {
    frame_sink_->SetLocalSurfaceId(GetCurrentLocalSurfaceId());
  }
}

const viz::LocalSurfaceId& Window::GetCurrentLocalSurfaceId() const {
  return parent_local_surface_id_allocator_->GetCurrentLocalSurfaceId();
}

bool Window::IsEmbeddingExternalContent() const {
  return parent_local_surface_id_allocator_.get() != nullptr;
}

void Window::OnScopedWindowCaptureRequestAdded() {
  if (++number_of_capture_requests_ == 1) {
    DCHECK(!subtree_capture_id_.is_valid());
    DCHECK(!layer() || !layer()->GetSubtreeCaptureId().is_valid());

    subtree_capture_id_ =
        Env::GetInstance()->context_factory()->AllocateSubtreeCaptureId();
    if (layer())
      layer()->SetSubtreeCaptureId(subtree_capture_id_);
  }

  DCHECK(subtree_capture_id_.is_valid());
}

void Window::OnScopedWindowCaptureRequestRemoved() {
  DCHECK(subtree_capture_id_.is_valid());
  DCHECK(number_of_capture_requests_);

  --number_of_capture_requests_;
  DCHECK_GE(number_of_capture_requests_, 0);

  if (number_of_capture_requests_ == 0) {
    subtree_capture_id_ = viz::SubtreeCaptureId();
    if (layer())
      layer()->SetSubtreeCaptureId(subtree_capture_id_);
  }
}

int Window::GetHeight() const {
  return bounds().height();
}

int Window::GetWidth() const {
  return bounds().width();
}

int Window::GetX() const {
  return bounds().x();
}

int Window::GetY() const {
  return bounds().y();
}

void Window::SetHeight(int height) {
  if (height == bounds().height())
    return;
  // Bounds changed notification is done within OnLayerBoundsChanged.
  // Here and below.
  SetBounds({bounds().x(), bounds().y(), bounds().width(), height});
}

void Window::SetWidth(int width) {
  if (width == bounds().width())
    return;
  SetBounds({bounds().x(), bounds().y(), width, bounds().height()});
}

void Window::SetX(int x) {
  if (x == bounds().x())
    return;
  SetBounds({x, bounds().y(), bounds().width(), bounds().height()});
}

void Window::SetY(int y) {
  if (y == bounds().y())
    return;
  SetBounds({bounds().x(), y, bounds().width(), bounds().height()});
}

void Window::SetLayoutManagerImpl(
    std::unique_ptr<LayoutManager> layout_manager) {
  layout_manager_ = std::move(layout_manager);
  if (!layout_manager_)
    return;
  // If we're changing to a new layout manager, ensure it is aware of all the
  // existing child windows.
  for (Windows::const_iterator it = children_.begin(); it != children_.end();
       ++it)
    layout_manager_->OnWindowAddedToLayout(*it);
}

bool Window::GetCapture() const {
  return const_cast<Window*>(this)->HasCapture();
}

bool Window::GetVisible() const {
  return IsVisible();
}

void Window::SetVisible(bool visible) {
  if (visible == IsVisible())
    return;
  if (visible)
    Show();
  else
    Hide();
  // Changed notification is handled in SetVisibleInternal().
}

BEGIN_METADATA_BASE(Window)
ADD_READONLY_PROPERTY_METADATA(gfx::Rect, ActualBoundsInRootWindow)
ADD_READONLY_PROPERTY_METADATA(gfx::Rect, ActualBoundsInScreen)
ADD_READONLY_PROPERTY_METADATA(gfx::Rect, BoundsInRootWindow)
ADD_READONLY_PROPERTY_METADATA(gfx::Rect, BoundsInScreen)
ADD_READONLY_PROPERTY_METADATA(bool, Capture)
ADD_PROPERTY_METADATA(int, Height)
ADD_PROPERTY_METADATA(int, Width)
ADD_PROPERTY_METADATA(int, X)
ADD_PROPERTY_METADATA(int, Y)
ADD_READONLY_PROPERTY_METADATA(Window::OcclusionState, OcclusionState)
ADD_READONLY_PROPERTY_METADATA(viz::SurfaceId, SurfaceId)
ADD_PROPERTY_METADATA(int, Id)
ADD_PROPERTY_METADATA(std::string, Name)
ADD_PROPERTY_METADATA(std::u16string, Title)
ADD_PROPERTY_METADATA(bool, Transparent)
ADD_PROPERTY_METADATA(client::WindowType, Type)
ADD_PROPERTY_METADATA(bool, Visible)
END_METADATA

}  // namespace aura
