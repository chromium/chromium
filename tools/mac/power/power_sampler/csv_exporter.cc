// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/mac/power/power_sampler/csv_exporter.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"

namespace power_sampler {

std::unique_ptr<CsvExporter> CsvExporter::Create(base::TimeTicks time_base,
                                                 base::FilePath file_path) {
  base::File output_file(file_path,
                         base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  if (!output_file.IsValid())
    return nullptr;

  return Create(time_base, std::move(output_file));
}

std::unique_ptr<CsvExporter> CsvExporter::Create(base::TimeTicks time_base,
                                                 base::File file) {
  return base::WrapUnique(new CsvExporter(time_base, std::move(file)));
}

CsvExporter::~CsvExporter() = default;

void CsvExporter::OnStartSession(const DataColumnKeyUnits& data_columns_units) {
  for (const auto& kv : data_columns_units) {
    bool inserted = column_keys_.emplace(kv.first).second;
    DCHECK(inserted);
  }

  std::string header_string = "time(s)";
  for (const auto& key : column_keys_) {
    const auto it = data_columns_units.find(key);
    DCHECK(it != data_columns_units.end());
    const std::string& units = it->second;

    base::StringAppendF(&header_string, ",%s_%s(%s)", key.sampler_name.c_str(),
                        key.column_name.c_str(), units.c_str());
  }

  header_string.push_back('\n');

  bool success = Append(header_string);
  DCHECK(success) << "Append(header_string) failed.";
}

bool CsvExporter::OnSample(base::TimeTicks sample_time,
                           const DataRow& data_row) {
  std::string row_string;

  base::TimeDelta delta = sample_time - time_base_;
  base::StringAppendF(&row_string, "%g", delta.InSecondsF());
  for (const auto& key : column_keys_) {
    auto it = data_row.find(key);
    if (it != data_row.end())
      base::StringAppendF(&row_string, ",%g", it->second);
    else
      row_string.push_back(',');
  }
  row_string.push_back('\n');

  // End the session on failure to write.
  return !Append(row_string);
}

void CsvExporter::OnEndSession() {}

CsvExporter::CsvExporter(base::TimeTicks time_base, base::File file)
    : time_base_(time_base), file_(std::move(file)) {
  DCHECK(file_.IsValid());
}

bool CsvExporter::Append(const std::string& text) {
  if (!file_.WriteAtCurrentPosAndCheck(base::as_byte_span(text))) {
    LOG(ERROR) << "WriteAtCurrentPos failed";
    return false;
  }
  return true;
}

}  // namespace power_sampler
