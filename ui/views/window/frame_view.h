// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_FRAME_VIEW_H_
#define UI_VIEWS_WINDOW_FRAME_VIEW_H_

#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/views_export.h"

namespace views {

class ClientView;

////////////////////////////////////////////////////////////////////////////////
// FrameView
//
//  An object that subclasses FrameView is a View that renders and
//  responds to events within the frame portions of the non-client area of a
//  window. This view contains the ClientView (see NonClientView comments for
//  details on View hierarchy).
class VIEWS_EXPORT FrameView : public View, public ViewTargeterDelegate {
  METADATA_HEADER(FrameView, View)

 public:
  FrameView();
  FrameView(const FrameView&) = delete;
  FrameView& operator=(const FrameView&) = delete;
  ~FrameView() override;

  // Helper for non-client view implementations to determine which area of the
  // window border the specified |point| falls within. The other parameters are
  // the size of the sizing edges, and whether or not the window can be
  // resized.
  int GetHTComponentForFrame(const gfx::Point& point,
                             const gfx::Insets& resize_border,
                             int top_resize_corner_height,
                             int resize_corner_width,
                             bool can_resize);

  // Returns the bounds (in this View's parent's coordinates) that the client
  // view should be laid out within.
  virtual gfx::Rect GetBoundsForClientView() const;

  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const;

  // Gets the clip mask (in this View's parent's coordinates) that should be
  // applied to the client view. Returns false if no special clip should be
  // used.
  virtual bool GetClientMask(const gfx::Size& size, SkPath* mask) const;

  // Returns whether FrameView has a custom title.
  // By default this returns false.
  // IMPORTANT: When a subclass of FrameView has a custom title,
  // HasWindowTitle() and IsWindowTitleVisible() need to be implemented to
  // ensure synchronization of title visibility when Widget::UpdateWindowTitle()
  // is called.
  virtual bool HasWindowTitle() const;

  // Returns whether the FrameView's window title is visible.
  // By default this returns false.
  // TODO(crbug.com/330198011): Implemented in subclasses of FrameView
  // when needed.
  virtual bool IsWindowTitleVisible() const;

#if BUILDFLAG(IS_WIN)
  // Returns the point in screen physical coordinates at which the system menu
  // should be opened.
  virtual gfx::Point GetSystemMenuScreenPixelLocation() const;
#endif

  // This function must ask the ClientView to do a hittest.  We don't do this in
  // the parent NonClientView because that makes it more difficult to calculate
  // hittests for regions that are partially obscured by the ClientView, e.g.
  // HTSYSMENU.
  // Return value is one of the windows HT constants (see ui/base/hit_test.h).
  virtual int NonClientHitTest(const gfx::Point& point);

  // Used to make the hosting widget shaped (non-rectangular). For a
  // rectangular window do nothing. For a shaped window update |window_mask|
  // accordingly. |size| is the size of the widget.
  virtual void GetWindowMask(const gfx::Size& size, SkPath* window_mask) {}
  virtual void ResetWindowControls() {}
  virtual void UpdateWindowIcon() {}
  virtual void UpdateWindowTitle() {}
  virtual void UpdateWindowRoundedCorners() {}

  // Whether the widget can be resized or maximized has changed.
  virtual void SizeConstraintsChanged() {}

  // Inserts the passed client view into this FrameView. Subclasses can
  // override this method to indicate a specific insertion spot for the client
  // view.
  virtual void InsertClientView(ClientView* client_view);

  // View:
  void OnThemeChanged() override;
  void Layout(PassKey) override;
  Views GetChildrenInZOrder() override;

 protected:
  // Used to determine if the frame should be painted as active. Convenience
  // method; equivalent to GetWidget()->ShouldPaintAsActive().
  bool ShouldPaintAsActive() const;

 private:
#if BUILDFLAG(IS_WIN)
  // Returns the y coordinate, in local coordinates, at which the system menu
  // should be opened.  Since this is in DIP, it does not include the 1 px
  // offset into the caption area; the caller will take care of this.
  virtual int GetSystemMenuY() const;
#endif
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, FrameView, View)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, FrameView)

#endif  // UI_VIEWS_WINDOW_FRAME_VIEW_H_
