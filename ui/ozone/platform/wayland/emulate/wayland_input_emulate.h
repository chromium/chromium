// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_EMULATE_WAYLAND_INPUT_EMULATE_H_
#define UI_OZONE_PLATFORM_WAYLAND_EMULATE_WAYLAND_INPUT_EMULATE_H_

#include <wayland-util.h>

#include <memory>
#include <string>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/host/proxy/wayland_proxy.h"

struct wl_buffer;
struct wl_registry;
struct zcr_ui_controls_v1;
struct wl_callback;

namespace wl {

// Uses the ui_controls protocol extension to emulate Keyboard, Pointer, and
// Touch events that the interactive_ui_tests test suite sends. Mustn't be
// linked in production code.
class WaylandInputEmulate : public wl::WaylandProxy::Delegate {
 public:
  explicit WaylandInputEmulate(base::RepeatingCallback<void(uint32_t)>);
  ~WaylandInputEmulate() override;

  // |key_state| is a bit-mask of ui_controls::KeyEventType.
  // |accelerator_state| is a bit-mask of ui_controls::AcceleratorState.
  void EmulateKeyboardKey(ui::DomCode dom_code,
                          int key_state,
                          int accelerator_state,
                          uint32_t request_id);

  // Both |mouse_surface_location| and |mouse_screen_location| are in DIP.
  void EmulatePointerMotion(gfx::AcceleratedWidget widget,
                            const gfx::Point& mouse_surface_location,
                            const gfx::Point& mouse_screen_location,
                            uint32_t request_id);

  // |button_state| is a bit-mask of ui_controls::MouseButtonState.
  // |accelerator_state| is a bit-mask of ui_controls::AcceleratorState.
  void EmulatePointerButton(ui_controls::MouseButton button,
                            int button_state,
                            int accelerator_state,
                            uint32_t request_id);

  // |touch_screen_location| is in DIP.
  void EmulateTouch(int action,
                    const gfx::Point& touch_screen_location,
                    int touch_id,
                    uint32_t request_id);

#if BUILDFLAG(IS_CHROMEOS)
  // |display_specs| is the spec for the display(s).
  void EmulateUpdateDisplay(const std::string& display_specs,
                            uint32_t request_id);
#endif

#if BUILDFLAG(IS_LINUX)
  void ForceUseScreenCoordinatesOnce();
#endif

 private:
  enum PendingRequestType {
    KeyPress,
    MouseMove,
    MouseButton,
    Touch,
  };

  // Pending emulation request.
  struct PendingRequest {
    PendingRequest(PendingRequestType request_type, uint32_t request_id);
    ~PendingRequest();

    PendingRequestType type;
    uint32_t request_id;

    // Set for type == KeyPress || type == MouseButton. A bit-mask of
    // ui_controls::AcceleratorState.
    int accelerator_state;

    // Set for type == KeyPress. |key_state| is a bit-mask of
    // ui_controls::KeyEventType.
    ui::DomCode key_dom_code = ui::DomCode::NONE;
    int key_state;

    // Set for type == MouseMove. We hold on to |widget| because we only decide
    // whether to use screen or surface-local coordinates after the window has
    // been configured.
    gfx::AcceleratedWidget widget;
    gfx::Point mouse_surface_location;
    gfx::Point mouse_screen_location;

    // Set for type == MouseButton. |button_state| is a bit-mask of
    // ui_controls::MouseButtonState.
    ui_controls::MouseButton button;
    int button_state;

    // Set for type == Touch. |action| is a bit-mask of ui_controls::TouchType.
    // |touch_screen_location| is in DIP screen coordinates.
    int action = 0;
    gfx::Point touch_screen_location;
    int touch_id = 0;
  };

  // A container that tracks created WaylandWindows and keeps some fundamental
  // bits to make emulation work flawlessly.
  struct TestWindow {
    TestWindow();
    ~TestWindow();

    // Control flag that says if the buffer has been attached and a consequent
    // frame callback has been received. This is required to be able to know
    // that the surface has consumed the attached buffer and Wayland properly
    // set the size of the surface. Otherwise, the surface in question may not
    // receive any events. Set during WaylandInputEmulate::FrameCallbackHandler
    // call.
    bool buffer_attached_and_configured = false;

    // Frame callback that invokes WaylandInputEmulate::FrameCallbackHandler.
    raw_ptr<wl_callback, DanglingUntriaged> frame_callback = nullptr;

    // The attached buffer.
    raw_ptr<wl_buffer, DanglingUntriaged> buffer = nullptr;

    // True if the window was created or assigned a role and is now waiting for
    // a buffer to be committed.
    bool waiting_for_buffer_commit = false;

    base::WeakPtrFactory<TestWindow> weak_factory{this};
  };

  // WaylandProxy::Delegate:
  void OnWindowAdded(gfx::AcceleratedWidget widget) override;
  void OnWindowRemoved(gfx::AcceleratedWidget widget) override;
  void OnWindowConfigured(gfx::AcceleratedWidget widget,
                          bool is_configured) override;
  void OnWindowRoleAssigned(gfx::AcceleratedWidget widget) override;

  // zcr_ui_controls_v1_listener callbacks:
  static void OnRequestProcessed(void* data,
                                 zcr_ui_controls_v1* ui_controls,
                                 uint32_t id);

  // wl_registry_listener callbacks:
  static void OnGlobal(void* data,
                       wl_registry* registry,
                       uint32_t name,
                       const char* interface,
                       uint32_t version);

  // wl_registry_listener callbacks:
  static void OnGlobalRemove(void* data, wl_registry* registry, uint32_t name);

  // wl_callback_listener callbacks:
  static void OnFrameDone(void* data, wl_callback* callback, uint32_t time);

  // Returns true if there is at least one window that has been created but that
  // does not yet have a buffer committed.
  bool AnyWindowWaitingForBufferCommit();

  // Dispatches all pending requests.
  void DispatchPendingRequests();

  // Window creation is asynchronous in wayland. First we create the window,
  // then we must attach and commit a buffer before the server will treat it
  // properly w.r.t. input events. This member stores all windows that have been
  // created.
  base::flat_map<gfx::AcceleratedWidget,
                 std::unique_ptr<WaylandInputEmulate::TestWindow>>
      windows_;

  // Stores pending requests in a global queue. We will not dispatch any pending
  // requests while there are windows that are still in the process of being
  // created.
  base::circular_deque<std::unique_ptr<PendingRequest>> pending_requests_;

  base::RepeatingCallback<void(uint32_t)> request_processed_callback_;

  // If true, the next `EmulatePointerMotion` call will use global screen
  // coordinates, i.e. send zcr_ui_controls_v1.mouse_move with the `surface`
  // parameter set to NULL.
  // Note: this does not affect whether `EmulatePointerMotion` uses the
  // coordinates from its `mouse_surface_location` or `mouse_screen_location`
  // parameter. See the comment in that method's definition for more details.
  bool force_use_screen_coordinates_once_ = false;

  // Owned raw pointers. wl::Object is not used because the component this
  // class belongs to cannot depend on the "wayland" target in the
  // //ui/ozone/platform/wayland/BUILD.gn
  raw_ptr<wl_registry> registry_ = nullptr;
  raw_ptr<zcr_ui_controls_v1> ui_controls_ = nullptr;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_EMULATE_WAYLAND_INPUT_EMULATE_H_
