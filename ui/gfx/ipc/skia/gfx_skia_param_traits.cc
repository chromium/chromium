// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"

#include <string>

#include "base/pickle.h"
#include "ipc/ipc_message_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/geometry/transform.h"

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
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

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
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

void ParamTraits<SkImageInfo>::Log(const SkImageInfo& p, std::string* l) {
  l->append("<SkImageInfo>");
}

void ParamTraits<SkBitmap>::Write(base::Pickle* m, const SkBitmap& p) {
  WriteParam(m, p.info());
  size_t pixel_size = p.computeByteSize();
  m->WriteData(reinterpret_cast<const char*>(p.getPixels()), pixel_size);
}

bool ParamTraits<SkBitmap>::Read(const base::Pickle* m,
                                 base::PickleIterator* iter,
                                 SkBitmap* r) {
  SkImageInfo image_info;
  if (!ReadParam(m, iter, &image_info))
    return false;

  const char* bitmap_data;
  size_t bitmap_data_size = 0;
  if (!iter->ReadData(&bitmap_data, &bitmap_data_size))
    return false;

  if (!r->tryAllocPixels(image_info))
    return false;

  if (bitmap_data_size != r->computeByteSize())
    return false;
  memcpy(r->getPixels(), bitmap_data, bitmap_data_size);
  return true;
}

void ParamTraits<SkBitmap>::Log(const SkBitmap& p, std::string* l) {
  l->append("<SkBitmap>");
  LogParam(p.info(), l);
}

void ParamTraits<gfx::Transform>::Write(base::Pickle* m, const param_type& p) {
  float column_major_data[16];
  p.GetColMajorF(column_major_data);
  // We do this in a single write for performance reasons.
  m->WriteBytes(&column_major_data, sizeof(column_major_data));
}

bool ParamTraits<gfx::Transform>::Read(const base::Pickle* m,
                                       base::PickleIterator* iter,
                                       param_type* r) {
  const char* column_major_data;
  if (!iter->ReadBytes(&column_major_data, sizeof(float) * 16))
    return false;
  *r = gfx::Transform::ColMajorF(
      reinterpret_cast<const float*>(column_major_data));
  return true;
}

void ParamTraits<gfx::Transform>::Log(const param_type& p, std::string* l) {
  l->append(p.ToString());
}

}  // namespace IPC
