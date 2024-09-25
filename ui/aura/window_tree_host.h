// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_TREE_HOST_H_
#define UI_AURA_WINDOW_TREE_HOST_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/scoped_enable_unadjusted_mouse_events.h"
#include "ui/aura/window.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/ime/ime_key_event_dispatcher.h"
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
}  // namespace gfx

namespace ui {
class Compositor;
enum class DomCode : uint32_t;
class EventSink;
class InputMethod;
class ViewProp;
struct PlatformWindowInitProperties;
}  // namespace ui

namespace viz {
class FrameSinkId;
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
class AURA_EXPORT WindowTreeHost : public ui::ImeKeyEventDispatcher,
                                   public ui::EventSource,
                                   public display::DisplayObserver,
                                   public ui::CompositorObserver {
 public:
  static const char kWindowTreeHostUsesParent[];

  // VideoCaptureLock ensures state necessary for capturing video remains in
  // effect. For example, this may force keeping the compositor visible when
  // it normally would not be.
  class AURA_EXPORT VideoCaptureLock {
   public:
    VideoCaptureLock(const VideoCaptureLock&) = delete;
    VideoCaptureLock& operator=(const VideoCaptureLock&) = delete;
    ~VideoCaptureLock();

   private:
    friend class WindowTreeHost;

    explicit VideoCaptureLock(WindowTreeHost* host);

    base::WeakPtr<WindowTreeHost> host_;
  };

  WindowTreeHost(const WindowTreeHost&) = delete;
  WindowTreeHost& operator=(const WindowTreeHost&) = delete;

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
  void ConvertDIPToPixels(gfx::Point* point) const;
  virtual void ConvertDIPToPixels(gfx::PointF* point) const;

  // Converts |point| from the host window's coordinate system to the
  // root window's.
  void ConvertPixelsToDIP(gfx::Point* point) const;
  virtual void ConvertPixelsToDIP(gfx::PointF* point) const;

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
  // 1) ChromeOS virtual keyboard needs to receive
  // SetVirtualKeyboardVisibilityIfEnabled() notification from InputMethod.
  // Multiple InputMethod instances makes it hard to register/unregister the
  // observer for that notification. 2) For Ozone, there is no native focus
  // state for the root window and WindowTreeHost. See
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

  // Overridden from ui::ImeKeyEventDispatcher:
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

  // Gets the bounds in DIP.
  virtual gfx::Rect GetBoundsInDIP() const;

  // Returns the bounds relative to the accelerated widget. In the typical case,
  // the origin is 0,0 and the size is the same as the pixel-bounds. On some
  // OSs the bounds may be inset (on Windows, this is referred to as the client
  // area). When the bounds are inset, this returns a non-zero origin with a
  // size smaller than GetBoundsInPixels().
  virtual gfx::Rect GetBoundsInAcceleratedWidgetPixelCoordinates();

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
      std::optional<base::flat_set<ui::DomCode>> codes);

  // Returns a map of KeyboardEvent code to KeyboardEvent key values.
  virtual base::flat_map<std::string, std::string> GetKeyboardLayoutMap() = 0;

  // Returns true if KeyEvents should be send to IME. This is called from
  // WindowEventDispatcher during event dispatch.
  virtual bool ShouldSendKeyEventToIme();

  // Determines if native window occlusion should be enabled or not.
  bool IsNativeWindowOcclusionEnabled() const;

  // Remembers the current occlusion state, and if it has changed, notifies
  // observers of the change. `raw_occluded_region` is only applicable when
  // visible and gives the occluded region. If `raw_occluded_region` is empty,
  // the entire AcceleratedWidget is visible.
  virtual void SetNativeWindowOcclusionState(
      Window::OcclusionState raw_occlusion_state,
      const SkRegion& raw_occluded_region);

  Window::OcclusionState GetNativeWindowOcclusionState() {
    return occlusion_state_;
  }

  const SkRegion& GetNativeOccludedRegion() const { return occluded_region_; }

  // Requests using unadjusted movement mouse events, i.e. WM_INPUT on Windows.
  // Returns a ScopedEnableUnadjustedMouseEvents instance which stops using
  // unadjusted mouse events when destroyed, returns nullptr if unadjusted mouse
  // event is not not implemented or failed. On some platforms this function may
  // temporarily affect the global state of mouse settings.  This function is
  // currently only intended to be used with PointerLock as it is not set up for
  // multiple calls.
  virtual std::unique_ptr<ScopedEnableUnadjustedMouseEvents>
  RequestUnadjustedMovement();

  // Whether or not the underlying platform supports native pointer locking.
  virtual bool SupportsMouseLock();
  virtual void LockMouse(Window* window);
  virtual void UnlockMouse(Window* window);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Provides the Unique ID that represents the AcceleratedWidget backing this
  // host. This is provided by the platform and not generated by Chrome.
  virtual std::string GetUniqueId() const = 0;
#endif

  // See VideoCaptureLock for details. This may return null.
  std::unique_ptr<VideoCaptureLock> CreateVideoCaptureLock();

#if BUILDFLAG(IS_WIN)
  // Returns whether a host's window is on the current workspace or not,
  // std::nullopt if the state is not known.
  std::optional<bool> on_current_workspace() const {
    return on_current_workspace_;
  }

  // Determining if a host's window is on the current workspace can be very
  // expensive COM call on Windows, so this caches that information.
  void set_on_current_workspace(std::optional<bool> on_current_workspace) {
    on_current_workspace_ = on_current_workspace;
  }
#endif  // BUILDFLAG_(IS_WIN)

 protected:
  friend class ScopedKeyboardHook;
  friend class TestScreen;  // TODO(beng): see if we can remove/consolidate.

  explicit WindowTreeHost(std::unique_ptr<Window> window = nullptr);

  // All calls to changing the visibility of the Compositor funnel into this.
  // In addition to changing the visibility this may also evict the root frame.
  void UpdateCompositorVisibility(bool visible);

  void DestroyCompositor();
  void DestroyDispatcher();

  // Sets whether the accelerated widget has been made visible. This is called
  // when platform specific api has been called to make the widget visible. The
  // widget is not necessarily shown/drawn (it may be occluded or minimized),
  // but from the OSs perspective, the window may be shown to the user.
  //
  // This is called from Show(), subclasses that do not call Show() must call
  // this.
  void OnAcceleratedWidgetMadeVisible(bool value);

  void CreateCompositor(bool force_software_compositor = false,
                        bool use_external_begin_frame_control = false,
                        bool enable_compositing_based_throttling = false,
                        size_t memory_limit_when_visible_mb = 0);

  void InitCompositor();
  void OnAcceleratedWidgetAvailable();

  // Returns the location of the RootWindow on native screen.
  virtual gfx::Point GetLocationOnScreenInPixels() const = 0;

  void OnHostMovedInPixels();
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
      std::optional<base::flat_set<ui::DomCode>> dom_codes) = 0;

  // Stops capturing system keyboard events.
  virtual void ReleaseSystemKeyEventCapture() = 0;

  // True if |dom_code| is reserved for an active KeyboardLock request.
  virtual bool IsKeyLocked(ui::DomCode dom_code) = 0;

  // Return root window size computed from given pixel size.
  virtual gfx::Rect GetTransformedRootWindowBoundsFromPixelSize(
      const gfx::Size& size_in_pixels) const;

  base::ObserverList<WindowTreeHostObserver>::Unchecked& observers() {
    return observers_;
  }

  // Called to enabled/disable native window occlusion calculation.
  void SetNativeWindowOcclusionEnabled(bool enable);

  // Updates the root window's size after WindowTreeHost's property changed.
  void UpdateRootWindowSize();

  // Calculates the root window bounds to be used by UpdateRootwindowSize().
  virtual gfx::Rect CalculateRootWindowBounds() const;

  virtual void OnVideoCaptureLockCreated();
  virtual void OnVideoCaptureLockDestroyed();

 private:
  friend class test::WindowTreeHostTestApi;

  void DecrementVideoCaptureCountForOcclusionTracking();
  void MaybeUpdateComposibleVisibilityForVideoLockCountChange();
  void MaybeUpdateCompositorVisibilityForNativeOcclusion();
  bool CalculateCompositorVisibilityFromOcclusionState() const;

  // See `kApplyNativeOcclusionToCompositorTypeThrottle` for details.
  bool NativeOcclusionAffectsThrottle() const;

  // True if native occlusion only affects throttle, not compositor visibility.
  bool NativeOcclusionAffectsVisibility() const;

  // True if we should throttle, assuming the native occlusion settings allow
  // it.
  bool ShouldThrottle() const;

  static const base::flat_set<raw_ptr<WindowTreeHost, CtnExperimental>>&
  GetThrottledHostsForTesting();

  // Moves the cursor to the specified location. This method is internally used
  // by MoveCursorToLocationInDIP() and MoveCursorToLocationInPixels().
  void MoveCursorToInternal(const gfx::Point& root_location,
                            const gfx::Point& host_location);

  // Overridden from CompositorObserver:
  void OnCompositingAckDeprecated(ui::Compositor* compositor) final;
  void OnCompositingChildResizing(ui::Compositor* compositor) final;
  void OnFrameSinksToThrottleUpdated(
      const base::flat_set<viz::FrameSinkId>& ids) final;
  void OnSetPreferredRefreshRate(ui::Compositor*,
                                 float preferred_refresh_rate) override;

  // We don't use a std::unique_ptr for |window_| since we need this ptr to be
  // valid during its deletion. (Window's dtor notifies observers that may
  // attempt to reach back up to access this object which will be valid until
  // the end of the dtor).
  raw_ptr<Window, AcrossTasksDanglingUntriaged> window_;  // Owning.

  // Keeps track of the occlusion state of the host, and used to send
  // notifications to observers when it changes.
  Window::OcclusionState occlusion_state_ = Window::OcclusionState::UNKNOWN;
  SkRegion occluded_region_;

  // If there are video capture locks, we need to force the occlusion state
  // to visible. But, when the video capture locks are done, we need to restore
  // the occlusion state to what the last occlusion state from the platform was.
  // We keep the latest occlusion state from the platform in
  // `raw_occlusion_state_` and `raw_occluded_region_`.
  Window::OcclusionState raw_occlusion_state_ = Window::OcclusionState::UNKNOWN;
  SkRegion raw_occluded_region_;

  // This is set if we know whether the window is on the current workspace.
  // This is useful on Windows, where a COM call is required to determine this,
  // which can block the UI. The native window occlusion tracking code already
  // figures this out, so it's cheaper to store the fact here.
  std::optional<bool> on_current_workspace_;

  base::ObserverList<WindowTreeHostObserver>::Unchecked observers_;

  display::ScopedDisplayObserver display_observer_{this};

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
  gfx::NativeCursor last_cursor_ = ui::mojom::CursorType::kNull;
  gfx::Point last_cursor_request_position_in_host_;

  std::unique_ptr<ui::ViewProp> prop_;

  std::unique_ptr<ui::InputMethod> input_method_owned_;
  // The InputMethod instance used to process key events.
  // If owned it, it is created in GetInputMethod() method;
  // If not owned it, it is passed in through SetSharedInputMethod() method.
  raw_ptr<ui::InputMethod> input_method_ = nullptr;

  // Set to true if this WindowTreeHost is currently holding pointer moves.
  bool holding_pointer_moves_ = false;

  // Set to true if native window occlusion should be calculated.
  bool native_window_occlusion_enabled_ = false;

  bool accelerated_widget_made_visible_ = false;

  // Number of VideoCaptureLocks that have been created and not destroyed.
  // This is only used when occlusion tracking is always enabled.
  int video_capture_count_for_occlusion_tracking_ = 0;

  base::WeakPtrFactory<WindowTreeHost> weak_factory_{this};
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_TREE_HOST_H_
