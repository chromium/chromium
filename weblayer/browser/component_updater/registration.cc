// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/component_updater/registration.h"

#include "weblayer/browser/component_updater/client_side_phishing_component_loader_policy.h"

namespace weblayer {

component_updater::ComponentLoaderPolicyVector GetComponentLoaderPolicies() {
  component_updater::ComponentLoaderPolicyVector policies;

  LoadClientSidePhishingComponent(policies);
  // TODO(crbug.com/1233490) register AutoFillRegex component loader policy.

  return policies;
}

}  // namespace weblayer
