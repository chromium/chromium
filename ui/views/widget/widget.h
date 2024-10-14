// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_WIDGET_H_
#define UI_VIEWS_WIDGET_WIDGET_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/safety_checks.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/class_property.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_types.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_source.h"
#include "ui/color/color_provider_utils.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/event_source.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/native_widget_delegate.h"
#include "ui/views/window/client_view.h"
#include "ui/views/window/non_client_view.h"

namespace base {
class TimeDelta;
}

namespace gfx {
class Insets;
class Point;
class Rect;
}  // namespace gfx

namespace ui {
class Accelerator;
class ColorProvider;
class Compositor;
class GestureRecognizer;
class InputMethod;
class Layer;
class OSExchangeData;
class ThemeProvider;
}  // namespace ui

namespace ui_devtools {
class BubbleLocking;
}

namespace views {

class DesktopWindowTreeHost;
class NativeWidget;
class NonClientFrameView;
class SublevelManager;
class TooltipManager;
class View;
class WidgetDelegate;
class WidgetObserver;
class WidgetRemovalsObserver;

namespace internal {
class NativeWidgetPrivate;
class RootView;
}  // namespace internal

enum class CloseRequestResult { kCanClose, kCannotClose };

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
                            public ui::NativeThemeObserver,
                            public ui::ColorProviderSource,
                            public ui::PropertyHandler,
                            public ui::metadata::MetaDataProvider {
  // Do not remove this macro!
  // The macro is maintained by the memory safety team.
  // `Widget` is used extensively and has a complicated lifetime. This macro
  // enables additional lifetime checks.
  ADVANCED_MEMORY_SAFETY_CHECKS();

 public:
  METADATA_HEADER_BASE(Widget);
  using Widgets = std::set<raw_ptr<Widget, SetExperimental>>;
  using ShapeRects = std::vector<gfx::Rect>;
  using PaintAsActiveCallbackList = base::RepeatingClosureList;

  enum class FrameType {
    kDefault,      // Use whatever the default would be.
    kForceCustom,  // Force the custom frame.
    kForceNative   // Force the native frame.
  };

  // Result from RunMoveLoop().
  enum class MoveLoopResult {
    // The move loop completed successfully.
    kSuccessful,

    // The user canceled the move loop.
    kCanceled
  };

  // Source that initiated the move loop.
  enum class MoveLoopSource {
    kMouse,
    kTouch,
  };

  // Behavior when escape is pressed during a move loop.
  enum class MoveLoopEscapeBehavior {
    // Indicates the window should be hidden.
    kHide,

    // Indicates the window should not be hidden.
    kDontHide,
  };

  // Type of visibility change transition that should animate.
  enum VisibilityTransition {
    ANIMATE_SHOW = 0x1,
    ANIMATE_HIDE = 0x2,
    ANIMATE_BOTH = ANIMATE_SHOW | ANIMATE_HIDE,
    ANIMATE_NONE = 0x4,
  };

  // Represents the reason a Widget was closed, if it is known.
  //
  // For backwards compatibility, we default to kUnspecified when
  // Widget::Close() is called. Note that we do not currently handle close
  // reason for menu or for the main Chrome browser, as we have no reason to
  // specifically differentiate those yet.
  //
  // Add additional values as needed. Do not change any existing values, as this
  // enum is logged to UMA.
  enum class ClosedReason {
    kUnspecified = 0,         // No reason was given for the widget closing.
    kEscKeyPressed = 1,       // The ESC key was pressed to cancel the widget.
    kCloseButtonClicked = 2,  // The [X] button was explicitly clicked.
    kLostFocus = 3,           // The widget destroyed itself when it lost focus.
    kCancelButtonClicked = 4,  // The widget's cancel button was clicked.
    kAcceptButtonClicked = 5,  // The widget's done/accept button was clicked.
    kMaxValue = kAcceptButtonClicked
  };

  struct VIEWS_EXPORT InitParams {
    enum Type {
      TYPE_WINDOW,  // A decorated Window, like a frame window.
                    // Widgets of TYPE_WINDOW will have a NonClientView.
      TYPE_WINDOW_FRAMELESS,  // An undecorated Window.
      TYPE_CONTROL,           // A control, like a button.
      TYPE_POPUP,  // An undecorated Window, with transient properties.
      TYPE_MENU,   // An undecorated Window, with transient properties
                   // specialized to menus.
      TYPE_TOOLTIP,
      TYPE_BUBBLE,
      TYPE_DRAG,  // An undecorated Window, used during a drag-and-drop to
                  // show the drag image.
    };

    enum class WindowOpacity {
      // Infer fully opaque or not. For WinAura, top-level windows that are not
      // of TYPE_WINDOW are translucent so that they can be made to fade in.
      // For LinuxAura, only windows that are TYPE_DRAG are translucent.  In all
      // other cases, windows are fully opaque.
      kInferred,
      // Fully opaque.
      kOpaque,
      // Possibly translucent/transparent.  Widgets that fade in or out using
      // SetOpacity() but do not make use of an alpha channel should use
      // kInferred.
      kTranslucent,
    };

    enum class Activatable {
      // Infer whether the window should be activatable from the window type.
      kDefault,

      kYes,
      kNo
    };

    enum Ownership {
      // Default. Creator is not responsible for managing the lifetime of the
      // Widget, it is destroyed when the corresponding NativeWidget is
      // destroyed.
      NATIVE_WIDGET_OWNS_WIDGET,
      // Used when the Widget is owned by someone other than the NativeWidget,
      // e.g. a scoped_ptr in tests. Production use is discouraged because the
      // Widget API might become unsafe after the platform window is closed.
      WIDGET_OWNS_NATIVE_WIDGET,
      // Preferred Ownership mode. This is intended to be a safe replacement for
      // WIDGET_OWNS_NATIVE_WIDGET. The NativeWidget will be closed along with
      // the platform window.
      // The above "default" reflects the behavior of various platforms in which
      // the NativeWidget is effectively "owned" by the platform itself. It is
      // possible that the NativeWidget is destroyed at the behest of the plat-
      // form, leaving the associated Widget reference dangling.
      // Using this ownership mode allows for the Widget being resilient to the
      // NativeWidget being destroyed out from under the Widget while being
      // able to manage the Widget independently.
      CLIENT_OWNS_WIDGET
    };

    enum class ShadowType {
      kDefault,  // Use default shadow setting. It will be one of
                 // the settings below depending on InitParams::type
                 // and the native widget's type.
      kNone,     // Don't draw any shadow.
      kDrop,     // Draw a drop shadow that emphasizes Z-order
                 // relationship to other windows.
    };

    // TODO(crbug.com/339619005): Remove this constructor once call sites
    //                            have been migrated to always specifying
    //                            the ownership mode as well as the type.
    explicit InitParams(Type type);

    // The preferred constructor. Must specify the ownership mode. The ownership
    // mode will eventually go away and will implicitly be CLIENT_OWNS_WIDGET.
    // This is here for migration purposes.
    explicit InitParams(Ownership ownership, Type type = TYPE_WINDOW);

    InitParams(InitParams&& other);
    ~InitParams();

    InitParams& operator=(InitParams&& rhs) = default;

    // Returns the activatablity based on |activatable|, but also handles the
    // case where |activatable| is |kDefault|.
    bool CanActivate() const;

    // Returns the z-order level, based on the overriding |z_order| but also
    // taking into account special levels due to |type|.
    ui::ZOrderLevel EffectiveZOrderLevel() const;

    // Returns whether the widget should be initialized as headless by checking
    // if |headless_mode| or the associated top level widget's |is_headless_|
    // are set.
    bool ShouldInitAsHeadless() const;

    Type type;

    // If null, a default implementation will be constructed. The default
    // implementation deletes itself when the Widget closes.
    raw_ptr<WidgetDelegate> delegate = nullptr;

    // Internal name. Propagated to the NativeWidget. Useful for debugging.
    std::string name;

    // False if this widget behaves like a top-level widget, true otherwise. A
    // top-level widget has its own focus and IME state, independent of any
    // other widget. A widget for which child is true should have a parent; if
    // it doesn't, it will not handle keyboard events or IME input at all.
    // TODO(crbug.com/40120838): DCHECK(parent || !child)
    bool child = false;

    // If kTranslucent, the widget may be fully or partially transparent.
    // If kOpaque, we can perform optimizations based on the widget being fully
    // opaque. Default is based on ViewsDelegate::GetOpacityForInitParams().
    // Defaults to kOpaque for non-window widgets. Translucent windows may not
    // always be supported.
    WindowOpacity opacity = WindowOpacity::kInferred;

    bool accept_events = true;

    Activatable activatable = Activatable::kDefault;

    // The class of window and its overall z-order level. This level is visible
    // to other applications in the system. A value other than `kNormal` will
    // create an "always on top" widget.
    std::optional<ui::ZOrderLevel> z_order;

    // The z-order sublevel that is invisible to other applications in the
    // system. Widgets of the same `z_order` are stacked in the order specified
    // by their sub-levels.
    int sublevel = 0;

    bool visible_on_all_workspaces = false;

    // See Widget class comment above.
    Ownership ownership;

    ShadowType shadow_type = ShadowType::kDefault;

    // A hint about the size of the shadow if the type is ShadowType::kDrop. May
    // be ignored on some platforms. No value indicates no preference.
    std::optional<int> shadow_elevation;

    // The window corner radius. May be ignored on some platforms.
    std::optional<int> corner_radius;

    // Specifies that the system default caption and icon should not be
    // rendered, and that the client area should be equivalent to the window
    // area. Only used on some platforms (Windows and Linux).
    bool remove_standard_frame = false;

    // Whether the widget should be maximized or minimized.
    ui::mojom::WindowShowState show_state =
        ui::mojom::WindowShowState::kDefault;

    // The native *view* (not native *window*) to which this widget should be
    // parented. If this widget has a parent, then:
    // * If that parent closes, this widget is closed too
    // * If that parent is hidden, this widget is hidden too
    // * This widget is stacked above the parent widget (always on Mac, usually
    //   elsewhere)
    // * This widget's initial bounds are constrained to the parent widget's
    //   bounds, which prevents window restoration from placing windows
    //   offscreen
    // Note: on some platforms (Mac) this directly implies a parent-child
    // relationship in the backing native windows, but on Aura platforms it does
    // not necessarily.
    //
    // Windows with no parent window are permitted, although in Aura these
    // windows instead need a "context". On Aura systems, if a widget has no
    // parent set, its backing aura::Window is parented to the Aura root window.
    //
    // TODO(crbug.com/40120838): It makes no sense that this is a
    // NativeView instead of a NativeWindow. On Aura, NativeView and
    // NativeWindow are synonyms, and NativeWidgetAura immediately treats the
    // provided NativeView as an aura::Window; on Mac, the NativeView is
    // immediately converted to an NSWindow (ie a gfx::NativeWindow) and used
    // that way throughout. This should simply be a NativeWindow - windows are
    // parented to other windows, not to views, and it being a view confuses
    // the concept with bubble anchoring a la BubbleDialogDelegateView.
    gfx::NativeView parent = gfx::NativeView();

    // Specifies the initial bounds of the Widget. Default is empty, which means
    // the NativeWidget may specify a default size. If the parent is specified,
    // |bounds| is in the parent's coordinate system. If the parent is not
    // specified, it's in screen's global coordinate system.
    gfx::Rect bounds;

#if BUILDFLAG(IS_CHROMEOS)
    // If specified and the `bounds` is inside the specified display, the widget
    // will be created on this display. Otherwise, the display matching the
    // `bounds` will be used.
    std::optional<int64_t> display_id;
#endif

    // The initial workspace of the Widget. Default is "", which means the
    // current workspace.
    std::string workspace;

    // If set, this value is used as the Widget's NativeWidget implementation.
    // The Widget will not construct a default one.
    raw_ptr<NativeWidget> native_widget = nullptr;

    // Aura-only. Provides a DesktopWindowTreeHost implementation to use instead
    // of the default one.
    // TODO(beng): Figure out if there's a better way to expose this, e.g. get
    // rid of NW subclasses and do this all via message handling.
    raw_ptr<DesktopWindowTreeHost> desktop_window_tree_host = nullptr;

    // Only used by NativeWidgetAura. Specifies the type of layer for the
    // aura::Window.
    ui::LayerType layer_type = ui::LAYER_TEXTURED;

    // Only used by Aura. Provides a context window whose RootWindow is
    // consulted during widget creation to determine where in the Window
    // hierarchy this widget should be placed. (This is separate from |parent|;
    // if you pass a RootWindow to |parent|, your window will be parented to
    // |parent|. If you pass a RootWindow to |context|, we ask that RootWindow
    // where it wants your window placed.) Nullptr is not allowed on Windows and
    // Linux. Nullptr is allowed on Chrome OS, which will place the window on
    // the default desktop for new windows.
    gfx::NativeWindow context = gfx::NativeWindow();

    // If true, forces the window to be shown in the taskbar, even for window
    // types that do not appear in the taskbar by default (popup and bubble).
    bool force_show_in_taskbar = false;

    // Only used by X11, for root level windows. Specifies the res_name and
    // res_class fields, respectively, of the WM_CLASS window property. Controls
    // window grouping and desktop file matching in Linux window managers.
    std::string wm_role_name;
    std::string wm_class_name;
    std::string wm_class_class;

    // Only used by Wayland, for root level windows.
    std::string wayland_app_id;

    // If true then the widget uses software compositing.
    bool force_software_compositing = false;

    // If set, mouse events will be sent to the widget even if inactive.
    bool wants_mouse_events_when_inactive = false;

    // If set, the widget was created in headless mode.
    bool headless_mode = false;

    // If set, the window size will follow the content preferred size.
    bool autosize = false;

#if BUILDFLAG(IS_OZONE)
    // Specifies whether the Widget should use a desktop native widget.
    //   true: desktop native widget will always be used
    //   false: desktop native widget will never be used
    //   unspecified: depends on experiment kOzoneBubblesUsePlatformWidgets and
    //     whether the platform actually supports desktop native subwindows (see
    //     ozone_platform.h `supports_subwindows_as_accelerated_widgets`)
    // See crbug.com/1280332
    std::optional<bool> use_accelerated_widget_override;
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // TODO(crbug.com/1327490): Rename restore info variables.
    // Only used by Wayland. Specifies the session id window key, the restore
    // window id, and the app id, respectively, respectively, used by the
    // compositor to restore window state upon creation. Only one of
    // `restore_window_id` and `restore_window_id_source` should be set, as
    // `restore_window_id_source` is used for widgets without inherent restore
    // window ids, e.g. Chrome apps.
    int32_t restore_session_id = 0;
    std::optional<int32_t> restore_window_id;
    std::optional<std::string> restore_window_id_source;
#endif

    // Contains any properties with which the native widget should be
    // initialized prior to adding it to the window hierarchy. All the
    // properties in |init_properties_container| will be moved to the native
    // widget.
    ui::PropertyHandler init_properties_container;

#if BUILDFLAG(IS_OZONE)
    // Only used by Wayland for root level windows. Specifies whether this
    // window should request the wayland compositor to send key events,
    // even if it matches with the compositor's keyboard shortcuts.
    bool inhibit_keyboard_shortcuts = false;
#endif

    // Directly sets the NativeTheme used by the Widget. Providing the
    // NativeTheme here vs setting afterwards potentially avoids lots of
    // notifications of theme changes.
    // A value of null results in the default theme being used.
    raw_ptr<ui::NativeTheme> native_theme = nullptr;

#if BUILDFLAG(IS_MAC)
    // If set to true, tags the widget as an invisible overlay widget that
    // allows the Views tree to be broken up into distinct NSViews for use by
    // immersive fullscreen. Not for general use.
    bool is_overlay = false;
#endif
  };

  // Represents a lock held on the widget's ShouldPaintAsActive() state. As
  // long as at least one lock is held, the widget will paint as active.
  // Multiple locks can exist for the same widget, and a lock can outlive its
  // associated widget. See Widget::LockPaintAsActive().
  class PaintAsActiveLock {
   public:
    PaintAsActiveLock(const PaintAsActiveLock&) = delete;
    PaintAsActiveLock& operator=(const PaintAsActiveLock&) = delete;

    virtual ~PaintAsActiveLock();

   protected:
    PaintAsActiveLock();
  };

  Widget();
  explicit Widget(InitParams params);

  Widget(const Widget&) = delete;
  Widget& operator=(const Widget&) = delete;

  ~Widget() override;

  // Creates a decorated window Widget with the specified properties. The
  // returned Widget is owned by its NativeWidget; see Widget class comment for
  // details.
  // The std::unique_ptr variant requires that delegate->owned_by_widget().
  static Widget* CreateWindowWithParent(WidgetDelegate* delegate,
                                        gfx::NativeView parent,
                                        const gfx::Rect& bounds = gfx::Rect());
  static Widget* CreateWindowWithParent(
      std::unique_ptr<WidgetDelegate> delegate,
      gfx::NativeView parent,
      const gfx::Rect& bounds = gfx::Rect());

  // Creates a decorated window Widget in the same desktop context as |context|.
  // The returned Widget is owned by its NativeWidget; see Widget class comment
  // for details.
  // The std::unique_ptr variant requires that delegate->owned_by_widget().
  static Widget* CreateWindowWithContext(WidgetDelegate* delegate,
                                         gfx::NativeWindow context,
                                         const gfx::Rect& bounds = gfx::Rect());
  static Widget* CreateWindowWithContext(
      std::unique_ptr<WidgetDelegate> delegate,
      gfx::NativeWindow context,
      const gfx::Rect& bounds = gfx::Rect());

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
  // TODO(tluk): This API should be updated to return Widgets rather than take
  // an out param.
  static void GetAllChildWidgets(gfx::NativeView native_view,
                                 Widgets* children);

  // Returns all Widgets owned by |native_view| (including child widgets, but
  // not including itself).
  // TODO(tluk): This API should be updated to return Widgets rather than take
  // an out param.
  static void GetAllOwnedWidgets(gfx::NativeView native_view, Widgets* owned);

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

  static bool IsWindowCompositingSupported();

  // Initializes the widget, and in turn, the native widget. |params| should be
  // moved to Init() by the caller.
  void Init(InitParams params);

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
  void ViewHierarchyChanged(const ViewHierarchyChangedDetails& details);

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
  //
  // If you want to get the absolute primary application window, accounting for
  // e.g. bubble and menu anchoring, use GetPrimaryWindowWidget() instead.
  Widget* GetTopLevelWidget();
  const Widget* GetTopLevelWidget() const;

  // Returns the widget of the primary window this widget is associated with,
  // such as an application window, accounting for anchoring and other
  // relationships not accounted for in GetTopLevelWidget().
  //
  // Equivalent to GetTopLevelWidget() by default; override in derived classes
  // that require additional logic.
  virtual Widget* GetPrimaryWindowWidget();
  const Widget* GetPrimaryWindowWidget() const;

  // Gets/Sets the WidgetDelegate.
  WidgetDelegate* widget_delegate() const { return widget_delegate_.get(); }

  // Sets the specified view as the contents of this Widget. There can only
  // be one contents view child of this Widget's RootView. This view is sized to
  // fit the entire size of the RootView. The RootView takes ownership of this
  // View, unless it is passed in as a raw pointer and set as not being
  // parent-owned. Prefer using SetContentsView(std::unique_ptr) over passing a
  // raw pointer for new code.
  template <typename T>
  T* SetContentsView(std::unique_ptr<T> view) {
    DCHECK(!view->owned_by_client())
        << "This should only be called if the client is passing over the "
           "ownership of |view|.";
    T* raw_pointer = view.get();
    SetContentsView(view.release());
    return raw_pointer;
  }
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

  // Retrieves the current workspace for the window. (On macOS: an opaque
  // binary blob that encodes the workspace and other window state. On ChromeOS,
  // this returns empty string if this widget is a window that appears on all
  // desks.)
  std::string GetWorkspace() const;

  // Sizes and/or places the widget to the specified bounds, size or position.
  // `bounds` is in screen coordinates.
  void SetBounds(const gfx::Rect& bounds);
  void SetSize(const gfx::Size& size);

  // Retrieves the restored size for the window.
  gfx::Size GetSize() const;

  // Returns the insets that each widget implementation can customize. It
  // returns empty insets by default.
  virtual gfx::Insets GetCustomInsetsInDIP() const;

  // Sizes the window to the specified size and centers it.
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

  // Whether calling RunMoveLoop() is supported for the widget.
  bool IsMoveLoopSupported() const;

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

  // Returns true if widget is above the specified window in z-order.
  bool IsStackedAbove(gfx::NativeView native_view);

  // Sets a shape on the widget. Passing a NULL |shape| reverts the widget to
  // be rectangular.
  void SetShape(std::unique_ptr<ShapeRects> shape);

  // Equivalent to CloseWithReason(ClosedReason::kUnspecified).
  // DEPRECATED: Please use CloseWithReason() instead.
  void Close();

  // Hides the widget, then closes it after a return to the message loop,
  // specifying the reason for it having been closed.
  // Note that while you can pass ClosedReason::kUnspecified, it is highly
  // discouraged and only supported for backwards-compatibility with Close().
  void CloseWithReason(ClosedReason closed_reason);

  // A UI test which tries to asynchronously examine a widget (e.g. the pixel
  // tests) will fail if the widget is closed before that.  This can happen
  // easily with widgets that close on focus loss coupled with tests being run
  // in parallel, since one test's widget can be closed by the appearance of
  // another test's.  This method can be used to temporarily disable
  // Widget::Close() for such asynchronous cases.
  void SetBlockCloseForTesting(bool block_close) { block_close_ = block_close; }

  // TODO(beng): Move off public API.
  // Closes the widget immediately. Compare to |Close|. This will destroy the
  // window handle associated with this Widget, so should not be called from
  // any code that expects it to be valid beyond this call.
  void CloseNow();

  // Whether the widget has been asked to close itself. In particular this is
  // set to true after Close() has been invoked on the NativeWidget.
  bool IsClosed() const;

  // Returns the reason the widget was closed, if it was specified.
  ClosedReason closed_reason() const { return closed_reason_; }

  // Shows the widget. The widget is activated if during initialization the
  // can_activate flag in the InitParams structure is set to true.
  void Show();

  // Hides the widget.
  void Hide();

  // Like Show(), but does not activate the window. Tests may be flaky on Mac:
  // Mac browsertests do not have an activation policy so the widget may be
  // activated.
  void ShowInactive();

  // Activates the widget, assuming it already exists and is visible.
  void Activate();

  // Deactivates the widget, making the next window in the Z order the active
  // window.
  void Deactivate();

  // Returns whether the Widget is the currently active window.
  virtual bool IsActive() const;

  // Returns true if views style follows the widget activation state. The
  // default implementation simply calls CanActivate().
  virtual bool ShouldViewsStyleFollowWidgetActivation() const;

  // Sets the z-order of the widget. This only applies to top-level widgets.
  void SetZOrderLevel(ui::ZOrderLevel order);

  // Gets the z-order of the widget. This only applies to top-level widgets.
  ui::ZOrderLevel GetZOrderLevel() const;

  // Sets the z-order sublevel of the widget. This applies to both top-level
  // and non top-level widgets.
  void SetZOrderSublevel(int sublevel);

  // Gets the z-order sublevel of the widget. This applies to both top-level
  // and non top-level widgets.
  int GetZOrderSublevel() const;

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
  // The `target_display_id` may only be specified if `fullscreen` is true, and
  // indicates a specific display to become fullscreen on (note that this may
  // move a fullscreen widget from one display to another).
  void SetFullscreen(bool fullscreen,
                     int64_t target_display_id = display::kInvalidDisplayId);
  bool IsFullscreen() const;

  // macOS: Sets whether the window can share fullscreen windows' spaces.
  void SetCanAppearInExistingFullscreenSpaces(
      bool can_appear_in_existing_fullscreen_spaces);

  // Sets the opacity of the widget. This may allow widgets behind the widget
  // in the Z-order to become visible, depending on the capabilities of the
  // underlying windowing system.
  void SetOpacity(float opacity);

  // Sets the aspect ratio of the widget's client view, which will be maintained
  // during interactive resizing.  Note that for widgets that have a client view
  // that is framed by custom-drawn borders / window frame / etc, the widget
  // size will be chosen so that the aspect ratio of client view, not the entire
  // widget, will be `aspect_ratio`.
  //
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

  // Returns a custom theme object suitable for use in a
  // ColorProviderKey. If this is null, the window has no custom theme.
  virtual ui::ColorProviderKey::ThemeInitializerSupplier* GetCustomTheme()
      const;

  ui::NativeTheme* GetNativeTheme() {
    return const_cast<ui::NativeTheme*>(
        static_cast<const Widget*>(this)->GetNativeTheme());
  }
  virtual const ui::NativeTheme* GetNativeTheme() const;

  // Returns the FocusManager for this widget.
  // Note that all widgets in a widget hierarchy share the same focus manager.
  FocusManager* GetFocusManager();
  const FocusManager* GetFocusManager() const;

  // Returns the ui::InputMethod for this widget.
  ui::InputMethod* GetInputMethod();

  // Returns the SublevelManager for this widget.
  SublevelManager* GetSublevelManager();

  // Starts a drag operation for the specified view. This blocks until the drag
  // operation completes or is cancelled by calling `CancelShellDrag()`.
  // |view| can be NULL.
  // If the view is non-NULL it can be accessed during the drag by calling
  // dragged_view(). If the view has not been deleted during the drag,
  // OnDragDone() is called on it. |location| is in the widget's coordinate
  // system. |view| must be hosted by this widget.
  void RunShellDrag(View* view,
                    std::unique_ptr<ui::OSExchangeData> data,
                    const gfx::Point& location,
                    int operation,
                    ui::mojom::DragEventSource source);

  // Cancels a currently running drag operation for the specified view. |view|
  // can be NULL.
  void CancelShellDrag(View* view);

  // Returns the view that requested the current drag operation via
  // RunShellDrag(), or NULL if there is no such view or drag operation.
  View* dragged_view() {
    return const_cast<View*>(const_cast<const Widget*>(this)->dragged_view());
  }
  const View* dragged_view() const { return dragged_view_; }

  // Adds the specified |rect| in client area coordinates to the rectangle to be
  // redrawn.
  virtual void SchedulePaintInRect(const gfx::Rect& rect);

  // Schedule a layout to occur. This is called by RootView, client code should
  // not need to call this.
  void ScheduleLayout();

  // Called when the contents view receives an `InvalidateLayout()`.
  void OnRootViewLayoutInvalidated();

  // Sets the currently visible cursor.
  void SetCursor(const ui::Cursor& cursor);

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

  // Shows the platform specific emoji picker for this widget.
  void ShowEmojiPanel();

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
  virtual std::unique_ptr<NonClientFrameView> CreateNonClientFrameView();

  // Whether we should be using a native frame.
  bool ShouldUseNativeFrame() const;

  // Determines whether the window contents should be rendered transparently
  // (for example, so that they can overhang onto the window title bar).
  bool ShouldWindowContentsBeTransparent() const;

  // Tell the window that something caused the frame type to change.
  void FrameTypeChanged();

  NonClientView* non_client_view() {
    return const_cast<NonClientView*>(
        const_cast<const Widget*>(this)->non_client_view());
  }
  const NonClientView* non_client_view() const { return non_client_view_; }

  ClientView* client_view() {
    return const_cast<ClientView*>(
        const_cast<const Widget*>(this)->client_view());
  }
  const ClientView* client_view() const {
    // non_client_view_ may be NULL, especially during creation.
    return non_client_view_ ? non_client_view_->client_view() : nullptr;
  }

  // Returns the compositor for this Widget, note that this may change during
  // the Widget's lifetime (e.g. when switching monitors on Chrome OS).
  ui::Compositor* GetCompositor() {
    return const_cast<ui::Compositor*>(
        const_cast<const Widget*>(this)->GetCompositor());
  }
  const ui::Compositor* GetCompositor() const;

  // Returns the widget's layer, if any.
  ui::Layer* GetLayer() {
    return const_cast<ui::Layer*>(const_cast<const Widget*>(this)->GetLayer());
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
    return native_widget_.get();
  }

  const internal::NativeWidgetPrivate* native_widget_private() const {
    return native_widget_.get();
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

  // Returns the parent of this widget. Note that
  // * A top-level widget is not necessarily the root and may have a parent.
  // * A child widget shares the same visual style, e.g. the dark/light theme,
  //   with its parent.
  // * The native widget may change a widget's parent.
  // * The native view's parent might or might not be the parent's native view.
  // * For a desktop widget with a non-desktop parent, this value might be
  //   nullptr during shutdown.
  Widget* parent() { return parent_.get(); }
  const Widget* parent() const { return parent_.get(); }

  // True if the widget is considered top level widget. Top level widget
  // is a widget of TYPE_WINDOW, TYPE_PANEL, TYPE_WINDOW_FRAMELESS, BUBBLE,
  // POPUP or MENU, and has a focus manager and input method object associated
  // with it. TYPE_CONTROL and TYPE_TOOLTIP is not considered top level.
  bool is_top_level() const { return is_top_level_; }

  // True if widget was created in headless mode.
  bool is_headless() const { return is_headless_; }

  // True if the window size will follow the content preferred size.
  bool is_autosized() const { return is_autosized_; }

  // True when window movement via mouse interaction with the frame is disabled.
  bool movement_disabled() const { return movement_disabled_; }
  void set_movement_disabled(bool disabled) { movement_disabled_ = disabled; }

  // Returns the work area bounds of the screen the Widget belongs to.
  gfx::Rect GetWorkAreaBoundsInScreen() const;

  // Creates and dispatches synthesized mouse move event using the current
  // mouse location to refresh hovering status in the widget.
  void SynthesizeMouseMoveEvent();

  // Returns the gesture recognizer which can handle touch/gesture events on
  // this.
  ui::GestureRecognizer* GetGestureRecognizer();

  // Returns the associated gesture consumer.
  ui::GestureConsumer* GetGestureConsumer();

  // Called when the delegate's CanResize or CanMaximize changes.
  void OnSizeConstraintsChanged();

  // Notification that our owner is closing.
  // NOTE: this is not invoked for aura as it's currently not needed there.
  // Under aura menus close by way of activation getting reset when the owner
  // closes.
  virtual void OnOwnerClosing();

  // Returns the internal name for this Widget and NativeWidget.
  std::string GetName() const;

  // Registers |callback| to be called whenever the "paint as active" state
  // changes.
  base::CallbackListSubscription RegisterPaintAsActiveChangedCallback(
      PaintAsActiveCallbackList::CallbackType callback);

  // Prevents the widget from being rendered as inactive during the lifetime of
  // the returned lock. Multiple locks can exist with disjoint lifetimes. The
  // returned lock can safely outlive the associated widget.
  std::unique_ptr<PaintAsActiveLock> LockPaintAsActive();

  // Undoes LockPaintAsActive(). This should never be called outside of
  // PaintAsActiveLock destructor.
  void UnlockPaintAsActive();

  // Returns true if the window should paint as active.
  bool ShouldPaintAsActive() const;

  // Called when the ShouldPaintAsActive() of parent changes.
  void OnParentShouldPaintAsActiveChanged();

  // Notifies registered callbacks and the native widget of changes to
  // the ShouldPaintAsActive() state.
  void NotifyPaintAsActiveChanged();

  base::WeakPtr<Widget> GetWeakPtr();

  // Overridden from NativeWidgetDelegate:
  bool IsModal() const override;
  bool IsDialogBox() const override;
  bool CanActivate() const override;
  bool IsNativeWidgetInitialized() const override;
  bool OnNativeWidgetActivationChanged(bool active) override;
  bool ShouldHandleNativeWidgetActivationChanged(bool active) override;
  void OnNativeFocus() override;
  void OnNativeBlur() override;
  void OnNativeWidgetVisibilityChanged(bool visible) override;
  void OnNativeWidgetCreated() override;
  void OnNativeWidgetDestroying() override;
  void OnNativeWidgetDestroyed() override;
  void OnNativeWidgetParentChanged(gfx::NativeView parent) override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnNativeWidgetMove() override;
  void OnNativeWidgetSizeChanged(const gfx::Size& new_size) override;
  void OnNativeWidgetWorkspaceChanged() override;
  void OnNativeWidgetWindowShowStateChanged() override;
  void OnNativeWidgetBeginUserBoundsChange() override;
  void OnNativeWidgetEndUserBoundsChange() override;
  void OnNativeWidgetAddedToCompositor() override;
  void OnNativeWidgetRemovingFromCompositor() override;
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
  void GetHitTestMask(SkPath* mask) const override;
  Widget* AsWidget() override;
  const Widget* AsWidget() const override;
  bool SetInitialFocus(ui::mojom::WindowShowState show_state) override;
  bool ShouldDescendIntoChildForEventHandling(
      ui::Layer* root_layer,
      gfx::NativeView child,
      ui::Layer* child_layer,
      const gfx::Point& location) override;
  void LayoutRootViewIfNecessary() override;

  // Overridden from ui::EventSource:
  ui::EventSink* GetEventSink() override;

  // Overridden from FocusTraversable:
  FocusSearch* GetFocusSearch() override;
  FocusTraversable* GetFocusTraversableParent() override;
  View* GetFocusTraversableParentView() override;

  // Overridden from ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

  // Sets an override for `color_mode` when `GetColorProvider()` is requested.
  // e.g. if set to kDark, colors will always be for the dark theme.
  void SetColorModeOverride(
      std::optional<ui::ColorProviderKey::ColorMode> color_mode);

  // ui::ColorProviderSource:
  const ui::ColorProvider* GetColorProvider() const override;
  ui::RendererColorMap GetRendererColorMap(
      ui::ColorProviderKey::ColorMode color_mode,
      ui::ColorProviderKey::ForcedColors forced_colors) const override;

  // Set the native theme from which this widget gets color from for testing.
  void SetNativeThemeForTest(ui::NativeTheme* native_theme) {
    SetNativeTheme(native_theme);
    native_theme_set_for_testing_ = true;
  }

  ui::ColorProviderKey GetColorProviderKeyForTesting() const;

  // Causes IsFullscreen() to also check parent state, since this widget is
  // logically part of the same window as the parent.
  void SetCheckParentForFullscreen();

  // Returns the current ownership model of the widget.
  InitParams::Ownership ownership() const { return ownership_; }

  bool native_widget_active() const { return native_widget_active_; }

  // Called to enable or disable screenshots of this widget.
  void SetAllowScreenshots(bool allow);
  bool AreScreenshotsAllowed();

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

  // Set the native theme from which this widget gets color from.
  void SetNativeTheme(ui::NativeTheme* native_theme);

  // The following methods are used by the property access system described in
  // the comments on views::View. They follow the required naming convention in
  // order to allow them to be visible via the metadata.
  // TODO(kylixrd): Refactor code to use these methods directly.
  int GetX() const;
  int GetY() const;
  int GetWidth() const;
  int GetHeight() const;
  bool GetVisible() const;
  void SetX(int x);
  void SetY(int y);
  void SetWidth(int width);
  void SetHeight(int height);
  void SetVisible(bool visible);

  // ui::ColorProviderSource:
  ui::ColorProviderKey GetColorProviderKey() const override;

 private:
  // Type of ways to ignore activation changes.
  enum class DisableActivationChangeHandlingType {
    kNone = 0,  // Don't ignore any activation changes.
    kIgnore,    // Ignore both activation and deactivation changes.
    kIgnoreDeactivationOnly,  // Ignore only deactivation changes.
  };

  class PaintAsActiveLockImpl;

  friend class ButtonTest;
  friend class ComboboxTest;
  friend class PaintAsActiveLockImpl;
  friend class TextfieldTest;
  friend class ViewAuraTest;
  friend class ui_devtools::BubbleLocking;
  friend void DisableActivationChangeHandlingForTests();

  // Sets/gets the type of disabling widget activation change handling.
  static void SetDisableActivationChangeHandling(
      DisableActivationChangeHandlingType new_type) {
    g_disable_activation_change_handling_ = new_type;
  }
  static DisableActivationChangeHandlingType
  GetDisableActivationChangeHandling() {
    return g_disable_activation_change_handling_;
  }

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

  // Set the parent of this widget.
  void SetParent(Widget* parent);

  // Returns the bounds and "show" state from the delegate. Returns true if
  // the delegate wants to use a specified bounds.
  bool GetSavedWindowPlacement(gfx::Rect* bounds,
                               ui::mojom::WindowShowState* show_state);

  // Returns the Views whose layers are parented directly to the Widget's
  // layer.
  const View::Views& GetViewsWithLayers();

  // If a descendent of |root_view_| is focused, then clear the focus.
  void ClearFocusFromWidget();

  // This holds logic that needs to called synchronously after showing, before
  // the native widget asynchronously invokes OnNativeWidgetVisibilityChanged().
  void HandleShowRequested();

  // This holds the logic for handling of the destroying and destroyed
  // notifications.
  void HandleWidgetDestroying();
  void HandleWidgetDestroyed();

  static DisableActivationChangeHandlingType
      g_disable_activation_change_handling_;

  base::WeakPtr<internal::NativeWidgetPrivate> native_widget_ = nullptr;

  // This unique pointer is only set when WIDGET_OWNS_NATIVE_WIDGET so that we
  // can destroy the NativeWidget. Except for managing lifetime for
  // WIDGET_OWNS_NATIVE_WIDGET, the NativeWidget should always be referenced
  // through the |native_widget_| weak ptr.
  std::unique_ptr<internal::NativeWidgetPrivate> owned_native_widget_;

  base::ObserverList<WidgetObserver> observers_;

  base::ObserverList<WidgetRemovalsObserver>::Unchecked removals_observers_;

  // Weak pointer to the Widget's delegate. If a NULL delegate is supplied
  // to Init() a default WidgetDelegate is created.
  base::WeakPtr<WidgetDelegate> widget_delegate_;

  // The parent of this widget. This is the widget that associates with
  // the |params.parent| supplied to Init(). If no parent is given or the native
  // view parent has no associating Widget, this value will be nullptr.
  // For a desktop widget with a non-desktop parent, this value might be nullptr
  // during shutdown.
  base::WeakPtr<Widget> parent_ = nullptr;

  // This boolean is true when the widget tree is being traversed for
  // WidgetObserver event broadcasting. It is used to CHECK that the widget tree
  // is not modified during the traversal.
  bool is_traversing_widget_tree_ = false;

  // The root of the View hierarchy attached to this window.
  // WARNING: see warning in tooltip_manager_ for ordering dependencies with
  // this and tooltip_manager_.
  std::unique_ptr<internal::RootView> root_view_;

  // The View that provides the non-client area of the window (title bar,
  // window controls, sizing borders etc). To use an implementation other than
  // the default, this class must be sub-classed and this value set to the
  // desired implementation before calling |InitWindow()|.
  raw_ptr<NonClientView> non_client_view_ = nullptr;

  // The focus manager keeping track of focus for this Widget and any of its
  // children.  NULL for non top-level widgets.
  // WARNING: RootView's destructor calls into the FocusManager. As such, this
  // must be destroyed AFTER root_view_. This is enforced in DestroyRootView().
  std::unique_ptr<FocusManager> focus_manager_;

  // The sublevel manager that ensures that the children are stacked in the
  // order specified by their InitParams::sublevel.
  std::unique_ptr<SublevelManager> sublevel_manager_;

  // Valid for the lifetime of RunShellDrag(), indicates the view the drag
  // started from.
  raw_ptr<View> dragged_view_ = nullptr;

  // See class documentation for Widget above for a note about ownership.
  InitParams::Ownership ownership_ = InitParams::NATIVE_WIDGET_OWNS_WIDGET;

  // See set_is_secondary_widget().
  bool is_secondary_widget_ = true;

  // If set, overrides this value is used instead of the one from NativeTheme
  // when constructing a ColorProvider.
  std::optional<ui::ColorProviderKey::ColorMode> color_mode_override_;

  // The current frame type in use by this window. Defaults to
  // FrameType::kDefault.
  FrameType frame_type_ = FrameType::kDefault;

  // Tracks whether the native widget is active.
  bool native_widget_active_ = false;

  // Count of paint-as-active locks on this widget. See LockPaintAsActive().
  size_t paint_as_active_refcount_ = 0;

  // Callbacks to notify when the ShouldPaintAsActive() changes.
  PaintAsActiveCallbackList paint_as_active_callbacks_;

  // Lock on the parent widget when this widget is active.
  // When this widget is destroyed, the lock is automatically released.
  std::unique_ptr<PaintAsActiveLock> parent_paint_as_active_lock_;

  // Subscription to parent's ShouldPaintAsActive() change.
  base::CallbackListSubscription parent_paint_as_active_subscription_;

  // Set to true if the widget is in the process of closing.
  bool widget_closed_ = false;

  // Set to true after OnWidgetDestroyed called.
  bool native_widget_destroyed_ = false;

  // The reason the widget was closed.
  // Note that this may be ClosedReason::kUnspecified if the deprecated Close()
  // method was called rather than CloseWithReason().
  ClosedReason closed_reason_ = ClosedReason::kUnspecified;

  // The saved "show" state for this window. See note in SetInitialBounds
  // that explains why we save this.
  ui::mojom::WindowShowState saved_show_state_ =
      ui::mojom::WindowShowState::kDefault;

  // The restored bounds used for the initial show. This is only used if
  // |saved_show_state_| is maximized. initial_restored_bounds_ is in DIP units
  // and is converted to pixels in DesktopWindowTreeHostWin::Show.
  gfx::Rect initial_restored_bounds_;

  // Focus is automatically set to the view provided by the delegate
  // when the widget is shown. Set this value to false to override
  // initial focus for the widget.
  bool focus_on_creation_ = true;

  // See |is_top_level()| accessor.
  bool is_top_level_ = false;

  // Tracks whether native widget has been initialized.
  bool native_widget_initialized_ = false;

  // TODO(beng): Remove NativeWidgetGtk's dependence on these:
  // If true, the mouse is currently down.
  bool is_mouse_button_pressed_ = false;

  // If set, the widget was created in headless mode.
  bool is_headless_ = false;

  // If set, the window size will follow the content preferred size.
  bool is_autosized_ = false;

  // True if capture losses should be ignored.
  bool ignore_capture_loss_ = false;

  // TODO(beng): Remove NativeWidgetGtk's dependence on these:
  // The following are used to detect duplicate mouse move events and not
  // deliver them. Displaying a window may result in the system generating
  // duplicate move events even though the mouse hasn't moved.
  bool last_mouse_event_was_move_ = false;
  gfx::Point last_mouse_event_position_;

  // True if event capture should be released on a mouse up event. Default is
  // true.
  bool auto_release_capture_ = true;

  // See description in GetViewsWithLayers().
  View::Views views_with_layers_;

  // Does |views_with_layers_| need updating?
  bool views_with_layers_dirty_ = false;

  // True when window movement via mouse interaction with the frame should be
  // disabled.
  bool movement_disabled_ = false;

  // Block the widget from closing.
  bool block_close_ = false;

  // The native theme this widget is using.
  // If nullptr, defaults to use the regular native theme.
  raw_ptr<ui::NativeTheme> native_theme_ = nullptr;

  // A flag that prevents the widget from updating its instance of
  // `native_theme_`. This is necessary during testing as theme updates may
  // trigger a reset of the explicitly set test theme.
  bool native_theme_set_for_testing_ = false;

  // By default, widgets are assumed to correspond to windows. If a parent
  // widget is fullscreen, then the child widget is a popup which is not
  // fullscreen. However, on macOS some child widgets logically correspond to
  // the same window. Their fullscreen state should inherit from their parents.
  bool check_parent_for_fullscreen_ = false;

  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      native_theme_observation_{this};

  base::WeakPtrFactory<Widget> weak_ptr_factory_{this};
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_WIDGET_H_
