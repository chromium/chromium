// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/mac/power/power_sampler/json_exporter.h"

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"

namespace power_sampler {

std::unique_ptr<JsonExporter> JsonExporter::Create(base::FilePath file_path,
                                                   base::TimeTicks time_base) {
  base::File ensure_created(
      file_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!ensure_created.IsValid())
    return nullptr;

  return base::WrapUnique(new JsonExporter(std::move(file_path), time_base));
}

JsonExporter::JsonExporter(base::FilePath file_path, base::TimeTicks time_base)
    : file_path_(file_path), time_base_(time_base) {}

JsonExporter::~JsonExporter() = default;

void JsonExporter::OnStartSession(
    const DataColumnKeyUnits& data_columns_units) {
  base::flat_map<std::string, base::Value> column_labels;
  for (const auto& column : data_columns_units) {
    column_labels.emplace(
        column.first.sampler_name + "_" + column.first.column_name,
        column.second);
  }
  column_labels_ = base::Value(std::move(column_labels));
}

bool JsonExporter::OnSample(base::TimeTicks sample_time,
                            const DataRow& data_row) {
  base::flat_map<std::string, base::Value> sample_value;

  sample_value.emplace("sample_time",
                       (sample_time - time_base_).InMicrosecondsF());
  for (const auto& datum : data_row) {
    sample_value.emplace(
        datum.first.sampler_name + "_" + datum.first.column_name, datum.second);
  }

  data_rows_.push_back(base::Value(std::move(sample_value)));
  return false;
}

void JsonExporter::OnEndSession() {
  base::flat_map<std::string, base::Value> output;
  output.emplace("column_labels", column_labels_.Clone());
  output.emplace("data_rows", base::Value(data_rows_));

  std::string json_string;
  bool success = base::JSONWriter::WriteWithOptions(
      base::Value(std::move(output)), base::JSONWriter::OPTIONS_PRETTY_PRINT,
      &json_string);

  DCHECK(success);
  success = base::WriteFile(file_path_, json_string);
  DCHECK(success) << "Failed to write JSON to file";
}

}  // namespace power_sampler
