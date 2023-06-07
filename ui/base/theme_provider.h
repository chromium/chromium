// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_THEME_PROVIDER_H_
#define UI_BASE_THEME_PROVIDER_H_

#include "base/component_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace base {
class RefCountedMemory;
}

namespace color_utils {
struct HSL;
}

namespace gfx {
class ImageSkia;
}

namespace ui {

////////////////////////////////////////////////////////////////////////////////
//
// ThemeProvider
//
//   ThemeProvider is an abstract class that defines the API that should be
//   implemented to provide bitmaps and color information for a given theme.
//
////////////////////////////////////////////////////////////////////////////////

class COMPONENT_EXPORT(UI_BASE) ThemeProvider {
 public:
  virtual ~ThemeProvider();

  // Get the image specified by |id|. An implementation of ThemeProvider should
  // have its own source of ids (e.g. an enum, or external resource bundle).
  virtual gfx::ImageSkia* GetImageSkiaNamed(int id) const = 0;

  // Get the HSL shift specified by |id|.
  virtual color_utils::HSL GetTint(int id) const = 0;

  // Get the property (e.g. an alignment expressed in an enum, or a width or
  // height) specified by |id|.
  virtual int GetDisplayProperty(int id) const = 0;

  // Whether we should use the native system frame (typically Aero glass) or
  // a custom frame.
  virtual bool ShouldUseNativeFrame() const = 0;

  // Whether or not we have a certain image. Used for when the default theme
  // doesn't provide a certain image, but custom themes might (badges, etc).
  virtual bool HasCustomImage(int id) const = 0;

  // Reads the image data from the theme file into the specified vector. Only
  // valid for un-themed resources and the themed IDR_THEME_NTP_* in most
  // implementations of ThemeProvider. Returns NULL on error.
  virtual base::RefCountedMemory* GetRawData(
      int id,
      ui::ResourceScaleFactor scale_factor) const = 0;
};

}  // namespace ui

#endif  // UI_BASE_THEME_PROVIDER_H_
