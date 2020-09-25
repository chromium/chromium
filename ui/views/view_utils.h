// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_UTILS_H_
#define UI_VIEWS_VIEW_UTILS_H_

#include <type_traits>

#include "ui/views/metadata/metadata_types.h"
#include "ui/views/view.h"

namespace views {

template <typename V>
bool IsViewClass(View* view) {
  static_assert(std::is_base_of<View, V>::value, "Only View classes supported");
  metadata::ClassMetaData* parent = V::MetaData();
  metadata::ClassMetaData* child = view->GetClassMetaData();
  while (child && child != parent)
    child = child->parent_class_meta_data();
  return !!child;
}

}  // namespace views

#endif  // UI_VIEWS_VIEW_UTILS_H_
