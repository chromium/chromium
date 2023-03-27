// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/memory/simulator/metrics_printer.h"

#include <iostream>

#include "base/strings/stringprintf.h"

namespace memory_simulator {

MetricsPrinter::MetricsPrinter() = default;

MetricsPrinter::~MetricsPrinter() = default;

void MetricsPrinter::AddProvider(std::unique_ptr<MetricsProvider> provider) {
  DCHECK(!did_print_header_);
  providers_.push_back(std::move(provider));
}

void MetricsPrinter::PrintHeader() {
  DCHECK(!did_print_header_);
  did_print_header_ = true;

  std::string out = "elapsed_time(s)";

  for (auto& provider : providers_) {
    std::vector<std::string> metric_names = provider->GetMetricNames();
    for (const std::string& metric_name : metric_names) {
      base::StringAppendF(&out, ",%s", metric_name.c_str());
    }
  }

  std::cout << out << std::endl;
}

void MetricsPrinter::PrintStats() {
  DCHECK(did_print_header_);

  std::string out;

  base::TimeTicks now = base::TimeTicks::Now();
  double seconds_since_start = (now - start_time_).InSecondsF();
  base::StringAppendF(&out, "%.2f", seconds_since_start);

  for (auto& provider : providers_) {
    std::vector<std::string> metric_names = provider->GetMetricNames();
    std::map<std::string, double> metric_values =
        provider->GetMetricValues(now);
    for (const std::string& metric_name : metric_names) {
      auto it = metric_values.find(metric_name);
      if (it == metric_values.end()) {
        out += ",";
      } else {
        base::StringAppendF(&out, ",%.2f", it->second);
      }
    }
  }

  std::cout << out << std::endl;
}

}  // namespace memory_simulator
