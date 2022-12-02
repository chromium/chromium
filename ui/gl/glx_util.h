// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GLX_UTIL_H_
#define UI_GL_GLX_UTIL_H_

#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/glx.h"
#include "ui/gl/gl_export.h"

using GLXFBConfig = struct __GLXFBConfigRec*;

namespace gfx {
class NativePixmapDmaBuf;
enum class BufferFormat;
}  // namespace gfx

namespace gl {

GL_EXPORT GLXFBConfig GetFbConfigForWindow(x11::Connection* connection,
                                           x11::Window window);

GL_EXPORT GLXFBConfig
GetGlxFbConfigForXProtoFbConfig(x11::Connection* connection,
                                x11::Glx::FbConfig xproto_config);

GL_EXPORT x11::Pixmap XPixmapFromNativePixmap(
    const gfx::NativePixmapDmaBuf& native_pixmap,
    gfx::BufferFormat buffer_format);

}  // namespace gl

#endif  // UI_GL_GLX_UTIL_H_
