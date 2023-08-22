// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MAC_DISPLAY_ICC_PROFILES_H_
#define UI_GFX_MAC_DISPLAY_ICC_PROFILES_H_

#include <CoreGraphics/CoreGraphics.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_space_export.h"

namespace gfx {

// A map from ColorSpace objects to the display ICC profile data from which the
// ColorSpace was derived.
//  - The color space for an IOSurface, when composited by CoreAnimation, is
//    specified via ICC profile metadata.
//  - The power cost of compositing an IOSurface that has the same color space
//    as the display it is being composited to is substantially less (~0.5 W for
//    fullscreen updates at 60fps) than the cost of compositing an IOSurface
//    that has a different color space than the display is being composited to.
//  - This power savings is realized only if the ICC profile metadata on the
//    IOSurface matches, byte-for-byte, the profile of the CGDirectDisplayID it
//    is being displayed on.
//  - This structure maintains a map from ColorSpace objects to ICC profile data
//    for all displays in the system (and auto-updates as displays change).
class COLOR_SPACE_EXPORT DisplayICCProfiles {
 public:
  static DisplayICCProfiles* GetInstance();

  DisplayICCProfiles(const DisplayICCProfiles&) = delete;
  DisplayICCProfiles& operator=(const DisplayICCProfiles&) = delete;

  // This will return null if |color_space| does not correspond to a display.
  base::apple::ScopedCFTypeRef<CFDataRef> GetDataForColorSpace(
      const ColorSpace& color_space);

 private:
  friend class base::NoDestructor<DisplayICCProfiles>;

  static void DisplayReconfigurationCallBack(CGDirectDisplayID display,
                                             CGDisplayChangeSummaryFlags flags,
                                             void* user_info);

  DisplayICCProfiles();
  ~DisplayICCProfiles();

  void UpdateIfNeeded();

  base::flat_map<ColorSpace, base::apple::ScopedCFTypeRef<CFDataRef>> map_;
  bool needs_update_ = true;
};

}  // namespace gfx

#endif  // UI_GFX_MAC_DISPLAY_ICC_PROFILES_H_
