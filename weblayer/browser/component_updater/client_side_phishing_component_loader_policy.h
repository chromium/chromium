// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_COMPONENT_UPDATER_CLIENT_SIDE_PHISHING_COMPONENT_LOADER_POLICY_H_
#define WEBLAYER_BROWSER_COMPONENT_UPDATER_CLIENT_SIDE_PHISHING_COMPONENT_LOADER_POLICY_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "base/values.h"
#include "components/component_updater/android/component_loader_policy.h"

namespace base {
class Version;
}  // namespace base

namespace weblayer {

class ClientSidePhishingComponentLoaderPolicy
    : public component_updater::ComponentLoaderPolicy {
 public:
  ClientSidePhishingComponentLoaderPolicy() = default;
  ~ClientSidePhishingComponentLoaderPolicy() override = default;

  ClientSidePhishingComponentLoaderPolicy(
      const ClientSidePhishingComponentLoaderPolicy&) = delete;
  ClientSidePhishingComponentLoaderPolicy& operator=(
      const ClientSidePhishingComponentLoaderPolicy&) = delete;

 private:
  // The following methods override ComponentLoaderPolicy.
  void ComponentLoaded(const base::Version& version,
                       base::flat_map<std::string, base::ScopedFD>& fd_map,
                       base::Value::Dict manifest) override;
  void ComponentLoadFailed(
      component_updater::ComponentLoadResult error) override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetMetricsSuffix() const override;
};

void LoadClientSidePhishingComponent(
    component_updater::ComponentLoaderPolicyVector& policies);

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_COMPONENT_UPDATER_CLIENT_SIDE_PHISHING_COMPONENT_LOADER_POLICY_H_
