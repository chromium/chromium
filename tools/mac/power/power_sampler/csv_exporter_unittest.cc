// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/mac/power/power_sampler/csv_exporter.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace power_sampler {
namespace {

class CsvExporterTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void TestExporter(base::FilePath file_path,
                    base::TimeTicks time_base,
                    std::unique_ptr<CsvExporter> exporter);

  base::ScopedTempDir temp_dir_;
};

void CsvExporterTest::TestExporter(base::FilePath file_path,
                                   base::TimeTicks time_base,
                                   std::unique_ptr<CsvExporter> exporter) {
  DataColumnKey speed1{"odometer", "speed"};
  DataColumnKey speed2{"satelite", "speed"};
  DataColumnKey height{"satelite", "height"};
  exporter->OnStartSession({
      {speed1, "m/s"},
      {speed2, "m/s"},
      {height, "km"},
  });
  exporter->OnSample(time_base + base::Seconds(1),
                     {{speed1, 0.5}, {speed2, 1.0}});
  exporter->OnSample(time_base + base::Seconds(2), {{speed2, 1.5}});
  exporter->OnEndSession();
  exporter.reset();

  std::string csv_string;
  EXPECT_TRUE(base::ReadFileToString(file_path, &csv_string));
  std::string expected_string =
      R"(time(s),odometer_speed(m/s),satelite_height(km),satelite_speed(m/s)
1,0.5,,1
2,,,1.5
)";

  EXPECT_EQ(expected_string, csv_string);
}

TEST_F(CsvExporterTest, CreateWithPath) {
  base::FilePath file_path = temp_dir_.GetPath().Append("dummy.csv");
  base::TimeTicks time_base = base::TimeTicks::Now();
  TestExporter(file_path, time_base, CsvExporter::Create(time_base, file_path));
}

TEST_F(CsvExporterTest, CreateWithFile) {
  base::FilePath file_path = temp_dir_.GetPath().Append("dummy.csv");
  base::File dummy(file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(dummy.IsValid());

  base::TimeTicks time_base = base::TimeTicks::Now();
  TestExporter(file_path, time_base,
               CsvExporter::Create(time_base, std::move(dummy)));
}

}  // namespace

}  // namespace power_sampler
