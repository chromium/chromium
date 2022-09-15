// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_RESOURCES_RESOURCE_H_
#define UI_ANDROID_RESOURCES_RESOURCE_H_

#include "cc/resources/scoped_ui_resource.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/android/ui_android_export.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

class UI_ANDROID_EXPORT Resource {
 public:
  enum class Type { BITMAP, NINE_PATCH_BITMAP, TOOLBAR };

  Resource();
  virtual ~Resource();

  virtual std::unique_ptr<Resource> CreateForCopy();
  void SetUIResource(std::unique_ptr<cc::ScopedUIResource> ui_resource,
                     const gfx::Size& size_in_px);
  size_t EstimateMemoryUsage() const;

  cc::ScopedUIResource* ui_resource() const { return ui_resource_.get(); }
  gfx::Size size() const { return size_; }
  Type type() const { return type_; }

 protected:
  Resource(Type type);

 private:
  const Type type_;

  // Size of the bitmap in physical pixels.
  gfx::Size size_;
  std::unique_ptr<cc::ScopedUIResource> ui_resource_;
};

}  // namespace ui

#endif  // UI_ANDROID_RESOURCES_RESOURCE_H_
