// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_TREE_HOST_H_
#define UI_AURA_WINDOW_TREE_HOST_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/scoped_enable_unadjusted_mouse_events.h"
#include "ui/aura/window.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/display/display_observer.h"
#include "ui/events/event_source.h"
#include "ui/events/platform_event.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_transform.h"

namespace gfx {
class Point;
class Rect;
class Size;
class Transform;
}

namespace ui {
class Compositor;
enum class DomCode;
class EventSink;
class InputMethod;
class ViewProp;
struct PlatformWindowInitProperties;
}

namespace aura {

namespace test {
class WindowTreeHostTestApi;
}

class ScopedKeyboardHook;
class WindowEventDispatcher;
class WindowTreeHostObserver;

// WindowTreeHost bridges between a native window and the embedded RootWindow.
// It provides the accelerated widget and maps events from the native os to
// aura.
class AURA_EXPORT WindowTreeHost : public ui::internal::InputMethodDelegate,
                                   public ui::EventSource,
                                   public display::DisplayObserver,
                                   public ui::CompositorObserver {
 public:
  ~WindowTreeHost() override;

  // Creates a new WindowTreeHost with the specified |properties|.
  static std::unique_ptr<WindowTreeHost> Create(
      ui::PlatformWindowInitProperties properties);

  // Returns the WindowTreeHost for the specified accelerated widget, or NULL
  // if there is none associated.
  static WindowTreeHost* GetForAcceleratedWidget(gfx::AcceleratedWidget widget);

  void InitHost();

  void AddObserver(WindowTreeHostObserver* observer);
  void RemoveObserver(WindowTreeHostObserver* observer);
  bool HasObserver(const WindowTreeHostObserver* observer) const;

  Window* window() { return window_; }
  const Window* window() const { return window_; }

  // TODO(msw): Remove this, callers should use GetEventSink().
  ui::EventSink* event_sink();

  WindowEventDispatcher* dispatcher() {
    return const_cast<WindowEventDispatcher*>(
        const_cast<const WindowTreeHost*>(this)->dispatcher());
  }
  const WindowEventDispatcher* dispatcher() const { return dispatcher_.get(); }

  ui::Compositor* compositor() { return compositor_.get(); }

  base::WeakPtr<WindowTreeHost> GetWeakPtr();

  // Gets/Sets the root window's transform.
  virtual gfx::Transform GetRootTransform() const;
  virtual void SetRootTransform(const gfx::Transform& transform);
  virtual gfx::Transform GetInverseRootTransform() const;

  void SetDisplayTransformHint(gfx::OverlayTransform transform);

  // These functions are used in event translation for translating the local
  // coordinates of LocatedEvents. Default implementation calls to non-local
  // ones (e.g. GetRootTransform()).
  virtual gfx::Transform GetRootTransformForLocalEventCoordinates() const;
  virtual gfx::Transform GetInverseRootTransformForLocalEventCoordinates()
      const;

  // Updates the root window's size using |host_size_in_pixels|, current
  // transform and outsets.
  // TODO(ccameron): Make this function no longer public. The interaction
  // between this call, GetBounds, and OnHostResizedInPixels is ambiguous and
  // allows for inconsistencies.
  void UpdateRootWindowSizeInPixels();

  // Updates the compositor's size and scale from |new_size_in_pixels|,
  // |device_scale_factor_| and the compositor's transform hint.
  void UpdateCompositorScaleAndSize(const gfx::Size& new_size_in_pixels);

  // Converts |point| from the root window's coordinate system to native
  // screen's.
  void ConvertDIPToScreenInPixels(gfx::Point* point) const;

  // Converts |point| from native screen coordinate system to the root window's.
  void ConvertScreenInPixelsToDIP(gfx::Point* point) const;

  // Converts |point| from the root window's coordinate system to the
  // host window's.
  virtual void ConvertDIPToPixels(gfx::Point* point) const;

  // Converts |point| from the host window's coordinate system to the
  // root window's.
  virtual void ConvertPixelsToDIP(gfx::Point* point) const;

  // Cursor.
  // Sets the currently-displayed cursor. If the cursor was previously hidden
  // via ShowCursor(false), it will remain hidden until ShowCursor(true) is
  // called, at which point the cursor that was last set via SetCursor() will be
  // used.
  void SetCursor(gfx::NativeCursor cursor);

  // Invoked when the cursor's visibility has changed.
  void OnCursorVisibilityChanged(bool visible);

  // Moves the cursor to the specified location relative to the root window.
  void MoveCursorToLocationInDIP(const gfx::Point& location_in_dip);

  // Moves the cursor to the |location_in_pixels| given in host coordinates.
  void MoveCursorToLocationInPixels(const gfx::Point& location_in_pixels);

  gfx::NativeCursor last_cursor() const { return last_cursor_; }

  // Gets the InputMethod instance, if NULL, creates & owns it.
  ui::InputMethod* GetInputMethod();
  bool has_input_method() const { return input_method_ != nullptr; }

  // Sets a shared unowned InputMethod. This is used when there is a singleton
  // InputMethod shared between multiple WindowTreeHost instances.
  //
  // This is used for Ash only. There are 2 reasons:
  // 1) ChromeOS virtual keyboard needs to receive ShowVirtualKeyboardIfEnabled
  // notification from InputMethod. Multiple InputMethod instances makes it hard
  // to register/unregister the observer for that notification. 2) For Ozone,
  // there is no native focus state for the root window and WindowTreeHost. See
  // DrmWindowHost::CanDispatchEvent, the key events always goes to the primary
  // WindowTreeHost. And after InputMethod processed the key event and continue
  // dispatching it, WindowTargeter::FindTargetForEvent may re-dispatch it to a
  // different WindowTreeHost. So the singleton InputMethod can make sure the
  // correct InputMethod instance processes the key event no matter which
  // WindowTreeHost is the target for event. Please refer to the test:
  // ExtendedDesktopTest.KeyEventsOnLockScreen.
  //
  // TODO(shuchen): remove this method after above reasons become invalid.
  // A possible solution is to make sure DrmWindowHost can find the correct
  // WindowTreeHost to dispatch events.
  void SetSharedInputMethod(ui::InputMethod* input_method);

  // Overridden from ui::internal::InputMethodDelegate:
  ui::EventDispatchDetails DispatchKeyEventPostIME(ui::KeyEvent* event) final;

  // Overridden from ui::EventSource:
  ui::EventSink* GetEventSink() override;

  // Returns the id of the display. Default implementation queries Screen.
  virtual int64_t GetDisplayId();

  // Returns the EventSource responsible for dispatching events to the window
  // tree.
  virtual ui::EventSource* GetEventSource() = 0;

  // Returns the accelerated widget.
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() = 0;

  // Shows the WindowTreeHost.
  void Show();

  // Hides the WindowTreeHost.
  void Hide();

  // Sets/Gets the bounds of the WindowTreeHost (in pixels). Note that a call to
  // GetBoundsInPixels() immediately following a SetBoundsInPixels() can return
  // the old bounds, because SetBoundsInPixels() can take effect asynchronously,
  // depending on the platform. The |local_surface_id| takes effect when (and
  // if) the new size is confirmed (potentially asynchronously) by the platform.
  virtual void SetBoundsInPixels(const gfx::Rect& bounds_in_pixels) = 0;
  virtual gfx::Rect GetBoundsInPixels() const = 0;

  // Sets the OS capture to the root window.
  virtual void SetCapture() = 0;

  // Releases OS capture of the root window.
  virtual void ReleaseCapture() = 0;

  // Returns the device scale assumed by the WindowTreeHost (set during the
  // most recent call to OnHostResizedInPixels).
  float device_scale_factor() const { return device_scale_factor_; }

  // Requests that |keys| be intercepted at the platform level and routed
  // directly to the web content.  If |codes| is empty, all keys will be
  // intercepted.  Returns a ScopedKeyboardHook instance which stops capturing
  // system key events when destroyed.
  std::unique_ptr<ScopedKeyboardHook> CaptureSystemKeyEvents(
      base::Optional<base::flat_set<ui::DomCode>> codes);

  // Returns a map of KeyboardEvent code to KeyboardEvent key values.
  virtual base::flat_map<std::string, std::string> GetKeyboardLayoutMap() = 0;

  // Returns true if KeyEvents should be send to IME. This is called from
  // WindowEventDispatcher during event dispatch.
  virtual bool ShouldSendKeyEventToIme();

  // Enables native window occlusion tracking for the native window this host
  // represents.
  virtual void EnableNativeWindowOcclusionTracking();

  // Disables native window occlusion tracking for the native window this host
  // represents.
  virtual void DisableNativeWindowOcclusionTracking();

  // Remembers the current occlusion state, and if it has changed, notifies
  // observers of the change.
  virtual void SetNativeWindowOcclusionState(Window::OcclusionState state);

  Window::OcclusionState GetNativeWindowOcclusionState() {
    return occlusion_state_;
  }

  // Requests using unadjusted movement mouse events, i.e. WM_INPUT on Windows.
  // Returns a ScopedEnableUnadjustedMouseEvents instance which stops using
  // unadjusted mouse events when destroyed, returns nullptr if unadjusted mouse
  // event is not not implemented or failed.
  virtual std::unique_ptr<ScopedEnableUnadjustedMouseEvents>
  RequestUnadjustedMovement();

  bool holding_pointer_moves() const { return holding_pointer_moves_; }

 protected:
  friend class ScopedKeyboardHook;
  friend class TestScreen;  // TODO(beng): see if we can remove/consolidate.

  explicit WindowTreeHost(std::unique_ptr<Window> window = nullptr);

  // Set the cached display device scale factor. This should only be called
  // during subclass initialization, when the value is needed before InitHost().
  void IntializeDeviceScaleFactor(float device_scale_factor);

  void DestroyCompositor();
  void DestroyDispatcher();

  // If frame_sink_id is not passed in, one will be grabbed from
  // ContextFactoryPrivate. See Compositor() for details on
  // |trace_environment_name|.
  void CreateCompositor(
      const viz::FrameSinkId& frame_sink_id = viz::FrameSinkId(),
      bool force_software_compositor = false,
      bool use_external_begin_frame_control = false,
      const char* trace_environment_name = nullptr);

  void InitCompositor();
  void OnAcceleratedWidgetAvailable();

  // Returns the location of the RootWindow on native screen.
  virtual gfx::Point GetLocationOnScreenInPixels() const = 0;

  void OnHostMovedInPixels(const gfx::Point& new_location_in_pixels);
  void OnHostResizedInPixels(const gfx::Size& new_size_in_pixels);
  void OnHostWorkspaceChanged();
  void OnHostDisplayChanged();
  void OnHostCloseRequested();
  void OnHostLostWindowCapture();

  // Sets the currently displayed cursor.
  virtual void SetCursorNative(gfx::NativeCursor cursor) = 0;

  // Moves the cursor to the specified location relative to the root window.
  virtual void MoveCursorToScreenLocationInPixels(
      const gfx::Point& location_in_pixels) = 0;

  // Called when the cursor visibility has changed.
  virtual void OnCursorVisibilityChangedNative(bool show) = 0;

  // Shows the WindowTreeHost.
  virtual void ShowImpl() = 0;

  // Hides the WindowTreeHost.
  virtual void HideImpl() = 0;

  // display::DisplayObserver implementation.
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // Begins capturing system key events.  Returns true if successful.
  virtual bool CaptureSystemKeyEventsImpl(
      base::Optional<base::flat_set<ui::DomCode>> dom_codes) = 0;

  // Stops capturing system keyboard events.
  virtual void ReleaseSystemKeyEventCapture() = 0;

  // True if |dom_code| is reserved for an active KeyboardLock request.
  virtual bool IsKeyLocked(ui::DomCode dom_code) = 0;

  virtual gfx::Rect GetTransformedRootWindowBoundsInPixels(
      const gfx::Size& size_in_pixels) const;

  const base::ObserverList<WindowTreeHostObserver>::Unchecked& observers()
      const {
    return observers_;
  }

 private:
  friend class test::WindowTreeHostTestApi;

  // Moves the cursor to the specified location. This method is internally used
  // by MoveCursorToLocationInDIP() and MoveCursorToLocationInPixels().
  void MoveCursorToInternal(const gfx::Point& root_location,
                            const gfx::Point& host_location);

  // Overrided from CompositorObserver:
  void OnCompositingEnded(ui::Compositor* compositor) override;
  void OnCompositingChildResizing(ui::Compositor* compositor) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  // We don't use a std::unique_ptr for |window_| since we need this ptr to be
  // valid during its deletion. (Window's dtor notifies observers that may
  // attempt to reach back up to access this object which will be valid until
  // the end of the dtor).
  Window* window_;  // Owning.

  // Keeps track of the occlusion state of the host, and used to send
  // notifications to observers when it changes.
  Window::OcclusionState occlusion_state_;

  base::ObserverList<WindowTreeHostObserver>::Unchecked observers_;

  std::unique_ptr<WindowEventDispatcher> dispatcher_;

  std::unique_ptr<ui::Compositor> compositor_;

  // The device scale factor is snapshotted in OnHostResizedInPixels.
  // NOTE: this value is cached rather than looked up from the Display as it is
  // entirely possible for the Display to be updated *after* |this|. For
  // example, display changes on Windows first result in the HWND bounds
  // changing and are then followed by changes to the set of displays
  //
  // TODO(ccameron): The size and location from OnHostResizedInPixels and
  // OnHostMovedInPixels should be snapshotted here as well.
  float device_scale_factor_ = 1.f;

  // Last cursor set.  Used for testing.
  gfx::NativeCursor last_cursor_;
  gfx::Point last_cursor_request_position_in_host_;

  std::unique_ptr<ui::ViewProp> prop_;

  // The InputMethod instance used to process key events.
  // If owned it, it is created in GetInputMethod() method;
  // If not owned it, it is passed in through SetSharedInputMethod() method.
  ui::InputMethod* input_method_;

  // Whether the InputMethod instance is owned by this WindowTreeHost.
  bool owned_input_method_;

  // Set to the time the synchronization event began.
  base::TimeTicks synchronization_start_time_;

  // Set to true if this WindowTreeHost is currently holding pointer moves.
  bool holding_pointer_moves_ = false;

  base::WeakPtrFactory<WindowTreeHost> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHost);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_TREE_HOST_H_
