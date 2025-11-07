// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_WIDGET_DELEGATE_H_
#define UI_VIEWS_WIDGET_WIDGET_DELEGATE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

class AutoPipSettingView;
class DesktopMediaPickerDialogView;
class DigitalIdentityMultiStepDialogDelegate;
class ExtensionsMenuCoordinator;
class ExternalProtocolNoHandlersTelSchemeDialog;
class ForceInstalledDeprecatedAppsDialogView;
class ForceInstalledPreinstalledDeprecatedAppDialogView;
class HoverDetectionBubbleView;
class ImmersiveModeControllerMacInteractiveTest;
class JavaScriptTabModalDialogViewViews;
class LoginHandlerViewsDialog;
class MediaGalleriesDialogTest;
class MediaGalleriesDialogViews;
class MessageBoxDialog;
class OverlayWindowWidgetDelegate;
class ParentAccessView;
class PresentationReceiverWindowView;
class PrivacySandboxDialog;
class ProcessSharingInfobarDelegate;
class ProfilePickerView;
class SSLClientCertificateSelector;
class ScheduledRebootDialog;
class ScreenCaptureNotificationUIViews;
class SearchEngineChoiceDialog;
class ShareThisTabDialogView;
class SigninViewControllerDelegateViews;
class TabModalConfirmDialogViews;
class TestBaseWidgetDelegate;
class UpdateRecommendedMessageBox;
class WebDialogBrowserTest;
FORWARD_DECLARE_TEST(AcceleratorCommandsFullscreenBrowserTest,
                     ToggleFullscreen);
FORWARD_DECLARE_TEST(TabStripScrollContainerTest, AnchoredWidgetHidesOnScroll);

#if !BUILDFLAG(IS_CHROMEOS)
class DownloadBubbleContentsViewTest;
class DownloadBubbleSecurityViewTest;
class DownloadToolbarUIController;
#endif

namespace arc {
class ArcTaskWindowBuilder;
class ArcVmDataMigrationConfirmationDialog;
}  // namespace arc

namespace ash {
class AccessibilityPanel;
class ActiveSessionAuthControllerImpl;
class AmbientWidgetDelegate;
class AppListView;
class AppsCollectionsDismissDialog;
class AssistantWebContainerView;
class AuthDialogContentsViewPixelTest;
class AuthDialogContentsViewTest;
class BootingAnimationController;
class CaptureModeSessionFocusCycler;
class ClientControlledStateTestWidgetDelegate;
class ConnectionErrorDialogDelegateView;
class DeferredUpdateDialog;
class DeskButtonWidgetDelegateView;
class DisclaimerView;
class DockedMagnifierTest;
class DropSenderView;
class DropTargetView;
class ExitWarningWidgetDelegateView;
class FrameCaptionButtonContainerViewTest;
class FrameSizeButtonTestWidgetDelegate;
class GuestSessionConfirmationDialog;
class HotseatWidgetDelegateView;
class IdleAppNameNotificationDelegateView;
class InSessionAuthDialog;
class InSessionAuthDialogControllerImpl;
class InformedRestoreController;
class KioskExternalUpdateNotificationView;
class LayoutWidgetDelegateView;
class LocalAuthenticationWithPinControllerImpl;
class LockDebugView;
class LockDebugViewDataDispatcherTransformer;
class LockScreen;
class LoginShelfWidgetDelegate;
class LoginTestBase;
class LoginTestWidgetDelegate;
class MahiPanelWidget;
class MaximizeDelegateView;
class FrameViewAshTestWidgetDelegate;
class PipTest;
class QuickInsertSubmenuView;
class QuickInsertView;
class ResizableWidgetDelegate;
class RootWindowControllerTest;
class ShelfNavigationWidgetDelegate;
class ShelfWidgetDelegateView;
class ShellTest;
class SimpleWebViewDialog;
class StatusAreaWidgetDelegate;
class StuckWidgetDelegate;
class SystemDialogDelegateView;
class SystemUIComponentsStyleViewerView;
class TestChildModalParent;
class TestTextInputView;
class TestWidgetBuilderDelegate;
class TestWidgetDelegateAsh;
class TestWindow;
class VirtualTrackpadView;
class WallpaperWidgetDelegate;
class WideFrameView;
class WidgetWithSystemUIComponentView;
class WindowCycleView;
FORWARD_DECLARE_TEST(ExtendedDesktopTest, SystemModal);
FORWARD_DECLARE_TEST(MruWindowTrackerOrderTest, Basic);
FORWARD_DECLARE_TEST(ShellTest, CreateWindowWithPreferredSize);

namespace hud_display {
class HUDDisplayView;
}

namespace sharesheet {
class TestWidgetDelegate;
}

namespace shell {
class ToplevelWindow;
}
}  // namespace ash

namespace autofill {
class PopupBaseView;

namespace payments {
class PaymentsWindowUserConsentDialogView;
}
}  // namespace autofill

namespace constrained_window {
class BrowserModalHelper;
}

namespace content {
class ShellPlatformDelegate;
}

namespace crostini {
class AppRestartDialog;
}

namespace exo {
class ShellSurfaceBase;
}

namespace extensions {
class WebFileHandlersPermissionHandler;
}

namespace eye_dropper {
class EyeDropperView;
}

namespace gfx {
class Rect;
}

namespace glic {
class GlicWidgetDelegate;
}

namespace javascript_dialogs {
class AppModalDialogViewViews;
}

namespace message_center {
class MessagePopupView;
}

namespace native_app_window {
class NativeAppWindowViews;
}

namespace plus_addresses {
class PlusAddressCreationDialogDelegate;
}

namespace remoting {
class MessageBoxCore;
}

namespace ui::ime {
class CandidateViewTest;
}

namespace views {
class BubbleDialogDelegate;
class BubbleDialogModelHost;
class ClientView;
class DefaultWidgetDelegate;
class DialogDelegate;
class FocusTraversalTest;
class MoveTestWidgetDelegate;
class FrameView;
class ShapedWidgetDelegate;
class TableViewFocusTest;
class View;
class WebDialogView;
FORWARD_DECLARE_TEST(BubbleUmaLoggerTest, LogMetricFromDelegate);
FORWARD_DECLARE_TEST(FocusManagerTest, AdvanceFocusStaysInWidget);
FORWARD_DECLARE_TEST(NativeWidgetAuraTest, TestPropertiesWhenAddedToLayout);
FORWARD_DECLARE_TEST(NativeWidgetAuraTest, TransientChildModalWindowVisibility);
FORWARD_DECLARE_TEST(NativeViewHostAuraTest,
                     FocusManagerUpdatedDuringDestruction);

namespace borealis {
class BorealisDisallowedDialog;
class BorealisLaunchErrorDialog;
}  // namespace borealis

namespace examples {
class ExamplesWindowContents;
}

namespace test {
FORWARD_DECLARE_TEST(DesktopWidgetTest, LockPaintAsActiveAndCloseParent);
}
}  // namespace views

namespace web_app {
class IsolatedWebAppInstallerViewController;
class SubAppsInstallDialogController;
}  // namespace web_app

namespace webid {
class TestAccountSelectionView;
}  // namespace webid

namespace views {

namespace test {
class GetNativeThemeFromDestructorView;
class TestingWidgetDelegateView;
FORWARD_DECLARE_TEST(WidgetOwnsNativeWidgetTest, WidgetDelegateView);
}  // namespace test

using TitleChangedCallback = base::RepeatingCallback<void()>;
using AccessibleTitleChangedCallback = base::RepeatingCallback<void()>;

// Handles events on Widgets in context-specific ways.
class VIEWS_EXPORT WidgetDelegate {
 public:
  using ClientViewFactory =
      base::OnceCallback<std::unique_ptr<ClientView>(Widget*,
                                                     /*contents_view=*/View*)>;
  using OverlayViewFactory = base::OnceCallback<std::unique_ptr<View>()>;

  // FrameViewFactory is a RepeatingCallback because the
  // FrameView is rebuilt on Aura platforms when WindowTreeHost
  // properties that might affect its appearance change. Rebuilding the entire
  // FrameView is a pretty big hammer for that but it's the one we
  // have.
  // TODO(b:387350163): Investigate if FrameView can handle these
  // changes in a more granular way.
  using FrameViewFactory =
      base::RepeatingCallback<std::unique_ptr<FrameView>(Widget*)>;

  struct Params {
    Params();
    ~Params();

    // The window's role. Useful values include kWindow (a plain window),
    // kDialog (a dialog), and kAlertDialog (a high-priority dialog whose body
    // is read when it appears). Using a role outside this set is not likely to
    // work across platforms.
    ax::mojom::Role accessible_role = ax::mojom::Role::kWindow;

    // The accessible title for the window, often more descriptive than the
    // plain title. If no accessible title is present the result of
    // GetWindowTitle() will be used.
    std::u16string accessible_title;

    // Whether the window should display controls for the user to minimize,
    // maximize, resize it, or go into fullscreen.
    bool can_fullscreen = false;
    bool can_maximize = false;
    bool can_minimize = false;
    bool can_resize = false;

#if defined(USE_AURA)
    // Whether to center the widget's title within the frame.
    bool center_title = false;
#endif

    // Controls focus traversal past the first/last focusable view.
    // If true, focus moves out of this Widget and to this Widget's toplevel
    // Widget; if false, focus cycles within this Widget.
    bool focus_traverses_out = false;

    // Controls whether the user can traverse a Widget's views using up/down
    // and left/right arrow keys in addition to TAB. Applies only to the
    // current widget so can be set independently even on widgets that share a
    // focus manager.
    bool enable_arrow_key_traversal = false;

    // The widget's icon, if any.
    ui::ImageModel icon;

    // The widget's app icon, a larger icon used for task bar and Alt-Tab.
    ui::ImageModel app_icon;

    // The widget's initially focused view, if any. This can only be set before
    // this WidgetDelegate is used to initialize a Widget.
    std::optional<View*> initially_focused_view;

    // This is used by modal dialogs to override and constrain desired bounds
    // calculations.
    // TODO(pbos): Consider if we could express bounds constraints in views and
    // keep them in sync rather than constrained_window owning the calculation
    // here. Considering this wasn't very expedient at the time.
    base::RepeatingCallback<gfx::Rect()> desired_bounds_delegate;

    // The widget's internal name, used to identify it in window-state
    // restoration (if this widget participates in that) and in debugging
    // contexts. Never displayed to the user, and not translated.
    std::string internal_name;

    // The widget's modality type. Note that MODAL_TYPE_SYSTEM does not work at
    // all on Mac.
    ui::mojom::ModalType modal_type = ui::mojom::ModalType::kNone;

    // Whether to show a close button in the widget frame.
    bool show_close_button = true;

    // Whether to show the widget's icon.
    // TODO(ellyjones): What if this was implied by !icon.isNull()?
    bool show_icon = false;

    // Whether to display the widget's title in the frame.
    bool show_title = true;

    // The widget's title, if any.
    // TODO(ellyjones): Should it be illegal to have show_title && !title?
    std::u16string title;

    // If set to true, force using desktop widget (DesktopNativeWidgetAura).
    // Otherwise, widget type is determined automatically.
    // This is used for child widgets on Desktop Aura (i.e. Windows and Linux)
    // when they need to be rendered beyond their parent window's boundary.
    // This setting has no effect on other platforms (e.g. ChromeOS or macOS).
    bool use_desktop_widget_override = false;
  };

  class OwnedByWidgetPassKey {
   private:
    // DO NOT ADD TO THIS LIST!
    // These existing cases are "grandfathered in", but there shouldn't be more.
    // See comments atop `SetOwnedByWidget()`.
    friend class ::AutoPipSettingView;
    friend class ::DigitalIdentityMultiStepDialogDelegate;
#if !BUILDFLAG(IS_CHROMEOS)
    friend class ::DownloadBubbleContentsViewTest;
    friend class ::DownloadBubbleSecurityViewTest;
    friend class ::DownloadToolbarUIController;
#endif
    friend class ::ExtensionsMenuCoordinator;
    friend class ::ExternalProtocolNoHandlersTelSchemeDialog;
    friend class ::ForceInstalledDeprecatedAppsDialogView;
    friend class ::ForceInstalledPreinstalledDeprecatedAppDialogView;
    friend class ::HoverDetectionBubbleView;
    friend class ::LoginHandlerViewsDialog;
    friend class ::MediaGalleriesDialogTest;
    friend class ::MessageBoxDialog;
    friend class ::OverlayWindowWidgetDelegate;
    friend class ::ParentAccessView;
    friend class ::PrivacySandboxDialog;
    friend class ::ProcessSharingInfobarDelegate;
    friend class ::ProfilePickerView;
    friend class ::ScheduledRebootDialog;
    friend class ::SearchEngineChoiceDialog;
    friend class ::TabModalConfirmDialogViews;
    friend class ::TestBaseWidgetDelegate;
    friend class ::UpdateRecommendedMessageBox;
    friend class ::WebDialogBrowserTest;
    FRIEND_TEST_ALL_PREFIXES(::TabStripScrollContainerTest,
                             AnchoredWidgetHidesOnScroll);
    friend class ::arc::ArcTaskWindowBuilder;
    friend class ::arc::ArcVmDataMigrationConfirmationDialog;
    friend class ::ash::AccessibilityPanel;
    friend class ::ash::ActiveSessionAuthControllerImpl;
    friend class ::ash::AmbientWidgetDelegate;
    friend class ::ash::AuthDialogContentsViewPixelTest;
    friend class ::ash::AuthDialogContentsViewTest;
    friend class ::ash::BootingAnimationController;
    friend class ::ash::CaptureModeSessionFocusCycler;
    friend class ::ash::DeferredUpdateDialog;
    friend class ::ash::DisclaimerView;
    friend class ::ash::GuestSessionConfirmationDialog;
    friend class ::ash::InSessionAuthDialog;
    friend class ::ash::InSessionAuthDialogControllerImpl;
    friend class ::ash::LocalAuthenticationWithPinControllerImpl;
    friend class ::ash::LockDebugView;
    friend class ::ash::LockDebugViewDataDispatcherTransformer;
    friend class ::ash::LockScreen;
    friend class ::ash::LoginShelfWidgetDelegate;
    friend class ::ash::LoginTestBase;
    friend class ::ash::LoginTestWidgetDelegate;
    friend class ::ash::MahiPanelWidget;
    friend class ::ash::ShelfNavigationWidgetDelegate;
    friend class ::ash::ShelfWidgetDelegateView;
    friend class ::ash::ShellTest;
    friend class ::ash::SimpleWebViewDialog;
    friend class ::ash::StatusAreaWidgetDelegate;
    friend class ::ash::StuckWidgetDelegate;
    friend class ::ash::SystemUIComponentsStyleViewerView;
    friend class ::ash::VirtualTrackpadView;
    friend class ::ash::hud_display::HUDDisplayView;
    friend class ::constrained_window::BrowserModalHelper;
    friend class ::content::ShellPlatformDelegate;
    friend class ::crostini::AppRestartDialog;
    friend class ::exo::ShellSurfaceBase;
    friend class ::extensions::WebFileHandlersPermissionHandler;
    friend class ::javascript_dialogs::AppModalDialogViewViews;
    friend class ::remoting::MessageBoxCore;
    friend class BubbleDialogModelHost;
    friend class FocusTraversalTest;
    FRIEND_TEST_ALL_PREFIXES(BubbleUmaLoggerTest, LogMetricFromDelegate);
    friend class borealis::BorealisDisallowedDialog;
    friend class borealis::BorealisLaunchErrorDialog;
    friend class ::web_app::IsolatedWebAppInstallerViewController;
    friend class ::web_app::SubAppsInstallDialogController;

    OwnedByWidgetPassKey() = default;
  };
  class RegisterWillCloseCallbackPassKey {
   private:
    // DO NOT ADD TO THIS LIST!
    // These existing cases are "grandfathered in", but there shouldn't be more.
    // See comments atop `RegisterWindowWillCloseCallback()`.
    friend class ::JavaScriptTabModalDialogViewViews;
    friend class ::autofill::payments::PaymentsWindowUserConsentDialogView;
    friend class DialogDelegate;
    friend class WebDialogView;

    RegisterWillCloseCallbackPassKey() = default;
  };
  class RegisterDeleteCallbackPassKey {
   private:
    // DO NOT ADD TO THIS LIST!
    // These existing cases are "grandfathered in", but there shouldn't be more.
    // See comments atop `RegisterDeleteDelegateCallback()`.
    friend class ::DesktopMediaPickerDialogView;
    friend class ::MediaGalleriesDialogViews;
    friend class ::PresentationReceiverWindowView;
    friend class ::SSLClientCertificateSelector;
    friend class ::ScreenCaptureNotificationUIViews;
    friend class ::ShareThisTabDialogView;
    friend class ::SigninViewControllerDelegateViews;
    friend class ::ash::InformedRestoreController;
    friend class ::glic::GlicWidgetDelegate;
    friend class ::native_app_window::NativeAppWindowViews;
    friend class ::plus_addresses::PlusAddressCreationDialogDelegate;
    friend class ::remoting::MessageBoxCore;
    friend class DefaultWidgetDelegate;
    friend class TableViewFocusTest;
    FRIEND_TEST_ALL_PREFIXES(FocusManagerTest, AdvanceFocusStaysInWidget);
    FRIEND_TEST_ALL_PREFIXES(NativeWidgetAuraTest,
                             TestPropertiesWhenAddedToLayout);
    FRIEND_TEST_ALL_PREFIXES(NativeWidgetAuraTest,
                             TransientChildModalWindowVisibility);
    FRIEND_TEST_ALL_PREFIXES(test::DesktopWidgetTest,
                             LockPaintAsActiveAndCloseParent);

    RegisterDeleteCallbackPassKey() = default;
  };

  WidgetDelegate();
  WidgetDelegate(const WidgetDelegate&) = delete;
  WidgetDelegate& operator=(const WidgetDelegate&) = delete;
  virtual ~WidgetDelegate();

  // Sets the return value of CanActivate(). Default is true.
  void SetCanActivate(bool can_activate);

  // Called whenever the widget's position changes.
  virtual void OnWidgetMove();

  // Called when the display changes (color depth or resolution).
  virtual void OnDisplayChanged();

  // Called when the work area (the desktop area minus task bars,
  // menu bars, etc.) changes in size.
  virtual void OnWorkAreaChanged();

  // Called when the widget's initialization is complete.
  virtual void OnWidgetInitialized() {}

  // Called when the window has been requested to close, after all other checks
  // have run. Returns whether the window should be allowed to close (default is
  // true).
  //
  // Can be used as an alternative to specifying a custom ClientView with
  // the CanClose() method, or in widget types which do not support a
  // ClientView.
  //
  // DEPRECATED. Don't use this. See Widget::MakeCloseSynchronous().
  virtual bool OnCloseRequested(Widget::ClosedReason close_reason);

  // Returns the view that should have the focus when the widget is shown.  If
  // nullptr no view is focused.
  virtual View* GetInitiallyFocusedView();
  bool HasConfiguredInitiallyFocusedView() const;

  virtual BubbleDialogDelegate* AsBubbleDialogDelegate();
  virtual DialogDelegate* AsDialogDelegate();

  // Returns true if the window can be resized.
  // This has side-effect on Windows. A resizable Windows cannot be translucent,
  // i.e. Widget::InitParam::WindowOpacity must be kInferred or kOpaque,
  // otherwise it draws nothing.
  virtual bool CanResize() const;

  // Returns true if the window can go into fullscreen.
  virtual bool CanFullscreen() const;

  // Returns true if the window can be maximized.
  virtual bool CanMaximize() const;

  // Returns true if the window can be minimized.
  virtual bool CanMinimize() const;

  // Returns true if the window can be activated.
  virtual bool CanActivate() const;

  // Returns the modal type that applies to the widget. Default is
  // ui::mojom::ModalType::kNone (not modal).
  virtual ui::mojom::ModalType GetModalType() const;

  virtual ax::mojom::Role GetAccessibleWindowRole();

  // Returns the title to be read with screen readers.
  virtual std::u16string GetAccessibleWindowTitle() const;

  // Returns the text to be displayed in the window title.
  virtual std::u16string GetWindowTitle() const;

  // Returns true if the window should show a title in the title bar.
  virtual bool ShouldShowWindowTitle() const;

  // Returns true if the window should show a close button in the title bar.
  virtual bool ShouldShowCloseButton() const;

  // Returns the app icon for the window. On Windows, this is the ICON_BIG used
  // in Alt-Tab list and Win7's taskbar.
  virtual ui::ImageModel GetWindowAppIcon();

  // Returns the icon to be displayed in the window.
  virtual ui::ImageModel GetWindowIcon();

  // Returns true if a window icon should be shown.
  virtual bool ShouldShowWindowIcon() const;

  // Execute a command in the window's controller. Returns true if the command
  // was handled, false if it was not.
  virtual bool ExecuteWindowsCommand(int command_id);

  // Returns the window's name identifier. Used to identify this window for
  // state restoration.
  virtual std::string GetWindowName() const;

  // Returns true if the widget should save its placement and state.
  virtual bool ShouldSaveWindowPlacement() const;

  // Saves the window's bounds and "show" state. By default this uses the
  // process' local state keyed by window name (See GetWindowName above). This
  // behavior can be overridden to provide additional functionality.
  virtual void SaveWindowPlacement(const gfx::Rect& bounds,
                                   ui::mojom::WindowShowState show_state);

  // Retrieves the window's bounds and "show" states.
  // This behavior can be overridden to provide additional functionality.
  virtual bool GetSavedWindowPlacement(
      const Widget* widget,
      gfx::Rect* bounds,
      ui::mojom::WindowShowState* show_state) const;

  // Hooks for the end of the Widget/Window lifecycle. As of this writing, these
  // callbacks happen like so:
  //   1. Client code calls Widget::CloseWithReason()
  //   2. WidgetDelegate::WindowWillClose() is called
  //   3. NativeWidget teardown (maybe async) starts OR the operating system
  //      abruptly closes the backing native window
  //   4. WidgetDelegate::WindowClosing() is called
  //   5. NativeWidget teardown completes, Widget teardown starts
  //   6. WidgetDelegate::DeleteDelegate() is called
  //   7. Widget teardown finishes, Widget is deleted
  // At step 3, the "maybe async" is controlled by whether the close is done via
  // Close() or CloseNow().
  // Important note: for OS-initiated window closes, steps 1 and 2 don't happen
  // - i.e, WindowWillClose() is never invoked.
  //
  // The default implementations of both of these call the callbacks described
  // below. It is better to use those callback mechanisms than to override one
  // of these methods.
  virtual void WindowClosing();

  // TODO (kylixrd): Rename this API once Widget ceases to "own" WidgetDelegate.
  //                 Update the comment below to match the new state of things.
  // Called when removed from a Widget. This first runs callbacks registered
  // through RegisterDeleteDelegateCallback() and then either deletes `this` or
  // not depending on SetOwnedByWidget(). If `this` is owned by Widget then the
  // delegate is destructed at the end.
  void DeleteDelegate();

  // When the ownership of the Widget is CLIENT_OWNS_WIDGET and the client also
  // owns the delegate, this function will be called when the Widget has
  // transitioned to a "zombie" state. It is safe to delete the Widget from
  // within this function.
  //
  // The "zombie" state is when the Widget is "alive" but the underlying
  // NativeWidget has been destroyed. Thus the Widget instance is still valid,
  // but it is functionally "dead", aka. "undead". The Widget (and underlying
  // NativeWidget) have can handle being in this state. Most Widget APIs will
  // not crash while in this state, but they may also do nothing meaningful.
  // Call Widget::IsClosed() to determine whether the Widget is in a usable
  // state. Widgets in the "zombie" state cannot be resurrected and must be
  // deleted or a new instance created.
  virtual void WidgetIsZombie(Widget* widget) {}

  // Called when the user begins/ends to change the bounds of the window.
  virtual void OnWindowBeginUserBoundsChange() {}
  virtual void OnWindowEndUserBoundsChange() {}

  // Returns the Widget associated with this delegate.
  virtual Widget* GetWidget();
  virtual const Widget* GetWidget() const;

  // Get the view that is contained within this widget.
  //
  // WARNING: This method has unusual ownership behavior:
  // * If the returned view is owned_by_client(), then the returned pointer is
  //   never an owning pointer;
  // * If the returned view is !owned_by_client() (the default & the
  //   recommendation), then the returned pointer is *sometimes* an owning
  //   pointer and sometimes not. Specifically, it is an owning pointer exactly
  //   once, when this method is being used to construct the ClientView, which
  //   takes ownership of the ContentsView() when !owned_by_client().
  //
  // Apart from being difficult to reason about this introduces a problem: a
  // WidgetDelegate can't know whether it owns its contents view or not, so
  // constructing a WidgetDelegate which one does not then use to construct a
  // Widget (often done in tests) leaks memory in a way that can't be locally
  // fixed.
  //
  // TODO(ellyjones): This is not tenable - figure out how this should work and
  // replace it.
  virtual View* GetContentsView();

  // Returns ownership of the contents view, which means something similar to
  // but not the same as C++ ownership in the unique_ptr sense. The caller
  // takes on responsibility for either destroying the returned View (if it
  // is !owned_by_client()) or not (if it is owned_by_client()). Since this
  // returns a raw pointer, this method serves only as a declaration of intent
  // by the caller.
  //
  // It is only legal to call this method one time on a given WidgetDelegate
  // instance.
  //
  // In future, this method will begin returning a unique_ptr<View> instead,
  // and will eventually be renamed to TakeContentsView() once WidgetDelegate
  // no longer retains any reference to the contents view internally.
  View* TransferOwnershipOfContentsView();

  // Called by the Widget to create the Client View used to host the contents
  // of the widget.
  virtual ClientView* CreateClientView(Widget* widget);

  // Called by the Widget to create the NonClient Frame View for this widget.
  // Return NULL to use the default one.
  virtual std::unique_ptr<FrameView> CreateFrameView(Widget* widget);

  // Called by the Widget to create the overlay View for this widget. Return
  // NULL for no overlay. The overlay View will fill the Widget and sit on top
  // of the ClientView and FrameView (both visually and wrt click
  // targeting).
  virtual View* CreateOverlayView();

  // Returns true if window has a hit-test mask.
  virtual bool WidgetHasHitTestMask() const;

  // Provides the hit-test mask if HasHitTestMask above returns true.
  virtual void GetWidgetHitTestMask(SkPath* mask) const;

  // Returns true if event handling should descend into |child|.
  // |location| is in terms of the Window.
  virtual bool ShouldDescendIntoChildForEventHandling(
      gfx::NativeView child,
      const gfx::Point& location);

  // Populates |panes| with accessible panes in this window that can
  // be cycled through with keyboard focus.
  virtual void GetAccessiblePanes(std::vector<View*>* panes) {}

  // Called when the widget wants to resize itself.
  // Default origin is the widget origin.
  // Default size is the ContentsView's PreferredSize.
  gfx::Rect GetDesiredWidgetBounds();

  // Setters for data parameters of the WidgetDelegate. If you use these
  // setters, there is no need to override the corresponding virtual getters.
  void SetAccessibleWindowRole(ax::mojom::Role role);
  void SetAccessibleTitle(std::u16string title);
  void SetCanFullscreen(bool can_fullscreen);
  void SetCanMaximize(bool can_maximize);
  void SetCanMinimize(bool can_minimize);
  void SetCanResize(bool can_resize);
  void SetFocusTraversesOut(bool focus_traverses_out);
  void SetEnableArrowKeyTraversal(bool enable_arrow_key_traversal);
  void SetIcon(ui::ImageModel icon);
  void SetAppIcon(ui::ImageModel icon);
  void SetInitiallyFocusedView(View* initially_focused_view);
  void SetModalType(ui::mojom::ModalType modal_type);
  void SetShowCloseButton(bool show_close_button);
  void SetShowIcon(bool show_icon);
  void SetShowTitle(bool show_title);
  void SetTitle(const std::u16string& title);
  void SetTitle(int title_message_id);
#if defined(USE_AURA)
  void SetCenterTitle(bool center_title);
#endif

  // DEPRECATED: Instead of having the Widget own the Delegate, the client
  // should directly own both (note: this implies the Widget is using the
  // CLIENT_OWNS_WIDGET ownership model and the Delegate is not also a View),
  // and should delete the delegate after deleting the Widget.
  void SetOwnedByWidget(OwnedByWidgetPassKey);

  template <typename T>
  T* SetContentsView(std::unique_ptr<T> contents) {
    T* raw_contents = contents.get();
    SetContentsViewImpl(std::move(contents));
    return raw_contents;
  }

  // A convenience wrapper that does all three of SetCanMaximize,
  // SetCanMinimize, and SetCanResize.
  void SetHasWindowSizeControls(bool has_controls);

  void RegisterWidgetInitializedCallback(base::OnceClosure callback);
  void RegisterWindowClosingCallback(base::OnceClosure callback);

  // DEPRECATED: Instead of using this to perform actions just before close, use
  // the CLIENT_OWNS_WIDGET ownership model for the Widget, then call
  // `Widget::MakeCloseSynchronous()` with a client callback that will do the
  // desired "on close" actions and delete the Widget.
  void RegisterWindowWillCloseCallback(RegisterWillCloseCallbackPassKey,
                                       base::OnceClosure callback);

  // DEPRECATED: This is unnecessary if the client directly owns the Delegate
  // (see comments on `SetOwnedByWidget()` above), since then it can do any
  // necessary cleanup directly before deleting the Delegate. Many callers also
  // use this for similar purposes to `RegisterWindowWillCloseCallback()`, e.g.
  // notifying someone of a dialog result; in this sort of case,
  // `Widget::MakeCloseSynchronous()` is similarly useful.
  void RegisterDeleteDelegateCallback(RegisterDeleteCallbackPassKey,
                                      base::OnceClosure callback);

  void SetClientViewFactory(ClientViewFactory factory);
  void SetFrameViewFactory(FrameViewFactory factory);
  void SetOverlayViewFactory(OverlayViewFactory factory);

  // Returns true if the title text should be centered.
  bool ShouldCenterWindowTitleText() const;

  bool focus_traverses_out() const { return params_.focus_traverses_out; }
  bool enable_arrow_key_traversal() const {
    return params_.enable_arrow_key_traversal;
  }
  // Rotates focus for panes contained in the current widget from the provided
  // view. If wrapping is enabled, rotation will continue after reaching the
  // end. This method will return  true if a rotation was performed and false
  // otherwise.
  // If the provided |focused_view| is not included by the widget's panes,
  // the method will not perform any rotation unless |enable_wrapping| is
  // set to true.
  virtual bool RotatePaneFocusFromView(views::View* focused_view,
                                       bool forward,
                                       bool enable_wrapping);

  void SetTitleChangedCallback(TitleChangedCallback callback);
  void SetAccessibleTitleChangedCallback(
      AccessibleTitleChangedCallback callback);

  bool owned_by_widget() const { return owned_by_widget_; }

  void set_internal_name(std::string name) { params_.internal_name = name; }
  std::string internal_name() const { return params_.internal_name; }

  void set_use_desktop_widget_override(bool use_desktop_widget_override) {
    params_.use_desktop_widget_override = use_desktop_widget_override;
  }
  bool use_desktop_widget_override() {
    return params_.use_desktop_widget_override;
  }

  bool has_desired_bounds_delegate() const {
    return static_cast<bool>(params_.desired_bounds_delegate);
  }
  void set_desired_bounds_delegate(
      base::RepeatingCallback<gfx::Rect()> desired_bounds_delegate) {
    params_.desired_bounds_delegate = std::move(desired_bounds_delegate);
  }

 private:
  // We're using a vector of OnceClosures instead of a OnceCallbackList because
  // most of the clients of WidgetDelegate don't have a convenient place to
  // store the CallbackLists' subscription objects.
  using ClosureVector = std::vector<base::OnceClosure>;

  friend class Widget;

  // Assign the widget associated with this delegate and return a `WeakPtr`
  // to this object. Since the delegate is not necessarily owned by
  // `Widget` it can be destroyed and the `Widget` needs to have a `WeakPtr`
  // to this object. This `WeakPtr` is valid until `DeleteDelegate` is called
  // which must be called in order to destroy this delegate.
  base::WeakPtr<WidgetDelegate> AttachWidgetAndGetHandle(Widget* widget);

  // Called to notify the WidgetDelegate of changes to the state of its Widget.
  // It is not usually necessary to call these from client code.
  void WidgetInitialized();
  void WidgetDestroying();
  void WindowWillClose();

  void SetContentsViewImpl(std::unique_ptr<View> contents);

  // The Widget that was initialized with this instance as its WidgetDelegate,
  // if any.
  raw_ptr<Widget, AcrossTasksDanglingUntriaged> widget_ = nullptr;
  Params params_;

  raw_ptr<View, AcrossTasksDanglingUntriaged> default_contents_view_ = nullptr;
  bool contents_view_taken_ = false;
  bool can_activate_ = true;

  raw_ptr<View, AcrossTasksDanglingUntriaged> unowned_contents_view_ = nullptr;
  std::unique_ptr<View> owned_contents_view_;

  // Whether this WidgetDelegate should delete itself when the Widget for
  // which it is the delegate is about to be destroyed.
  // See https://crbug.com/1119898 for more details.
  bool owned_by_widget_ = false;

  // Managed by Widget. Ensures |this| outlives its Widget.
  bool can_delete_this_ = true;

  // This is stored as a unique_ptr to make it easier to check in the
  // registration methods whether a callback is being registered too late in the
  // WidgetDelegate's lifecycle.
  std::unique_ptr<ClosureVector> widget_initialized_callbacks_;
  ClosureVector window_will_close_callbacks_;
  ClosureVector window_closing_callbacks_;
  ClosureVector delete_delegate_callbacks_;

  ClientViewFactory client_view_factory_;
  FrameViewFactory frame_view_factory_;
  OverlayViewFactory overlay_view_factory_;

  TitleChangedCallback title_changed_callback_;
  AccessibleTitleChangedCallback accessible_title_changed_callback_;

  base::WeakPtrFactory<WidgetDelegate> weak_ptr_factory_{this};
};

// A WidgetDelegate implementation that is-a View. Used to override GetWidget()
// to call View's GetWidget() for the common case where a WidgetDelegate
// implementation is-a View. Note that WidgetDelegateView is not owned by
// view's hierarchy and is expected to be deleted on DeleteDelegate call.
//
// DEPRECATED: Using this class makes it more challenging to reason about object
// ownership/lifetimes and promotes writing "fat" views that also contain
// business logic. Instead, use DialogModel if possible; otherwise, use separate
// subclasses of WidgetDelegate and View to handle those interfaces' respective
// concerns.
class VIEWS_EXPORT WidgetDelegateView : public WidgetDelegate, public View {
  METADATA_HEADER(WidgetDelegateView, View)

 public:
  // Not named `PassKey` as `View::PassKey` already exists in this hierarchy.
  using WdvPassKey = base::PassKey<WidgetDelegateView>;

  // For use with std::make_unique<>(). Callers still must be in the friend list
  // below, just as with the private constructor.
  explicit WidgetDelegateView(WdvPassKey) {}
  WidgetDelegateView(const WidgetDelegateView&) = delete;
  WidgetDelegateView& operator=(const WidgetDelegateView&) = delete;
  ~WidgetDelegateView() override;

  // WidgetDelegate:
  Widget* GetWidget() override;
  const Widget* GetWidget() const override;
  View* GetContentsView() override;

 private:
  // DO NOT ADD TO THIS LIST!
  // These existing cases are "grandfathered in", but there shouldn't be more.
  // See comments atop class.
  friend class ::ImmersiveModeControllerMacInteractiveTest;
  friend class ::PresentationReceiverWindowView;
  friend class ::ProfilePickerView;
  friend class ::ScreenCaptureNotificationUIViews;
  FRIEND_TEST_ALL_PREFIXES(::AcceleratorCommandsFullscreenBrowserTest,
                           ToggleFullscreen);
  friend class ::ash::AppListView;
  friend class ::ash::AppsCollectionsDismissDialog;
  friend class ::ash::AssistantWebContainerView;
  friend class ::ash::ClientControlledStateTestWidgetDelegate;
  friend class ::ash::ConnectionErrorDialogDelegateView;
  friend class ::ash::DeskButtonWidgetDelegateView;
  friend class ::ash::DockedMagnifierTest;
  friend class ::ash::DropSenderView;
  friend class ::ash::DropTargetView;
  friend class ::ash::ExitWarningWidgetDelegateView;
  friend class ::ash::FrameCaptionButtonContainerViewTest;
  friend class ::ash::FrameSizeButtonTestWidgetDelegate;
  friend class ::ash::HotseatWidgetDelegateView;
  friend class ::ash::IdleAppNameNotificationDelegateView;
  friend class ::ash::KioskExternalUpdateNotificationView;
  friend class ::ash::LayoutWidgetDelegateView;
  friend class ::ash::MaximizeDelegateView;
  friend class ::ash::FrameViewAshTestWidgetDelegate;
  friend class ::ash::PipTest;
  friend class ::ash::QuickInsertSubmenuView;
  friend class ::ash::QuickInsertView;
  friend class ::ash::ResizableWidgetDelegate;
  friend class ::ash::RootWindowControllerTest;
  friend class ::ash::ShellTest;
  friend class ::ash::SystemDialogDelegateView;
  friend class ::ash::SystemUIComponentsStyleViewerView;
  friend class ::ash::TestChildModalParent;
  friend class ::ash::TestTextInputView;
  friend class ::ash::TestWidgetBuilderDelegate;
  friend class ::ash::TestWidgetDelegateAsh;
  friend class ::ash::TestWindow;
  friend class ::ash::WallpaperWidgetDelegate;
  friend class ::ash::WideFrameView;
  friend class ::ash::WidgetWithSystemUIComponentView;
  friend class ::ash::WindowCycleView;
  FRIEND_TEST_ALL_PREFIXES(::ash::ExtendedDesktopTest, SystemModal);
  FRIEND_TEST_ALL_PREFIXES(::ash::MruWindowTrackerOrderTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(::ash::ShellTest, CreateWindowWithPreferredSize);
  friend class ::ash::sharesheet::TestWidgetDelegate;
  friend class ::ash::shell::ToplevelWindow;
  friend class ::autofill::PopupBaseView;
  friend class ::eye_dropper::EyeDropperView;
  friend class ::message_center::MessagePopupView;
  friend class ::native_app_window::NativeAppWindowViews;
  friend class ::ui::ime::CandidateViewTest;
  friend class MoveTestWidgetDelegate;
  friend class ShapedWidgetDelegate;
  FRIEND_TEST_ALL_PREFIXES(NativeViewHostAuraTest,
                           FocusManagerUpdatedDuringDestruction);
  friend class examples::ExamplesWindowContents;
  friend class test::GetNativeThemeFromDestructorView;
  friend class test::TestingWidgetDelegateView;
  friend class webid::TestAccountSelectionView;
  FRIEND_TEST_ALL_PREFIXES(test::WidgetOwnsNativeWidgetTest,
                           WidgetDelegateView);

  WidgetDelegateView();

  static WdvPassKey CreatePassKey() { return WdvPassKey(); }
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, WidgetDelegateView, View)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, WidgetDelegateView)

#endif  // UI_VIEWS_WIDGET_WIDGET_DELEGATE_H_
