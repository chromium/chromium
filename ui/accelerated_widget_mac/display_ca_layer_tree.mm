// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accelerated_widget_mac/display_ca_layer_tree.h"

#import <Cocoa/Cocoa.h>
#include <IOSurface/IOSurface.h>

#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/trace_event/trace_event.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size_conversions.h"

@interface CALayer (PrivateAPI)
- (void)setContentsChanged;
@end

namespace ui {
namespace {

// The maximum number of times to dump before throttling (to avoid sending
// thousands of crash dumps).
const int kMaxCrashDumps = 10;

}  // namespace

DisplayCALayerTree::DisplayCALayerTree(CALayer* root_layer)
    : root_layer_([root_layer retain]) {
  // Disable the fade-in animation as the layers are added.
  ScopedCAActionDisabler disabler;

  // Add a flipped transparent layer as a child, so that we don't need to
  // fiddle with the position of sub-layers -- they will always be at the
  // origin.
  flipped_layer_.reset([[CALayer alloc] init]);
  [flipped_layer_ setGeometryFlipped:YES];
  [flipped_layer_ setAnchorPoint:CGPointMake(0, 0)];
  [flipped_layer_
      setAutoresizingMask:kCALayerWidthSizable | kCALayerHeightSizable];
  [root_layer_ addSublayer:flipped_layer_];
}

DisplayCALayerTree::~DisplayCALayerTree() {
  // Disable the fade-out animation as the view is removed.
  ScopedCAActionDisabler disabler;

  [flipped_layer_ removeFromSuperlayer];
  [remote_layer_ removeFromSuperlayer];
  [io_surface_layer_ removeFromSuperlayer];
  remote_layer_.reset();
  io_surface_layer_.reset();
}

void DisplayCALayerTree::UpdateCALayerTree(
    const gfx::CALayerParams& ca_layer_params) {
  // Remote layers are the most common case.
  if (ca_layer_params.ca_context_id) {
    GotCALayerFrame(ca_layer_params.ca_context_id);
    return;
  }

  // IOSurfaces can be sent from software compositing, or if remote layers are
  // manually disabled.
  if (ca_layer_params.io_surface_mach_port) {
    base::ScopedCFTypeRef<IOSurfaceRef> io_surface(
        IOSurfaceLookupFromMachPort(ca_layer_params.io_surface_mach_port));
    if (io_surface) {
      GotIOSurfaceFrame(io_surface, ca_layer_params.pixel_size,
                        ca_layer_params.scale_factor);
      return;
    }
    LOG(ERROR) << "Unable to open IOSurface for frame.";
    static int dump_counter = kMaxCrashDumps;
    if (dump_counter) {
      dump_counter -= 1;
      base::debug::DumpWithoutCrashing();
    }
  }

  // Warn if the frame specified neither.
  if (ca_layer_params.io_surface_mach_port && !ca_layer_params.ca_context_id)
    LOG(ERROR) << "Frame had neither valid CAContext nor valid IOSurface.";

  // If there was an error or if the frame specified nothing, then clear all
  // contents.
  if (io_surface_layer_ || remote_layer_) {
    ScopedCAActionDisabler disabler;
    [io_surface_layer_ removeFromSuperlayer];
    io_surface_layer_.reset();
    [remote_layer_ removeFromSuperlayer];
    remote_layer_.reset();
  }
}

void DisplayCALayerTree::GotCALayerFrame(uint32_t ca_context_id) {
  // Early-out if the remote layer has not changed.
  if ([remote_layer_ contextId] == ca_context_id)
    return;

  TRACE_EVENT0("ui", "DisplayCALayerTree::GotCAContextFrame");
  ScopedCAActionDisabler disabler;

  // Create the new CALayerHost.
  base::scoped_nsobject<CALayerHost> new_remote_layer(
      [[CALayerHost alloc] init]);
  [new_remote_layer setContextId:ca_context_id];
  [new_remote_layer
      setAutoresizingMask:kCALayerMaxXMargin | kCALayerMaxYMargin];

  // Update the local CALayer tree.
  [flipped_layer_ addSublayer:new_remote_layer];
  [remote_layer_ removeFromSuperlayer];
  remote_layer_ = new_remote_layer;

  // Ensure that the IOSurface layer be removed.
  if (io_surface_layer_) {
    [io_surface_layer_ removeFromSuperlayer];
    io_surface_layer_.reset();
  }
}

void DisplayCALayerTree::GotIOSurfaceFrame(
    base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
    const gfx::Size& pixel_size,
    float scale_factor) {
  DCHECK(io_surface);
  TRACE_EVENT0("ui", "DisplayCALayerTree::GotIOSurfaceFrame");
  ScopedCAActionDisabler disabler;

  // Create (if needed) and update the IOSurface layer with new content.
  if (!io_surface_layer_) {
    io_surface_layer_.reset([[CALayer alloc] init]);
    [io_surface_layer_ setContentsGravity:kCAGravityTopLeft];
    [io_surface_layer_ setAnchorPoint:CGPointMake(0, 0)];
    [flipped_layer_ addSublayer:io_surface_layer_];
  }
  id new_contents = static_cast<id>(io_surface.get());
  if (new_contents && new_contents == [io_surface_layer_ contents])
    [io_surface_layer_ setContentsChanged];
  else
    [io_surface_layer_ setContents:new_contents];
  // TODO(danakj): We should avoid lossy conversions to integer DIPs. The OS
  // wants a floating point value.
  gfx::Size bounds_dip =
      gfx::ToFlooredSize(gfx::ConvertSizeToDips(pixel_size, scale_factor));
  [io_surface_layer_
      setBounds:CGRectMake(0, 0, bounds_dip.width(), bounds_dip.height())];
  if ([io_surface_layer_ contentsScale] != scale_factor)
    [io_surface_layer_ setContentsScale:scale_factor];

  // Ensure that the remote layer be removed.
  if (remote_layer_) {
    [remote_layer_ removeFromSuperlayer];
    remote_layer_.reset();
  }
}

}  // namespace ui
