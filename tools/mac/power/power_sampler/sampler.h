// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_MAC_POWER_POWER_SAMPLER_SAMPLER_H_
#define TOOLS_MAC_POWER_POWER_SAMPLER_SAMPLER_H_

#include <functional>
#include <map>
#include <string>

#include "base/strings/string_piece.h"
#include "base/time/time.h"

namespace power_sampler {

// A sample is a potentially sparse collection of named datums.
class Sample {
 public:
  using Datums = std::map<std::string, double>;

  Sample();
  explicit Sample(base::StringPiece sampler_name);
  Sample(Sample&&);
  ~Sample();

  // Allow copy and comparison for ease of use in testing.
  Sample(const Sample&);
  Sample& operator=(const Sample&);
  bool operator==(const Sample& other) const;

  // Add a datum with |name| and |value|.
  // Note: |name| must not already exist in the sample.
  void AddDatum(base::StringPiece name, double value);

  const std::string& sampler_name() const { return sampler_name_; }
  const Datums& datums() const { return datums_; }

 private:
  std::string sampler_name_;
  Datums datums_;
};

// Concrete sampler classes override this interface.
class Sampler {
 public:
  using DatumNameUnits = std::map<std::string, std::string, std::less<>>;

  Sampler() = default;
  virtual ~Sampler() = 0;

  // Returns the name of the sampler.
  virtual std::string GetName() = 0;

  // Returns the names and units of the datums provided by this sampler.
  virtual DatumNameUnits GetDatumNameUnits() = 0;

  // Subclasses override to return their sample, |sample_time| is the time
  // when the controller started the acquisition of this sample.
  // Returns the new sample, which must have the sampler_name set to the
  // same value as |GetName()| of this sampler.
  virtual Sample GetSample(base::TimeTicks sample_time) = 0;
};

}  // namespace power_sampler

#endif  // TOOLS_MAC_POWER_POWER_SAMPLER_SAMPLER_H_
