// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_CLIENT_VIEW_H_
#define UI_VIEWS_WINDOW_CLIENT_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

namespace views {

class Widget;
enum class CloseRequestResult;

///////////////////////////////////////////////////////////////////////////////
// ClientView
//
//  A ClientView is a View subclass that is used to occupy the "client area"
//  of a widget. It provides basic information to the widget that contains it
//  such as non-client hit testing information, sizing etc. Sub-classes of
//  ClientView are used to create more elaborate contents.
class VIEWS_EXPORT ClientView : public View {
  METADATA_HEADER(ClientView, View)

 public:
  // Constructs a ClientView object for the specified widget with the specified
  // contents. Since this object is created during the process of creating
  // |widget|, |contents_view| must be valid if you want the initial size of
  // the widget to be based on |contents_view|'s preferred size.
  ClientView(Widget* widget, View* contents_view);
  ~ClientView() override = default;

  // Returned value signals whether the Widget can be closed. Specialized
  // ClientView subclasses can override this default behavior to allow the
  // close to be blocked until the user corrects mistakes, accepts a warning
  // dialog, etc.
  virtual CloseRequestResult OnWindowCloseRequested();

  // Notification that the widget is closing.
  virtual void WidgetClosing();

  // Tests to see if the specified point (in view coordinates) is within the
  // bounds of this view. If so, it returns HTCLIENT in this default
  // implementation. If it is outside the bounds of this view, this must return
  // HTNOWHERE to tell the caller to do further processing to determine where
  // in the non-client area it is (if it is).
  // Subclasses of ClientView can extend this logic by overriding this method
  // to detect if regions within the client area count as parts of the "non-
  // client" area. A good example of this is the size box at the bottom right
  // corner of resizable dialog boxes.
  virtual int NonClientHitTest(const gfx::Point& point);

  // Updates and applies the `corner_radius` to the ClientView's contents as
  // part of rounding the window.
  // Some platforms, such as ChromeOS, do not have borders surrounding
  // ClientView part of the NonClientFrameView. Therefore, the
  // NonClientFrameView has to delegate part of the rounding logic to the
  // ClientView.
  virtual void UpdateWindowRoundedCorners(int corner_radius);

  // Overridden from View:
  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;

 protected:
  // Overridden from View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;

  // Accessors for private data members.
  View* contents_view() const { return contents_view_; }
  void set_contents_view(View* contents_view) {
    contents_view_ = contents_view;
  }

 private:
  // The View that this ClientView contains. This can temporarily dangle during
  // teardown of the Widget in some hard-to-resolve cases. Specifically, if the
  // contents view is also a WidgetDelegate (which happens with the
  // DialogDelegateView subclasses) *and* that WidgetDelegate is marked as owned
  // by the widget, the WidgetDelegate can be destroyed earlier during Widget
  // teardown than the View tree is, which can allow `this` to outlive the View
  // pointed to by `contents_view_`, even though `contents_view_` was previously
  // a child view of `this`.
  //
  // TODO(crbug.com/40279653): Fix that. Good luck!
  raw_ptr<View, DisableDanglingPtrDetection> contents_view_;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, ClientView, View)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, ClientView)

#endif  // UI_VIEWS_WINDOW_CLIENT_VIEW_H_
