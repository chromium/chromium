// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/features.h"

#include <algorithm>
#include <cstddef>

#include "ipcz/ipcz.h"
#include "ipcz/message.h"

#include "third_party/abseil-cpp/absl/types/span.h"

namespace ipcz {

Features Features::Intersect(const Features& rhs) const {
  Features features = {};
  for (size_t i = 0; i < std::size(bitfield_values); ++i) {
    features.bitfield_values[i] = bitfield_values[i] & rhs.bitfield_values[i];
  }
  return features;
}

// static
Features Features::FromNodeOptions(const IpczCreateNodeOptions* options) {
  if (!options) {
    return {};
  }

  Features features;
  for (IpczFeature id : absl::MakeSpan(options->enabled_features,
                                       options->num_enabled_features)) {
    features.SetFeatureEnabled(id, true);
  }
  for (IpczFeature id : absl::MakeSpan(options->disabled_features,
                                       options->num_disabled_features)) {
    features.SetFeatureEnabled(id, false);
  }
  return features;
}

uint32_t Features::Serialize(Message& message) const {
  return message.AllocateAndSetArray(absl::MakeSpan(bitfield_values));
}

Features Features::Deserialize(Message& message, uint32_t offset) {
  // NOTE: The serialized features may be smaller than the current version.
  Features features = {};
  const auto view = message.GetArrayView<Bitfield>(offset);
  const auto size = std::min(std::size(features.bitfield_values), view.size());
  for (size_t i = 0; i < size; ++i) {
    features.bitfield_values[i] = view[i];
  }
  return features;
}

void Features::SetFeatureEnabled(IpczFeature feature, bool enabled) {
  switch (feature) {
    case IPCZ_FEATURE_MEM_V2:
      set_bit(kMemV2Bit, enabled);
      break;

    default:
      break;
  }
}

}  // namespace ipcz
