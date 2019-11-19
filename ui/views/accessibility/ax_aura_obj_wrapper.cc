// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_aura_obj_wrapper.h"

namespace views {

AXAuraObjWrapper::AXAuraObjWrapper(AXAuraObjCache* cache)
    : aura_obj_cache_(cache) {}

bool AXAuraObjWrapper::HandleAccessibleAction(const ui::AXActionData& action) {
  return false;
}

}  // namespace views
