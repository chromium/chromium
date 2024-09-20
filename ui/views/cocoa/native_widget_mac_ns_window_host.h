// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_NATIVE_WIDGET_MAC_NS_WINDOW_HOST_H_
#define UI_VIEWS_COCOA_NATIVE_WIDGET_MAC_NS_WINDOW_HOST_H_

#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/remote_cocoa/app_shim/native_widget_ns_window_host_helper.h"
#include "components/remote_cocoa/app_shim/ns_view_ids.h"
#include "components/remote_cocoa/browser/application_host.h"
#include "components/remote_cocoa/browser/scoped_cg_window_id.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom.h"
#include "components/remote_cocoa/common/native_widget_ns_window_host.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"
#include "ui/base/cocoa/accessibility_focus_overrider.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/compositor/layer_owner.h"
#include "ui/views/cocoa/drag_drop_client_mac.h"
#include "ui/views/cocoa/native_widget_mac_event_monitor.h"
#include "ui/views/view_observer.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_observer.h"

@class NativeWidgetMacNSWindow;
@class NSAccessibilityRemoteUIElement;
@class NSView;

namespace remote_cocoa {
class NativeWidgetNSWindowBridge;
class ScopedNativeWindowMapping;
}  // namespace remote_cocoa

namespace ui {
class RecyclableCompositorMac;
}  // namespace ui

namespace views {

class ImmersiveModeRevealClient;
class NativeWidgetMac;
class NativeWidgetMacEventMonitor;
class TextInputHost;

// The portion of NativeWidgetMac that lives in the browser process. This
// communicates to the NativeWidgetNSWindowBridge, which interacts with the
// Cocoa APIs, and which may live in an app shim process.
class VIEWS_EXPORT NativeWidgetMacNSWindowHost
    : public remote_cocoa::NativeWidgetNSWindowHostHelper,
      public remote_cocoa::ApplicationHost::Observer,
      public remote_cocoa::mojom::NativeWidgetNSWindowHost,
      public DialogObserver,
      public ui::AccessibilityFocusOverrider::Client,
      public ui::LayerDelegate,
      public ui::LayerOwner,
      public ui::AcceleratedWidgetMacNSView,
      public ViewObserver {
 public:
  // Retrieves the bridge host associated with the given NativeWindow. Returns
  // null if the supplied handle has no associated Widget.
  static NativeWidgetMacNSWindowHost* GetFromNativeWindow(
      gfx::NativeWindow window);
  static NativeWidgetMacNSWindowHost* GetFromNativeView(gfx::NativeView view);

  // Key used to bind the content NSView to the widget when it becomes
  // a child widget. NOTE: This is unowned because it is owned by another
  // widget; use a __bridge cast to convert to and from NSView*.
  static const char kMovedContentNSView[];

  // Unique integer id handles are used to bridge between the
  // NativeWidgetMacNSWindowHost in one process and the NativeWidgetNSWindowHost
  // potentially in another.
  static NativeWidgetMacNSWindowHost* GetFromId(
      uint64_t bridged_native_widget_id);
  uint64_t bridged_native_widget_id() const { return widget_id_; }

  // Creates one side of the bridge. |owner| must not be NULL.
  explicit NativeWidgetMacNSWindowHost(NativeWidgetMac* owner);

  NativeWidgetMacNSWindowHost(const NativeWidgetMacNSWindowHost&) = delete;
  NativeWidgetMacNSWindowHost& operator=(const NativeWidgetMacNSWindowHost&) =
      delete;

  ~NativeWidgetMacNSWindowHost() override;

  // The NativeWidgetMac that owns |this|.
  views::NativeWidgetMac* native_widget_mac() const {
    return native_widget_mac_;
  }
  NativeWidgetMacNSWindowHost* parent() const { return parent_; }
  std::vector<NativeWidgetMacNSWindowHost*> children() const {
    return children_;
  }

  // The Widget associated with the NativeWidgetMac.
  // Returns nullptr if it doesn't exist.
  Widget* GetWidget();

  // The bridge factory that was used to create the true NSWindow for this
  // widget. This is nullptr for in-process windows.
  remote_cocoa::ApplicationHost* application_host() const {
    return application_host_;
  }

  TextInputHost* text_input_host() const { return text_input_host_.get(); }

  // A NSWindow that is guaranteed to exist in this process. If the bridge
  // object for this host is in this process, then this points to the bridge's
  // NSWindow. Otherwise, it mirrors the id and bounds of the child window.
  NativeWidgetMacNSWindow* GetInProcessNSWindow() const;

  // Return the accessibility object for the content NSView.
  gfx::NativeViewAccessible GetNativeViewAccessibleForNSView() const;

  // Return the accessibility object for the NSWindow.
  gfx::NativeViewAccessible GetNativeViewAccessibleForNSWindow() const;

  // The mojo interface through which to communicate with the underlying
  // NSWindow and NSView. This points to either |remote_ns_window_remote_| or
  // |in_process_ns_window_bridge_|.
  remote_cocoa::mojom::NativeWidgetNSWindow* GetNSWindowMojo() const;

  // Direct access to the NativeWidgetNSWindowBridge that this is hosting.
  // TODO(ccameron): Remove all accesses to this member, and replace them
  // with methods that may be sent across processes.
  remote_cocoa::NativeWidgetNSWindowBridge* GetInProcessNSWindowBridge() const {
    return in_process_ns_window_bridge_.get();
  }

  TooltipManager* tooltip_manager() { return tooltip_manager_.get(); }

  DragDropClientMac* drag_drop_client() const {
    return drag_drop_client_.get();
  }

  // Create and set the bridge object to be in this process.
  void CreateInProcessNSWindowBridge(NativeWidgetMacNSWindow* window);

  // Create and set the bridge object to be potentially in another process.
  void CreateRemoteNSWindow(
      remote_cocoa::ApplicationHost* application_host,
      remote_cocoa::mojom::CreateWindowParamsPtr window_create_params);

  void InitWindow(const Widget::InitParams& params,
                  const gfx::Rect& initial_bounds_in_screen);

  // Close the window immediately. This function may result in |this| being
  // deleted.
  void CloseWindowNow();

  // Changes the bounds of the window and the hosted layer if present. The
  // argument is always a location in screen coordinates (in contrast to the
  // views::Widget::SetBounds method, when the argument is only sometimes in
  // screen coordinates).
  void SetBoundsInScreen(const gfx::Rect& bounds);

  // Changes the size of the window, leaving the top-left corner in its current
  // location.
  void SetSize(const gfx::Size& size);

  // Tell the window to transition to being fullscreen or not-fullscreen.
  // If `fullscreen` is true, then `target_display_id` specifies the display to
  // which window should move (or an invalid display, to use the default).
  void SetFullscreen(bool fullscreen,
                     int64_t target_display_id = display::kInvalidDisplayId);

  // The ultimate fullscreen state that is being targeted (irrespective of any
  // active transitions).
  bool target_fullscreen_state() const { return target_fullscreen_state_; }

  // Set the root view (set during initialization and un-set during teardown).
  void SetRootView(views::View* root_view);

  // Return the id through which the NSView for |root_view_| may be looked up.
  uint64_t GetRootViewNSViewId() const { return root_view_id_; }

  void set_immersive_mode_reveal_client(
      ImmersiveModeRevealClient* reveal_client) {
    immersive_mode_reveal_client_ = reveal_client;
  }

  // Initialize the ui::Compositor and ui::Layer.
  void CreateCompositor(const Widget::InitParams& params);

  // Set the window's title, returning true if the title has changed.
  bool SetWindowTitle(const std::u16string& title);

  // Called when the owning Widget's Init method has completed.
  void OnWidgetInitDone();

  // Redispatch a keyboard event using the widget's window's CommandDispatcher.
  // Return true if the event is handled.
  bool RedispatchKeyEvent(NSEvent* event);

  // Geometry of the window, in DIPs.
  const gfx::Rect& GetWindowBoundsInScreen() const {
    return window_bounds_in_screen_;
  }

  // Geometry of the content area of the window, in DIPs. Note that this is not
  // necessarily the same as the views::View's size.
  gfx::Rect GetContentBoundsInScreen() const;

  // The display that the window is currently on (or best guess thereof).
  const display::Display& GetCurrentDisplay() const { return display_; }

  // The restored bounds will be derived from the current NSWindow frame unless
  // fullscreen or transitioning between fullscreen states.
  gfx::Rect GetRestoredBounds() const;

  // An opaque blob of AppKit data which includes, among other things, a
  // window's workspace and fullscreen state, and can be retrieved from or
  // applied to a window.
  const std::vector<uint8_t>& GetWindowStateRestorationData() const;

  // Set |parent_| and update the old and new parents' |children_|. It is valid
  // to set |new_parent| to nullptr. Propagate this to the BridgedNativeWidget.
  void SetParent(NativeWidgetMacNSWindowHost* new_parent);

  // Properties set and queried by views. Not actually native.
  void SetNativeWindowProperty(const char* name, void* value);
  void* GetNativeWindowProperty(const char* name) const;

  // Updates |attached_native_view_host_views_| on
  // NativeViewHost::Attach()/Detach().
  void OnNativeViewHostAttach(const views::View* view, NSView* native_view);
  void OnNativeViewHostDetach(const views::View* view);

  // Sorts child NSViews according to NativeViewHosts order in views hierarchy.
  void ReorderChildViews();

  bool IsVisible() const { return is_visible_; }
  bool IsMiniaturized() const { return is_miniaturized_; }
  bool IsWindowKey() const { return is_window_key_; }
  bool IsMouseCaptureActive() const { return is_mouse_capture_active_; }
  bool IsZoomed() const { return is_zoomed_; }

  void SetVisibilityState(remote_cocoa::mojom::WindowVisibilityState new_state);

  // Add a NSEvent local event monitor, which will send events to `client`
  // before they are dispatched to their ordinary target. Clients may specify
  // that they have handled an event, which will prevent further dispatch. All
  // clients will receive all events, in the order that the clients were added,
  // regardless of whether or not a previous client handled the event.
  std::unique_ptr<NativeWidgetMacEventMonitor> AddEventMonitor(
      NativeWidgetMacEventMonitor::Client* client);
  void RemoveEventMonitor(NativeWidgetMacEventMonitor*);

  // Used by NativeWidgetPrivate::GetGlobalCapture.
  static NSView* GetGlobalCaptureView();

  // Notify PWA whether can GoBack/GoForward.
  void CanGoBack(bool can_go_back);
  void CanGoForward(bool can_go_forward);

  // Accessors to control screenshot availability of the in-process/remote
  // window associated to this host.
  void SetAllowScreenshots(bool allow);
  bool AllowScreenshots() const;

 private:
  friend class TextInputHost;

  void UpdateCompositorProperties();
  void DestroyCompositor();

  // Sort |attached_native_view_host_views_| by the order in which their
  // NSViews should appear as subviews. This does a recursive pre-order
  // traversal of the views::View tree starting at |view|.
  void GetAttachedNativeViewHostViewsRecursive(
      View* view,
      std::vector<NSView*>* attached_native_view_host_views_ordered) const;

  // If we are accessing the BridgedNativeWidget through mojo, then
  // |in_process_ns_window_| is not the true window that is resized. This
  // function updates the frame of |in_process_ns_window_| to keep it in sync
  // for any native calls that may use it (e.g, for context menu positioning).
  void UpdateLocalWindowFrame(const gfx::Rect& frame);

  void DropRootViewReferences();

  // Get the geometry of the window, in DIPs, clamped to specified
  // minimum/maximum window size constraints.
  gfx::Rect GetAdjustedContentBoundsInScreen();

  // NativeWidgetNSWindowHostHelper:
  id GetNativeViewAccessible() override;
  void DispatchKeyEvent(ui::KeyEvent* event) override;
  bool DispatchKeyEventToMenuController(ui::KeyEvent* event) override;
  void GetWordAt(const gfx::Point& location_in_content,
                 bool* found_word,
                 gfx::DecoratedText* decorated_word,
                 gfx::Point* baseline_point) override;
  remote_cocoa::DragDropClient* GetDragDropClient() override;
  ui::TextInputClient* GetTextInputClient() override;
  bool MustPostTaskToRunModalSheetAnimation() const override;

  // remote_cocoa::ApplicationHost::Observer:
  void OnApplicationHostDestroying(
      remote_cocoa::ApplicationHost* host) override;

  // remote_cocoa::mojom::NativeWidgetNSWindowHost:
  void OnVisibilityChanged(bool visible) override;
  void OnWindowNativeThemeChanged() override;
  void OnViewSizeChanged(const gfx::Size& new_size) override;
  bool GetSheetOffsetY(int32_t* offset_y) override;
  void SetKeyboardAccessible(bool enabled) override;
  void OnIsFirstResponderChanged(bool is_first_responder) override;
  void OnMouseCaptureActiveChanged(bool capture_is_active) override;
  void OnScrollEvent(std::unique_ptr<ui::Event> event) override;
  void OnMouseEvent(std::unique_ptr<ui::Event> event) override;
  void OnGestureEvent(std::unique_ptr<ui::Event> event) override;
  bool DispatchKeyEventRemote(std::unique_ptr<ui::Event> event,
                              bool* event_handled) override;
  bool DispatchKeyEventToMenuControllerRemote(std::unique_ptr<ui::Event> event,
                                              bool* event_swallowed,
                                              bool* event_handled) override;
  bool DispatchMonitorEvent(std::unique_ptr<ui::Event> event,
                            bool* event_handled) override;
  bool GetHasMenuController(bool* has_menu_controller) override;
  bool GetIsDraggableBackgroundAt(const gfx::Point& location_in_content,
                                  bool* is_draggable_background) override;
  bool GetWidgetIsModal(bool* widget_is_modal) override;
  bool GetIsFocusedViewTextual(bool* is_textual) override;
  void OnWindowGeometryChanged(
      const gfx::Rect& window_bounds_in_screen_dips,
      const gfx::Rect& content_bounds_in_screen_dips) override;
  void OnWindowFullscreenTransitionStart(bool target_fullscreen_state) override;
  void OnWindowFullscreenTransitionComplete(
      bool target_fullscreen_state) override;
  void OnWindowMiniaturizedChanged(bool miniaturized) override;
  void OnWindowZoomedChanged(bool zoomed) override;
  void OnWindowDisplayChanged(const display::Display& display) override;
  void OnWindowWillClose() override;
  void OnWindowHasClosed() override;
  void OnWindowKeyStatusChanged(bool is_key,
                                bool is_content_first_responder,
                                bool full_keyboard_access_enabled) override;
  void OnWindowStateRestorationDataChanged(
      const std::vector<uint8_t>& data) override;
  void OnImmersiveFullscreenToolbarRevealChanged(bool is_revealed) override;
  void OnImmersiveFullscreenMenuBarRevealChanged(double reveal_amount) override;
  void OnAutohidingMenuBarHeightChanged(int menu_bar_height) override;
  void DoDialogButtonAction(ui::mojom::DialogButton button) override;
  bool GetDialogButtonInfo(ui::mojom::DialogButton type,
                           bool* button_exists,
                           std::u16string* button_label,
                           bool* is_button_enabled,
                           bool* is_button_default) override;
  bool GetDoDialogButtonsExist(bool* buttons_exist) override;
  bool GetShouldShowWindowTitle(bool* should_show_window_title) override;
  bool GetCanWindowBecomeKey(bool* can_window_become_key) override;
  bool GetAlwaysRenderWindowAsKey(bool* always_render_as_key) override;
  bool OnWindowCloseRequested(bool* can_window_close) override;
  bool GetWindowFrameTitlebarHeight(bool* override_titlebar_height,
                                    float* titlebar_height) override;
  void OnFocusWindowToolbar() override;
  void SetRemoteAccessibilityTokens(
      const std::vector<uint8_t>& window_token,
      const std::vector<uint8_t>& view_token) override;
  bool GetRootViewAccessibilityToken(base::ProcessId* pid,
                                     std::vector<uint8_t>* token) override;
  bool ValidateUserInterfaceItem(
      int32_t command,
      remote_cocoa::mojom::ValidateUserInterfaceItemResultPtr* out_result)
      override;
  bool WillExecuteCommand(int32_t command,
                          WindowOpenDisposition window_open_disposition,
                          bool is_before_first_responder,
                          bool* will_execute) override;
  bool ExecuteCommand(int32_t command,
                      WindowOpenDisposition window_open_disposition,
                      bool is_before_first_responder,
                      bool* was_executed) override;
  bool HandleAccelerator(const ui::Accelerator& accelerator,
                         bool require_priority_handler,
                         bool* was_handled) override;

  // remote_cocoa::mojom::NativeWidgetNSWindowHost, synchronous callbacks:
  void GetSheetOffsetY(GetSheetOffsetYCallback callback) override;
  void DispatchKeyEventRemote(std::unique_ptr<ui::Event> event,
                              DispatchKeyEventRemoteCallback callback) override;
  void DispatchKeyEventToMenuControllerRemote(
      std::unique_ptr<ui::Event> event,
      DispatchKeyEventToMenuControllerRemoteCallback callback) override;
  void DispatchMonitorEvent(std::unique_ptr<ui::Event> event,
                            DispatchMonitorEventCallback callback) override;
  void GetHasMenuController(GetHasMenuControllerCallback callback) override;
  void GetIsDraggableBackgroundAt(
      const gfx::Point& location_in_content,
      GetIsDraggableBackgroundAtCallback callback) override;
  void GetTooltipTextAt(const gfx::Point& location_in_content,
                        GetTooltipTextAtCallback callback) override;
  void GetWidgetIsModal(GetWidgetIsModalCallback callback) override;
  void GetIsFocusedViewTextual(
      GetIsFocusedViewTextualCallback callback) override;
  void GetDialogButtonInfo(ui::mojom::DialogButton button,
                           GetDialogButtonInfoCallback callback) override;
  void GetDoDialogButtonsExist(
      GetDoDialogButtonsExistCallback callback) override;
  void GetShouldShowWindowTitle(
      GetShouldShowWindowTitleCallback callback) override;
  void GetCanWindowBecomeKey(GetCanWindowBecomeKeyCallback callback) override;
  void GetAlwaysRenderWindowAsKey(
      GetAlwaysRenderWindowAsKeyCallback callback) override;
  void OnWindowCloseRequested(OnWindowCloseRequestedCallback callback) override;
  void GetWindowFrameTitlebarHeight(
      GetWindowFrameTitlebarHeightCallback callback) override;
  void GetRootViewAccessibilityToken(
      GetRootViewAccessibilityTokenCallback callback) override;
  void ValidateUserInterfaceItem(
      int32_t command,
      ValidateUserInterfaceItemCallback callback) override;
  void WillExecuteCommand(int32_t command,
                          WindowOpenDisposition window_open_disposition,
                          bool is_before_first_responder,
                          ExecuteCommandCallback callback) override;
  void ExecuteCommand(int32_t command,
                      WindowOpenDisposition window_open_disposition,
                      bool is_before_first_responder,
                      ExecuteCommandCallback callback) override;
  void HandleAccelerator(const ui::Accelerator& accelerator,
                         bool require_priority_handler,
                         HandleAcceleratorCallback callback) override;

  // DialogObserver:
  void OnDialogChanged() override;

  // ui::AccessibilityFocusOverrider::Client:
  id GetAccessibilityFocusedUIElement() override;

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;
  void UpdateVisualState() override;

  // ui::AcceleratedWidgetMacNSView:
  void AcceleratedWidgetCALayerParamsUpdated() override;

  // ViewObserver:
  void OnViewIsDeleting(View* observed_view) override;

  // The id that this bridge may be looked up from.
  const uint64_t widget_id_;
  const raw_ptr<views::NativeWidgetMac>
      native_widget_mac_;  // Weak. Owns |this_|.

  // Structure used to look up this structure's interfaces from its
  // gfx::NativeWindow.
  std::unique_ptr<remote_cocoa::ScopedNativeWindowMapping>
      native_window_mapping_;

  // Parent and child widgets.
  raw_ptr<NativeWidgetMacNSWindowHost> parent_ = nullptr;
  std::vector<NativeWidgetMacNSWindowHost*> children_;

  // The factory that was used to create |remote_ns_window_remote_|. This must
  // be the same as |parent_->application_host_|.
  raw_ptr<remote_cocoa::ApplicationHost> application_host_ = nullptr;

  Widget::InitParams::Type widget_type_ = Widget::InitParams::TYPE_WINDOW;

  // The id that may be used to look up the NSView for |root_view_|.
  const uint64_t root_view_id_;

  // Weak. Owned by |native_widget_mac_|.
  raw_ptr<views::View> root_view_ = nullptr;

  std::unique_ptr<DragDropClientMac> drag_drop_client_;

  // The mojo remote for a BridgedNativeWidget, which may exist in another
  // process.
  mojo::AssociatedRemote<remote_cocoa::mojom::NativeWidgetNSWindow>
      remote_ns_window_remote_;

  // Remote accessibility objects corresponding to the NSWindow and its root
  // NSView.
  NSAccessibilityRemoteUIElement* __strong remote_window_accessible_;
  NSAccessibilityRemoteUIElement* __strong remote_view_accessible_;

  // Used to force the NSApplication's focused accessibility element to be the
  // views::Views accessibility tree when the NSView for this is focused.
  ui::AccessibilityFocusOverrider accessibility_focus_overrider_;

  // TODO(ccameron): Rather than instantiate a NativeWidgetNSWindowBridge here,
  // we will instantiate a mojo NativeWidgetNSWindowBridge interface to a Cocoa
  // instance that may be in another process.
  std::unique_ptr<remote_cocoa::NativeWidgetNSWindowBridge>
      in_process_ns_window_bridge_;

  // Window that is guaranteed to exist in this process (see
  // GetInProcessNSWindow).
  NativeWidgetMacNSWindow* __strong in_process_ns_window_;

  // Id mapping for |in_process_ns_window_|'s content NSView.
  std::unique_ptr<remote_cocoa::ScopedNSViewIdMapping>
      in_process_view_id_mapping_;

  std::unique_ptr<TooltipManager> tooltip_manager_;
  std::unique_ptr<TextInputHost> text_input_host_;

  raw_ptr<ImmersiveModeRevealClient> immersive_mode_reveal_client_;

  std::u16string window_title_;

  // The display that the window is currently on.
  display::Display display_;

  // The geometry of the window and its contents view, in screen coordinates.
  gfx::Rect window_bounds_in_screen_;
  gfx::Rect content_bounds_in_screen_;
  std::vector<uint8_t> state_restoration_data_;
  bool is_visible_ = false;
  bool target_fullscreen_state_ = false;
  bool in_fullscreen_transition_ = false;
  bool is_miniaturized_ = false;
  bool is_window_key_ = false;
  bool is_mouse_capture_active_ = false;
  bool is_headless_mode_window_ = false;
  bool is_zoomed_ = false;
  gfx::Rect window_bounds_before_fullscreen_;

  // Weak pointers to event monitors for this widget. The event monitors
  // themselves will remove themselves from this list.
  std::list<NativeWidgetMacEventMonitor*> event_monitors_;

  std::unique_ptr<ui::RecyclableCompositorMac> compositor_;
  std::unique_ptr<remote_cocoa::ScopedCGWindowID> scoped_cg_window_id_;

  // Properties used by Set/GetNativeWindowProperty.
  std::map<std::string, void*> native_window_properties_;

  // Contains NativeViewHost->gfx::NativeView associations for NativeViewHosts
  // attached to |this|.
  std::map<const views::View*, NSView*> attached_native_view_host_views_;

  // Indicates whether the window is allowed to be included in screenshots,
  // based on enterprise policies.
  bool allow_screenshots_ = true;

  mojo::AssociatedReceiver<remote_cocoa::mojom::NativeWidgetNSWindowHost>
      remote_ns_window_host_receiver_{this};

  base::ScopedObservation<View, ViewObserver> root_view_observation_{this};

  base::WeakPtrFactory<NativeWidgetMacNSWindowHost>
      weak_factory_for_vsync_update_{this};
  base::WeakPtrFactory<NativeWidgetMacNSWindowHost> weak_factory_{this};
};

}  // namespace views

#endif  // UI_VIEWS_COCOA_NATIVE_WIDGET_MAC_NS_WINDOW_HOST_H_
