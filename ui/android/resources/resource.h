// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_RESOURCES_RESOURCE_H_
#define UI_ANDROID_RESOURCES_RESOURCE_H_

#include <cstdint>
#include <memory>

#include "cc/resources/scoped_ui_resource.h"
#include "ui/android/ui_android_export.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

class UI_ANDROID_EXPORT Resource {
 public:
  enum class Type : uint8_t { BITMAP, NINE_PATCH_BITMAP, TOOLBAR };

  Resource();
  virtual ~Resource();

  constexpr static int kInvalidResourceId = 0;

  virtual std::unique_ptr<Resource> CreateForCopy();
  void SetUIResource(std::unique_ptr<cc::ScopedUIResource> ui_resource,
                     const gfx::Size& size_in_px);
  size_t EstimateMemoryUsage() const;

  cc::ScopedUIResource* ui_resource() const { return ui_resource_.get(); }
  gfx::Size size() const { return size_; }
  Type type() const { return type_; }

 protected:
  explicit Resource(Type type);

 private:
  std::unique_ptr<cc::ScopedUIResource> ui_resource_;

  // Size of the bitmap in physical pixels.
  gfx::Size size_;
  const Type type_;
};

}  // namespace ui

#endif  // UI_ANDROID_RESOURCES_RESOURCE_H_
