// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/resources/resource.h"

#include "base/trace_event/memory_usage_estimator.h"

namespace ui {

Resource::Resource() : Resource(Type::BITMAP) {}

Resource::Resource(Type type) : type_(type) {}

Resource::~Resource() = default;

void Resource::SetUIResource(std::unique_ptr<cc::ScopedUIResource> ui_resource,
                             const gfx::Size& size) {
  ui_resource_ = std::move(ui_resource);
  size_ = size;
}

std::unique_ptr<Resource> Resource::CreateForCopy() {
  return std::make_unique<Resource>();
}

size_t Resource::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(ui_resource_);
}

}  // namespace ui
