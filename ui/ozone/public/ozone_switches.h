// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_OZONE_SWITCHES_H_
#define UI_OZONE_PUBLIC_OZONE_SWITCHES_H_

#include "base/compiler_specific.h"
#include "ui/ozone/ozone_base_export.h"

namespace switches {

OZONE_BASE_EXPORT extern const char kOzonePlatform[];

OZONE_BASE_EXPORT extern const char kOzoneDumpFile[];

OZONE_BASE_EXPORT extern const char kEnableWaylandIme[];

OZONE_BASE_EXPORT extern const char kDisableExplicitDmaFences[];

}  // namespace switches

#endif  // UI_OZONE_PUBLIC_OZONE_SWITCHES_H_
