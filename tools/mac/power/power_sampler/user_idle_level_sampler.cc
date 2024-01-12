// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/mac/power/power_sampler/user_idle_level_sampler.h"

#include <sys/sysctl.h>
#include <sys/types.h>

#include <optional>

#include "base/logging.h"
#include "base/memory/ptr_util.h"

namespace power_sampler {

namespace {

std::optional<int> GetIntSysCtl(const std::vector<int>& mib_name) {
  int value = 0;
  size_t size = sizeof(value);
  int ret = sysctl(const_cast<int*>(mib_name.data()), mib_name.size(), &value,
                   &size, nullptr, 0);
  if (ret != 0) {
    PLOG(ERROR) << "Error in sysctl";
  } else if (size != sizeof(value)) {
    LOG(ERROR)
        << "sysctl returns an unexpected size for machdep.user_idle_level";
  } else {
    return value;
  }

  return std::nullopt;
}

}  // namespace

UserIdleLevelSampler::~UserIdleLevelSampler() = default;

// static
std::unique_ptr<UserIdleLevelSampler> UserIdleLevelSampler::Create() {
  std::vector<int> mib_name(10);

  size_t size = mib_name.size();
  int ret = sysctlnametomib("machdep.user_idle_level", mib_name.data(), &size);
  if (ret != 0) {
    PLOG(ERROR) << "Error in sysctlnametomib";
    return nullptr;
  }

  DCHECK_NE(0u, size);
  mib_name.resize(size);

  if (!GetIntSysCtl(mib_name).has_value())
    return nullptr;

  return base::WrapUnique(new UserIdleLevelSampler(std::move(mib_name)));
}

std::string UserIdleLevelSampler::GetName() {
  return kSamplerName;
}

Sampler::DatumNameUnits UserIdleLevelSampler::GetDatumNameUnits() {
  DatumNameUnits ret;
  ret.insert(std::make_pair("user_idle_level", "int"));
  return ret;
}

Sampler::Sample UserIdleLevelSampler::GetSample(base::TimeTicks sample_time) {
  DCHECK(!mib_name_.empty());

  Sample sample;
  auto value = GetIntSysCtl(mib_name_);
  if (value.has_value())
    sample.emplace("user_idle_level", value.value());

  return sample;
}

UserIdleLevelSampler::UserIdleLevelSampler(std::vector<int> mib_name)
    : mib_name_(std::move(mib_name)) {
  DCHECK(!mib_name_.empty());
}

}  // namespace power_sampler
