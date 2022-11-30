// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/vector_icon_utils.h"

#include <ostream>

#include "base/check_op.h"
#include "ui/gfx/vector_icon_types.h"

namespace gfx {

int GetDefaultSizeOfVectorIcon(const VectorIcon& icon) {
  if (icon.is_empty())
    return -1;
  const PathElement* default_icon_path = icon.reps[icon.reps_size - 1].path;
  DCHECK_EQ(default_icon_path[0].command, CANVAS_DIMENSIONS)
      << " " << icon.name
      << " has no size in its icon definition, and it seems unlikely you want "
         "to display at the default of 48dip. Please specify a size in "
         "CreateVectorIcon().";
  return default_icon_path[1].arg;
}

}  // namespace gfx
