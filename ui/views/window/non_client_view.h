// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_NON_CLIENT_VIEW_H_
#define UI_VIEWS_WINDOW_NON_CLIENT_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/window/frame_view.h"

namespace views {

class ClientView;
enum class CloseRequestResult;

////////////////////////////////////////////////////////////////////////////////
// NonClientView
//
//  The NonClientView is the logical root of all Views contained within a
//  Window, except for the RootView which is its parent and of which it is the
//  sole child. The NonClientView has one child, the FrameView which is
//  responsible for painting and responding to events from the non-client
//  portions of the window, and for forwarding events to its child, the
//  ClientView, which is responsible for the same for the client area of the
//  window:
//
//  +- views::Widget ------------------------------------+
//  | +- views::RootView ------------------------------+ |
//  | | +- views::NonClientView ---------------------+ | |
//  | | | +- views::FrameView subclass ------------+ | | |
//  | | | |                                        | | | |
//  | | | | << all painting and event receiving >> | | | |
//  | | | | << of the non-client areas of a     >> | | | |
//  | | | | << views::Widget.                   >> | | | |
//  | | | |                                        | | | |
//  | | | | +- views::ClientView or subclass ----+ | | | |
//  | | | | |                                    | | | | |
//  | | | | | << all painting and event       >> | | | | |
//  | | | | | << receiving of the client      >> | | | | |
//  | | | | | << areas of a views::Widget.    >> | | | | |
//  | | | | +------------------------------------+ | | | |
//  | | | +----------------------------------------+ | | |
//  | | +--------------------------------------------+ | |
//  | +------------------------------------------------+ |
//  +----------------------------------------------------+
//
class VIEWS_EXPORT NonClientView : public View, public ViewTargeterDelegate {
  METADATA_HEADER(NonClientView, View)

 public:
  explicit NonClientView(ClientView* client_view);
  NonClientView(const NonClientView&) = delete;
  NonClientView& operator=(const NonClientView&) = delete;
  ~NonClientView() override;

  // Returns the current FrameView instance, or NULL if
  // it does not exist.
  FrameView* frame_view() const { return frame_view_.get(); }

  // Replaces the current FrameView (if any) with the specified one.
  void SetFrameView(std::unique_ptr<FrameView> frame_view);

  // Replaces the current |overlay_view_| (if any) with the specified one.
  void SetOverlayView(View* view);

  // Returned value signals whether the ClientView can be closed.
  CloseRequestResult OnWindowCloseRequested();

  // Called by the containing Window when it is closed.
  void WindowClosing();

  // Replaces the frame view with a new one. Used when switching window theme
  // or frame style.
  void UpdateFrame();

  // Returns the bounds of the window required to display the content area at
  // the specified bounds.
  gfx::Rect GetWindowBoundsForClientBounds(const gfx::Rect client_bounds) const;

  // Determines the windows HT* code when the mouse cursor is at the
  // specified point, in window coordinates.
  int NonClientHitTest(const gfx::Point& point);

  // Returns a mask to be used to clip the top level window for the given
  // size. This is used to create the non-rectangular window shape.
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask);

  // Tells the window controls as rendered by the NonClientView to reset
  // themselves to a normal state. This happens in situations where the
  // containing window does not receive a normal sequences of messages that
  // would lead to the controls returning to this normal state naturally, e.g.
  // when the window is maximized, minimized or restored.
  void ResetWindowControls();

  // Tells the NonClientView to invalidate the FrameView's window icon.
  void UpdateWindowIcon();

  // Tells the NonClientView to invalidate the FrameView's window
  // title.
  void UpdateWindowTitle();

  // Called when the size constraints of the window change.
  void SizeConstraintsChanged();

  // Returns whether FrameView has a custom title.
  bool HasWindowTitle() const;

  // Returns whether the FrameView's window title is visible.
  bool IsWindowTitleVisible() const;

  // Get/Set client_view property.
  ClientView* client_view() const { return client_view_; }

  // NonClientView, View overrides:
  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void Layout(PassKey) override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;

 protected:
  // NonClientView, View overrides:
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;

 private:
  // ViewTargeterDelegate:
  View* TargetForRect(View* root, const gfx::Rect& rect) override;

  // The FrameView that renders the non-client portions of the window.
  // This object is not owned by the view hierarchy because it can be replaced
  // dynamically as the system settings change.
  std::unique_ptr<FrameView> frame_view_;

  // A ClientView object or subclass, responsible for sizing the contents view
  // of the window, hit testing and perhaps other tasks depending on the
  // implementation.
  const raw_ptr<ClientView, DanglingUntriaged> client_view_;

  // The overlay view, when non-NULL and visible, takes up the entire widget and
  // is placed on top of the ClientView and FrameView.
  raw_ptr<View> overlay_view_ = nullptr;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, NonClientView, View)
VIEW_BUILDER_VIEW_PROPERTY(FrameView, FrameView)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, NonClientView)

#endif  // UI_VIEWS_WINDOW_NON_CLIENT_VIEW_H_
