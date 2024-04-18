// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_png_rep.h"

#include "base/logging.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {

ImagePNGRep::ImagePNGRep() = default;

ImagePNGRep::ImagePNGRep(const scoped_refptr<base::RefCountedMemory>& data,
                         float data_scale)
    : raw_data(data),
      scale(data_scale) {
}

ImagePNGRep::ImagePNGRep(const ImagePNGRep& other) = default;

ImagePNGRep::~ImagePNGRep() {
}

gfx::Size ImagePNGRep::Size() const {
  // The only way to get the width and height of a raw PNG stream, at least
  // using the gfx::PNGCodec API, is to decode the whole thing.
  CHECK(raw_data.get());
  SkBitmap bitmap;
  if (!gfx::PNGCodec::Decode(raw_data->data(), raw_data->size(), &bitmap)) {
    LOG(ERROR) << "Unable to decode PNG.";
    return gfx::Size(0, 0);
  }
  return gfx::Size(bitmap.width(), bitmap.height());
}

}  // namespace gfx
