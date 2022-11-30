// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/mac/power/power_sampler/sample_counter.h"

namespace power_sampler {

SampleCounter::SampleCounter(size_t max_sample_count)
    : sample_count_(max_sample_count) {
  DCHECK_GT(sample_count_, 0U);
}

bool SampleCounter::OnSample(base::TimeTicks sample_time,
                             const DataRow& data_row) {
  DCHECK_GT(sample_count_, 0U);
  --sample_count_;
  return sample_count_ == 0;
}

}  // namespace power_sampler
