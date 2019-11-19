// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_WIDGET_DELEGATE_H_
#define UI_VIEWS_WIDGET_WIDGET_DELEGATE_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace gfx {
class ImageSkia;
class Rect;
}

namespace views {
class BubbleDialogDelegateView;
class ClientView;
class DialogDelegate;
class NonClientFrameView;
class View;

// Handles events on Widgets in context-specific ways.
class VIEWS_EXPORT WidgetDelegate {
 public:
  WidgetDelegate();

  // Sets the return value of CanActivate(). Default is true.
  void SetCanActivate(bool can_activate);

  // Called whenever the widget's position changes.
  virtual void OnWidgetMove();

  // Called with the display changes (color depth or resolution).
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
  virtual bool OnCloseRequested(Widget::ClosedReason close_reason);

  // Called when the widget transitions from a state in which it should render
  // as active to one in which it should render as inactive or vice-versa.
  virtual void OnPaintAsActiveChanged(bool paint_as_active);

  // Returns the view that should have the focus when the widget is shown.  If
  // NULL no view is focused.
  virtual View* GetInitiallyFocusedView();

  virtual BubbleDialogDelegateView* AsBubbleDialogDelegate();
  virtual DialogDelegate* AsDialogDelegate();

  // Returns true if the window can be resized.
  virtual bool CanResize() const;

  // Returns true if the window can be maximized.
  virtual bool CanMaximize() const;

  // Returns true if the window can be minimized.
  virtual bool CanMinimize() const;

  // Returns true if the window can be activated.
  virtual bool CanActivate() const;

  // Returns the modal type that applies to the widget. Default is
  // ui::MODAL_TYPE_NONE (not modal).
  virtual ui::ModalType GetModalType() const;

  virtual ax::mojom::Role GetAccessibleWindowRole();

  // Returns the title to be read with screen readers.
  virtual base::string16 GetAccessibleWindowTitle() const;

  // Returns the text to be displayed in the window title.
  virtual base::string16 GetWindowTitle() const;

  // Returns true if the window should show a title in the title bar.
  virtual bool ShouldShowWindowTitle() const;

  // Returns true if the title text should be centered. Default is false.
  virtual bool ShouldCenterWindowTitleText() const;

  // Returns true if the window should show a close button in the title bar.
  virtual bool ShouldShowCloseButton() const;

  // Returns the app icon for the window. On Windows, this is the ICON_BIG used
  // in Alt-Tab list and Win7's taskbar.
  virtual gfx::ImageSkia GetWindowAppIcon();

  // Returns the icon to be displayed in the window.
  virtual gfx::ImageSkia GetWindowIcon();

  // Returns true if a window icon should be shown.
  virtual bool ShouldShowWindowIcon() const;

  // Execute a command in the window's controller. Returns true if the command
  // was handled, false if it was not.
  virtual bool ExecuteWindowsCommand(int command_id);

  // Returns the window's name identifier. Used to identify this window for
  // state restoration.
  virtual std::string GetWindowName() const;

  // Saves the window's bounds and "show" state. By default this uses the
  // process' local state keyed by window name (See GetWindowName above). This
  // behavior can be overridden to provide additional functionality.
  virtual void SaveWindowPlacement(const gfx::Rect& bounds,
                                   ui::WindowShowState show_state);

  // Retrieves the window's bounds and "show" states.
  // This behavior can be overridden to provide additional functionality.
  virtual bool GetSavedWindowPlacement(const Widget* widget,
                                       gfx::Rect* bounds,
                                       ui::WindowShowState* show_state) const;

  // Returns true if the window's size should be restored. If this is false,
  // only the window's origin is restored and the window is given its
  // preferred size.
  // Default is true.
  virtual bool ShouldRestoreWindowSize() const;

  // Called when the window closes. The delegate MUST NOT delete itself during
  // this call, since it can be called afterwards. See DeleteDelegate().
  virtual void WindowClosing() {}

  // Called when the window is destroyed. No events must be sent or received
  // after this point. The delegate can use this opportunity to delete itself at
  // this time if necessary.
  virtual void DeleteDelegate() {}

  // Called when the user begins/ends to change the bounds of the window.
  virtual void OnWindowBeginUserBoundsChange() {}
  virtual void OnWindowEndUserBoundsChange() {}

  // Returns the Widget associated with this delegate.
  virtual Widget* GetWidget() = 0;
  virtual const Widget* GetWidget() const = 0;

  // Returns the View that is contained within this Widget.
  virtual View* GetContentsView();

  // Called by the Widget to create the Client View used to host the contents
  // of the widget.
  virtual ClientView* CreateClientView(Widget* widget);

  // Called by the Widget to create the NonClient Frame View for this widget.
  // Return NULL to use the default one.
  virtual NonClientFrameView* CreateNonClientFrameView(Widget* widget);

  // Called by the Widget to create the overlay View for this widget. Return
  // NULL for no overlay. The overlay View will fill the Widget and sit on top
  // of the ClientView and NonClientFrameView (both visually and wrt click
  // targeting).
  virtual View* CreateOverlayView();

  // Returns true if the window can be notified with the work area change.
  // Otherwise, the work area change for the top window will be processed by
  // the default window manager. In some cases, like panel, we would like to
  // manage the positions by ourselves.
  virtual bool WillProcessWorkAreaChange() const;

  // Returns true if window has a hit-test mask.
  virtual bool WidgetHasHitTestMask() const;

  // Provides the hit-test mask if HasHitTestMask above returns true.
  virtual void GetWidgetHitTestMask(SkPath* mask) const;

  // Returns true if focus should advance to the top level widget when
  // tab/shift-tab is hit and on the last/first focusable view. Default returns
  // false, which means tab/shift-tab never advance to the top level Widget.
  virtual bool ShouldAdvanceFocusToTopLevelWidget() const;

  // Returns true if event handling should descend into |child|.
  // |location| is in terms of the Window.
  virtual bool ShouldDescendIntoChildForEventHandling(
      gfx::NativeView child,
      const gfx::Point& location);

  // Populates |panes| with accessible panes in this window that can
  // be cycled through with keyboard focus.
  virtual void GetAccessiblePanes(std::vector<View*>* panes) {}

 protected:
  virtual ~WidgetDelegate();

 private:
  friend class Widget;

  View* default_contents_view_ = nullptr;
  bool can_activate_ = true;

  // Managed by Widget. Ensures |this| outlives its Widget.
  bool can_delete_this_ = true;

  DISALLOW_COPY_AND_ASSIGN(WidgetDelegate);
};

// A WidgetDelegate implementation that is-a View. Used to override GetWidget()
// to call View's GetWidget() for the common case where a WidgetDelegate
// implementation is-a View. Note that WidgetDelegateView is not owned by
// view's hierarchy and is expected to be deleted on DeleteDelegate call.
class VIEWS_EXPORT WidgetDelegateView : public WidgetDelegate, public View {
 public:
  METADATA_HEADER(WidgetDelegateView);

  WidgetDelegateView();
  ~WidgetDelegateView() override;

  // WidgetDelegate:
  void DeleteDelegate() override;
  Widget* GetWidget() override;
  const Widget* GetWidget() const override;
  views::View* GetContentsView() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WidgetDelegateView);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_WIDGET_DELEGATE_H_
