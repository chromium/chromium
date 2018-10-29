// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_EXTRA_SKIA_VECTOR_RESOURCE_H_
#define UI_AURA_EXTRA_SKIA_VECTOR_RESOURCE_H_

#include <memory>

namespace gfx {
class SkiaVectorAnimation;
}  // namespace gfx

namespace aura_extra {

// Gets a vector graphic resource specified by |resource_id| from the current
// module data and returns it as a SkiaVectorAnimation object. This expects the
// resource to be gzipped.
std::unique_ptr<gfx::SkiaVectorAnimation> GetVectorAnimationNamed(
    int resource_id);

}  // namespace aura_extra

#endif  // UI_AURA_EXTRA_SKIA_VECTOR_RESOURCE_H_
