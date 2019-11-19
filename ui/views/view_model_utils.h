// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_MODEL_UTILS_H_
#define UI_VIEWS_VIEW_MODEL_UTILS_H_

#include "base/macros.h"
#include "ui/views/views_export.h"

namespace views {

class View;
class ViewModelBase;

class VIEWS_EXPORT ViewModelUtils {
 public:
  // Sets the bounds of each view to its ideal bounds.
  static void SetViewBoundsToIdealBounds(const ViewModelBase& model);

  // Returns true if the Views in |model| are at their ideal bounds.
  static bool IsAtIdealBounds(const ViewModelBase& model);

  // Returns the index to move |view| to based on a coordinate of |x| and |y|.
  static int DetermineMoveIndex(const ViewModelBase& model,
                                View* view,
                                bool is_horizontal,
                                int x,
                                int y);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ViewModelUtils);
};

}  // namespace views

#endif  // UI_VIEWS_VIEW_MODEL_UTILS_H_
