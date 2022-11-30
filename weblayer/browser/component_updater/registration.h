// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_COMPONENT_UPDATER_REGISTRATION_H_
#define WEBLAYER_BROWSER_COMPONENT_UPDATER_REGISTRATION_H_

#include "components/component_updater/android/component_loader_policy.h"

namespace weblayer {

// ComponentLoaderPolicies for component to load in WebLayer during startup.
component_updater::ComponentLoaderPolicyVector GetComponentLoaderPolicies();

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_COMPONENT_UPDATER_REGISTRATION_H_