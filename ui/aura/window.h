// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_H_
#define UI_AURA_WINDOW_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/viz/common/surfaces/scoped_surface_id_allocator.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_port.h"
#include "ui/base/class_property.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_owner.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_target.h"
#include "ui/events/event_targeter.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace cc {
class LayerTreeFrameSink;
}

namespace display {
class Display;
}

namespace gfx {
class Transform;
}

namespace ui {
enum class DomCode;
class Layer;
}  // namespace ui

namespace ws {
namespace mojom {
enum class EventTargetingPolicy;
}
}

namespace aura {

class Env;
class LayoutManager;
class ScopedKeyboardHook;
class WindowDelegate;
class WindowObserver;
class WindowPortForShutdown;
class WindowTargeter;
class WindowTreeHost;

// Defined in class_property.h (which we do not include)
template<typename T>
using WindowProperty = ui::ClassProperty<T>;

namespace test {
class WindowTestApi;
}

// Aura window implementation. Interesting events are sent to the
// WindowDelegate.
// TODO(beng): resolve ownership.
class AURA_EXPORT Window : public ui::LayerDelegate,
                           public ui::LayerOwner,
                           public ui::EventTarget,
                           public ui::GestureConsumer,
                           public ui::PropertyHandler {
 public:
  // Initial value of id() for newly created windows.
  static constexpr int kInitialId = -1;

  // Used when stacking windows.
  enum StackDirection {
    STACK_ABOVE,
    STACK_BELOW
  };
  enum class OcclusionState {
    // The window's occlusion state isn't tracked (Window::TrackOcclusionState)
    // or hasn't been computed yet.
    UNKNOWN,
    // The window or one of its descendants IsVisible() [1] and:
    // - Its bounds aren't completely covered by fully opaque windows [2], or,
    // - Its transform, bounds or opacity is animated.
    VISIBLE,
    // The window or one of its descendants IsVisible() [1], but they all:
    // - Have bounds completely covered by fully opaque windows [2], and,
    // - Have no transform, bounds or opacity animation.
    OCCLUDED,
    // The window is not IsVisible() [1].
    HIDDEN,
    // [1] A window can only be IsVisible() if all its parent are IsVisible().
    // [2] A window is "fully opaque" if:
    // - It's visible (IsVisible()).
    // - It's not transparent (transparent()).
    // - It's transform, bounds and opacity aren't animated.
    // - Its combined opacity is 1 (GetCombinedOpacity()).
    // - It has content to draw. Either the type of its layer is not
    //     ui::LAYER_NOT_DRAWN, or it is a server window hosting remote client
    //     content in Window Service.
    //
    // TODO(fdoray): A window that clips its children shouldn't be VISIBLE just
    // because it has an animated child.
  };

  typedef std::vector<Window*> Windows;

  explicit Window(WindowDelegate* delegate,
                  client::WindowType type = client::WINDOW_TYPE_UNKNOWN,
                  Env* env = nullptr);
  Window(WindowDelegate* delegate,
         std::unique_ptr<WindowPort> port,
         client::WindowType type = client::WINDOW_TYPE_UNKNOWN,
         Env* env = nullptr);
  ~Window() override;

  // Initializes the window. This creates the window's layer.
  void Init(ui::LayerType layer_type);

  void set_owned_by_parent(bool owned_by_parent) {
    owned_by_parent_ = owned_by_parent;
  }
  bool owned_by_parent() const { return owned_by_parent_; }

  // A type is used to identify a class of Windows and customize behavior such
  // as event handling and parenting.  This field should only be consumed by the
  // shell -- Aura itself shouldn't contain type-specific logic.
  client::WindowType type() const { return type_; }
  void SetType(client::WindowType type);

  int id() const { return id_; }
  void set_id(int id) { id_ = id; }

  const std::string& GetName() const;
  void SetName(const std::string& name);

  const base::string16& GetTitle() const;
  void SetTitle(const base::string16& title);

  bool transparent() const { return transparent_; }

  // Note: Setting a window transparent has significant performance impact,
  // especially on low-end Chrome OS devices. Please ensure you are not
  // adding unnecessary overdraw. When in doubt, talk to the graphics team.
  void SetTransparent(bool transparent);

  // See description in Layer::SetFillsBoundsCompletely.
  void SetFillsBoundsCompletely(bool fills_bounds);

  WindowDelegate* delegate() { return delegate_; }
  const WindowDelegate* delegate() const { return delegate_; }

  const gfx::Rect& bounds() const { return bounds_; }

  Window* parent() { return parent_; }
  const Window* parent() const { return parent_; }

  // Returns the root Window that contains this Window. The root Window is
  // defined as the Window that has a dispatcher. These functions return NULL if
  // the Window is contained in a hierarchy that does not have a dispatcher at
  // its root.
  Window* GetRootWindow();
  const Window* GetRootWindow() const;

  WindowTreeHost* GetHost();
  const WindowTreeHost* GetHost() const;
  void set_host(WindowTreeHost* host) { host_ = host; }
  bool IsRootWindow() const { return !!host_; }

  // Changes the visibility of the window.
  void Show();
  void Hide();
  // Returns true if this window and all its ancestors are visible.
  bool IsVisible() const;
  // Returns the visibility requested by this window. IsVisible() takes into
  // account the visibility of the layer and ancestors, where as this tracks
  // whether Show() without a Hide() has been invoked.
  bool TargetVisibility() const { return visible_; }
  // Returns the occlusion state of this window. Is UNKNOWN if the occlusion
  // state of this window isn't tracked (Window::TrackOcclusionState) or
  // hasn't been computed yet. Is stale if called within the scope of a
  // WindowOcclusionTracker::ScopedPause.
  OcclusionState occlusion_state() const { return occlusion_state_; }

  // Returns the window's bounds in root window's coordinates.
  gfx::Rect GetBoundsInRootWindow() const;

  // Returns the window's bounds in screen coordinates.
  // How the root window's coordinates is mapped to screen's coordinates
  // is platform dependent and defined in the implementation of the
  // |aura::client::ScreenPositionClient| interface.
  gfx::Rect GetBoundsInScreen() const;

  void SetTransform(const gfx::Transform& transform);
  const gfx::Transform& transform() const { return layer()->transform(); }

  // Assigns a LayoutManager to size and place child windows.
  // The Window takes ownership of the LayoutManager.
  void SetLayoutManager(LayoutManager* layout_manager);
  LayoutManager* layout_manager() { return layout_manager_.get(); }

  // Sets a new event-targeter for the window, and returns the previous
  // event-targeter.
  std::unique_ptr<WindowTargeter> SetEventTargeter(
      std::unique_ptr<WindowTargeter> targeter);
  WindowTargeter* targeter() { return targeter_.get(); }
  const WindowTargeter* targeter() const { return targeter_.get(); }

  // Changes the bounds of the window. If present, the window's parent's
  // LayoutManager may adjust the bounds.
  void SetBounds(const gfx::Rect& new_bounds);

  // Changes the bounds of the window in the screen coordinates.
  // If present, the window's parent's LayoutManager may adjust the bounds.
  void SetBoundsInScreen(const gfx::Rect& new_bounds_in_screen_coords,
                         const display::Display& dst_display);

  // Returns the target bounds of the window. If the window's layer is
  // not animating, it simply returns the current bounds.
  gfx::Rect GetTargetBounds() const;

  // Marks the a portion of window as needing to be painted.
  void SchedulePaintInRect(const gfx::Rect& rect);

  // Stacks the specified child of this Window at the front of the z-order.
  void StackChildAtTop(Window* child);

  // Stacks |child| above |target|.  Does nothing if |child| is already above
  // |target|.  Does not stack on top of windows with NULL layer delegates,
  // see WindowTest.StackingMadrigal for details.
  void StackChildAbove(Window* child, Window* target);

  // Stacks the specified child of this window at the bottom of the z-order.
  void StackChildAtBottom(Window* child);

  // Stacks |child| below |target|. Does nothing if |child| is already below
  // |target|.
  void StackChildBelow(Window* child, Window* target);

  // Tree operations.
  void AddChild(Window* child);
  void RemoveChild(Window* child);

  const Windows& children() const { return children_; }

  // Returns true if this Window contains |other| somewhere in its children.
  bool Contains(const Window* other) const;

  // Retrieves the first-level child with the specified id, or NULL if no first-
  // level child is found matching |id|.
  Window* GetChildById(int id);
  const Window* GetChildById(int id) const;

  // Converts |point| from |source|'s coordinates to |target|'s. If |source| is
  // NULL, the function returns without modifying |point|. |target| cannot be
  // NULL.
  static void ConvertPointToTarget(const Window* source,
                                   const Window* target,
                                   gfx::PointF* point);
  static void ConvertPointToTarget(const Window* source,
                                   const Window* target,
                                   gfx::Point* point);
  static void ConvertRectToTarget(const Window* source,
                                  const Window* target,
                                  gfx::Rect* rect);

  // Convert the native |point| in pixels to the target's host's coordiantes if
  // source and target have different hosts.
  static void ConvertNativePointToTargetHost(const Window* source,
                                             const Window* target,
                                             gfx::PointF* point);
  static void ConvertNativePointToTargetHost(const Window* source,
                                             const Window* target,
                                             gfx::Point* point);

  // Moves the cursor to the specified location relative to the window.
  void MoveCursorTo(const gfx::Point& point_in_window);

  // Returns the cursor for the specified point, in window coordinates.
  gfx::NativeCursor GetCursor(const gfx::Point& point) const;

  // Returns true if the children of this should be restacked by the
  // transient window related classes to honor transient window stacking.
  bool ShouldRestackTransientChildren();

  // Add/remove observer.
  void AddObserver(WindowObserver* observer);
  void RemoveObserver(WindowObserver* observer);
  bool HasObserver(const WindowObserver* observer) const;

  void SetEventTargetingPolicy(ws::mojom::EventTargetingPolicy policy);
  ws::mojom::EventTargetingPolicy event_targeting_policy() const {
    return event_targeting_policy_;
  }

  // Returns true if the |point_in_root| in root window's coordinate falls
  // within this window's bounds. Returns false if the window is detached
  // from root window.
  bool ContainsPointInRoot(const gfx::Point& point_in_root) const;

  // Returns true if relative-to-this-Window's-origin |local_point| falls
  // within this Window's bounds.
  bool ContainsPoint(const gfx::Point& local_point) const;

  // Returns the Window that most closely encloses |local_point| for the
  // purposes of event targeting.
  Window* GetEventHandlerForPoint(const gfx::Point& local_point);

  // Returns this window's toplevel window (the highest-up-the-tree ancestor
  // that has a delegate set).  The toplevel window may be |this|.
  Window* GetToplevelWindow();

  // Claims focus.
  void Focus();

  // Returns true if the Window is currently the focused window.
  bool HasFocus() const;

  // Returns true if the Window can be focused.
  bool CanFocus() const;

  // Returns true if the Window can receive events.
  bool CanReceiveEvents() const;

  // Does a capture on the window. This does nothing if the window isn't showing
  // (VISIBILITY_SHOWN) or isn't contained in a valid window hierarchy.
  void SetCapture();

  // Releases a capture.
  void ReleaseCapture();

  // Returns true if this window has capture.
  bool HasCapture();

  // Requests that |keys| be intercepted at the platform level and routed
  // directly to the web content.  If |codes| has no value, all keys will be
  // intercepted.  Returns a ScopedKeyboardHook instance which stops capturing
  // system key events when destroyed.
  std::unique_ptr<ScopedKeyboardHook> CaptureSystemKeyEvents(
      base::Optional<base::flat_set<ui::DomCode>> codes);

  // Suppresses painting window content by disgarding damaged rect and ignoring
  // new paint requests. This is a one way operation and there is no way to
  // reenable painting.
  void SuppressPaint();

  // NativeWidget::[GS]etNativeWindowProperty use strings as keys, and this is
  // difficult to change while retaining compatibility with other platforms.
  // TODO(benrg): Find a better solution.
  void SetNativeWindowProperty(const char* key, void* value);
  void* GetNativeWindowProperty(const char* key) const;

  // Type of a function to delete a property that this window owns.
  //typedef void (*PropertyDeallocator)(int64_t value);

  // Overridden from ui::LayerDelegate:
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

  // Overridden from ui::LayerOwner:
  std::unique_ptr<ui::Layer> RecreateLayer() override;

#if !defined(NDEBUG)
  // These methods are useful when debugging.
  std::string GetDebugInfo() const;
  void PrintWindowHierarchy(int depth) const;
#endif

  // Returns true if there was state needing to be cleaned up.
  bool CleanupGestureState();

  // Create a LayerTreeFrameSink for the aura::Window.
  std::unique_ptr<cc::LayerTreeFrameSink> CreateLayerTreeFrameSink();

  // Gets the current viz::SurfaceId.
  viz::SurfaceId GetSurfaceId() const;

  // Get the time at which the current viz::LocalSurfaceId was allocated.
  base::TimeTicks GetLocalSurfaceIdAllocationTime() const;

  // Forces the window to allocate a new viz::LocalSurfaceId for the next
  // CompositorFrame submission in anticipation of a synchronization operation
  // that does not involve a resize or a device scale factor change.
  void AllocateLocalSurfaceId();

  viz::ScopedSurfaceIdAllocator GetSurfaceIdAllocator(
      base::OnceCallback<void()> allocation_task);

  // Gets the current viz::LocalSurfaceId.
  const viz::LocalSurfaceId& GetLocalSurfaceId() const;

  // Sets the current viz::LocalSurfaceId, in cases where the embedded client
  // has allocated one. Also sets child sequence number component of the
  // viz::LocalSurfaceId allocator.
  void UpdateLocalSurfaceIdFromEmbeddedClient(
      const base::Optional<viz::LocalSurfaceId>&
          embedded_client_local_surface_id,
      const base::Optional<base::TimeTicks>&
          embedded_client_local_surface_id_allocation_time);

  // Returns the FrameSinkId. In LOCAL mode, this returns a valid FrameSinkId
  // only if a LayerTreeFrameSink has been created. In MUS mode, this always
  // return a valid FrameSinkId.
  const viz::FrameSinkId& GetFrameSinkId() const;

  // Use SetEmbedFrameSinkId() when this window is embedding another client.
  // See comment for |frame_sink_id_| below for more details.
  void SetEmbedFrameSinkId(const viz::FrameSinkId& embed_frame_sink_id);
  void set_frame_sink_id(const viz::FrameSinkId& frame_sink_id) {
    DCHECK(!embeds_external_client_);
    DCHECK(!frame_sink_id_.is_valid());
    frame_sink_id_ = frame_sink_id;
  }

  // Returns whether this window is embedding another client.
  bool IsEmbeddingClient() const;

  // Starts occlusion state tracking.
  void TrackOcclusionState();

  Env* env() { return env_; }
  const Env* env() const { return env_; }

#if DCHECK_IS_ON()
  // If passed a non-null value then a non-null aura::Env must be supplied to
  // the constructor. |error_string| is the string supplied to the DCHECK
  // calls.
  static void SetEnvArgRequired(const char* error_string);
#endif

  // ui::GestureConsumer:
  bool RequiresDoubleTapGestureEvents() const override;

 protected:
  // Deletes (or removes if not owned by parent) all child windows. Intended for
  // use from the destructor.
  void RemoveOrDestroyChildren();

  // Overrides from ui::PropertyHandler
  std::unique_ptr<ui::PropertyData> BeforePropertyChange(const void* key)
      override;
  void AfterPropertyChange(const void* key,
                           int64_t old_value,
                           std::unique_ptr<ui::PropertyData> data) override;
 private:
  friend class HitTestDataProviderAura;
  friend class LayoutManager;
  friend class PropertyConverter;
  friend class WindowOcclusionTracker;
  friend class WindowPort;
  friend class WindowPortForShutdown;
  friend class WindowPortMus;
  friend class WindowTargeter;
  friend class test::WindowTestApi;

  // Returns true if the mouse pointer at relative-to-this-Window's-origin
  // |local_point| can trigger an event for this Window.
  // TODO(beng): A Window can supply a hit-test mask to cause some portions of
  // itself to not trigger events, causing the events to fall through to the
  // Window behind.
  bool HitTest(const gfx::Point& local_point);

  // Changes the bounds of the window without condition.
  void SetBoundsInternal(const gfx::Rect& new_bounds);

  // Updates the visible state of the layer, but does not make visible-state
  // specific changes. Called from Show()/Hide().
  void SetVisible(bool visible);

  // Updates the occlusion state of the window.
  void SetOcclusionState(OcclusionState occlusion_state);

  // Schedules a paint for the Window's entire bounds.
  void SchedulePaint();

  // Asks the delegate to paint the window.
  void Paint(const ui::PaintContext& context);

  // Implementation of RemoveChild(). If |child| is being removed as the result
  // of an add, |new_parent| is the new parent |child| is going to be parented
  // to.
  void RemoveChildImpl(Window* child, Window* new_parent);

  // Called when this window's parent has changed.
  void OnParentChanged();

  // The various stacking functions call into this to do the actual stacking.
  void StackChildRelativeTo(Window* child,
                            Window* target,
                            StackDirection direction);

  // Invoked from StackChildRelativeTo() to stack the layers appropriately
  // when stacking |child| relative to |target|.
  void StackChildLayerRelativeTo(Window* child,
                                 Window* target,
                                 StackDirection direction);

  // Called when this window's stacking order among its siblings is changed.
  void OnStackingChanged();

  // Notifies observers registered with this Window (and its subtree) when the
  // Window has been added or is about to be removed from a RootWindow.
  void NotifyRemovingFromRootWindow(Window* new_root);
  void NotifyAddedToRootWindow();

  // Methods implementing hierarchy change notifications. See WindowObserver for
  // more details.
  void NotifyWindowHierarchyChange(
      const WindowObserver::HierarchyChangeParams& params);
  // Notifies this window and its child hierarchy.
  void NotifyWindowHierarchyChangeDown(
      const WindowObserver::HierarchyChangeParams& params);
  // Notifies this window and its parent hierarchy.
  void NotifyWindowHierarchyChangeUp(
      const WindowObserver::HierarchyChangeParams& params);
  // Notifies this window's observers.
  void NotifyWindowHierarchyChangeAtReceiver(
      const WindowObserver::HierarchyChangeParams& params);

  // Methods implementing visibility change notifications. See WindowObserver
  // for more details.
  void NotifyWindowVisibilityChanged(aura::Window* target, bool visible);
  // Notifies this window's observers. Returns false if |this| was deleted
  // during the call (by an observer), otherwise true.
  bool NotifyWindowVisibilityChangedAtReceiver(aura::Window* target,
                                               bool visible);
  // Notifies this window and its child hierarchy. Returns false if
  // |this| was deleted during the call (by an observer), otherwise
  // true.
  bool NotifyWindowVisibilityChangedDown(aura::Window* target, bool visible);
  // Notifies this window and its parent hierarchy.
  void NotifyWindowVisibilityChangedUp(aura::Window* target, bool visible);

  // Overridden from ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnLayerBoundsChanged(const gfx::Rect& old_bounds,
                            ui::PropertyChangeReason reason) override;
  void OnLayerTransformed(const gfx::Transform& old_transform,
                          ui::PropertyChangeReason reason) override;
  void OnLayerOpacityChanged(ui::PropertyChangeReason reason) override;
  void OnLayerAlphaShapeChanged() override;

  // Overridden from ui::EventTarget:
  bool CanAcceptEvent(const ui::Event& event) override;
  EventTarget* GetParentTarget() override;
  std::unique_ptr<ui::EventTargetIterator> GetChildIterator() const override;
  ui::EventTargeter* GetEventTargeter() override;
  void ConvertEventToTarget(ui::EventTarget* target,
                            ui::LocatedEvent* event) override;
  gfx::PointF GetScreenLocationF(const ui::LocatedEvent& event) const override;

  // Updates the layer name based on the window's name and id.
  void UpdateLayerName();

  void RegisterFrameSinkId();
  void UnregisterFrameSinkId();

  // Env this window was created with. Env::GetInstance() if a null Env was
  // supplied.
  Env* const env_;

  bool registered_frame_sink_id_ = false;
  bool disable_frame_sink_id_registration_ = false;

  bool created_layer_tree_frame_sink_ = false;

  // Window owns its corresponding WindowPort, but the ref is held as a raw
  // pointer in |port_| so that it can still be accessed during destruction.
  // This is important as deleting the WindowPort may result in trying to lookup
  // the WindowPort associated with the Window.
  //
  // NOTE: this value is reset for windows that exist when WindowTreeClient
  // is deleted.
  std::unique_ptr<WindowPort> port_owner_;
  WindowPort* port_;

  // Bounds of this window relative to the parent. This is cached as the bounds
  // of the Layer and Window are not necessarily the same. In particular bounds
  // of the Layer are relative to the first ancestor with a Layer, where as this
  // is relative to the parent Window.
  gfx::Rect bounds_;

  WindowTreeHost* host_;

  client::WindowType type_;

  // True if the Window is owned by its parent - i.e. it will be deleted by its
  // parent during its parents destruction. True is the default.
  bool owned_by_parent_;

  WindowDelegate* delegate_;

  // The Window's parent.
  Window* parent_;

  // Child windows. Topmost is last.
  Windows children_;

  // The visibility state of the window as set by Show()/Hide(). This may differ
  // from the visibility of the underlying layer, which may remain visible after
  // the window is hidden (e.g. to animate its disappearance).
  bool visible_;

  // Occlusion state of the window.
  OcclusionState occlusion_state_;

  int id_;

  // The FrameSinkId associated with this window. If this window is embedding
  // another client, then this should be set to the FrameSinkId of that client,
  // and |embeds_external_client_| is turned on. However, a window can still
  // have a valid FrameSinkId without embedding another client, to facilitate
  // hit-testing.
  viz::FrameSinkId frame_sink_id_;
  bool embeds_external_client_ = false;

  // Whether layer is initialized as non-opaque. Defaults to false.
  bool transparent_;

  std::unique_ptr<LayoutManager> layout_manager_;
  std::unique_ptr<WindowTargeter> targeter_;

  // Makes the window pass all events through to any windows behind it.
  ws::mojom::EventTargetingPolicy event_targeting_policy_;

  base::ObserverList<WindowObserver, true>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(Window);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_H_
