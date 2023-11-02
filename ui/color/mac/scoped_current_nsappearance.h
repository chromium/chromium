// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_MAC_SCOPED_CURRENT_NSAPPEARANCE_H_
#define UI_COLOR_MAC_SCOPED_CURRENT_NSAPPEARANCE_H_

#include "base/component_export.h"

namespace ui {

// Class for handling changing the NSAppearance to get colors in a scoped way
// based on the desired light/dark colors scheme.
class COMPONENT_EXPORT(COLOR) ScopedCurrentNSAppearance {
 public:
  explicit ScopedCurrentNSAppearance(bool dark, bool high_contrast);

  // There should be no reason to copy or move a ScopedCurrentNSAppearance.
  ScopedCurrentNSAppearance(const ScopedCurrentNSAppearance&) = delete;
  ScopedCurrentNSAppearance& operator=(const ScopedCurrentNSAppearance&) =
      delete;

  ~ScopedCurrentNSAppearance();
};

}  // namespace ui

#endif  // UI_COLOR_MAC_SCOPED_CURRENT_NSAPPEARANCE_H_
