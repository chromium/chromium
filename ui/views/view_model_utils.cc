// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_model_utils.h"

#include <algorithm>

#include "ui/views/view.h"
#include "ui/views/view_model.h"

namespace views {

namespace {

// Used in calculating ideal bounds.
int primary_axis_coordinate(bool is_horizontal, int x, int y) {
  return is_horizontal ? x : y;
}

}  // namespace

// static
void ViewModelUtils::SetViewBoundsToIdealBounds(const ViewModelBase& model) {
  for (int i = 0; i < model.view_size(); ++i)
    model.ViewAtBase(i)->SetBoundsRect(model.ideal_bounds(i));
}

// static
bool ViewModelUtils::IsAtIdealBounds(const ViewModelBase& model) {
  for (int i = 0; i < model.view_size(); ++i) {
    View* view = model.ViewAtBase(i);
    if (view->bounds() != model.ideal_bounds(i))
      return false;
  }
  return true;
}

// static
int ViewModelUtils::DetermineMoveIndex(const ViewModelBase& model,
                                       View* view,
                                       bool is_horizontal,
                                       int x,
                                       int y) {
  int value = primary_axis_coordinate(is_horizontal, x, y);
  int current_index = model.GetIndexOfView(view);
  DCHECK_NE(-1, current_index);
  for (int i = 0; i < current_index; ++i) {
    int mid_point = primary_axis_coordinate(
        is_horizontal,
        model.ideal_bounds(i).x() + model.ideal_bounds(i).width() / 2,
        model.ideal_bounds(i).y() + model.ideal_bounds(i).height() / 2);
    if (value < mid_point)
      return i;
  }

  if (current_index + 1 == model.view_size())
    return current_index;

  // For indices after the current index ignore the bounds of the view being
  // dragged. This keeps the view from bouncing around as moved.
  int delta =
      primary_axis_coordinate(is_horizontal,
                              model.ideal_bounds(current_index + 1).x() -
                                  model.ideal_bounds(current_index).x(),
                              model.ideal_bounds(current_index + 1).y() -
                                  model.ideal_bounds(current_index).y());
  for (int i = current_index + 1; i < model.view_size(); ++i) {
    const gfx::Rect& bounds(model.ideal_bounds(i));
    int mid_point = primary_axis_coordinate(
        is_horizontal, bounds.x() + bounds.width() / 2 - delta,
        bounds.y() + bounds.height() / 2 - delta);
    if (value < mid_point)
      return i - 1;
  }
  return model.view_size() - 1;
}

}  // namespace views
