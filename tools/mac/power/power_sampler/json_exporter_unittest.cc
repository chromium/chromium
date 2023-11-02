// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/mac/power/power_sampler/json_exporter.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace power_sampler {

class JsonExporterTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::ScopedTempDir temp_dir_;
};

TEST_F(JsonExporterTest, CreateFile) {
  base::FilePath file_path = temp_dir_.GetPath().Append("dummy.json");
  std::unique_ptr<JsonExporter> exporter =
      JsonExporter::Create(file_path, base::TimeTicks());

  base::File dummy(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  EXPECT_TRUE(dummy.IsValid());
}

TEST_F(JsonExporterTest, InvalidPath) {
  base::FilePath file_path =
      temp_dir_.GetPath().Append("invalid").Append("dummy.json");
  std::unique_ptr<JsonExporter> exporter =
      JsonExporter::Create(file_path, base::TimeTicks());
  EXPECT_EQ(nullptr, exporter);
}

TEST_F(JsonExporterTest, JSONFile) {
  // ScopedTempDir temp_dir;
  base::FilePath file_path = temp_dir_.GetPath().Append("output.json");
  std::unique_ptr<JsonExporter> exporter =
      JsonExporter::Create(file_path, base::TimeTicks());

  DataColumnKey speed1{"odometer", "speed"};
  DataColumnKey speed2{"satelite", "speed"};
  DataColumnKey height{"satelite", "height"};
  exporter->OnStartSession({
      {speed1, "m/s"},
      {speed2, "m/s"},
      {height, "km"},
  });

  EXPECT_EQ(base::JSONReader::Read(R"json(
    {
      "odometer_speed": "m/s",
      "satelite_speed": "m/s",
      "satelite_height": "km"
    }
    )json"),
            exporter->GetColumnLabelsForTesting());

  exporter->OnSample(base::TimeTicks() + base::Milliseconds(1),
                     {{speed1, 0.5}, {speed2, 1.0}});
  exporter->OnSample(base::TimeTicks() + base::Milliseconds(2),
                     {{speed2, 1.5}});

  EXPECT_EQ(base::JSONReader::Read(R"json(
    [ {
      "sample_time": 1000.0,
      "odometer_speed": 0.5,
      "satelite_speed": 1.0
    }, {
      "sample_time": 2000.0,
      "satelite_speed": 1.5
    } ]
    )json"),
            exporter->GetDataRowsForTesting());

  exporter->OnEndSession();

  std::string json_string;
  EXPECT_TRUE(base::ReadFileToString(file_path, &json_string));
  EXPECT_EQ(base::JSONReader::Read(R"json(
    {
      "column_labels": {
        "odometer_speed": "m/s",
        "satelite_height": "km",
        "satelite_speed": "m/s"
      },
      "data_rows": [ {
        "odometer_speed": 0.5,
        "sample_time": 1000.0,
        "satelite_speed": 1.0
      }, {
        "sample_time": 2000.0,
        "satelite_speed": 1.5
      } ]
    }
    )json"),
            base::JSONReader::Read(json_string));
}

}  // namespace power_sampler
