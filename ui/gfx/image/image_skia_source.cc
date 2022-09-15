// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_skia_source.h"

namespace gfx {

ImageSkiaSource::~ImageSkiaSource() {}

bool ImageSkiaSource::HasRepresentationAtAllScales() const { return false; }

}  // namespace gfx
