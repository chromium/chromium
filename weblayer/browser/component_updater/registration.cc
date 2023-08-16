// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/component_updater/registration.h"

namespace weblayer {

component_updater::ComponentLoaderPolicyVector GetComponentLoaderPolicies() {
  component_updater::ComponentLoaderPolicyVector policies;

  // TODO(crbug.com/1233490) register AutoFillRegex component loader policy.

  return policies;
}

}  // namespace weblayer
