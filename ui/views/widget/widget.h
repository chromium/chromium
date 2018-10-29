// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_WIDGET_H_
#define UI_VIEWS_WIDGET_WIDGET_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "build/build_config.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event_source.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/native_theme/native_theme_observer.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/native_widget_delegate.h"
#include "ui/views/window/client_view.h"
#include "ui/views/window/non_client_view.h"

namespace base {
class TimeDelta;
}

namespace gfx {
class Point;
class Rect;
}

namespace ui {
class Accelerator;
class Compositor;
class DefaultThemeProvider;
class GestureRecognizer;
class InputMethod;
class Layer;
class NativeTheme;
class OSExchangeData;
class ThemeProvider;
}  // namespace ui

namespace views {

class DesktopWindowTreeHost;
class NativeWidget;
class NonClientFrameView;
class TooltipManager;
class View;
class WidgetDelegate;
class WidgetObserver;
class WidgetRemovalsObserver;

namespace internal {
class NativeWidgetPrivate;
class RootView;
}

////////////////////////////////////////////////////////////////////////////////
// Widget class
//
//  Encapsulates the platform-specific rendering, event receiving and widget
//  management aspects of the UI framework.
//
//  Owns a RootView and thus a View hierarchy. Can contain child Widgets.
//  Widget is a platform-independent type that communicates with a platform or
//  context specific NativeWidget implementation.
//
//  A special note on ownership:
//
//    Depending on the value of the InitParams' ownership field, the Widget
//    either owns or is owned by its NativeWidget:
//
//    ownership = NATIVE_WIDGET_OWNS_WIDGET (default)
//      The Widget instance is owned by its NativeWidget. When the NativeWidget
//      is destroyed (in response to a native destruction message), it deletes
//      the Widget from its destructor.
//    ownership = WIDGET_OWNS_NATIVE_WIDGET (non-default)
//      The Widget instance owns its NativeWidget. This state implies someone
//      else wants to control the lifetime of this object. When they destroy
//      the Widget it is responsible for destroying the NativeWidget (from its
//      destructor). This is often used to place a Widget in a std::unique_ptr<>
//      or on the stack in a test.
class VIEWS_EXPORT Widget : public internal::NativeWidgetDelegate,
                            public ui::EventSource,
                            public FocusTraversable,
                            public ui::NativeThemeObserver {
 public:
  using Widgets = std::set<Widget*>;
  using ShapeRects = std::vector<gfx::Rect>;

  enum FrameType {
    FRAME_TYPE_DEFAULT,         // Use whatever the default would be.
    FRAME_TYPE_FORCE_CUSTOM,    // Force the custom frame.
    FRAME_TYPE_FORCE_NATIVE     // Force the native frame.
  };

  // Result from RunMoveLoop().
  enum MoveLoopResult {
    // The move loop completed successfully.
    MOVE_LOOP_SUCCESSFUL,

    // The user canceled the move loop.
    MOVE_LOOP_CANCELED
  };

  // Source that initiated the move loop.
  enum MoveLoopSource {
    MOVE_LOOP_SOURCE_MOUSE,
    MOVE_LOOP_SOURCE_TOUCH,
  };

  // Behavior when escape is pressed during a move loop.
  enum MoveLoopEscapeBehavior {
    // Indicates the window should be hidden.
    MOVE_LOOP_ESCAPE_BEHAVIOR_HIDE,

    // Indicates the window should not be hidden.
    MOVE_LOOP_ESCAPE_BEHAVIOR_DONT_HIDE,
  };

  // Type of visibility change transition that should animate.
  enum VisibilityTransition {
    ANIMATE_SHOW = 0x1,
    ANIMATE_HIDE = 0x2,
    ANIMATE_BOTH = ANIMATE_SHOW | ANIMATE_HIDE,
    ANIMATE_NONE = 0x4,
  };

  struct VIEWS_EXPORT InitParams {
    enum Type {
      TYPE_WINDOW,      // A decorated Window, like a frame window.
                        // Widgets of TYPE_WINDOW will have a NonClientView.
      TYPE_PANEL,       // Always on top window managed by PanelManager.
                        // Widgets of TYPE_PANEL will have a NonClientView.
      TYPE_WINDOW_FRAMELESS,
                        // An undecorated Window.
      TYPE_CONTROL,     // A control, like a button.
      TYPE_POPUP,       // An undecorated Window, with transient properties.
      TYPE_MENU,        // An undecorated Window, with transient properties
                        // specialized to menus.
      TYPE_TOOLTIP,
      TYPE_BUBBLE,
      TYPE_DRAG,        // An undecorated Window, used during a drag-and-drop to
                        // show the drag image.
    };

    enum WindowOpacity {
      // Infer fully opaque or not. For WinAura, top-level windows that are not
      // of TYPE_WINDOW are translucent so that they can be made to fade in.
      // For LinuxAura, only windows that are TYPE_DRAG are translucent.  In all
      // other cases, windows are fully opaque.
      INFER_OPACITY,
      // Fully opaque.
      OPAQUE_WINDOW,
      // Possibly translucent/transparent.  Widgets that fade in or out using
      // SetOpacity() but do not make use of an alpha channel should use
      // INFER_OPACITY.
      TRANSLUCENT_WINDOW,
    };

    enum Activatable {
      // Infer whether the window should be activatable from the window type.
      ACTIVATABLE_DEFAULT,

      ACTIVATABLE_YES,
      ACTIVATABLE_NO
    };

    enum Ownership {
      // Default. Creator is not responsible for managing the lifetime of the
      // Widget, it is destroyed when the corresponding NativeWidget is
      // destroyed.
      NATIVE_WIDGET_OWNS_WIDGET,
      // Used when the Widget is owned by someone other than the NativeWidget,
      // e.g. a scoped_ptr in tests.
      WIDGET_OWNS_NATIVE_WIDGET
    };

    enum ShadowType {
      SHADOW_TYPE_DEFAULT,  // Use default shadow setting. It will be one of
                            // the settings below depending on InitParams::type
                            // and the native widget's type.
      SHADOW_TYPE_NONE,     // Don't draw any shadow.
      SHADOW_TYPE_DROP,     // Draw a drop shadow that emphasizes Z-order
                            // relationship to other windows.
    };

    InitParams();
    explicit InitParams(Type type);
    InitParams(const InitParams& other);
    ~InitParams();

    // Returns the activatablity based on |activatable|, but also handles the
    // case where |activatable| is |ACTIVATABLE_DEFAULT|.
    bool CanActivate() const;

    Type type;
    // If null, a default implementation will be constructed. The default
    // implementation deletes itself when the Widget closes.
    WidgetDelegate* delegate;
    // Internal name. Propagated to the NativeWidget. Useful for debugging.
    std::string name;
    bool child;
    // If TRANSLUCENT_WINDOW, the widget may be fully or partially transparent.
    // If OPAQUE_WINDOW, we can perform optimizations based on the widget being
    // fully opaque.
    // Default is based on ViewsDelegate::GetOpacityForInitParams().  Defaults
    // to OPAQUE_WINDOW for non-window widgets.
    // Translucent windows may not always be supported. Use
    // IsTranslucentWindowOpacitySupported to determine whether they are.
    WindowOpacity opacity;
    bool accept_events;
    Activatable activatable;
    bool keep_on_top;
    bool visible_on_all_workspaces;
    // See Widget class comment above.
    Ownership ownership;
    bool mirror_origin_in_rtl;
    ShadowType shadow_type;
    // A hint about the size of the shadow if the type is SHADOW_TYPE_DROP. May
    // be ignored on some platforms. No value indicates no preference.
    base::Optional<int> shadow_elevation;
    // The window corner radius. May be ignored on some platforms.
    base::Optional<int> corner_radius;
    // Specifies that the system default caption and icon should not be
    // rendered, and that the client area should be equivalent to the window
    // area. Only used on some platforms (Windows and Linux).
    bool remove_standard_frame;
    // Only used by ShellWindow on Windows. Specifies that the default icon of
    // packaged app should be the system default icon.
    bool use_system_default_icon;
    // Whether the widget should be maximized or minimized.
    ui::WindowShowState show_state;
    gfx::NativeView parent;
    // Specifies the initial bounds of the Widget. Default is empty, which means
    // the NativeWidget may specify a default size. If the parent is specified,
    // |bounds| is in the parent's coordinate system. If the parent is not
    // specified, it's in screen's global coordinate system.
    gfx::Rect bounds;
    // The initial workspace of the Widget.  Default is "", which means the
    // current workspace.
    std::string workspace;
    // When set, this value is used as the Widget's NativeWidget implementation.
    // The Widget will not construct a default one. Default is NULL.
    NativeWidget* native_widget;
    // Aura-only. Provides a DesktopWindowTreeHost implementation to use instead
    // of the default one.
    // TODO(beng): Figure out if there's a better way to expose this, e.g. get
    // rid of NW subclasses and do this all via message handling.
    DesktopWindowTreeHost* desktop_window_tree_host;
    // Only used by NativeWidgetAura. Specifies the type of layer for the
    // aura::Window. Default is ui::LAYER_TEXTURED.
    ui::LayerType layer_type;
    // Only used by Aura. Provides a context window whose RootWindow is
    // consulted during widget creation to determine where in the Window
    // hierarchy this widget should be placed. (This is separate from |parent|;
    // if you pass a RootWindow to |parent|, your window will be parented to
    // |parent|. If you pass a RootWindow to |context|, we ask that RootWindow
    // where it wants your window placed.) Nullptr is not allowed on Windows and
    // Linux. Nullptr is allowed on Chrome OS, which will place the window on
    // the default desktop for new windows.
    gfx::NativeWindow context;
    // If true, forces the window to be shown in the taskbar, even for window
    // types that do not appear in the taskbar by default (popup and bubble).
    bool force_show_in_taskbar;
    // Only used by X11, for root level windows. Specifies the res_name and
    // res_class fields, respectively, of the WM_CLASS window property. Controls
    // window grouping and desktop file matching in Linux window managers.
    std::string wm_role_name;
    std::string wm_class_name;
    std::string wm_class_class;

    // If true then the widget uses software compositing. Defaults to false.
    // Only used on Windows.
    bool force_software_compositing;

    // Used if widget is not activatable to do determine if mouse events should
    // be sent to the widget.
    bool wants_mouse_events_when_inactive = false;

    // A map of properties applied to windows when running in mus.
    std::map<std::string, std::vector<uint8_t>> mus_properties;
  };

  Widget();
  ~Widget() override;

  // Creates a decorated window Widget with the specified properties. The
  // returned Widget is owned by its NativeWidget; see Widget class comment for
  // details.
  static Widget* CreateWindowWithParent(WidgetDelegate* delegate,
                                        gfx::NativeView parent);
  static Widget* CreateWindowWithParentAndBounds(WidgetDelegate* delegate,
                                                 gfx::NativeView parent,
                                                 const gfx::Rect& bounds);

  // Creates a decorated window Widget in the same desktop context as |context|.
  // The returned Widget is owned by its NativeWidget; see Widget class comment
  // for details.
  static Widget* CreateWindowWithContext(WidgetDelegate* delegate,
                                         gfx::NativeWindow context);
  static Widget* CreateWindowWithContextAndBounds(WidgetDelegate* delegate,
                                                  gfx::NativeWindow context,
                                                  const gfx::Rect& bounds);

  // Closes all Widgets that aren't identified as "secondary widgets". Called
  // during application shutdown when the last non-secondary widget is closed.
  static void CloseAllSecondaryWidgets();

  // Retrieves the Widget implementation associated with the given
  // NativeView or Window, or NULL if the supplied handle has no associated
  // Widget.
  static Widget* GetWidgetForNativeView(gfx::NativeView native_view);
  static Widget* GetWidgetForNativeWindow(gfx::NativeWindow native_window);

  // Retrieves the top level widget in a native view hierarchy
  // starting at |native_view|. Top level widget is a widget with TYPE_WINDOW,
  // TYPE_PANEL, TYPE_WINDOW_FRAMELESS, POPUP or MENU and has its own
  // focus manager. This may be itself if the |native_view| is top level,
  // or NULL if there is no toplevel in a native view hierarchy.
  static Widget* GetTopLevelWidgetForNativeView(gfx::NativeView native_view);

  // Returns all Widgets in |native_view|'s hierarchy, including itself if
  // it is one.
  static void GetAllChildWidgets(gfx::NativeView native_view,
                                 Widgets* children);

  // Returns all Widgets owned by |native_view| (including child widgets, but
  // not including itself).
  static void GetAllOwnedWidgets(gfx::NativeView native_view,
                                 Widgets* owned);

  // Re-parent a NativeView and notify all Widgets in |native_view|'s hierarchy
  // of the change.
  static void ReparentNativeView(gfx::NativeView native_view,
                                 gfx::NativeView new_parent);

  // Returns the preferred size of the contents view of this window based on
  // its localized size data. The width in cols is held in a localized string
  // resource identified by |col_resource_id|, the height in the same fashion.
  // TODO(beng): This should eventually live somewhere else, probably closer to
  //             ClientView.
  static int GetLocalizedContentsWidth(int col_resource_id);
  static int GetLocalizedContentsHeight(int row_resource_id);
  static gfx::Size GetLocalizedContentsSize(int col_resource_id,
                                            int row_resource_id);

  // Returns true if the specified type requires a NonClientView.
  static bool RequiresNonClientView(InitParams::Type type);

  void Init(const InitParams& params);

  // Returns the gfx::NativeView associated with this Widget.
  gfx::NativeView GetNativeView() const;

  // Returns the gfx::NativeWindow associated with this Widget. This may return
  // NULL on some platforms if the widget was created with a type other than
  // TYPE_WINDOW or TYPE_PANEL.
  gfx::NativeWindow GetNativeWindow() const;

  // Add/remove observer.
  void AddObserver(WidgetObserver* observer);
  void RemoveObserver(WidgetObserver* observer);
  bool HasObserver(const WidgetObserver* observer) const;

  // Add/remove removals observer.
  void AddRemovalsObserver(WidgetRemovalsObserver* observer);
  void RemoveRemovalsObserver(WidgetRemovalsObserver* observer);
  bool HasRemovalsObserver(const WidgetRemovalsObserver* observer) const;

  // Returns the accelerator given a command id. Returns false if there is
  // no accelerator associated with a given id, which is a common condition.
  virtual bool GetAccelerator(int cmd_id, ui::Accelerator* accelerator) const;

  // Forwarded from the RootView so that the widget can do any cleanup.
  void ViewHierarchyChanged(const View::ViewHierarchyChangedDetails& details);

  // Called right before changing the widget's parent NativeView to do any
  // cleanup.
  void NotifyNativeViewHierarchyWillChange();

  // Called after changing the widget's parent NativeView. Notifies the RootView
  // about the change.
  void NotifyNativeViewHierarchyChanged();

  // Called immediately before removing |view| from this widget.
  void NotifyWillRemoveView(View* view);

  // Returns the top level widget in a hierarchy (see is_top_level() for
  // the definition of top level widget.) Will return NULL if called
  // before the widget is attached to the top level widget's hierarchy.
  Widget* GetTopLevelWidget();
  const Widget* GetTopLevelWidget() const;

  // Gets/Sets the WidgetDelegate.
  WidgetDelegate* widget_delegate() const { return widget_delegate_; }

  // Sets the specified view as the contents of this Widget. There can only
  // be one contents view child of this Widget's RootView. This view is sized to
  // fit the entire size of the RootView. The RootView takes ownership of this
  // View, unless it is set as not being parent-owned.
  void SetContentsView(View* view);

  // NOTE: This may not be the same view as WidgetDelegate::GetContentsView().
  // See RootView::GetContentsView().
  View* GetContentsView();

  // Returns the bounds of the Widget in screen coordinates.
  gfx::Rect GetWindowBoundsInScreen() const;

  // Returns the bounds of the Widget's client area in screen coordinates.
  gfx::Rect GetClientAreaBoundsInScreen() const;

  // Retrieves the restored bounds for the window.
  gfx::Rect GetRestoredBounds() const;

  // Retrieves the current workspace for the window.
  std::string GetWorkspace() const;

  // Sizes and/or places the widget to the specified bounds, size or position.
  void SetBounds(const gfx::Rect& bounds);
  void SetSize(const gfx::Size& size);

  // Sizes the window to the specified size and centerizes it.
  void CenterWindow(const gfx::Size& size);

  // Like SetBounds(), but ensures the Widget is fully visible on screen or
  // parent widget, resizing and/or repositioning as necessary.
  void SetBoundsConstrained(const gfx::Rect& bounds);

  // Sets whether animations that occur when visibility is changed are enabled.
  // Default is true.
  void SetVisibilityChangedAnimationsEnabled(bool value);

  // Sets the duration of visibility change animations.
  void SetVisibilityAnimationDuration(const base::TimeDelta& duration);

  // Sets the visibility transitions that should animate.
  // Default behavior is to animate both show and hide.
  void SetVisibilityAnimationTransition(VisibilityTransition transition);

  // Starts a nested run loop that moves the window. This can be used to
  // start a window move operation from a mouse or touch event. This returns
  // when the move completes. |drag_offset| is the offset from the top left
  // corner of the window to the point where the cursor is dragging, and is used
  // to offset the bounds of the window from the cursor.
  MoveLoopResult RunMoveLoop(const gfx::Vector2d& drag_offset,
                             MoveLoopSource source,
                             MoveLoopEscapeBehavior escape_behavior);

  // Stops a previously started move loop. This is not immediate.
  void EndMoveLoop();

  // Places the widget in front of the specified widget in z-order.
  void StackAboveWidget(Widget* widget);
  void StackAbove(gfx::NativeView native_view);
  void StackAtTop();

  // Sets a shape on the widget. Passing a NULL |shape| reverts the widget to
  // be rectangular.
  void SetShape(std::unique_ptr<ShapeRects> shape);

  // Hides the widget then closes it after a return to the message loop.
  virtual void Close();

  // TODO(beng): Move off public API.
  // Closes the widget immediately. Compare to |Close|. This will destroy the
  // window handle associated with this Widget, so should not be called from
  // any code that expects it to be valid beyond this call.
  void CloseNow();

  // Whether the widget has been asked to close itself. In particular this is
  // set to true after Close() has been invoked on the NativeWidget.
  bool IsClosed() const;

  // Shows the widget. The widget is activated if during initialization the
  // can_activate flag in the InitParams structure is set to true.
  void Show();

  // Hides the widget.
  void Hide();

  // Like Show(), but does not activate the window.
  void ShowInactive();

  // Activates the widget, assuming it already exists and is visible.
  void Activate();

  // Deactivates the widget, making the next window in the Z order the active
  // window.
  void Deactivate();

  // Returns whether the Widget is the currently active window.
  virtual bool IsActive() const;

  // Sets the widget to be on top of all other widgets in the windowing system.
  void SetAlwaysOnTop(bool on_top);

  // Returns whether the widget has been set to be on top of most other widgets
  // in the windowing system.
  bool IsAlwaysOnTop() const;

  // Sets the widget to be visible on all work spaces.
  void SetVisibleOnAllWorkspaces(bool always_visible);

  // Is this widget currently visible on all workspaces?
  // A call to SetVisibleOnAllWorkspaces(true) won't necessarily mean
  // IsVisbleOnAllWorkspaces() == true (for example, when the platform doesn't
  // support workspaces).
  bool IsVisibleOnAllWorkspaces() const;

  // Maximizes/minimizes/restores the window.
  void Maximize();
  void Minimize();
  void Restore();

  // Whether or not the window is maximized or minimized.
  virtual bool IsMaximized() const;
  bool IsMinimized() const;

  // Accessors for fullscreen state.
  void SetFullscreen(bool fullscreen);
  bool IsFullscreen() const;

  // Sets the opacity of the widget. This may allow widgets behind the widget
  // in the Z-order to become visible, depending on the capabilities of the
  // underlying windowing system.
  void SetOpacity(float opacity);

  // Sets the aspect ratio of the widget's content, which will be maintained
  // during interactive resizing. This size disregards title bar and borders.
  // Once set, some platforms ensure the content will only size to integer
  // multiples of |aspect_ratio|.
  void SetAspectRatio(const gfx::SizeF& aspect_ratio);

  // Flashes the frame of the window to draw attention to it. Currently only
  // implemented on Windows for non-Aura.
  void FlashFrame(bool flash);

  // Returns the View at the root of the View hierarchy contained by this
  // Widget.
  View* GetRootView();
  const View* GetRootView() const;

  // A secondary widget is one that is automatically closed (via Close()) when
  // all non-secondary widgets are closed.
  // Default is true.
  // TODO(beng): This is an ugly API, should be handled implicitly via
  //             transience.
  void set_is_secondary_widget(bool is_secondary_widget) {
    is_secondary_widget_ = is_secondary_widget;
  }
  bool is_secondary_widget() const { return is_secondary_widget_; }

  // Returns whether the Widget is visible to the user.
  virtual bool IsVisible() const;

  // Returns the ThemeProvider that provides theme resources for this Widget.
  virtual const ui::ThemeProvider* GetThemeProvider() const;

  ui::NativeTheme* GetNativeTheme() {
    return const_cast<ui::NativeTheme*>(
        const_cast<const Widget*>(this)->GetNativeTheme());
  }
  virtual const ui::NativeTheme* GetNativeTheme() const;

  // Returns the FocusManager for this widget.
  // Note that all widgets in a widget hierarchy share the same focus manager.
  FocusManager* GetFocusManager();
  const FocusManager* GetFocusManager() const;

  // Returns the ui::InputMethod for this widget.
  ui::InputMethod* GetInputMethod();

  // Starts a drag operation for the specified view. This blocks until the drag
  // operation completes. |view| can be NULL.
  // If the view is non-NULL it can be accessed during the drag by calling
  // dragged_view(). If the view has not been deleted during the drag,
  // OnDragDone() is called on it. |location| is in the widget's coordinate
  // system.
  void RunShellDrag(View* view,
                    const ui::OSExchangeData& data,
                    const gfx::Point& location,
                    int operation,
                    ui::DragDropTypes::DragEventSource source);

  // Returns the view that requested the current drag operation via
  // RunShellDrag(), or NULL if there is no such view or drag operation.
  View* dragged_view() {
    return const_cast<View*>(const_cast<const Widget*>(this)->dragged_view());
  }
  const View* dragged_view() const { return dragged_view_; }

  // Adds the specified |rect| in client area coordinates to the rectangle to be
  // redrawn.
  virtual void SchedulePaintInRect(const gfx::Rect& rect);

  // Sets the currently visible cursor. If |cursor| is NULL, the cursor used
  // before the current is restored.
  void SetCursor(gfx::NativeCursor cursor);

  // Returns true if and only if mouse events are enabled.
  bool IsMouseEventsEnabled() const;

  // Sets/Gets a native window property on the underlying native window object.
  // Returns NULL if the property does not exist. Setting the property value to
  // NULL removes the property.
  void SetNativeWindowProperty(const char* name, void* value);
  void* GetNativeWindowProperty(const char* name) const;

  // Tell the window to update its title from the delegate.
  void UpdateWindowTitle();

  // Tell the window to update its icon from the delegate.
  void UpdateWindowIcon();

  // Retrieves the focus traversable for this widget.
  FocusTraversable* GetFocusTraversable();

  // Notifies the view hierarchy contained in this widget that theme resources
  // changed.
  void ThemeChanged();

  // Notifies the view hierarchy contained in this widget that the device scale
  // factor changed.
  void DeviceScaleFactorChanged(float old_device_scale_factor,
                                float new_device_scale_factor);

  void SetFocusTraversableParent(FocusTraversable* parent);
  void SetFocusTraversableParentView(View* parent_view);

  // Clear native focus set to the Widget's NativeWidget.
  void ClearNativeFocus();

  void set_frame_type(FrameType frame_type) { frame_type_ = frame_type; }
  FrameType frame_type() const { return frame_type_; }

  // Creates an appropriate NonClientFrameView for this widget. The
  // WidgetDelegate is given the first opportunity to create one, followed by
  // the NativeWidget implementation. If both return NULL, a default one is
  // created.
  virtual NonClientFrameView* CreateNonClientFrameView();

  // Whether we should be using a native frame.
  bool ShouldUseNativeFrame() const;

  // Determines whether the window contents should be rendered transparently
  // (for example, so that they can overhang onto the window title bar).
  bool ShouldWindowContentsBeTransparent() const;

  // Forces the frame into the alternate frame type (custom or native) depending
  // on its current state.
  void DebugToggleFrameType();

  // Tell the window that something caused the frame type to change.
  void FrameTypeChanged();

  NonClientView* non_client_view() {
    return const_cast<NonClientView*>(
        const_cast<const Widget*>(this)->non_client_view());
  }
  const NonClientView* non_client_view() const {
    return non_client_view_;
  }

  ClientView* client_view() {
    return const_cast<ClientView*>(
        const_cast<const Widget*>(this)->client_view());
  }
  const ClientView* client_view() const {
    // non_client_view_ may be NULL, especially during creation.
    return non_client_view_ ? non_client_view_->client_view() : NULL;
  }

  ui::Compositor* GetCompositor() {
    return const_cast<ui::Compositor*>(
        const_cast<const Widget*>(this)->GetCompositor());
  }
  const ui::Compositor* GetCompositor() const;

  // Returns the widget's layer, if any.
  ui::Layer* GetLayer() {
    return const_cast<ui::Layer*>(
        const_cast<const Widget*>(this)->GetLayer());
  }
  const ui::Layer* GetLayer() const;

  // Reorders the widget's child NativeViews which are associated to the view
  // tree (eg via a NativeViewHost) to match the z-order of the views in the
  // view tree. The z-order of views with layers relative to views with
  // associated NativeViews is used to reorder the NativeView layers. This
  // method assumes that the widget's child layers which are owned by a view are
  // already in the correct z-order relative to each other and does no
  // reordering if there are no views with an associated NativeView.
  void ReorderNativeViews();

  // Called by a View when the status of it's layer or one of the views
  // descendants layer status changes.
  void LayerTreeChanged();

  const NativeWidget* native_widget() const;
  NativeWidget* native_widget();

  internal::NativeWidgetPrivate* native_widget_private() {
    return native_widget_;
  }
  const internal::NativeWidgetPrivate* native_widget_private() const {
    return native_widget_;
  }

  // Sets capture to the specified view. This makes it so that all mouse, touch
  // and gesture events go to |view|. If |view| is NULL, the widget still
  // obtains event capture, but the events will go to the view they'd normally
  // go to.
  void SetCapture(View* view);

  // Releases capture.
  void ReleaseCapture();

  // Returns true if the widget has capture.
  bool HasCapture();

  void set_auto_release_capture(bool auto_release_capture) {
    auto_release_capture_ = auto_release_capture;
  }

  // Returns the font used for tooltips.
  TooltipManager* GetTooltipManager();
  const TooltipManager* GetTooltipManager() const;

  void set_focus_on_creation(bool focus_on_creation) {
    focus_on_creation_ = focus_on_creation;
  }

  // True if the widget is considered top level widget. Top level widget
  // is a widget of TYPE_WINDOW, TYPE_PANEL, TYPE_WINDOW_FRAMELESS, BUBBLE,
  // POPUP or MENU, and has a focus manager and input method object associated
  // with it. TYPE_CONTROL and TYPE_TOOLTIP is not considered top level.
  bool is_top_level() const { return is_top_level_; }

  // True when window movement via mouse interaction with the frame is disabled.
  bool movement_disabled() const { return movement_disabled_; }
  void set_movement_disabled(bool disabled) { movement_disabled_ = disabled; }

  // Returns the work area bounds of the screen the Widget belongs to.
  gfx::Rect GetWorkAreaBoundsInScreen() const;

  // Creates and dispatches synthesized mouse move event using the current
  // mouse location to refresh hovering status in the widget.
  void SynthesizeMouseMoveEvent();

  // Whether the widget supports translucency.
  bool IsTranslucentWindowOpacitySupported() const;

  // Returns the gesture recognizer which can handle touch/gesture events on
  // this.
  ui::GestureRecognizer* GetGestureRecognizer();

  // Called when the delegate's CanResize or CanMaximize changes.
  void OnSizeConstraintsChanged();

  // Notification that our owner is closing.
  // NOTE: this is not invoked for aura as it's currently not needed there.
  // Under aura menus close by way of activation getting reset when the owner
  // closes.
  virtual void OnOwnerClosing();

  // Returns the internal name for this Widget and NativeWidget.
  std::string GetName() const;

  // Overridden from NativeWidgetDelegate:
  bool IsModal() const override;
  bool IsDialogBox() const override;
  bool CanActivate() const override;
  bool IsAlwaysRenderAsActive() const override;
  void SetAlwaysRenderAsActive(bool always_render_as_active) override;
  bool OnNativeWidgetActivationChanged(bool active) override;
  void OnNativeFocus() override;
  void OnNativeBlur() override;
  void OnNativeWidgetVisibilityChanging(bool visible) override;
  void OnNativeWidgetVisibilityChanged(bool visible) override;
  void OnNativeWidgetCreated(bool desktop_widget) override;
  void OnNativeWidgetDestroying() override;
  void OnNativeWidgetDestroyed() override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnNativeWidgetMove() override;
  void OnNativeWidgetSizeChanged(const gfx::Size& new_size) override;
  void OnNativeWidgetWorkspaceChanged() override;
  void OnNativeWidgetWindowShowStateChanged() override;
  void OnNativeWidgetBeginUserBoundsChange() override;
  void OnNativeWidgetEndUserBoundsChange() override;
  bool HasFocusManager() const override;
  void OnNativeWidgetPaint(const ui::PaintContext& context) override;
  int GetNonClientComponent(const gfx::Point& point) override;
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnMouseCaptureLost() override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool ExecuteCommand(int command_id) override;
  bool HasHitTestMask() const override;
  void GetHitTestMask(gfx::Path* mask) const override;
  Widget* AsWidget() override;
  const Widget* AsWidget() const override;
  bool SetInitialFocus(ui::WindowShowState show_state) override;
  bool ShouldDescendIntoChildForEventHandling(
      ui::Layer* root_layer,
      gfx::NativeView child,
      ui::Layer* child_layer,
      const gfx::Point& location) override;

  // Overridden from ui::EventSource:
  ui::EventSink* GetEventSink() override;

  // Overridden from FocusTraversable:
  FocusSearch* GetFocusSearch() override;
  FocusTraversable* GetFocusTraversableParent() override;
  View* GetFocusTraversableParentView() override;

  // Overridden from ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

 protected:
  // Creates the RootView to be used within this Widget. Subclasses may override
  // to create custom RootViews that do specialized event processing.
  // TODO(beng): Investigate whether or not this is needed.
  virtual internal::RootView* CreateRootView();

  // Provided to allow the NativeWidget implementations to destroy the RootView
  // _before_ the focus manager/tooltip manager.
  // TODO(beng): remove once we fold those objects onto this one.
  void DestroyRootView();

  // Notification that a drag will start. Default implementation does nothing.
  virtual void OnDragWillStart();

  // Notification that the drag performed by RunShellDrag() has completed.
  virtual void OnDragComplete();

 private:
  friend class ComboboxTest;
  friend class ButtonTest;
  friend class TextfieldTest;
  friend class ViewAuraTest;
  friend void DisableActivationChangeHandlingForTests();

  // Persists the window's restored position and "show" state using the
  // window delegate.
  void SaveWindowPlacement();

  // Invokes SaveWindowPlacement() if the native widget has been initialized.
  // This is called at times when the native widget may not have been
  // initialized.
  void SaveWindowPlacementIfInitialized();

  // Sizes and positions the window just after it is created.
  void SetInitialBounds(const gfx::Rect& bounds);

  // Sizes and positions the frameless window just after it is created.
  void SetInitialBoundsForFramelessWindow(const gfx::Rect& bounds);

  // Returns the bounds and "show" state from the delegate. Returns true if
  // the delegate wants to use a specified bounds.
  bool GetSavedWindowPlacement(gfx::Rect* bounds,
                               ui::WindowShowState* show_state);

  // Returns the Views whose layers are parented directly to the Widget's
  // layer.
  const View::Views& GetViewsWithLayers();

  static bool g_disable_activation_change_handling_;

  internal::NativeWidgetPrivate* native_widget_;

  base::ObserverList<WidgetObserver> observers_;

  base::ObserverList<WidgetRemovalsObserver>::Unchecked removals_observers_;

  // Non-owned pointer to the Widget's delegate. If a NULL delegate is supplied
  // to Init() a default WidgetDelegate is created.
  WidgetDelegate* widget_delegate_;

  // The root of the View hierarchy attached to this window.
  // WARNING: see warning in tooltip_manager_ for ordering dependencies with
  // this and tooltip_manager_.
  std::unique_ptr<internal::RootView> root_view_;

  // The View that provides the non-client area of the window (title bar,
  // window controls, sizing borders etc). To use an implementation other than
  // the default, this class must be sub-classed and this value set to the
  // desired implementation before calling |InitWindow()|.
  NonClientView* non_client_view_;

  // The focus manager keeping track of focus for this Widget and any of its
  // children.  NULL for non top-level widgets.
  // WARNING: RootView's destructor calls into the FocusManager. As such, this
  // must be destroyed AFTER root_view_. This is enforced in DestroyRootView().
  std::unique_ptr<FocusManager> focus_manager_;

  // A theme provider to use when no other theme provider is specified.
  std::unique_ptr<ui::DefaultThemeProvider> default_theme_provider_;

  // Valid for the lifetime of RunShellDrag(), indicates the view the drag
  // started from.
  View* dragged_view_;

  // See class documentation for Widget above for a note about ownership.
  InitParams::Ownership ownership_;

  // See set_is_secondary_widget().
  bool is_secondary_widget_;

  // The current frame type in use by this window. Defaults to
  // FRAME_TYPE_DEFAULT.
  FrameType frame_type_;

  // True when the window should be rendered as active, regardless of whether
  // or not it actually is.
  bool always_render_as_active_;

  // Set to true if the widget is in the process of closing.
  bool widget_closed_;

  // The saved "show" state for this window. See note in SetInitialBounds
  // that explains why we save this.
  ui::WindowShowState saved_show_state_;

  // The restored bounds used for the initial show. This is only used if
  // |saved_show_state_| is maximized.
  gfx::Rect initial_restored_bounds_;

  // Focus is automatically set to the view provided by the delegate
  // when the widget is shown. Set this value to false to override
  // initial focus for the widget.
  bool focus_on_creation_;

  // See |is_top_level()| accessor.
  bool is_top_level_;

  // Tracks whether native widget has been initialized.
  bool native_widget_initialized_;

  // Whether native widget has been destroyed.
  bool native_widget_destroyed_;

  // TODO(beng): Remove NativeWidgetGtk's dependence on these:
  // If true, the mouse is currently down.
  bool is_mouse_button_pressed_;

  // True if capture losses should be ignored.
  bool ignore_capture_loss_;

  // TODO(beng): Remove NativeWidgetGtk's dependence on these:
  // The following are used to detect duplicate mouse move events and not
  // deliver them. Displaying a window may result in the system generating
  // duplicate move events even though the mouse hasn't moved.
  bool last_mouse_event_was_move_;
  gfx::Point last_mouse_event_position_;

  // True if event capture should be released on a mouse up event. Default is
  // true.
  bool auto_release_capture_;

  // See description in GetViewsWithLayers().
  View::Views views_with_layers_;

  // Does |views_with_layers_| need updating?
  bool views_with_layers_dirty_;

  // True when window movement via mouse interaction with the frame should be
  // disabled.
  bool movement_disabled_;

  ScopedObserver<ui::NativeTheme, ui::NativeThemeObserver> observer_manager_;

  DISALLOW_COPY_AND_ASSIGN(Widget);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_WIDGET_H_
