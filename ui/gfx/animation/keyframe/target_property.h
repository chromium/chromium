// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANIMATION_KEYFRAME_TARGET_PROPERTY_H_
#define UI_GFX_ANIMATION_KEYFRAME_TARGET_PROPERTY_H_

#include <bitset>

namespace gfx {

static constexpr size_t kMaxTargetPropertyIndex = 32u;

// A set of target properties.
using TargetProperties = std::bitset<kMaxTargetPropertyIndex>;

}  // namespace gfx

#endif  // UI_GFX_ANIMATION_KEYFRAME_TARGET_PROPERTY_H_
