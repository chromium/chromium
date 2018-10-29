// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_PATH_H_
#define UI_GFX_PATH_H_

#include <stddef.h>

#include "base/macros.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {

// DEPRECATED, use SkPath directly.
// TODO(collinbaker): remove this class and replace all references with SkPath.
class GFX_EXPORT Path : public SkPath {
 public:
  Path();
  ~Path();
};

}

#endif  // UI_GFX_PATH_H_
