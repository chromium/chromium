// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_NATIVE_WIDGET_MAC_NS_WINDOW_HOST_H_
#define UI_VIEWS_COCOA_NATIVE_WIDGET_MAC_NS_WINDOW_HOST_H_

#include <memory>
#include <vector>

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "components/remote_cocoa/app_shim/native_widget_ns_window_host_helper.h"
#include "components/remote_cocoa/app_shim/ns_view_ids.h"
#include "components/remote_cocoa/browser/application_host.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom.h"
#include "components/remote_cocoa/common/native_widget_ns_window_host.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"
#include "ui/base/cocoa/accessibility_focus_overrider.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/compositor/layer_owner.h"
#include "ui/display/mac/display_link_mac.h"
#include "ui/views/cocoa/drag_drop_client_mac.h"
#include "ui/views/focus/focus_manager.h"
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

class NativeWidgetMac;
class TextInputHost;

// The portion of NativeWidgetMac that lives in the browser process. This
// communicates to the NativeWidgetNSWindowBridge, which interacts with the
// Cocoa APIs, and which may live in an app shim process.
class VIEWS_EXPORT NativeWidgetMacNSWindowHost
    : public remote_cocoa::NativeWidgetNSWindowHostHelper,
      public remote_cocoa::ApplicationHost::Observer,
      public remote_cocoa::mojom::NativeWidgetNSWindowHost,
      public DialogObserver,
      public FocusChangeListener,
      public ui::internal::InputMethodDelegate,
      public ui::AccessibilityFocusOverrider::Client,
      public ui::LayerDelegate,
      public ui::LayerOwner,
      public ui::AcceleratedWidgetMacNSView {
 public:
  // Retrieves the bridge host associated with the given NativeWindow. Returns
  // null if the supplied handle has no associated Widget.
  static NativeWidgetMacNSWindowHost* GetFromNativeWindow(
      gfx::NativeWindow window);
  static NativeWidgetMacNSWindowHost* GetFromNativeView(gfx::NativeView view);

  // Unique integer id handles are used to bridge between the
  // NativeWidgetMacNSWindowHost in one process and the NativeWidgetNSWindowHost
  // potentially in another.
  static NativeWidgetMacNSWindowHost* GetFromId(
      uint64_t bridged_native_widget_id);
  uint64_t bridged_native_widget_id() const { return widget_id_; }

  // Creates one side of the bridge. |owner| must not be NULL.
  explicit NativeWidgetMacNSWindowHost(NativeWidgetMac* owner);
  ~NativeWidgetMacNSWindowHost() override;

  // The NativeWidgetMac that owns |this|.
  views::NativeWidgetMac* native_widget_mac() const {
    return native_widget_mac_;
  }
  NativeWidgetMacNSWindowHost* parent() const { return parent_; }
  std::vector<NativeWidgetMacNSWindowHost*> children() const {
    return children_;
  }

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
  void CreateInProcessNSWindowBridge(
      base::scoped_nsobject<NativeWidgetMacNSWindow> window);

  // Create and set the bridge object to be potentially in another process.
  void CreateRemoteNSWindow(
      remote_cocoa::ApplicationHost* application_host,
      remote_cocoa::mojom::CreateWindowParamsPtr window_create_params);

  void InitWindow(const Widget::InitParams& params);

  // Close the window immediately. This function may result in |this| being
  // deleted.
  void CloseWindowNow();

  // Changes the bounds of the window and the hosted layer if present. The
  // origin is a location in screen coordinates except for "child" windows,
  // which are positioned relative to their parent. SetBounds() considers a
  // "child" window to be one initialized with InitParams specifying all of:
  // a |parent| NSWindow, the |child| attribute, and a |type| that
  // views::GetAuraWindowTypeForWidgetType does not consider a "popup" type.
  void SetBounds(const gfx::Rect& bounds);

  // Tell the window to transition to being fullscreen or not-fullscreen.
  void SetFullscreen(bool fullscreen);

  // The ultimate fullscreen state that is being targeted (irrespective of any
  // active transitions).
  bool target_fullscreen_state() const { return target_fullscreen_state_; }

  // Set the root view (set during initialization and un-set during teardown).
  void SetRootView(views::View* root_view);

  // Return the id through which the NSView for |root_view_| may be looked up.
  uint64_t GetRootViewNSViewId() const { return root_view_id_; }

  // Initialize the ui::Compositor and ui::Layer.
  void CreateCompositor(const Widget::InitParams& params);

  // Sets or clears the focus manager to use for tracking focused views.
  // This does NOT take ownership of |focus_manager|.
  void SetFocusManager(FocusManager* focus_manager);

  // Set the window's title, returning true if the title has changed.
  bool SetWindowTitle(const base::string16& title);

  // Called when the owning Widget's Init method has completed.
  void OnWidgetInitDone();

  // Redispatch a keyboard event using the widget's window's CommandDispatcher.
  // Return true if the event is handled.
  bool RedispatchKeyEvent(NSEvent* event);

  // See widget.h for documentation.
  ui::InputMethod* GetInputMethod();

  // Geometry of the window, in DIPs.
  const gfx::Rect& GetWindowBoundsInScreen() const {
    return window_bounds_in_screen_;
  }

  // Geometry of the content area of the window, in DIPs. Note that this is not
  // necessarily the same as the views::View's size.
  const gfx::Rect& GetContentBoundsInScreen() const {
    return content_bounds_in_screen_;
  }

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

  // Used by NativeWidgetPrivate::GetGlobalCapture.
  static NSView* GetGlobalCaptureView();

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
  bool GetHasMenuController(bool* has_menu_controller) override;
  bool GetIsDraggableBackgroundAt(const gfx::Point& location_in_content,
                                  bool* is_draggable_background) override;
  bool GetTooltipTextAt(const gfx::Point& location_in_content,
                        base::string16* new_tooltip_text) override;
  bool GetWidgetIsModal(bool* widget_is_modal) override;
  bool GetIsFocusedViewTextual(bool* is_textual) override;
  void OnWindowGeometryChanged(
      const gfx::Rect& window_bounds_in_screen_dips,
      const gfx::Rect& content_bounds_in_screen_dips) override;
  void OnWindowFullscreenTransitionStart(bool target_fullscreen_state) override;
  void OnWindowFullscreenTransitionComplete(
      bool target_fullscreen_state) override;
  void OnWindowMiniaturizedChanged(bool miniaturized) override;
  void OnWindowDisplayChanged(const display::Display& display) override;
  void OnWindowWillClose() override;
  void OnWindowHasClosed() override;
  void OnWindowKeyStatusChanged(bool is_key,
                                bool is_content_first_responder,
                                bool full_keyboard_access_enabled) override;
  void OnWindowStateRestorationDataChanged(
      const std::vector<uint8_t>& data) override;
  void DoDialogButtonAction(ui::DialogButton button) override;
  bool GetDialogButtonInfo(ui::DialogButton type,
                           bool* button_exists,
                           base::string16* button_label,
                           bool* is_button_enabled,
                           bool* is_button_default) override;
  bool GetDoDialogButtonsExist(bool* buttons_exist) override;
  bool GetShouldShowWindowTitle(bool* should_show_window_title) override;
  bool GetCanWindowBecomeKey(bool* can_window_become_key) override;
  bool GetAlwaysRenderWindowAsKey(bool* always_render_as_key) override;
  bool GetCanWindowClose(bool* can_window_close) override;
  bool GetWindowFrameTitlebarHeight(bool* override_titlebar_height,
                                    float* titlebar_height) override;
  void OnFocusWindowToolbar() override;
  void SetRemoteAccessibilityTokens(
      const std::vector<uint8_t>& window_token,
      const std::vector<uint8_t>& view_token) override;
  bool GetRootViewAccessibilityToken(int64_t* pid,
                                     std::vector<uint8_t>* token) override;
  bool ValidateUserInterfaceItem(
      int32_t command,
      remote_cocoa::mojom::ValidateUserInterfaceItemResultPtr* out_result)
      override;
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
  void GetHasMenuController(GetHasMenuControllerCallback callback) override;
  void GetIsDraggableBackgroundAt(
      const gfx::Point& location_in_content,
      GetIsDraggableBackgroundAtCallback callback) override;
  void GetTooltipTextAt(const gfx::Point& location_in_content,
                        GetTooltipTextAtCallback callback) override;
  void GetWidgetIsModal(GetWidgetIsModalCallback callback) override;
  void GetIsFocusedViewTextual(
      GetIsFocusedViewTextualCallback callback) override;
  void GetDialogButtonInfo(ui::DialogButton button,
                           GetDialogButtonInfoCallback callback) override;
  void GetDoDialogButtonsExist(
      GetDoDialogButtonsExistCallback callback) override;
  void GetShouldShowWindowTitle(
      GetShouldShowWindowTitleCallback callback) override;
  void GetCanWindowBecomeKey(GetCanWindowBecomeKeyCallback callback) override;
  void GetAlwaysRenderWindowAsKey(
      GetAlwaysRenderWindowAsKeyCallback callback) override;
  void GetCanWindowClose(GetCanWindowCloseCallback callback) override;
  void GetWindowFrameTitlebarHeight(
      GetWindowFrameTitlebarHeightCallback callback) override;
  void GetRootViewAccessibilityToken(
      GetRootViewAccessibilityTokenCallback callback) override;
  void ValidateUserInterfaceItem(
      int32_t command,
      ValidateUserInterfaceItemCallback callback) override;
  void ExecuteCommand(int32_t command,
                      WindowOpenDisposition window_open_disposition,
                      bool is_before_first_responder,
                      ExecuteCommandCallback callback) override;
  void HandleAccelerator(const ui::Accelerator& accelerator,
                         bool require_priority_handler,
                         HandleAcceleratorCallback callback) override;

  // DialogObserver:
  void OnDialogChanged() override;

  // FocusChangeListener:
  void OnWillChangeFocus(View* focused_before, View* focused_now) override;
  void OnDidChangeFocus(View* focused_before, View* focused_now) override;

  // ui::internal::InputMethodDelegate:
  ui::EventDispatchDetails DispatchKeyEventPostIME(ui::KeyEvent* key) override;

  // ui::AccessibilityFocusOverrider::Client:
  id GetAccessibilityFocusedUIElement() override;

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;
  void UpdateVisualState() override;

  // ui::AcceleratedWidgetMacNSView:
  void AcceleratedWidgetCALayerParamsUpdated() override;

  // The id that this bridge may be looked up from.
  const uint64_t widget_id_;
  views::NativeWidgetMac* const native_widget_mac_;  // Weak. Owns |this_|.

  // Structure used to look up this structure's interfaces from its
  // gfx::NativeWindow.
  std::unique_ptr<remote_cocoa::ScopedNativeWindowMapping>
      native_window_mapping_;

  // Parent and child widgets.
  NativeWidgetMacNSWindowHost* parent_ = nullptr;
  std::vector<NativeWidgetMacNSWindowHost*> children_;

  // The factory that was used to create |remote_ns_window_remote_|. This must
  // be the same as |parent_->application_host_|.
  remote_cocoa::ApplicationHost* application_host_ = nullptr;

  Widget::InitParams::Type widget_type_ = Widget::InitParams::TYPE_WINDOW;

  // The id that may be used to look up the NSView for |root_view_|.
  const uint64_t root_view_id_;

  // Weak. Owned by |native_widget_mac_|.
  views::View* root_view_ = nullptr;

  std::unique_ptr<DragDropClientMac> drag_drop_client_;

  // The mojo remote for a BridgedNativeWidget, which may exist in another
  // process.
  mojo::AssociatedRemote<remote_cocoa::mojom::NativeWidgetNSWindow>
      remote_ns_window_remote_;

  // Remote accessibility objects corresponding to the NSWindow and its root
  // NSView.
  base::scoped_nsobject<NSAccessibilityRemoteUIElement>
      remote_window_accessible_;
  base::scoped_nsobject<NSAccessibilityRemoteUIElement> remote_view_accessible_;

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
  base::scoped_nsobject<NativeWidgetMacNSWindow> in_process_ns_window_;

  // Id mapping for |in_process_ns_window_|'s content NSView.
  std::unique_ptr<remote_cocoa::ScopedNSViewIdMapping>
      in_process_view_id_mapping_;

  std::unique_ptr<TooltipManager> tooltip_manager_;
  std::unique_ptr<ui::InputMethod> input_method_;
  std::unique_ptr<TextInputHost> text_input_host_;
  FocusManager* focus_manager_ = nullptr;  // Weak. Owned by our Widget.

  base::string16 window_title_;

  // The display that the window is currently on.
  display::Display display_;

  // Display link for getting vsync info for |display_|.
  scoped_refptr<ui::DisplayLinkMac> display_link_;

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
  gfx::Rect window_bounds_before_fullscreen_;

  std::unique_ptr<ui::RecyclableCompositorMac> compositor_;

  // Properties used by Set/GetNativeWindowProperty.
  std::map<std::string, void*> native_window_properties_;

  // Contains NativeViewHost->gfx::NativeView associations for NativeViewHosts
  // attached to |this|.
  std::map<const views::View*, NSView*> attached_native_view_host_views_;

  mojo::AssociatedReceiver<remote_cocoa::mojom::NativeWidgetNSWindowHost>
      remote_ns_window_host_receiver_{this};
  DISALLOW_COPY_AND_ASSIGN(NativeWidgetMacNSWindowHost);
};

}  // namespace views

#endif  // UI_VIEWS_COCOA_NATIVE_WIDGET_MAC_NS_WINDOW_HOST_H_
