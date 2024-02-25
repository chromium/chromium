// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SNAPSHOT_SNAPSHOT_H_
#define UI_SNAPSHOT_SNAPSHOT_H_

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/snapshot/snapshot_export.h"

namespace gfx {
class Rect;
class Image;
class Size;
}

// Utility functions to grab snapshots of views and windows. These functions do
// no security checks, so these are useful for debugging purposes where no
// BrowserProcess instance is available (ie. tests), and other user-driven
// scenarios.

namespace ui {

using GrabSnapshotImageCallback = base::OnceCallback<void(gfx::Image snapshot)>;
using GrabSnapshotDataCallback =
    base::OnceCallback<void(scoped_refptr<base::RefCountedMemory> data)>;

// These functions take a snapshot of the specified view or window, within
// `source_rect`, specified in layer space coordinates (DIP for desktop,
// physical pixels for Android).
//
// Returns the snapshot via the provided `callback`. In case of an error, an
// empty image (`gfx::Image::IsEmpty()`) will be returned.

SNAPSHOT_EXPORT void GrabWindowSnapshot(gfx::NativeWindow window,
                                        const gfx::Rect& source_rect,
                                        GrabSnapshotImageCallback callback);

SNAPSHOT_EXPORT void GrabViewSnapshot(gfx::NativeView view,
                                      const gfx::Rect& source_rect,
                                      GrabSnapshotImageCallback callback);

// Takes a snapshot as with `GrabWindowSnapshot()` and scales it to
// `target_size` (in physical pixels).
//
// Returns the snapshot via the provided `callback`. In case of an error, an
// empty image (`gfx::Image::IsEmpty()`) will be returned.
SNAPSHOT_EXPORT void GrabWindowSnapshotAndScale(
    gfx::NativeWindow window,
    const gfx::Rect& source_rect,
    const gfx::Size& target_size,
    GrabSnapshotImageCallback callback);

// Takes a snapshot as with `GrabWindowSnapshot()` and encodes it as PNG data.
//
// Returns the data via the provided `callback`. In case of an error, a null
// pointer will be returned.
SNAPSHOT_EXPORT void GrabWindowSnapshotAsPNG(gfx::NativeWindow window,
                                             const gfx::Rect& source_rect,
                                             GrabSnapshotDataCallback callback);

// Takes a snapshot as with `GrabWindowSnapshot()` and encodes it as JPEG data.
//
// Returns the data via the provided `callback`. In case of an error, a null
// pointer will be returned.
SNAPSHOT_EXPORT void GrabWindowSnapshotAsJPEG(
    gfx::NativeWindow window,
    const gfx::Rect& source_rect,
    GrabSnapshotDataCallback callback);

}  // namespace ui

#endif  // UI_SNAPSHOT_SNAPSHOT_H_
