// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IMAGE_IMAGE_PNG_REP_H_
#define UI_GFX_IMAGE_IMAGE_PNG_REP_H_

#include "base/component_export.h"
#include "base/memory/ref_counted_memory.h"

namespace gfx {
class Size;

// An ImagePNGRep represents a bitmap's png encoded data and the scale factor it
// was intended for.
struct COMPONENT_EXPORT(GFX) ImagePNGRep {
 public:
  ImagePNGRep();
  ImagePNGRep(const scoped_refptr<base::RefCountedMemory>& data,
              float data_scale);
  ImagePNGRep(const ImagePNGRep& other);
  ~ImagePNGRep();

  // Width and height of the image, in pixels.
  // If the image is invalid, returns gfx::Size(0, 0).
  // Warning: This operation processes the entire image stream, so its result
  // should be cached if it is needed multiple times.
  gfx::Size Size() const;

  scoped_refptr<base::RefCountedMemory> raw_data;
  float scale = 1.0f;
};

}  // namespace gfx

#endif  // UI_GFX_IMAGE_IMAGE_PNG_REP_H_
