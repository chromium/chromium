// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_X11_PATH_H_
#define UI_GFX_X_X11_PATH_H_

#include "base/component_export.h"
#include "ui/gfx/x/xproto.h"

class SkPath;
class SkRegion;

namespace x11 {

// Creates a new XRegion given |region|. The caller is responsible for
// destroying the returned region.
COMPONENT_EXPORT(X11)
std::unique_ptr<std::vector<Rectangle>> CreateRegionFromSkRegion(
    const SkRegion& region);

// Creates a new XRegion given |path|. The caller is responsible for destroying
// the returned region.
COMPONENT_EXPORT(X11)
std::unique_ptr<std::vector<Rectangle>> CreateRegionFromSkPath(
    const SkPath& path);

}  // namespace x11

#endif  // UI_GFX_X_X11_PATH_H_
