// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_TRACKING_AREA_H_
#define UI_BASE_COCOA_TRACKING_AREA_H_

#import <AppKit/AppKit.h>

#include "base/component_export.h"

// The CrTrackingArea can be used in place of an NSTrackingArea to shut off
// messaging to the |owner| at a specific point in time.
COMPONENT_EXPORT(UI_BASE)
@interface CrTrackingArea : NSTrackingArea

// Designated initializer. Forwards all arguments to the superclass, but wraps
// |owner| in a proxy object.
- (instancetype)initWithRect:(NSRect)rect
           options:(NSTrackingAreaOptions)options
             owner:(id)owner
          userInfo:(NSDictionary*)userInfo;

// Prevents any future messages from being delivered to the |owner|.
- (void)clearOwner;

@end

// Scoper //////////////////////////////////////////////////////////////////////

namespace ui {

// Use an instance of this class to call |-clearOwner| on the |tracking_area_|
// when this goes out of scope.
class COMPONENT_EXPORT(UI_BASE) ScopedCrTrackingArea {
 public:
  // Takes ownership of |tracking_area|.
  explicit ScopedCrTrackingArea(CrTrackingArea* tracking_area = nil);

  ScopedCrTrackingArea(const ScopedCrTrackingArea&) = delete;
  ScopedCrTrackingArea& operator=(const ScopedCrTrackingArea&) = delete;

  ~ScopedCrTrackingArea();

  // This will take ownership of the new tracking area.  Note that -clearOwner
  // is NOT called on the existing tracking area.
  void reset(CrTrackingArea* tracking_area = nil);

  CrTrackingArea* get() const;

 private:
  CrTrackingArea* __strong tracking_area_;
};

}  // namespace ui

#endif  // UI_BASE_COCOA_TRACKING_AREA_H_
