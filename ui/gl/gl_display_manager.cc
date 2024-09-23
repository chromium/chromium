// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_display_manager.h"

namespace gl {
template class EXPORT_TEMPLATE_DEFINE(GL_EXPORT) GLDisplayManager<GLDisplayEGL>;
}  // namespace gl
