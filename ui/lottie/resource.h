// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LOTTIE_RESOURCE_H_
#define UI_LOTTIE_RESOURCE_H_

#include <memory>

#include "base/component_export.h"

namespace lottie {
class Animation;

// Gets a vector graphic resource specified by |resource_id| from the current
// module data and returns it as an |Animation| object. This expects the
// resource to be gzipped.
COMPONENT_EXPORT(UI_LOTTIE)
std::unique_ptr<Animation> GetVectorAnimationNamed(int resource_id);

}  // namespace lottie

#endif  // UI_LOTTIE_RESOURCE_H_
