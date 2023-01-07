// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IPC_BUFFER_TYPES_GFX_PARAM_TRAITS_H_
#define UI_GFX_IPC_BUFFER_TYPES_GFX_PARAM_TRAITS_H_

#include <string>

#include "ipc/ipc_message_utils.h"
#include "ipc/param_traits_macros.h"
#include "ui/gfx/ipc/buffer_types/gfx_ipc_export.h"
#include "ui/gfx/ipc/buffer_types/gfx_param_traits_macros.h"

namespace IPC {

template <>
struct GFX_IPC_BUFFER_TYPES_EXPORT ParamTraits<gfx::BufferUsageAndFormat> {
  typedef gfx::BufferUsageAndFormat param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // UI_GFX_IPC_BUFFER_TYPES_GFX_PARAM_TRAITS_H_
