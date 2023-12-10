// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/ipc/buffer_types/gfx_param_traits.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"

namespace IPC {

void ParamTraits<gfx::BufferUsageAndFormat>::Write(
    base::Pickle* m,
    const gfx::BufferUsageAndFormat& p) {
  WriteParam(m, p.usage);
  WriteParam(m, p.format);
}

bool ParamTraits<gfx::BufferUsageAndFormat>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    gfx::BufferUsageAndFormat* r) {
  if (!ReadParam(m, iter, &r->usage) || !ReadParam(m, iter, &r->format))
    return false;
  return true;
}

void ParamTraits<gfx::BufferUsageAndFormat>::Log(
    const gfx::BufferUsageAndFormat& p,
    std::string* l) {
  l->append(base::StringPrintf("(%d, %u)", static_cast<int>(p.usage),
                               base::strict_cast<uint32_t>(p.format)));
}

}  // namespace IPC

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef UI_GFX_IPC_BUFFER_TYPES_GFX_PARAM_TRAITS_MACROS_H_
#include "ui/gfx/ipc/buffer_types/gfx_param_traits_macros.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef UI_GFX_IPC_BUFFER_TYPES_GFX_PARAM_TRAITS_MACROS_H_
#include "ui/gfx/ipc/buffer_types/gfx_param_traits_macros.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef UI_GFX_IPC_BUFFER_TYPES_GFX_PARAM_TRAITS_MACROS_H_
#include "ui/gfx/ipc/buffer_types/gfx_param_traits_macros.h"
}  // namespace IPC
