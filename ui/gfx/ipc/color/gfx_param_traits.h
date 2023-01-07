// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IPC_COLOR_GFX_PARAM_TRAITS_H_
#define UI_GFX_IPC_COLOR_GFX_PARAM_TRAITS_H_

#include "ipc/ipc_message_utils.h"
#include "ipc/param_traits_macros.h"
#include "ipc/ipc_message_macros.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/ipc/color/gfx_ipc_color_export.h"
#include "ui/gfx/ipc/color/gfx_param_traits_macros.h"

namespace gfx {
class ColorSpace;
class DisplayColorSpaces;
}

namespace IPC {

template <>
struct GFX_IPC_COLOR_EXPORT ParamTraits<gfx::ColorSpace> {
  typedef gfx::ColorSpace param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct GFX_IPC_COLOR_EXPORT ParamTraits<gfx::DisplayColorSpaces> {
  typedef gfx::DisplayColorSpaces param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // UI_GFX_IPC_COLOR_GFX_PARAM_TRAITS_H_
