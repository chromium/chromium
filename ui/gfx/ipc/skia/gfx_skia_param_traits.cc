// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/pickle.h"
#include "ipc/param_traits_utils.h"
// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
#include "third_party/skia/include/core/SkImageInfo.h"
namespace IPC {
#undef UI_GFX_IPC_SKIA_GFX_SKIA_PARAM_TRAITS_MACROS_H_
#include "ui/gfx/ipc/skia/gfx_skia_param_traits_macros.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef UI_GFX_IPC_SKIA_GFX_SKIA_PARAM_TRAITS_MACROS_H_
#include "ui/gfx/ipc/skia/gfx_skia_param_traits_macros.h"
}  // namespace IPC

namespace IPC {

void ParamTraits<SkImageInfo>::Write(base::Pickle* m, const SkImageInfo& p) {
  WriteParam(m, p.colorType());
  WriteParam(m, p.alphaType());
  WriteParam(m, p.width());
  WriteParam(m, p.height());
}

bool ParamTraits<SkImageInfo>::Read(const base::Pickle* m,
                                    base::PickleIterator* iter,
                                    SkImageInfo* r) {
  SkColorType color_type;
  SkAlphaType alpha_type;
  uint32_t width;
  uint32_t height;
  if (!ReadParam(m, iter, &color_type) || !ReadParam(m, iter, &alpha_type) ||
      !ReadParam(m, iter, &width) || !ReadParam(m, iter, &height)) {
    return false;
  }

  *r = SkImageInfo::Make(width, height, color_type, alpha_type);
  return true;
}

}  // namespace IPC
