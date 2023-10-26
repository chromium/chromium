// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accelerated_widget_mac/display_ca_layer_tree.h"

#import <QuartzCore/QuartzCore.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
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
    : root_layer_(root_layer) {
  // Disable the fade-in animation as the layers are added.
  ScopedCAActionDisabler disabler;

  // Add a flipped transparent layer as a child, so that we don't need to
  // fiddle with the position of sub-layers -- they will always be at the
  // origin. Note that flipping is only applicable to macOS.
  maybe_flipped_layer_ = [[CALayer alloc] init];
#if BUILDFLAG(IS_MAC)
  maybe_flipped_layer_.geometryFlipped = YES;
  maybe_flipped_layer_.autoresizingMask =
      kCALayerWidthSizable | kCALayerHeightSizable;
#endif
  maybe_flipped_layer_.anchorPoint = CGPointZero;
  [root_layer_ addSublayer:maybe_flipped_layer_];

#if BUILDFLAG(IS_IOS)
  [root_layer_ setDrawsAsynchronously:YES];
#endif
}

DisplayCALayerTree::~DisplayCALayerTree() {
  // Disable the fade-out animation as the view is removed.
  ScopedCAActionDisabler disabler;

  [maybe_flipped_layer_ removeFromSuperlayer];
  [remote_layer_ removeFromSuperlayer];
  [io_surface_layer_ removeFromSuperlayer];
  remote_layer_ = nil;
  io_surface_layer_ = nil;
}

void DisplayCALayerTree::UpdateCALayerTree(
    const gfx::CALayerParams& ca_layer_params) {
  // TODO(danakj): We should avoid lossy conversions to integer DIPs. The OS
  // wants a floating point value.
  gfx::Size dip_size = gfx::ToFlooredSize(gfx::ConvertSizeToDips(
      ca_layer_params.pixel_size, ca_layer_params.scale_factor));

  // iOS doesn't support autoresizing mask. Thus, adjust the bounds.
#if BUILDFLAG(IS_IOS)
  maybe_flipped_layer_.bounds =
      CGRectMake(0, 0, dip_size.width(), dip_size.height());

  if (maybe_flipped_layer_.contentsScale != ca_layer_params.scale_factor) {
    maybe_flipped_layer_.contentsScale = ca_layer_params.scale_factor;
  }
#endif

  // Remote layers are the most common case.
  if (ca_layer_params.ca_context_id) {
    GotCALayerFrame(ca_layer_params.ca_context_id);
    return;
  }

  // IOSurfaces can be sent from software compositing, or if remote layers are
  // manually disabled.
  if (ca_layer_params.io_surface_mach_port) {
    base::apple::ScopedCFTypeRef<IOSurfaceRef> io_surface(
        IOSurfaceLookupFromMachPort(
            ca_layer_params.io_surface_mach_port.get()));
    if (io_surface) {
      GotIOSurfaceFrame(io_surface, dip_size, ca_layer_params.scale_factor);
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
  if (ca_layer_params.io_surface_mach_port && !ca_layer_params.ca_context_id) {
    LOG(ERROR) << "Frame had neither valid CAContext nor valid IOSurface.";
  }

  // If there was an error or if the frame specified nothing, then clear all
  // contents.
  if (io_surface_layer_ || remote_layer_) {
    ScopedCAActionDisabler disabler;
    [io_surface_layer_ removeFromSuperlayer];
    io_surface_layer_ = nil;
    [remote_layer_ removeFromSuperlayer];
    remote_layer_ = nil;
  }
}

void DisplayCALayerTree::GotCALayerFrame(uint32_t ca_context_id) {
  // Early-out if the remote layer has not changed.
  if (remote_layer_.contextId == ca_context_id) {
    return;
  }

  TRACE_EVENT0("ui", "DisplayCALayerTree::GotCAContextFrame");
  ScopedCAActionDisabler disabler;

  // Create the new CALayerHost.
  CALayerHost* new_remote_layer = [[CALayerHost alloc] init];
  // Anchor point on iOS might be at (0.5,0.5) as it's a default value there.
  // Thus, explicitly set it to (0,0), which doesn't hurt macOS as it also
  // expects to have all the attached layers of the context at (0,0).
  new_remote_layer.anchorPoint = CGPointZero;
  new_remote_layer.contextId = ca_context_id;
#if BUILDFLAG(IS_MAC)
  new_remote_layer.autoresizingMask = kCALayerMaxXMargin | kCALayerMaxYMargin;
#endif

  // Update the local CALayer tree.
  [maybe_flipped_layer_ addSublayer:new_remote_layer];
  [remote_layer_ removeFromSuperlayer];
  remote_layer_ = new_remote_layer;

  // Ensure that the IOSurface layer be removed.
  if (io_surface_layer_) {
    [io_surface_layer_ removeFromSuperlayer];
    io_surface_layer_ = nil;
  }
}

void DisplayCALayerTree::GotIOSurfaceFrame(
    base::apple::ScopedCFTypeRef<IOSurfaceRef> io_surface,
    const gfx::Size& dip_size,
    float scale_factor) {
  DCHECK(io_surface);
  TRACE_EVENT0("ui", "DisplayCALayerTree::GotIOSurfaceFrame");
  ScopedCAActionDisabler disabler;

  // Create (if needed) and update the IOSurface layer with new content.
  if (!io_surface_layer_) {
    io_surface_layer_ = [[CALayer alloc] init];
    io_surface_layer_.contentsGravity = kCAGravityTopLeft;
    io_surface_layer_.anchorPoint = CGPointZero;
    [maybe_flipped_layer_ addSublayer:io_surface_layer_];
  }
  id new_contents = (__bridge id)io_surface.get();
  if (new_contents && new_contents == io_surface_layer_.contents) {
    [io_surface_layer_ setContentsChanged];
  } else {
    io_surface_layer_.contents = new_contents;
  }

  io_surface_layer_.bounds =
      CGRectMake(0, 0, dip_size.width(), dip_size.height());
  if (io_surface_layer_.contentsScale != scale_factor) {
    io_surface_layer_.contentsScale = scale_factor;
  }

  // Ensure that the remote layer be removed.
  if (remote_layer_) {
    [remote_layer_ removeFromSuperlayer];
    remote_layer_ = nil;
  }
}

}  // namespace ui
