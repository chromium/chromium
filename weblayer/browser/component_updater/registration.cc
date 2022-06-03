// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/component_updater/registration.h"

namespace weblayer {

component_updater::ComponentLoaderPolicyVector GetComponentLoaderPolicies() {
  // TODO(crbug.com/1233490) register AutoFillRegex component loader policy.
  return component_updater::ComponentLoaderPolicyVector();
}

}  // namespace weblayer
