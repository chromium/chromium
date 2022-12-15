// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/presenter.h"

namespace gl {

Presenter::Presenter(GLDisplayEGL* display, const gfx::Size& size)
    : SurfacelessEGL(display, size) {}
Presenter::~Presenter() = default;

}  // namespace gl