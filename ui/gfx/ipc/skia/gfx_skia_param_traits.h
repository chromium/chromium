// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IPC_SKIA_GFX_SKIA_PARAM_TRAITS_H_
#define UI_GFX_IPC_SKIA_GFX_SKIA_PARAM_TRAITS_H_

#include <string>

#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_param_traits.h"
#include "ui/gfx/ipc/skia/gfx_skia_ipc_export.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits_macros.h"

class SkBitmap;
struct SkImageInfo;

namespace base {
class Pickle;
class PickleIterator;
}

namespace gfx {
class Transform;
}

namespace IPC {

template <>
struct GFX_SKIA_IPC_EXPORT ParamTraits<SkImageInfo> {
  using param_type = SkImageInfo;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct GFX_SKIA_IPC_EXPORT ParamTraits<SkBitmap> {
  using param_type = SkBitmap;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct GFX_SKIA_IPC_EXPORT ParamTraits<gfx::Transform> {
  using param_type = gfx::Transform;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // UI_GFX_IPC_SKIA_GFX_SKIA_PARAM_TRAITS_H_
