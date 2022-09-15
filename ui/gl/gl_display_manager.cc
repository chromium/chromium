// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_display_manager.h"

namespace gl {
#if defined(USE_EGL)
template class EXPORT_TEMPLATE_DEFINE(GL_EXPORT) GLDisplayManager<GLDisplayEGL>;
#endif

#if defined(USE_GLX)
template class EXPORT_TEMPLATE_DEFINE(GL_EXPORT) GLDisplayManager<GLDisplayX11>;
#endif
}  // namespace gl
