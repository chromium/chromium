// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/scoped_canvas.h"

#include "base/i18n/rtl.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {

ScopedCanvas::ScopedCanvas(gfx::Canvas* canvas) : canvas_(canvas) {
  if (canvas_)
    canvas_->Save();
}

ScopedCanvas::~ScopedCanvas() {
  if (canvas_)
    canvas_->Restore();
}

void ScopedCanvas::FlipIfRTL(int width) {
  if (base::i18n::IsRTL()) {
    canvas_->Translate(gfx::Vector2d(width, 0));
    canvas_->Scale(-1, 1);
  }
}

}  // namespace gfx
