// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SCOPED_NS_GRAPHICS_CONTEXT_SAVE_GSTATE_MAC_H_
#define UI_GFX_SCOPED_NS_GRAPHICS_CONTEXT_SAVE_GSTATE_MAC_H_

#include "ui/gfx/gfx_export.h"

#if defined(__OBJC__)
@class NSGraphicsContext;
#else
class NSGraphicsContext;
#endif

namespace gfx {

// A class to save/restore the state of the current context.
class GFX_EXPORT ScopedNSGraphicsContextSaveGState {
 public:
  ScopedNSGraphicsContextSaveGState();

  ScopedNSGraphicsContextSaveGState(const ScopedNSGraphicsContextSaveGState&) =
      delete;
  ScopedNSGraphicsContextSaveGState& operator=(
      const ScopedNSGraphicsContextSaveGState&) = delete;

  ~ScopedNSGraphicsContextSaveGState();

 private:
  NSGraphicsContext* context_;  // weak
};

}  // namespace gfx

#endif  // UI_GFX_SCOPED_NS_GRAPHICS_CONTEXT_SAVE_GSTATE_MAC_H_
