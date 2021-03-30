// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_UTILS_H_
#define UI_VIEWS_VIEW_UTILS_H_

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "ui/views/debug/debugger_utils.h"
#include "ui/views/metadata/metadata_types.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace views {

class ViewDebugWrapperImpl : public debug::ViewDebugWrapper {
 public:
  explicit ViewDebugWrapperImpl(View* view);
  ViewDebugWrapperImpl(const ViewDebugWrapperImpl&) = delete;
  ViewDebugWrapperImpl& operator=(const ViewDebugWrapperImpl&) = delete;
  ~ViewDebugWrapperImpl() override;

  // debug::ViewDebugWrapper:
  std::string GetViewClassName() override;
  int GetID() override;
  debug::ViewDebugWrapper::BoundsTuple GetBounds() override;
  bool GetVisible() override;
  bool GetNeedsLayout() override;
  bool GetEnabled() override;
  std::vector<debug::ViewDebugWrapper*> GetChildren() override;
  void ForAllProperties(PropCallback callback) override;

 private:
  const View* const view_;
  std::vector<std::unique_ptr<ViewDebugWrapperImpl>> children_;
};

template <typename V>
bool IsViewClass(View* view) {
  static_assert(std::is_base_of<View, V>::value, "Only View classes supported");
  metadata::ClassMetaData* parent = V::MetaData();
  metadata::ClassMetaData* child = view->GetClassMetaData();
  while (child && child != parent)
    child = child->parent_class_meta_data();
  return !!child;
}

VIEWS_EXPORT void PrintViewHierarchy(View* view,
                                     bool verbose = false,
                                     int depth = -1);

}  // namespace views

#endif  // UI_VIEWS_VIEW_UTILS_H_
