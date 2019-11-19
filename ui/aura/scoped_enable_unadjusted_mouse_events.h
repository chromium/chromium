// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SCOPED_ENABLE_UNADJUSTED_MOUSE_EVENTS_H_
#define UI_AURA_SCOPED_ENABLE_UNADJUSTED_MOUSE_EVENTS_H_

#include "ui/aura/aura_export.h"

namespace aura {

// Scoping class that ensures correctly exit unadjusted mouse input. Start using
// unadjusted mouse events (i.e. WM_INPUT on Windows) when this is constructed.
// Destroying an instance of this class will exit the unadjusted mouse event
// mode.
class AURA_EXPORT ScopedEnableUnadjustedMouseEvents {
 public:
  virtual ~ScopedEnableUnadjustedMouseEvents() = default;

 protected:
  ScopedEnableUnadjustedMouseEvents() = default;
};

}  // namespace aura

#endif  // UI_AURA_SCOPED_ENABLE_UNADJUSTED_MOUSE_EVENTS_H_
