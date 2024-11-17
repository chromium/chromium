// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_data_dll_win.h"

#include <stddef.h>

#include <string_view>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/ref_counted_memory.h"
#include "base/win/resource_util.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace ui {

ResourceDataDLL::ResourceDataDLL(HINSTANCE module) : module_(module) {
  DCHECK(module_);
}

ResourceDataDLL::~ResourceDataDLL() {
}

bool ResourceDataDLL::HasResource(uint16_t resource_id) const {
  void* data_ptr;
  size_t data_size;
  return base::win::GetDataResourceFromModule(module_,
                                              resource_id,
                                              &data_ptr,
                                              &data_size);
}

std::optional<std::string_view> ResourceDataDLL::GetStringView(
    uint16_t resource_id) const {
  void* data_ptr;
  size_t data_size;
  if (base::win::GetDataResourceFromModule(module_,
                                           resource_id,
                                           &data_ptr,
                                           &data_size)) {
    return std::string_view(static_cast<const char*>(data_ptr), data_size);
  }
  return std::nullopt;
}

base::RefCountedStaticMemory* ResourceDataDLL::GetStaticMemory(
    uint16_t resource_id) const {
  void* data_ptr;
  size_t data_size;
  if (base::win::GetDataResourceFromModule(module_, resource_id, &data_ptr,
                                           &data_size)) {
    return new base::RefCountedStaticMemory(
        UNSAFE_TODO(base::span(static_cast<uint8_t*>(data_ptr), data_size)));
  }
  return NULL;
}

ResourceHandle::TextEncodingType ResourceDataDLL::GetTextEncodingType() const {
  return BINARY;
}

ResourceScaleFactor ResourceDataDLL::GetResourceScaleFactor() const {
  return ui::kScaleFactorNone;
}

}  // namespace ui
