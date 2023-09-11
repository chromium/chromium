// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_METADATA_METADATA_UTILS_H_
#define UI_BASE_METADATA_METADATA_UTILS_H_

#include <type_traits>

#include "ui/base/metadata/metadata_types.h"

namespace ui::metadata {

template <typename V, typename B>
bool IsClass(const B* instance) {
  if (!instance) {
    return false;
  }
  static_assert(std::is_base_of_v<B, V>,
                "Only classes derived from template param B allowed");
  const ui::metadata::ClassMetaData* child = instance->GetClassMetaData();
  for (const ui::metadata::ClassMetaData* parent = V::MetaData();
       child && child != parent; child = child->parent_class_meta_data())
    ;
  return !!child;
}

template <typename V, typename B>
V* AsClass(B* instance) {
  return IsClass<V, B>(instance) ? static_cast<V*>(instance) : nullptr;
}

template <typename V, typename B>
const V* AsClass(const B* instance) {
  return IsClass<V, B>(instance) ? static_cast<const V*>(instance) : nullptr;
}

}  // namespace ui::metadata

#endif  // UI_BASE_METADATA_METADATA_UTILS_H_
