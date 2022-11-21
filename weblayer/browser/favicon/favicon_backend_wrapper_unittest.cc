// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/favicon/favicon_backend_wrapper.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/favicon/core/favicon_backend.h"
#include "components/favicon/core/favicon_database.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace weblayer {
namespace {

// Blobs for adding favicons.
const unsigned char kBlob1[] =
    "12346102356120394751634516591348710478123649165419234519234512349134";

}  // namespace

class FaviconBackendWrapperTest : public testing::Test {
 protected:
  favicon::FaviconBackend* backend() {
    return wrapper_->favicon_backend_.get();
  }

  // testing::Test:
  void SetUp() override {
    // Get a temporary directory for the test DB files.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    db_path_ = temp_dir_.GetPath().AppendASCII("test_db");
  }

  void TearDown() override {
    wrapper_ = nullptr;
    testing::Test::TearDown();
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
  scoped_refptr<FaviconBackendWrapper> wrapper_;
};

TEST_F(FaviconBackendWrapperTest, BasicExpire) {
  wrapper_ = base::MakeRefCounted<FaviconBackendWrapper>(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  wrapper_->Init(db_path_);
  ASSERT_TRUE(backend());
  auto* db = backend()->db();

  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));
  GURL url("http://google.com");
  const base::Time time1 = base::Time::Now();
  favicon_base::FaviconID favicon_id1 =
      db->AddFavicon(url, favicon_base::IconType::kTouchIcon, favicon,
                     favicon::FaviconBitmapType::ON_VISIT, time1, gfx::Size());
  ASSERT_NE(0, favicon_id1);
  favicon::IconMappingID icon_mapping_id1 =
      db->AddIconMapping(url, favicon_id1);
  ASSERT_NE(0, icon_mapping_id1);

  // Fast forward past first expire running.
  task_environment_.FastForwardBy(kTimeDeltaForRunningExpireWithRemainingWork *
                                  2);
  // The icon should still be there.
  EXPECT_TRUE(db->GetFaviconHeader(favicon_id1, nullptr, nullptr));
  EXPECT_TRUE(db->HasMappingFor(favicon_id1));

  // Fast forward such that the icon is removed.
  task_environment_.FastForwardBy(kTimeDeltaWhenEntriesAreRemoved);
  EXPECT_FALSE(db->GetFaviconHeader(favicon_id1, nullptr, nullptr));
  EXPECT_FALSE(db->HasMappingFor(favicon_id1));
}

TEST_F(FaviconBackendWrapperTest, ExpireWithOneRemaining) {
  wrapper_ = base::MakeRefCounted<FaviconBackendWrapper>(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  wrapper_->Init(db_path_);
  ASSERT_TRUE(backend());
  auto* db = backend()->db();

  // Add two entries. The second is more recent then the first.
  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));
  GURL url("http://google.com");
  const base::Time time1 = base::Time::Now();
  favicon_base::FaviconID favicon_id1 =
      db->AddFavicon(url, favicon_base::IconType::kTouchIcon, favicon,
                     favicon::FaviconBitmapType::ON_VISIT, time1, gfx::Size());
  ASSERT_NE(0, favicon_id1);
  favicon::IconMappingID icon_mapping_id1 =
      db->AddIconMapping(url, favicon_id1);
  ASSERT_NE(0, icon_mapping_id1);
  const base::Time time2 = time1 + kTimeDeltaWhenEntriesAreRemoved / 2;
  favicon_base::FaviconID favicon_id2 =
      db->AddFavicon(url, favicon_base::IconType::kTouchIcon, favicon,
                     favicon::FaviconBitmapType::ON_VISIT, time2, gfx::Size());
  ASSERT_NE(0, favicon_id2);
  favicon::IconMappingID icon_mapping_id2 =
      db->AddIconMapping(url, favicon_id2);
  ASSERT_NE(0, icon_mapping_id2);

  // Fast forward such the first entry is expired and should be removed, but
  // not the second.
  task_environment_.FastForwardBy(kTimeDeltaWhenEntriesAreRemoved +
                                  base::Days(1));
  EXPECT_FALSE(db->GetFaviconHeader(favicon_id1, nullptr, nullptr));
  EXPECT_FALSE(db->HasMappingFor(favicon_id1));
  EXPECT_TRUE(db->GetFaviconHeader(favicon_id2, nullptr, nullptr));
  EXPECT_TRUE(db->HasMappingFor(favicon_id2));

  // Fast forward enough such that second is removed.
  task_environment_.FastForwardBy(kTimeDeltaWhenEntriesAreRemoved +
                                  base::Days(1));
  EXPECT_FALSE(db->GetFaviconHeader(favicon_id2, nullptr, nullptr));
  EXPECT_FALSE(db->HasMappingFor(favicon_id2));
}

}  // namespace weblayer
