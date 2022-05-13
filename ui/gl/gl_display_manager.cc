// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_display_manager.h"

namespace gl {
#if defined(USE_EGL)
template class GLDisplayManager<GLDisplayEGL>;
#endif

#if defined(USE_GLX)
template class GLDisplayManager<GLDisplayX11>;
#endif
}  // namespace gl
