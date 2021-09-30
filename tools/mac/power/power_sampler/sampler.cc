// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/mac/power/power_sampler/sampler.h"

namespace power_sampler {

Sample::Sample() = default;
Sample::Sample(base::StringPiece sampler_name) : sampler_name_(sampler_name) {}
Sample::Sample(Sample&&) = default;
Sample::~Sample() = default;

Sample::Sample(const Sample&) = default;
Sample& Sample::operator=(const Sample&) = default;

void Sample::AddDatum(base::StringPiece name, double datum) {
  bool inserted = datums_.insert(std::make_pair(name, datum)).second;
  DCHECK(inserted);
}

bool Sample::operator==(const Sample& other) const {
  if (sampler_name_ != other.sampler_name_)
    return false;

  if (datums_.size() != other.datums_.size())
    return false;

  for (const auto& datum : datums_) {
    auto it = other.datums_.find(datum.first);
    if (it == other.datums_.end())
      return false;

    if (datum.second != it->second)
      return false;
  }

  return true;
}

Sampler::~Sampler() = default;

}  // namespace power_sampler
