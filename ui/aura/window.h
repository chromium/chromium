// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_H_
#define UI_AURA_WINDOW_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/scoped_surface_id_allocator.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"
#include "components/viz/host/host_frame_sink_client.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/scoped_window_capture_request.h"
#include "ui/aura/window_observer.h"
#include "ui/base/class_property.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_owner.h"
#include "ui/compositor/layer_type.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_target.h"
#include "ui/events/event_targeter.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

#if BUILDFLAG(IS_APPLE)
#error "This file must not be included on macOS; Chromium Mac doesn't use Aura."
#endif

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
enum class DomCode : uint32_t;
class Layer;
}  // namespace ui

namespace viz {
class ParentLocalSurfaceIdAllocator;
class SurfaceId;
}

namespace aura {

class LayoutManager;
class ScopedKeyboardHook;
class ScopedWindowEventTargetingBlocker;
class WindowDelegate;
class WindowTargeter;
class WindowTreeHost;

// Defined in class_property.h (which we do not include)
template <typename T>
using WindowProperty = ui::ClassProperty<T>;

namespace test {
class WindowTestApi;
}

enum class EventTargetingPolicy {
  // The target is a valid target for events, but none of its descendants are
  // considered.
  kTargetOnly,

  // The target and its descendants are possible targets. This is the default.
  kTargetAndDescendants,

  // The target is not a valid target, but its descendants are possible targets.
  kDescendantsOnly,

  // Neither the target nor its descendants are valid targets.
  kNone
};

// Aura window implementation. Interesting events are sent to the
// WindowDelegate.
class AURA_EXPORT Window : public ui::LayerDelegate,
                           public ui::LayerOwner,
                           public ui::EventTarget,
                           public ui::GestureConsumer,
                           public ui::PropertyHandler,
                           public ui::metadata::MetaDataProvider,
                           public viz::HostFrameSinkClient {
 public:
  METADATA_HEADER_BASE(Window);

  // Initial value of id() for newly created windows.
  static constexpr int kInitialId = -1;

  // Used when stacking windows.
  enum StackDirection { STACK_ABOVE, STACK_BELOW };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class OcclusionState {
    // The window's occlusion state isn't tracked (Window::TrackOcclusionState)
    // or hasn't been computed yet.
    UNKNOWN = 0,
    // The window or one of its descendants IsVisible() [1] and:
    // - Its bounds aren't completely covered by fully opaque windows [2], or,
    // - Its transform, bounds or opacity is animated.
    VISIBLE = 1,
    // The window or one of its descendants IsVisible() [1], but they all:
    // - Have bounds completely covered by fully opaque windows [2], and,
    // - Have no transform, bounds or opacity animation.
    OCCLUDED = 2,
    // The window is not IsVisible() [1].
    HIDDEN = 3,
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
    kMaxValue = HIDDEN,
  };

  using Windows = std::vector<raw_ptr<Window, VectorExperimental>>;

  explicit Window(WindowDelegate* delegate,
                  client::WindowType type = client::WINDOW_TYPE_UNKNOWN);

  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  ~Window() override;

  // Initializes the window. This creates the window's layer.
  void Init(ui::LayerType layer_type);

  bool is_destroying() const { return is_destroying_; }
  void set_owned_by_parent(bool owned_by_parent) {
    owned_by_parent_ = owned_by_parent;
  }
  bool owned_by_parent() const { return owned_by_parent_; }

  // A type is used to identify a class of Windows and customize behavior such
  // as event handling and parenting.  This field should only be consumed by the
  // shell -- Aura itself shouldn't contain type-specific logic.
  client::WindowType GetType() const;
  void SetType(client::WindowType type);

  int GetId() const;
  void SetId(int id);

  // ui::GestureConsumer:
  const std::string& GetName() const override;
  void SetName(const std::string& name);

  const std::u16string& GetTitle() const;
  void SetTitle(const std::u16string& title);

  bool GetTransparent() const;

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
  // defined as the Window that has a dispatcher. These functions return nullptr
  // if the Window is contained in a hierarchy that does not have a dispatcher
  // at its root.
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
  OcclusionState GetOcclusionState() const;

  // Returns the currently occluded region in the root Window coordinates. This
  // will be empty unless the window is tracked and has a VISIBLE occlusion
  // state. That is, this is only maintained when the window is partially
  // occluded. Further, this region may extend outside the window bounds. For
  // performance reasons, the actual intersection with the window is not
  // computed. The occluded region is the set of window rectangles that may
  // occlude this window. Note that this means that the occluded region may be
  // updated if one of those windows moves, even if the actual intersection of
  // the occluded region with this window does not change. Clients may compute
  // the actual intersection region if necessary.
  const SkRegion& occluded_region_in_root() const {
    return occluded_region_in_root_;
  }

  // Makes this *non-root* window individually capturable by the
  // |FrameSinkVideoCapturer| by tagging its layer with a unique
  // |viz::SubtreeCaptureId| which will force the layer tree root at this
  // window's layer to a render surface that draws into a render pass that is
  // identifiable by the capturer using that ID.
  //
  // Note that this should only be called for non-root windows. Root windows are
  // already capturable by the capturer as they're identifiable by their
  // |viz::FrameSinkId| and thei associated root render pass, so there's no need
  // to call this.
  //
  // This returns a scoped object associated with this request to make the
  // window capturable, since multiple capturers can capture the same window at
  // the same time. Once all requests are destroyed, this window will no longer
  // be individually capturable, and its layer won't be tagged with a valid
  // |viz::SubtreeCaptureId|.
  // See https://crbug.com/1143930 for more details.
  [[nodiscard]] ScopedWindowCaptureRequest MakeWindowCapturable();
  const viz::SubtreeCaptureId& subtree_capture_id() const {
    return subtree_capture_id_;
  }

  // Returns the window's bounds in root window's coordinates. The returned
  // value is calculated using the target transform. The target transform is the
  // end value of a transform animation. If there is no animation ongoing, the
  // target transform is the same as the current transform.
  gfx::Rect GetBoundsInRootWindow() const;

  // Similar to `GetBoundsInRootWindow()` except that the returned value is
  // calculated using the current transform. If there is no animation ongoing,
  // this function returns the same value as `GetBoundsInRootWindow()`.
  gfx::Rect GetActualBoundsInRootWindow() const;

  // Returns the window's bounds in screen coordinates. The returned
  // value is calculated using the target transform. The target transform is the
  // end value of a transform animation. If there is no animation ongoing, the
  // target transform is the same as the current transform.
  // How the root window's coordinates is mapped to screen's coordinates
  // is platform dependent and defined in the implementation of the
  // |aura::client::ScreenPositionClient| interface.
  gfx::Rect GetBoundsInScreen() const;

  // Similar to `GetBoundsInScreen()` except that the returned value is
  // calculated using the current transform. If there is no animation ongoing,
  // this function returns the same value as `GetBoundsInScreen()`.
  gfx::Rect GetActualBoundsInScreen() const;

  void SetTransform(const gfx::Transform& transform);
  const gfx::Transform& transform() const;

  // Assigns a LayoutManager to size and place child windows.
  template <typename LayoutManager>
  LayoutManager* SetLayoutManager(
      std::unique_ptr<LayoutManager> layout_manager) {
    LayoutManager* layout_manager_local = layout_manager.get();
    SetLayoutManagerImpl(std::move(layout_manager));
    return layout_manager_local;
  }
  void SetLayoutManager(std::nullptr_t);
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

  // Forwards directly to the layer. See Layer::ScheduleDraw() for details.
  void ScheduleDraw();

  // Marks the a portion of window as needing to be painted.
  void SchedulePaintInRect(const gfx::Rect& rect);

  // Stacks the specified child of this Window at the front of the z-order.
  void StackChildAtTop(Window* child);

  // Stacks |child| above |target|.  Does nothing if |child| is already above
  // |target|.  Does not stack on top of windows with nullptr layer delegates,
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

  // Retrieves the first-level child with the specified id, or nullptr if no
  // first- level child is found matching |id|.
  Window* GetChildById(int id);
  const Window* GetChildById(int id) const;

  // Converts |point| from |source|'s coordinates to |target|'s. If |source| is
  // nullptr, the function returns without modifying |point|. |target| cannot be
  // nullptr. Use layers' target transform in coordinate conversions. The target
  // transform is the end value of a transform animation. If there is no
  // animation ongoing, the target transform is the same as the current
  // transform.
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

  // Add/remove observer.
  void AddObserver(WindowObserver* observer);
  void RemoveObserver(WindowObserver* observer);
  bool HasObserver(const WindowObserver* observer) const;

  void SetEventTargetingPolicy(EventTargetingPolicy policy);
  EventTargetingPolicy event_targeting_policy() const {
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
      std::optional<base::flat_set<ui::DomCode>> codes);

  // NativeWidget::[GS]etNativeWindowProperty use strings as keys, and this is
  // difficult to change while retaining compatibility with other platforms.
  // TODO(benrg): Find a better solution.
  void SetNativeWindowProperty(const char* key, void* value);
  void* GetNativeWindowProperty(const char* key) const;

  // Type of a function to delete a property that this window owns.
  // typedef void (*PropertyDeallocator)(int64_t value);

  // Overridden from ui::LayerDelegate:
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;
  void UpdateVisualState() override;

  // ui::LayerOwner:
  std::unique_ptr<ui::Layer> ReleaseLayer() override;
  std::unique_ptr<ui::Layer> RecreateLayer() override;
  void SetLayer(std::unique_ptr<ui::Layer> layer) override;

  void GetDebugInfo(const aura::Window* active_window,
                    const aura::Window* focused_window,
                    const aura::Window* capture_window,
                    std::ostringstream* out) const;
#if DCHECK_IS_ON()
  // These methods are useful when debugging.
  std::string GetWindowHierarchy(int depth) const;
  void PrintWindowHierarchy(int depth) const;
#endif

  // Returns true if there was state needing to be cleaned up.
  bool CleanupGestureState();

  // Create a LayerTreeFrameSink for the aura::Window.
  std::unique_ptr<cc::LayerTreeFrameSink> CreateLayerTreeFrameSink();

  // Gets the current viz::SurfaceId.
  viz::SurfaceId GetSurfaceId();

  // Forces the window to allocate a new viz::LocalSurfaceId for the next
  // CompositorFrame submission in anticipation of a synchronization operation
  // that does not involve a resize or a device scale factor change.
  void AllocateLocalSurfaceId();

  viz::ScopedSurfaceIdAllocator GetSurfaceIdAllocator(
      base::OnceCallback<void()> allocation_task);

  const viz::LocalSurfaceId& GetLocalSurfaceId();

  // Marks the current viz::LocalSurfaceId as invalid. AllocateLocalSurfaceId
  // must be called before submitting new CompositorFrames.
  void InvalidateLocalSurfaceId(bool also_invalidate_allocation_group = false);

  // Sets the current viz::LocalSurfaceId, in cases where the embedded client
  // has allocated one. Also sets child sequence number component of the
  // viz::LocalSurfaceId allocator.
  void UpdateLocalSurfaceIdFromEmbeddedClient(
      const std::optional<viz::LocalSurfaceId>& local_surface_id);

  // Returns the FrameSinkId. In LOCAL mode, this returns a valid FrameSinkId
  // only if a LayerTreeFrameSink has been created. In MUS mode, this always
  // return a valid FrameSinkId.
  const viz::FrameSinkId& GetFrameSinkId() const;

  // Use SetEmbedFrameSinkId() when this window is embedding another client.
  // See comment for |frame_sink_id_| below for more details.
  void SetEmbedFrameSinkId(const viz::FrameSinkId& embed_frame_sink_id);

  // Starts occlusion state tracking.
  void TrackOcclusionState();

  // Notifies observers of the state of a resize loop.
  void NotifyResizeLoopStarted();
  void NotifyResizeLoopEnded();

  // ui::GestureConsumer:
  bool RequiresDoubleTapGestureEvents() const override;

  // Returns |state| as a string. This is generally only useful for debugging.
  static const std::u16string OcclusionStateToString(OcclusionState state);
  // Returns |type| as a string. This is generally only useful for debugging.
  static std::string_view WindowTypeToString(client::WindowType type);

  // Sets the regions of this window to consider opaque when computing the
  // occlusion of underneath windows. Opaque regions can only be set for a
  // transparent() window, and cannot extend outside of the window bounds.
  // Opaque regions are relative to the window, i.e. the top-left corner of the
  // window is considered to be the point (0, 0). If
  // |opaque_regions_for_occlusion| is empty, the window is considered fully
  // transparent. Opaque regions do not affect what parts of the window are
  // visible; they only affect occlusion tracking of underneath windows. An
  // example use case for this is when a window is made transparent because of
  // rounded corners, and therefore does not contribute to occlusion. But,
  // almost all of that window could be opaque and we would want it to
  // contribute to occlusion. Occlusion tracking can affect rendering, page
  // behaviour, and triggering for Picture-in-picture, for example. Clients
  // should set the opaque regions for occlusion if they have transparent
  // regions, but want to specify that certain areas of them are completely
  // opaque. Clients that use the window shape API should also specify their
  // shape region as a region for occlusion, if it is opaque. The opaque regions
  // for occlusion for a window do not affect occlusion for that window itself,
  // only what parts of other windows that window occludes.
  // TODO: Currently, we only support one Rect in
  //       |opaque_regions_for_occlusion|. Supporting multiple Rects will
  //       enable window shape based occlusion.
  void SetOpaqueRegionsForOcclusion(
      const std::vector<gfx::Rect>& opaque_regions_for_occlusion);

  const std::vector<gfx::Rect>& opaque_regions_for_occlusion() const {
    return opaque_regions_for_occlusion_;
  }

 protected:
  // Deletes (or removes if not owned by parent) all child windows. Intended for
  // use from the destructor.
  void RemoveOrDestroyChildren();

  // Overrides from ui::PropertyHandler
  void AfterPropertyChange(const void* key, int64_t old_value) override;

  // viz::HostFrameSinkClient:
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override;
  void OnFrameTokenChanged(uint32_t frame_token,
                           base::TimeTicks activation_time) override;

 private:
  friend class DefaultWindowOcclusionChangeBuilder;
  friend class HitTestDataProviderAura;
  friend class LayoutManager;
  friend class PropertyConverter;
  friend class ScopedWindowCaptureRequest;
  friend class ScopedWindowEventTargetingBlocker;
  friend class WindowTargeter;
  friend class test::WindowTestApi;
  friend class TestScreen;

  // Handles registering FrameSinkId hierarchy for SetEmbedFrameSinkId() and
  // CreateLayerTreeFrameSink().
  void SetEmbedFrameSinkIdImpl(const viz::FrameSinkId& frame_sink_id);

  // Returns true if the mouse pointer at relative-to-this-Window's-origin
  // |local_point| can trigger an event for this Window.
  // TODO(beng): A Window can supply a hit-test mask to cause some portions of
  // itself to not trigger events, causing the events to fall through to the
  // Window behind.
  bool HitTest(const gfx::Point& local_point);

  // Changes the bounds of the window without condition.
  void SetBoundsInternal(const gfx::Rect& new_bounds);

  // Updates the visible state of the layer and the Window, but does not make
  // visible-state specific changes. Called from Show()/Hide().
  void SetVisibleInternal(bool visible);

  // Updates the occlusion info of the window.
  void SetOcclusionInfo(OcclusionState occlusion_state,
                        const SkRegion& occluded_region);

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
  void OnLayerFillsBoundsOpaquelyChanged(
      ui::PropertyChangeReason reason) override;

  // Overridden from ui::EventTarget:
  bool CanAcceptEvent(const ui::Event& event) override;
  EventTarget* GetParentTarget() override;
  std::unique_ptr<ui::EventTargetIterator> GetChildIterator() const override;
  ui::EventTargeter* GetEventTargeter() override;
  void ConvertEventToTarget(const ui::EventTarget* target,
                            ui::LocatedEvent* event) const override;
  gfx::PointF GetScreenLocationF(const ui::LocatedEvent& event) const override;

  // Updates the layer name based on the window's name and id.
  void UpdateLayerName();

  void RegisterFrameSinkId();
  void UnregisterFrameSinkId();
  void UpdateLocalSurfaceId();
  const viz::LocalSurfaceId& GetCurrentLocalSurfaceId() const;
  bool IsEmbeddingExternalContent() const;

  // Called by the constructor of ScopedWindowCaptureRequest to add a request to
  // make this non-root window capturable by the FrameSinkVideoCapturer.
  void OnScopedWindowCaptureRequestAdded();

  // Called by the destructor of ScopedWindowCaptureRequest to remove a request
  // to make this non-root window capturable by the FrameSinkVideoCapturer.
  void OnScopedWindowCaptureRequestRemoved();

  // The following are intended for use by the metadata to access the internals
  // of instances of this class. At some point they should be moved to the
  // public section and code refactored to use them.

  // Break out the separate elements of the Window bounds.
  int GetHeight() const;
  int GetWidth() const;
  int GetX() const;
  int GetY() const;
  void SetHeight(int height);
  void SetWidth(int width);
  void SetX(int x);
  void SetY(int y);

  void SetLayoutManagerImpl(std::unique_ptr<LayoutManager> layout_manager);

  bool GetCapture() const;

  viz::SurfaceId GetSurfaceId() const;

  bool GetVisible() const;
  void SetVisible(bool visible);

  // Bounds of this window relative to the parent. This is cached as the bounds
  // of the Layer and Window are not necessarily the same. In particular bounds
  // of the Layer are relative to the first ancestor with a Layer, where as this
  // is relative to the parent Window.
  gfx::Rect bounds_;

  raw_ptr<WindowTreeHost> host_ = nullptr;

  client::WindowType type_;

  // True if this window is being destroyed.
  bool is_destroying_ = false;

  // True if the Window is owned by its parent - i.e. it will be deleted by its
  // parent during its parents destruction.
  bool owned_by_parent_ = true;

  raw_ptr<WindowDelegate, AcrossTasksDanglingUntriaged> delegate_;

  // The Window's parent.
  raw_ptr<Window> parent_ = nullptr;

  // Child windows. Topmost is last.
  Windows children_;

  // The visibility state of the window as set by Show()/Hide(). This may differ
  // from the visibility of the underlying layer, which may remain visible after
  // the window is hidden (e.g. to animate its disappearance).
  bool visible_ = false;

  // Occlusion state of the window.
  OcclusionState occlusion_state_ = OcclusionState::UNKNOWN;

  // Occluded region of the window in the root window coordiantes.
  SkRegion occluded_region_in_root_;

  int id_ = kInitialId;

  // Whether layer is initialized as non-opaque. Defaults to false.
  bool transparent_ = false;

  // Whether it's in a process of CleanupGestureState() or not.
  bool cleaning_up_gesture_state_ = false;

  std::unique_ptr<LayoutManager> layout_manager_;
  std::unique_ptr<WindowTargeter> targeter_;

  // The opaque regions for occlusion for this window. See comment on
  // |SetOpaqueRegionsForOcclusion| for documentation.
  std::vector<gfx::Rect> opaque_regions_for_occlusion_;

  // Makes the window pass all events through to any windows behind it.
  EventTargetingPolicy event_targeting_policy_;
  // Used to restore to the original event targeting policy after all event
  // targeting blockers on this window are removed.
  EventTargetingPolicy restore_event_targeting_policy_;
  int event_targeting_blocker_count_ = 0;

  base::ReentrantObserverList<WindowObserver, true> observers_;

  // Video capturing support ---------------------------------------------------

  // A non-root window must be marked with a viz::SubtreeCaptureId so that it
  // can be captured by a FrameSinkVideoCapturer. Multiple clients can request
  // to capture the same window at the same time. This is the number of those
  // requests, which once it goes to zero, we well clear the
  // viz::SubtreeCaptureId from the layer associated with this window.
  int number_of_capture_requests_ = 0;

  // The ID allocated for the layer tree rooted at this window's layer, so that
  // it can be uniquely identified by the FrameSinkVideoCapturer. This can only
  // be set for non-root windows. Root windows can be captured normally by the
  // capturer using their frame sink ID, since those root windows are already
  // associated with a root compositor render pass.
  viz::SubtreeCaptureId subtree_capture_id_;

  // Embedding support ---------------------------------------------------------

  // Used to detect changes in device scale factor that require generating a
  // new LocalSurfaceId.
  float last_device_scale_factor_ = 1.0f;

  // The FrameSinkId associated with this window. If this window is embedding
  // another client, then this should be set to the FrameSinkId of that client,
  // and |embeds_external_client_| is turned on. However, a window can still
  // have a valid FrameSinkId without embedding another client, to facilitate
  // hit-testing.
  viz::FrameSinkId frame_sink_id_;

  // Set to true if |frame_sink_id_| has been registered in the Compositor
  // associated this this.
  bool registered_frame_sink_id_ = false;

  // Used by tests to disable registering the FrameSinkId with the Compositor.
  bool disable_frame_sink_id_registration_ = false;

  // Set to true if SetEmbedFrameSinkId() has been called.
  bool embeds_external_client_ = false;

  // Used to allocate LocalSurfaceIds when this is embedding external content.
  std::unique_ptr<viz::ParentLocalSurfaceIdAllocator>
      parent_local_surface_id_allocator_;

#if DCHECK_IS_ON()
  // Set to true if CreateLayerTreeFrameSink() was called.
  bool created_layer_tree_frame_sink_ = false;
#endif

  // Used when this is embedding external content.
  base::WeakPtr<cc::LayerTreeFrameSink> frame_sink_;
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_H_
