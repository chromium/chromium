// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_CONSTRUCTOR_LIST_H_
#define UI_OZONE_PLATFORM_CONSTRUCTOR_LIST_H_


#include "ui/ozone/platform_list.h"

namespace ui {

template <class T>
struct PlatformConstructorList {
  typedef T* (*Constructor)();
  static const Constructor kConstructors[kPlatformCount];
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_CONSTRUCTOR_LIST_H_
