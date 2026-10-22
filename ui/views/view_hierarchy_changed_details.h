// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_HIERARCHY_CHANGED_DETAILS_H_
#define UI_VIEWS_VIEW_HIERARCHY_CHANGED_DETAILS_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/views_export.h"

namespace views {

class View;

// Struct used to describe how a View hierarchy has changed. See
// View::ViewHierarchyChanged.
struct VIEWS_EXPORT ViewHierarchyChangedDetails {
  ViewHierarchyChangedDetails(bool is_add,
                              View* parent,
                              View* child,
                              View* move_view)
      : is_add(is_add), parent(parent), child(child), move_view(move_view) {}
  const bool is_add;
  // New parent if |is_add| is true, old parent if |is_add| is false.
  const raw_ptr<View> parent;
  // The view being added or removed.
  const raw_ptr<View> child;
  // If this is a move (reparent), meaning AddChildViewAt() is invoked with an
  // existing parent, then a notification for the remove is sent first,
  // followed by one for the add.  This case can be distinguished by a
  // non-null |move_view|.
  // For the remove part of move, |move_view| is the new parent of the View
  // being removed.
  // For the add part of move, |move_view| is the old parent of the View being
  // added.
  const raw_ptr<View> move_view;
};

}  // namespace views

#endif  // UI_VIEWS_VIEW_HIERARCHY_CHANGED_DETAILS_H_
