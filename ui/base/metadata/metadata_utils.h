// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_METADATA_METADATA_UTILS_H_
#define UI_BASE_METADATA_METADATA_UTILS_H_

#include <type_traits>

#include "build/build_config.h"
#include "build/buildflag.h"
#include "ui/base/metadata/metadata_types.h"

namespace ui::metadata {

template <typename T, typename = void>
static constexpr bool kHasClassMetadata = false;
template <typename T>
static constexpr bool kHasClassMetadata<
    T,
    std::void_t<
        typename std::remove_cvref_t<std::remove_pointer_t<T>>::kMetadataTag>> =
    std::is_same_v<
        typename std::remove_cvref_t<std::remove_pointer_t<T>>::kMetadataTag,
        typename std::remove_cvref_t<std::remove_pointer_t<T>>>;

#define CHECK_CLASS_HAS_METADATA(class_type)                                  \
  static_assert(ui::metadata::kHasClassMetadata<class_type>,                  \
                "The class_type param doesn't implement metadata. Make sure " \
                "class publicly calls METADATA_HEADER in the declaration.");

template <typename V, typename B>
bool IsClass(const B* instance) {
  if (!instance) {
    return false;
  }
  CHECK_CLASS_HAS_METADATA(V)
  static_assert(std::is_base_of_v<B, V>,
                "Only classes derived from template param B allowed");
  const ClassMetaData* child = instance->GetClassMetaData();
  for (const ClassMetaData* parent = V::MetaData(); child && child != parent;
       child = child->parent_class_meta_data())
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
