// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DROP_HELPER_H_
#define UI_VIEWS_WIDGET_DROP_HELPER_H_

#include <utility>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "ui/views/views_export.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace ui {
class OSExchangeData;
}  // namespace ui
using ui::OSExchangeData;

namespace views {

class RootView;
class View;

// DropHelper provides support for managing the view a drop is going to occur
// at during dnd as well as sending the view the appropriate dnd methods.
// DropHelper is intended to be used by a class that interacts with the system
// drag and drop. The system class invokes OnDragOver as the mouse moves,
// then either OnDragExit or OnDrop when the drop is done.
class VIEWS_EXPORT DropHelper {
 public:
  explicit DropHelper(View* root_view);
  ~DropHelper();

  // Sets a callback that is run any time a drag enters |view|.  Only exposed
  // for testing.
  static void SetDragEnteredCallbackForTesting(const View* view,
                                               base::RepeatingClosure callback);

  // Current view drop events are targeted at, may be NULL.
  View* target_view() const { return target_view_; }

  // Returns the RootView the DropHelper was created with.
  View* root_view() const { return root_view_; }

  // Resets the target_view_ to NULL if it equals view.
  //
  // This is invoked when a View is removed from the RootView to make sure
  // we don't target a view that was removed during dnd.
  void ResetTargetViewIfEquals(View* view);

  // Invoked when a the mouse is dragged over the root view during a drag and
  // drop operation. This method returns a bitmask of the types in DragDropTypes
  // for the target view. If no view wants the drop, DRAG_NONE is returned.
  int OnDragOver(const OSExchangeData& data,
                 const gfx::Point& root_view_location,
                 int drag_operation);

  // Invoked when a the mouse is dragged out of the root view during a drag and
  // drop operation.
  void OnDragExit();

  // Invoked when the user drops data on the root view during a drag and drop
  // operation. See OnDragOver for details on return type.
  //
  // NOTE: implementations must invoke OnDragOver before invoking this,
  // supplying the return value from OnDragOver as the drag_operation.
  int OnDrop(const OSExchangeData& data,
             const gfx::Point& root_view_location,
             int drag_operation);

  // Calculates the target view for a drop given the specified location in
  // the coordinate system of the rootview. This tries to avoid continually
  // querying CanDrop by returning target_view_ if the mouse is still over
  // target_view_.
  View* CalculateTargetView(const gfx::Point& root_view_location,
                            const OSExchangeData& data,
                            bool check_can_drop);

 private:
  // Implementation of CalculateTargetView. If |deepest_view| is non-NULL it is
  // set to the deepest descendant of the RootView that contains the point
  // |root_view_location|
  View* CalculateTargetViewImpl(const gfx::Point& root_view_location,
                                const OSExchangeData& data,
                                bool check_can_drop,
                                View** deepest_view);

  // Methods to send the appropriate drop notification to the targeted view.
  // These do nothing if the target view is NULL.
  void NotifyDragEntered(const OSExchangeData& data,
                         const gfx::Point& root_view_location,
                         int drag_operation);
  int NotifyDragOver(const OSExchangeData& data,
                     const gfx::Point& root_view_location,
                     int drag_operation);
  void NotifyDragExit();

  // RootView we were created for.
  View* root_view_;

  // View we're targeting events at.
  View* target_view_;

  // The deepest view under the current drop coordinate.
  View* deepest_view_;

  DISALLOW_COPY_AND_ASSIGN(DropHelper);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DROP_HELPER_H_
