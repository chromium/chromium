// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_MOJOM_FRAME_DATA_MOJOM_TRAITS_H_
#define UI_GL_MOJOM_FRAME_DATA_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/mojom/frame_data.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<gl::mojom::FrameDataDataView, gl::FrameData> {
  static int64_t seq(const gl::FrameData& data) { return data.seq; }

  static bool Read(gl::mojom::FrameDataDataView data, gl::FrameData* out) {
    out->seq = data.seq();
    return true;
  }
};

}  // namespace mojo

#endif  // UI_GL_MOJOM_FRAME_DATA_MOJOM_TRAITS_H_
