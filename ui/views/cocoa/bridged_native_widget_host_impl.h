// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_BRIDGED_NATIVE_WIDGET_HOST_IMPL_H_
#define UI_VIEWS_COCOA_BRIDGED_NATIVE_WIDGET_HOST_IMPL_H_

#include <memory>
#include <vector>

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"
#include "ui/accelerated_widget_mac/display_link_mac.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/compositor/layer_owner.h"
#include "ui/views/cocoa/bridge_factory_host.h"
#include "ui/views/cocoa/drag_drop_client_mac.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_observer.h"
#include "ui/views_bridge_mac/bridged_native_widget_host_helper.h"
#include "ui/views_bridge_mac/mojo/bridged_native_widget.mojom.h"
#include "ui/views_bridge_mac/mojo/bridged_native_widget_host.mojom.h"

@class NativeWidgetMacNSWindow;
@class NSView;

namespace ui {
class RecyclableCompositorMac;
}  // namespace ui

namespace views {

class BridgedNativeWidgetImpl;
class NativeWidgetMac;

// The portion of NativeWidgetMac that lives in the browser process. This
// communicates to the BridgedNativeWidgetImpl, which interacts with the Cocoa
// APIs, and which may live in an app shim process.
class VIEWS_EXPORT BridgedNativeWidgetHostImpl
    : public views_bridge_mac::BridgedNativeWidgetHostHelper,
      public BridgeFactoryHost::Observer,
      public views_bridge_mac::mojom::BridgedNativeWidgetHost,
      public DialogObserver,
      public FocusChangeListener,
      public ui::internal::InputMethodDelegate,
      public ui::LayerDelegate,
      public ui::LayerOwner,
      public ui::AcceleratedWidgetMacNSView {
 public:
  // Retrieves the bridge host associated with the given NativeWindow. Returns
  // null if the supplied handle has no associated Widget.
  static BridgedNativeWidgetHostImpl* GetFromNativeWindow(
      gfx::NativeWindow window);
  static BridgedNativeWidgetHostImpl* GetFromNativeView(gfx::NativeView view);

  // Unique integer id handles are used to bridge between the
  // BridgedNativeWidgetHostImpl in one process and the BridgedNativeWidgetHost
  // potentially in another.
  static BridgedNativeWidgetHostImpl* GetFromId(
      uint64_t bridged_native_widget_id);
  uint64_t bridged_native_widget_id() const { return widget_id_; }

  // Creates one side of the bridge. |owner| must not be NULL.
  explicit BridgedNativeWidgetHostImpl(NativeWidgetMac* owner);
  ~BridgedNativeWidgetHostImpl() override;

  // The NativeWidgetMac that owns |this|.
  views::NativeWidgetMac* native_widget_mac() const {
    return native_widget_mac_;
  }
  BridgedNativeWidgetHostImpl* parent() const { return parent_; }
  std::vector<BridgedNativeWidgetHostImpl*> children() const {
    return children_;
  }

  // The bridge factory that was used to create the true NSWindow for this
  // widget. This is nullptr for in-process windows.
  BridgeFactoryHost* bridge_factory_host() const {
    return bridge_factory_host_;
  }

  // A NSWindow that is guaranteed to exist in this process. If the bridge
  // object for this host is in this process, then this points to the bridge's
  // NSWindow. Otherwise, it mirrors the id and bounds of the child window.
  NativeWidgetMacNSWindow* GetLocalNSWindow() const;

  // The mojo interface through which to communicate with the underlying
  // NSWindow and NSView.
  views_bridge_mac::mojom::BridgedNativeWidget* bridge() const;

  // Direct access to the BridgedNativeWidgetImpl that this is hosting.
  // TODO(ccameron): Remove all accesses to this member, and replace them
  // with methods that may be sent across processes.
  BridgedNativeWidgetImpl* bridge_impl() const { return bridge_impl_.get(); }

  TooltipManager* tooltip_manager() { return tooltip_manager_.get(); }

  DragDropClientMac* drag_drop_client() const {
    return drag_drop_client_.get();
  }

  // Create and set the bridge object to be in this process.
  void CreateLocalBridge(base::scoped_nsobject<NativeWidgetMacNSWindow> window);

  // Create and set the bridge object to be potentially in another process.
  void CreateRemoteBridge(
      BridgeFactoryHost* bridge_factory_host,
      views_bridge_mac::mojom::CreateWindowParamsPtr window_create_params);

  void InitWindow(const Widget::InitParams& params);

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

  // Set |parent_| and update the old and new parents' |children_|. It is valid
  // to set |new_parent| to nullptr. Propagate this to the BridgedNativeWidget.
  void SetParent(BridgedNativeWidgetHostImpl* new_parent);

  // Properties set and queried by views. Not actually native.
  void SetNativeWindowProperty(const char* name, void* value);
  void* GetNativeWindowProperty(const char* name) const;

  // Updates |associated_views_| on NativeViewHost::Attach()/Detach().
  void SetAssociationForView(const views::View* view, NSView* native_view);
  void ClearAssociationForView(const views::View* view);

  // Sorts child NSViews according to NativeViewHosts order in views hierarchy.
  void ReorderChildViews();

  bool IsVisible() const { return is_visible_; }
  bool IsMiniaturized() const { return is_miniaturized_; }
  bool IsWindowKey() const { return is_window_key_; }
  bool IsMouseCaptureActive() const { return is_mouse_capture_active_; }

  // Used by NativeWidgetPrivate::GetGlobalCapture.
  static NSView* GetGlobalCaptureView();

 private:
  void UpdateCompositorProperties();
  void DestroyCompositor();
  void RankNSViewsRecursive(View* view, std::map<NSView*, int>* rank) const;

  // BridgedNativeWidgetHostHelper:
  NSView* GetNativeViewAccessible() override;
  void DispatchKeyEvent(ui::KeyEvent* event) override;
  bool DispatchKeyEventToMenuController(ui::KeyEvent* event) override;
  void GetWordAt(const gfx::Point& location_in_content,
                 bool* found_word,
                 gfx::DecoratedText* decorated_word,
                 gfx::Point* baseline_point) override;
  double SheetPositionY() override;
  views_bridge_mac::DragDropClient* GetDragDropClient() override;

  // BridgeFactoryHost::Observer:
  void OnBridgeFactoryHostDestroying(BridgeFactoryHost* host) override;

  // views_bridge_mac::mojom::BridgedNativeWidgetHost:
  void OnVisibilityChanged(bool visible) override;
  void OnWindowNativeThemeChanged() override;
  void OnViewSizeChanged(const gfx::Size& new_size) override;
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

  // views_bridge_mac::mojom::BridgedNativeWidgetHost, synchronous callbacks:
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

  // DialogObserver:
  void OnDialogModelChanged() override;

  // FocusChangeListener:
  void OnWillChangeFocus(View* focused_before, View* focused_now) override;
  void OnDidChangeFocus(View* focused_before, View* focused_now) override;

  // ui::internal::InputMethodDelegate:
  ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* key,
      base::OnceCallback<void(bool)> ack_callback) override;

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

  // ui::AcceleratedWidgetMacNSView:
  void AcceleratedWidgetCALayerParamsUpdated() override;

  // The id that this bridge may be looked up from.
  const uint64_t widget_id_;
  views::NativeWidgetMac* const native_widget_mac_;  // Weak. Owns |this_|.

  // Parent and child widgets.
  BridgedNativeWidgetHostImpl* parent_ = nullptr;
  std::vector<BridgedNativeWidgetHostImpl*> children_;

  // The factory that was used to create |bridge_ptr_|. This must be the same
  // as |parent_->bridge_factory_host_|.
  BridgeFactoryHost* bridge_factory_host_ = nullptr;

  Widget::InitParams::Type widget_type_ = Widget::InitParams::TYPE_WINDOW;

  // The id that may be used to look up the NSView for |root_view_|.
  const uint64_t root_view_id_;
  views::View* root_view_ = nullptr;  // Weak. Owned by |native_widget_mac_|.
  std::unique_ptr<DragDropClientMac> drag_drop_client_;

  // The mojo pointer to a BridgedNativeWidget, which may exist in another
  // process.
  views_bridge_mac::mojom::BridgedNativeWidgetAssociatedPtr bridge_ptr_;

  // TODO(ccameron): Rather than instantiate a BridgedNativeWidgetImpl here,
  // we will instantiate a mojo BridgedNativeWidgetImpl interface to a Cocoa
  // instance that may be in another process.
  std::unique_ptr<BridgedNativeWidgetImpl> bridge_impl_;

  // Window that is guaranteed to exist in this process (see GetLocalNSWindow).
  base::scoped_nsobject<NativeWidgetMacNSWindow> local_window_;

  std::unique_ptr<TooltipManager> tooltip_manager_;
  std::unique_ptr<ui::InputMethod> input_method_;
  FocusManager* focus_manager_ = nullptr;  // Weak. Owned by our Widget.

  base::string16 window_title_;

  // The display that the window is currently on.
  display::Display display_;

  // Display link for getting vsync info for |display_|.
  scoped_refptr<ui::DisplayLinkMac> display_link_;

  // The geometry of the window and its contents view, in screen coordinates.
  gfx::Rect window_bounds_in_screen_;
  gfx::Rect content_bounds_in_screen_;
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

  // Contains NativeViewHost->gfx::NativeView associations.
  std::map<const views::View*, NSView*> associated_views_;

  mojo::AssociatedBinding<views_bridge_mac::mojom::BridgedNativeWidgetHost>
      host_mojo_binding_;
  DISALLOW_COPY_AND_ASSIGN(BridgedNativeWidgetHostImpl);
};

}  // namespace views

#endif  // UI_VIEWS_COCOA_BRIDGED_NATIVE_WIDGET_HOST_IMPL_H_
