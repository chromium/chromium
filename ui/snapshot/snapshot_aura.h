// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SNAPSHOT_SNAPSHOT_AURA_H_
#define UI_SNAPSHOT_SNAPSHOT_AURA_H_

#include "ui/snapshot/snapshot.h"

namespace ui {

class Layer;

// These functions are identical to those in snapshot.h, except they're
// guaranteed to read the frame using an Aura CopyOutputRequest and not the
// native windowing system. `source_rect` and `target_size` are in DIP.

SNAPSHOT_EXPORT void GrabWindowSnapshotAndScaleAura(
    aura::Window* window,
    const gfx::Rect& source_rect,
    const gfx::Size& target_size,
    GrabSnapshotImageCallback callback);

SNAPSHOT_EXPORT void GrabWindowSnapshotAura(aura::Window* window,
                                            const gfx::Rect& source_rect,
                                            GrabSnapshotImageCallback callback);

// Grabs a snapshot of a |layer| and all its descendants.
// |source_rect| is the bounds of the snapshot content relative to |layer|.
SNAPSHOT_EXPORT void GrabLayerSnapshot(Layer* layer,
                                       const gfx::Rect& source_rect,
                                       GrabSnapshotImageCallback callback);

}  // namespace ui

#endif  // UI_SNAPSHOT_SNAPSHOT_AURA_H_
