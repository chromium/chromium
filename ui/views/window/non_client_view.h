// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_NON_CLIENT_VIEW_H_
#define UI_VIEWS_WINDOW_NON_CLIENT_VIEW_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter_delegate.h"

namespace views {

class ClientView;

////////////////////////////////////////////////////////////////////////////////
// NonClientFrameView
//
//  An object that subclasses NonClientFrameView is a View that renders and
//  responds to events within the frame portions of the non-client area of a
//  window. This view does _not_ contain the ClientView, but rather is a sibling
//  of it.
class VIEWS_EXPORT NonClientFrameView : public View,
                                        public ViewTargeterDelegate {
 public:
  METADATA_HEADER(NonClientFrameView);

  enum {
    // Various edges of the frame border have a 1 px shadow along their edges;
    // in a few cases we shift elements based on this amount for visual appeal.
    kFrameShadowThickness = 1,

    // In restored mode, we draw a 1 px edge around the content area inside the
    // frame border.
    kClientEdgeThickness = 1,
  };

  ~NonClientFrameView() override;

  // Used to determine if the frame should be painted as active. Keyed off the
  // window's actual active state and whether the widget should be rendered as
  // active.
  bool ShouldPaintAsActive() const;

  // Helper for non-client view implementations to determine which area of the
  // window border the specified |point| falls within. The other parameters are
  // the size of the sizing edges, and whether or not the window can be
  // resized.
  int GetHTComponentForFrame(const gfx::Point& point,
                             int top_resize_border_height,
                             int resize_border_thickness,
                             int top_resize_corner_height,
                             int resize_corner_width,
                             bool can_resize);

  // Returns the bounds (in this View's parent's coordinates) that the client
  // view should be laid out within.
  virtual gfx::Rect GetBoundsForClientView() const = 0;

  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const = 0;

  // Gets the clip mask (in this View's parent's coordinates) that should be
  // applied to the client view. Returns false if no special clip should be
  // used.
  virtual bool GetClientMask(const gfx::Size& size, SkPath* mask) const;

#if defined(OS_WIN)
  // Returns the point in screen physical coordinates at which the system menu
  // should be opened.
  virtual gfx::Point GetSystemMenuScreenPixelLocation() const;
#endif

  // This function must ask the ClientView to do a hittest.  We don't do this in
  // the parent NonClientView because that makes it more difficult to calculate
  // hittests for regions that are partially obscured by the ClientView, e.g.
  // HTSYSMENU.
  // Return value is one of the windows HT constants (see ui/base/hit_test.h).
  virtual int NonClientHitTest(const gfx::Point& point) = 0;

  // Used to make the hosting widget shaped (non-rectangular). For a
  // rectangular window do nothing. For a shaped window update |window_mask|
  // accordingly. |size| is the size of the widget.
  virtual void GetWindowMask(const gfx::Size& size, SkPath* window_mask) = 0;
  virtual void ResetWindowControls() = 0;
  virtual void UpdateWindowIcon() = 0;
  virtual void UpdateWindowTitle() = 0;

  // Whether the widget can be resized or maximized has changed.
  virtual void SizeConstraintsChanged() = 0;

  // Called when whether the non-client view should paint as active has changed.
  virtual void PaintAsActiveChanged(bool active);

  // View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnThemeChanged() override;

 protected:
  NonClientFrameView();

  // ViewTargeterDelegate:
  bool DoesIntersectRect(const View* target,
                         const gfx::Rect& rect) const override;

 private:
#if defined(OS_WIN)
  // Returns the y coordinate, in local coordinates, at which the system menu
  // should be opened.  Since this is in DIP, it does not include the 1 px
  // offset into the caption area; the caller will take care of this.
  virtual int GetSystemMenuY() const;
#endif

  DISALLOW_COPY_AND_ASSIGN(NonClientFrameView);
};

////////////////////////////////////////////////////////////////////////////////
// NonClientView
//
//  The NonClientView is the logical root of all Views contained within a
//  Window, except for the RootView which is its parent and of which it is the
//  sole child. The NonClientView has two children, the NonClientFrameView which
//  is responsible for painting and responding to events from the non-client
//  portions of the window, and the ClientView, which is responsible for the
//  same for the client area of the window:
//
//  +- views::Widget ------------------------------------+
//  | +- views::RootView ------------------------------+ |
//  | | +- views::NonClientView ---------------------+ | |
//  | | | +- views::NonClientFrameView subclass ---+ | | |
//  | | | |                                        | | | |
//  | | | | << all painting and event receiving >> | | | |
//  | | | | << of the non-client areas of a     >> | | | |
//  | | | | << views::Widget.                   >> | | | |
//  | | | |                                        | | | |
//  | | | +----------------------------------------+ | | |
//  | | | +- views::ClientView or subclass --------+ | | |
//  | | | |                                        | | | |
//  | | | | << all painting and event receiving >> | | | |
//  | | | | << of the client areas of a         >> | | | |
//  | | | | << views::Widget.                   >> | | | |
//  | | | |                                        | | | |
//  | | | +----------------------------------------+ | | |
//  | | +--------------------------------------------+ | |
//  | +------------------------------------------------+ |
//  +----------------------------------------------------+
//
// The NonClientFrameView and ClientView are siblings because due to theme
// changes the NonClientFrameView may be replaced with different
// implementations (e.g. during the switch from DWM/Aero-Glass to Vista Basic/
// Classic rendering).
//
class VIEWS_EXPORT NonClientView : public View, public ViewTargeterDelegate {
 public:
  METADATA_HEADER(NonClientView);

  NonClientView();
  ~NonClientView() override;

  // Returns the current NonClientFrameView instance, or NULL if
  // it does not exist.
  NonClientFrameView* frame_view() const { return frame_view_.get(); }

  // Replaces the current NonClientFrameView (if any) with the specified one.
  void SetFrameView(NonClientFrameView* frame_view);

  // Replaces the current |overlay_view_| (if any) with the specified one.
  void SetOverlayView(View* view);

  // Returns true if the ClientView determines that the containing window can be
  // closed, false otherwise.
  bool CanClose();

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

  // Tells the NonClientView to invalidate the NonClientFrameView's window icon.
  void UpdateWindowIcon();

  // Tells the NonClientView to invalidate the NonClientFrameView's window
  // title.
  void UpdateWindowTitle();

  // Called when the size constraints of the window change.
  void SizeConstraintsChanged();

  // Get/Set client_view property.
  ClientView* client_view() const { return client_view_; }
  void set_client_view(ClientView* client_view) {
    client_view_ = client_view;
  }

  // Set the accessible name of this view.
  void SetAccessibleName(const base::string16& name);

  // NonClientView, View overrides:
  gfx::Size CalculatePreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void Layout() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;

 protected:
  // NonClientView, View overrides:
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;

 private:
  // ViewTargeterDelegate:
  View* TargetForRect(View* root, const gfx::Rect& rect) override;

  // A ClientView object or subclass, responsible for sizing the contents view
  // of the window, hit testing and perhaps other tasks depending on the
  // implementation.
  ClientView* client_view_ = nullptr;

  // The NonClientFrameView that renders the non-client portions of the window.
  // This object is not owned by the view hierarchy because it can be replaced
  // dynamically as the system settings change.
  std::unique_ptr<NonClientFrameView> frame_view_;

  // The overlay view, when non-NULL and visible, takes up the entire widget and
  // is placed on top of the ClientView and NonClientFrameView.
  View* overlay_view_ = nullptr;

  // The accessible name of this view.
  base::string16 accessible_name_;

  DISALLOW_COPY_AND_ASSIGN(NonClientView);
};

}  // namespace views

#endif  // UI_VIEWS_WINDOW_NON_CLIENT_VIEW_H_
