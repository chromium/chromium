// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GLX_UTIL_H_
#define UI_GL_GLX_UTIL_H_

#include "ui/gfx/x/x11_types.h"
#include "ui/gl/gl_export.h"

namespace gl {

GL_EXPORT GLXFBConfig GetFbConfigForWindow(x11::Connection* connection,
                                           x11::Window window);

}

#endif  // UI_GL_GLX_UTIL_H_
