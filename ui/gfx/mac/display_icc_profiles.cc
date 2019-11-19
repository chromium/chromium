// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mac/display_icc_profiles.h"

#include "ui/gfx/icc_profile.h"

namespace gfx {

DisplayICCProfiles* DisplayICCProfiles::GetInstance() {
  static base::NoDestructor<DisplayICCProfiles> profiles;
  return profiles.get();
}

base::ScopedCFTypeRef<CFDataRef> DisplayICCProfiles::GetDataForColorSpace(
    const ColorSpace& color_space) {
  UpdateIfNeeded();
  base::ScopedCFTypeRef<CFDataRef> result;
  auto found = map_.find(color_space);
  if (found != map_.end())
    result = found->second;
  return result;
}

DisplayICCProfiles::DisplayICCProfiles() {
  CGDisplayRegisterReconfigurationCallback(
      DisplayICCProfiles::DisplayReconfigurationCallBack, this);
}

DisplayICCProfiles::~DisplayICCProfiles() {
  NOTREACHED();
}

void DisplayICCProfiles::UpdateIfNeeded() {
  if (!needs_update_)
    return;
  needs_update_ = false;
  map_.clear();

  // Always add Apple's sRGB profile.
  base::ScopedCFTypeRef<CFDataRef> srgb_icc(CGColorSpaceCopyICCProfile(
      CGColorSpaceCreateWithName(kCGColorSpaceSRGB)));
  map_[ColorSpace::CreateSRGB()] = srgb_icc;

  // Add the profiles for all active displays.
  uint32_t display_count = 0;
  CGError error = kCGErrorSuccess;
  error = CGGetActiveDisplayList(0, nullptr, &display_count);
  if (error != kCGErrorSuccess)
    return;
  if (!display_count)
    return;

  std::vector<CGDirectDisplayID> displays(display_count);
  error =
      CGGetActiveDisplayList(displays.size(), displays.data(), &display_count);
  if (error != kCGErrorSuccess)
    return;

  for (uint32_t i = 0; i < display_count; ++i) {
    base::ScopedCFTypeRef<CGColorSpaceRef> cg_color_space(
        CGDisplayCopyColorSpace(displays[i]));
    if (!cg_color_space)
      continue;
    base::ScopedCFTypeRef<CFDataRef> icc_data(
        CGColorSpaceCopyICCProfile(cg_color_space));
    if (!icc_data)
      continue;
    ICCProfile icc_profile = ICCProfile::FromData(CFDataGetBytePtr(icc_data),
                                                  CFDataGetLength(icc_data));
    ColorSpace color_space = icc_profile.GetColorSpace();
    // If the ICC profile isn't accurately parametrically approximated, then
    // don't store its data (we will assign the best parametric fit to
    // IOSurfaces, and rely on the system compositor to do conversion to the
    // display profile).
    if (color_space.IsValid() && icc_profile.IsColorSpaceAccurate())
      map_[color_space] = icc_data;
  }
}

// static
void DisplayICCProfiles::DisplayReconfigurationCallBack(
    CGDirectDisplayID display,
    CGDisplayChangeSummaryFlags flags,
    void* user_info) {
  DisplayICCProfiles* profiles =
      reinterpret_cast<DisplayICCProfiles*>(user_info);
  profiles->needs_update_ = true;
}

}  // namespace gfx
