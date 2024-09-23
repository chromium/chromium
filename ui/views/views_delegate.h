// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEWS_DELEGATE_H_
#define UI_VIEWS_VIEWS_DELEGATE_H_

#include <memory>
#include <string>
#include <utility>

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#include "base/functional/callback.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/buildflags.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget.h"

namespace gfx {
class ImageSkia;
class Rect;
}  // namespace gfx

namespace ui {
#if BUILDFLAG(IS_MAC)
class ContextFactory;
#endif
}  // namespace ui

namespace views {

class NativeWidget;
class NonClientFrameView;
class Widget;

#if defined(USE_AURA)
class TouchSelectionMenuRunnerViews;
#endif

namespace internal {
class NativeWidgetDelegate;
}

// ViewsDelegate is an interface implemented by an object using the views
// framework. It is used to obtain various high level application utilities
// and perform some actions such as window placement saving.
//
// The embedding app must set the ViewsDelegate instance by instantiating an
// implementation of ViewsDelegate (the constructor will set the instance).
class VIEWS_EXPORT ViewsDelegate {
 public:
  using NativeWidgetFactory =
      base::RepeatingCallback<NativeWidget*(const Widget::InitParams&,
                                            internal::NativeWidgetDelegate*)>;
#if BUILDFLAG(IS_WIN)
  enum AppbarAutohideEdge {
    EDGE_TOP = 1 << 0,
    EDGE_LEFT = 1 << 1,
    EDGE_BOTTOM = 1 << 2,
    EDGE_RIGHT = 1 << 3,
  };
#endif

  enum class ProcessMenuAcceleratorResult {
    // The accelerator was handled while the menu was showing. No further action
    // is needed and the menu should be kept open.
    LEAVE_MENU_OPEN,

    // The accelerator was not handled. The menu should be closed and event
    // handling should stop for this event.
    CLOSE_MENU,
  };

  ViewsDelegate(const ViewsDelegate&) = delete;
  ViewsDelegate& operator=(const ViewsDelegate&) = delete;

  virtual ~ViewsDelegate();

  // Returns the ViewsDelegate instance.  This should never return non-null
  // unless the binary has not yet initialized the delegate, so callers should
  // not generally null-check.
  static ViewsDelegate* GetInstance();

  // Call this method to set a factory callback that will be used to construct
  // NativeWidget implementations overriding the platform defaults.
  void set_native_widget_factory(NativeWidgetFactory factory) {
    native_widget_factory_ = std::move(factory);
  }
  const NativeWidgetFactory& native_widget_factory() const {
    return native_widget_factory_;
  }

  // Saves the position, size and "show" state for the window with the
  // specified name.
  virtual void SaveWindowPlacement(const Widget* widget,
                                   const std::string& window_name,
                                   const gfx::Rect& bounds,
                                   ui::mojom::WindowShowState show_state);

  // Retrieves the saved position and size and "show" state for the window with
  // the specified name.
  virtual bool GetSavedWindowPlacement(
      const Widget* widget,
      const std::string& window_name,
      gfx::Rect* bounds,
      ui::mojom::WindowShowState* show_state) const;

  // For accessibility, notify the delegate that a menu item was focused
  // so that alternate feedback (speech / magnified text) can be provided.
  virtual void NotifyMenuItemFocused(const std::u16string& menu_name,
                                     const std::u16string& menu_item_name,
                                     int item_index,
                                     int item_count,
                                     bool has_submenu);

  // If |accelerator| can be processed while a menu is showing, it will be
  // processed now and LEAVE_MENU_OPEN is returned. Otherwise, |accelerator|
  // will be reposted for processing later after the menu closes and CLOSE_MENU
  // will be returned.
  virtual ProcessMenuAcceleratorResult ProcessAcceleratorWhileMenuShowing(
      const ui::Accelerator& accelerator);

  // If a menu is showing and its window loses mouse capture, it will close if
  // this returns true.
  virtual bool ShouldCloseMenuIfMouseCaptureLost() const;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Returns true if the native `window` should have rounded corners. The
  // decision can be based on multiple factors, including the window's current
  // state.
  virtual bool ShouldWindowHaveRoundedCorners(
      const gfx::NativeWindow window) const;
#endif

#if BUILDFLAG(IS_WIN)
  // Retrieves the default window icon to use for windows if none is specified.
  virtual HICON GetDefaultWindowIcon() const;
  // Retrieves the small window icon to use for windows if none is specified.
  virtual HICON GetSmallWindowIcon() const;
  // Returns true if the window passed in is in the Windows 8 metro
  // environment.
  virtual bool IsWindowInMetro(gfx::NativeWindow window) const;
#elif BUILDFLAG(ENABLE_DESKTOP_AURA) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))
  virtual gfx::ImageSkia* GetDefaultWindowIcon() const;
#endif

  // Creates a default NonClientFrameView to be used for windows that don't
  // specify their own. If this function returns NULL, the
  // views::CustomFrameView type will be used.
  virtual std::unique_ptr<NonClientFrameView> CreateDefaultNonClientFrameView(
      Widget* widget);

  // AddRef/ReleaseRef are invoked while a menu is visible. They are used to
  // ensure we don't attempt to exit while a menu is showing.
  virtual void AddRef();
  virtual void ReleaseRef();
  // Returns true if the application is shutting down. AddRef/Release should not
  // be called in this situation.
  virtual bool IsShuttingDown() const;

  // Gives the platform a chance to modify the properties of a Widget.
  virtual void OnBeforeWidgetInit(Widget::InitParams* params,
                                  internal::NativeWidgetDelegate* delegate);

  // Returns true if the operating system's window manager will always provide a
  // title bar with caption buttons (ignoring the setting to
  // |remove_standard_frame| in InitParams). If |maximized|, this applies to
  // maximized windows; otherwise to restored windows.
  virtual bool WindowManagerProvidesTitleBar(bool maximized);

#if BUILDFLAG(IS_MAC)
  // Returns the context factory for new windows.
  virtual ui::ContextFactory* GetContextFactory();
#endif

  // Returns the user-visible name of the application.
  virtual std::string GetApplicationName();

#if BUILDFLAG(IS_WIN)
  // Starts a query for the appbar autohide edges of the specified monitor and
  // returns the current value.  If the query finds the edges have changed from
  // the current value, |callback| is subsequently invoked.  If the edges have
  // not changed, |callback| is never run.
  //
  // The return value is a bitmask of AppbarAutohideEdge.
  virtual int GetAppbarAutohideEdges(HMONITOR monitor,
                                     base::OnceClosure callback);
#endif

 protected:
  ViewsDelegate();

#if defined(USE_AURA)
  void SetTouchSelectionMenuRunner(
      std::unique_ptr<TouchSelectionMenuRunnerViews> menu_runner);
#endif

 private:
#if defined(USE_AURA)
  std::unique_ptr<TouchSelectionMenuRunnerViews> touch_selection_menu_runner_;
#endif

  NativeWidgetFactory native_widget_factory_;
};

}  // namespace views

#endif  // UI_VIEWS_VIEWS_DELEGATE_H_
