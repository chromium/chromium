// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_UTILS_H_
#define UI_VIEWS_VIEW_UTILS_H_

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "base/debug/stack_trace.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/class_property.h"
#include "ui/base/metadata/metadata_types.h"
#include "ui/base/metadata/metadata_utils.h"
#include "ui/views/debug/debugger_utils.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace views {

VIEWS_EXPORT extern const ui::ClassProperty<base::debug::StackTrace*>* const
    kViewStackTraceKey;

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
  const raw_ptr<const View> view_;
  std::vector<std::unique_ptr<ViewDebugWrapperImpl>> children_;
};

template <typename V>
bool IsViewClass(const View* view) {
  return ui::metadata::IsClass<V, View>(view);
}

template <typename V>
V* AsViewClass(View* view) {
  return IsViewClass<V>(view) ? static_cast<V*>(view) : nullptr;
}

template <typename V>
const V* AsViewClass(const View* view) {
  return IsViewClass<V>(view) ? static_cast<const V*>(view) : nullptr;
}

VIEWS_EXPORT void PrintViewHierarchy(View* view,
                                     bool verbose = false,
                                     int depth = -1);

VIEWS_EXPORT std::string GetViewDebugInfo(View* view);

}  // namespace views

#endif  // UI_VIEWS_VIEW_UTILS_H_
