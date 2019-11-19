// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_OBJECT_INTERNAL_H_
#define UI_OZONE_PLATFORM_OBJECT_INTERNAL_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/ptr_util.h"
#include "ui/ozone/platform_constructor_list.h"
#include "ui/ozone/platform_object.h"
#include "ui/ozone/platform_selection.h"

namespace ui {

template <class T>
std::unique_ptr<T> PlatformObject<T>::Create() {
  typedef typename PlatformConstructorList<T>::Constructor Constructor;

  // Determine selected platform (from --ozone-platform flag, or default).
  int platform = GetOzonePlatformId();

  // Look up the constructor in the constructor list.
  Constructor constructor = PlatformConstructorList<T>::kConstructors[platform];

  // Call the constructor.
  return base::WrapUnique(constructor());
}

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_OBJECT_INTERNAL_H_
