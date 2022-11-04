// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/metadata/view_factory_internal.h"

#include "ui/views/view.h"

namespace views::internal {

ViewBuilderCore::ViewBuilderCore() = default;

ViewBuilderCore::ViewBuilderCore(ViewBuilderCore&&) = default;

ViewBuilderCore::~ViewBuilderCore() = default;

ViewBuilderCore& ViewBuilderCore::operator=(ViewBuilderCore&&) = default;

std::unique_ptr<View> ViewBuilderCore::Build() && {
  return DoBuild();
}

void ViewBuilderCore::AddPropertySetter(
    std::unique_ptr<PropertySetterBase> setter) {
  property_list_.push_back(std::move(setter));
}

void ViewBuilderCore::CreateChildren(View* parent) {
  for (auto& builder : children_) {
    if (builder.second)
      parent->AddChildViewAt(builder.first->DoBuild(), builder.second.value());
    else
      parent->AddChildView(builder.first->DoBuild());
  }
}

void ViewBuilderCore::SetProperties(View* view) {
  for (auto& property : property_list_)
    property->SetProperty(view);
}

}  // namespace views::internal
