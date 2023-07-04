// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/drag_utils.h"

#include "base/notreached.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace views {

void RunShellDrag(gfx::NativeView view,
                  std::unique_ptr<ui::OSExchangeData> data,
                  const gfx::Point& location,
                  int operation,
                  ui::mojom::DragEventSource source) {
  NOTIMPLEMENTED();
}

}  // namespace views
