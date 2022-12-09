// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/component_updater/client_side_phishing_component_loader_policy.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/location.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/android/component_loader_policy.h"
#include "components/component_updater/installer_policies/client_side_phishing_component_installer_policy.h"
#include "components/safe_browsing/content/browser/client_side_phishing_model.h"
#include "weblayer/common/features.h"

namespace weblayer {

namespace {
// Persisted to logs, should never change.
constexpr char kClientSidePhishingComponentMetricsSuffix[] =
    "ClientSidePhishing";

void LoadFromDisk(base::ScopedFD pb_fd, base::ScopedFD visual_tflite_model_fd) {
  std::string binary_pb;
  base::ScopedFILE pb_file_stream(
      base::FileToFILE(base::File(std::move(pb_fd)), "r"));
  if (!base::ReadStreamToString(pb_file_stream.get(), &binary_pb))
    binary_pb.clear();

  base::File visual_tflite_model(std::move(visual_tflite_model_fd),
                                 base::File::FLAG_OPEN | base::File::FLAG_READ);

  // The ClientSidePhishingModel singleton will react appropriately if the
  // |binary_pb| is empty or |visual_tflite_model| is invalid.
  safe_browsing::ClientSidePhishingModel::GetInstance()
      ->PopulateFromDynamicUpdate(binary_pb, std::move(visual_tflite_model));
}

}  // namespace

void ClientSidePhishingComponentLoaderPolicy::ComponentLoaded(
    const base::Version& version,
    base::flat_map<std::string, base::ScopedFD>& fd_map,
    base::Value::Dict manifest) {
  DCHECK(version.IsValid());

  auto pb_iterator =
      fd_map.find(component_updater::kClientModelBinaryPbFileName);
  if (pb_iterator == fd_map.end())
    return;

  auto visual_tflite_model_iterator =
      fd_map.find(component_updater::kVisualTfLiteModelFileName);

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&LoadFromDisk, std::move(pb_iterator->second),
                     visual_tflite_model_iterator == fd_map.end()
                         ? base::ScopedFD()
                         : std::move(visual_tflite_model_iterator->second)));
}

void ClientSidePhishingComponentLoaderPolicy::ComponentLoadFailed(
    component_updater::ComponentLoadResult /*error*/) {}

void ClientSidePhishingComponentLoaderPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  component_updater::ClientSidePhishingComponentInstallerPolicy::GetPublicHash(
      hash);
}

std::string ClientSidePhishingComponentLoaderPolicy::GetMetricsSuffix() const {
  return kClientSidePhishingComponentMetricsSuffix;
}

void LoadClientSidePhishingComponent(
    component_updater::ComponentLoaderPolicyVector& policies) {
  if (!base::FeatureList::IsEnabled(
          weblayer::features::kWebLayerClientSidePhishingDetection)) {
    return;
  }

  policies.push_back(
      std::make_unique<ClientSidePhishingComponentLoaderPolicy>());
}

}  // namespace weblayer
