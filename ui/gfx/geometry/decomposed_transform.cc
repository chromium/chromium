// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/geometry/decomposed_transform.h"

#include <cstring>

#include "base/strings/stringprintf.h"

namespace gfx {

std::string DecomposedTransform::ToString() const {
  return base::StringPrintf(
      "translate: %+lg %+lg %+lg\n"
      "scale: %+lg %+lg %+lg\n"
      "skew: %+lg %+lg %+lg\n"
      "perspective: %+lg %+lg %+lg %+lg\n"
      "quaternion: %+lg %+lg %+lg %+lg\n",
      translate[0], translate[1], translate[2], scale[0], scale[1], scale[2],
      skew[0], skew[1], skew[2], perspective[0], perspective[1], perspective[2],
      perspective[3], quaternion.x(), quaternion.y(), quaternion.z(),
      quaternion.w());
}

}  // namespace gfx
