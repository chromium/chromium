// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <vector>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_suite.h"
#include "base/trace_event/process_memory_dump.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/leveldb_features.h"
#include "third_party/leveldatabase/src/include/leveldb/cache.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

#define FPL FILE_PATH_LITERAL

#if defined(OS_WIN) && defined(DeleteFile)
#undef DeleteFile
#endif

using base::trace_event::MemoryDumpArgs;
using base::trace_event::MemoryDumpLevelOfDetail;
using base::trace_event::ProcessMemoryDump;
using leveldb::DB;
using leveldb::Env;
using leveldb::ReadOptions;
using leveldb::Slice;
using leveldb::Status;
using leveldb::WritableFile;
using leveldb::WriteOptions;
using leveldb_env::ChromiumEnv;
using leveldb_env::DBTracker;
using leveldb_env::MethodID;
using leveldb_env::Options;

namespace leveldb_env {

static const int kReadOnlyFileLimit = 4;

TEST(ErrorEncoding, OnlyAMethod) {
  const MethodID in_method = leveldb_env::kSequentialFileRead;
  const Status s = MakeIOError("Somefile.txt", "message", in_method);
  MethodID method;
  base::File::Error error = base::File::FILE_ERROR_MAX;
  EXPECT_EQ(leveldb_env::METHOD_ONLY, ParseMethodAndError(s, &method, &error));
  EXPECT_EQ(in_method, method);
  EXPECT_EQ(base::File::FILE_ERROR_MAX, error);
}

TEST(ErrorEncoding, FileError) {
  const MethodID in_method = leveldb_env::kWritableFileClose;
  const base::File::Error fe = base::File::FILE_ERROR_INVALID_OPERATION;
  const Status s = MakeIOError("Somefile.txt", "message", in_method, fe);
  MethodID method;
  base::File::Error error;
  EXPECT_EQ(leveldb_env::METHOD_AND_BFE,
            ParseMethodAndError(s, &method, &error));
  EXPECT_EQ(in_method, method);
  EXPECT_EQ(fe, error);
}

TEST(ErrorEncoding, NoEncodedMessage) {
  Status s = Status::IOError("Some message", "from leveldb itself");
  MethodID method = leveldb_env::kRandomAccessFileRead;
  base::File::Error error = base::File::FILE_ERROR_MAX;
  EXPECT_EQ(leveldb_env::NONE, ParseMethodAndError(s, &method, &error));
  EXPECT_EQ(leveldb_env::kRandomAccessFileRead, method);
  EXPECT_EQ(base::File::FILE_ERROR_MAX, error);
}

template <typename T>
class ChromiumEnvMultiPlatformTests : public ::testing::Test {
 public:
};

typedef ::testing::Types<ChromiumEnv> ChromiumEnvMultiPlatformTestsTypes;
TYPED_TEST_SUITE(ChromiumEnvMultiPlatformTests,
                 ChromiumEnvMultiPlatformTestsTypes);

int CountFilesWithExtension(const base::FilePath& dir,
                            const base::FilePath::StringType& extension) {
  int matching_files = 0;
  base::FileEnumerator dir_reader(
      dir, false, base::FileEnumerator::FILES);
  for (base::FilePath fname = dir_reader.Next(); !fname.empty();
       fname = dir_reader.Next()) {
    if (fname.MatchesExtension(extension))
      matching_files++;
  }
  return matching_files;
}

bool GetFirstLDBFile(const base::FilePath& dir, base::FilePath* ldb_file) {
  base::FileEnumerator dir_reader(
      dir, false, base::FileEnumerator::FILES);
  for (base::FilePath fname = dir_reader.Next(); !fname.empty();
       fname = dir_reader.Next()) {
    if (fname.MatchesExtension(FPL(".ldb"))) {
      *ldb_file = fname;
      return true;
    }
  }
  return false;
}

TEST(ChromiumEnv, DeleteBackupTables) {
  Options options;
  options.create_if_missing = true;
  options.env = Env::Default();

  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath dir = scoped_temp_dir.GetPath();

  DB* db;
  Status status = DB::Open(options, dir.AsUTF8Unsafe(), &db);
  EXPECT_TRUE(status.ok()) << status.ToString();
  status = db->Put(WriteOptions(), "key", "value");
  EXPECT_TRUE(status.ok()) << status.ToString();
  Slice a = "a";
  Slice z = "z";
  db->CompactRange(&a, &z);  // Ensure manifest written out to table.
  delete db;
  db = nullptr;

  // Current ChromiumEnv no longer makes backup tables - verify for sanity.
  EXPECT_EQ(1, CountFilesWithExtension(dir, FPL(".ldb")));
  EXPECT_EQ(0, CountFilesWithExtension(dir, FPL(".bak")));

  // Manually create our own backup table to simulate opening db created by
  // prior release.
  base::FilePath ldb_path;
  ASSERT_TRUE(GetFirstLDBFile(dir, &ldb_path));
  base::FilePath bak_path = ldb_path.ReplaceExtension(FPL(".bak"));
  ASSERT_TRUE(base::CopyFile(ldb_path, bak_path));
  EXPECT_EQ(1, CountFilesWithExtension(dir, FPL(".bak")));

  // Now reopen and close then verify the backup file was deleted.
  status = DB::Open(options, dir.AsUTF8Unsafe(), &db);
  EXPECT_TRUE(status.ok()) << status.ToString();
  EXPECT_EQ(0, CountFilesWithExtension(dir, FPL(".bak")));
  delete db;
  EXPECT_EQ(1, CountFilesWithExtension(dir, FPL(".ldb")));
  EXPECT_EQ(0, CountFilesWithExtension(dir, FPL(".bak")));
}

TEST(ChromiumEnv, GetChildrenEmptyDir) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath dir = scoped_temp_dir.GetPath();

  Env* env = Env::Default();
  std::vector<std::string> result;
  leveldb::Status status = env->GetChildren(dir.AsUTF8Unsafe(), &result);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(0U, result.size());
}

TEST(ChromiumEnv, GetChildrenPriorResults) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath dir = scoped_temp_dir.GetPath();

  base::FilePath new_file_dir = dir.Append(FPL("tmp_file"));
  FILE* f = fopen(new_file_dir.AsUTF8Unsafe().c_str(), "w");
  if (f) {
    fputs("Temp file contents", f);
    fclose(f);
  }

  Env* env = Env::Default();
  std::vector<std::string> result;
  leveldb::Status status = env->GetChildren(dir.AsUTF8Unsafe(), &result);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(1U, result.size());

  // And a second time should also return one result
  status = env->GetChildren(dir.AsUTF8Unsafe(), &result);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(1U, result.size());
}

TEST(ChromiumEnv, TestWriteBufferSize) {
  // If can't get disk size, use leveldb defaults.
  const int64_t MB = 1024 * 1024;
  EXPECT_EQ(size_t(4 * MB), leveldb_env::WriteBufferSize(-1));

  // A very small disk (check lower clamp value).
  EXPECT_EQ(size_t(1 * MB), leveldb_env::WriteBufferSize(1 * MB));

  // Some value on the linear equation between min and max.
  EXPECT_EQ(size_t(2.5 * MB), leveldb_env::WriteBufferSize(25 * MB));

  // The disk size equating to the max buffer size
  EXPECT_EQ(size_t(4 * MB), leveldb_env::WriteBufferSize(40 * MB));

  // Make sure sizes larger than 40MB are clamped to max buffer size.
  EXPECT_EQ(size_t(4 * MB), leveldb_env::WriteBufferSize(80 * MB));

  // Check for very large disk size (catch overflow).
  EXPECT_EQ(size_t(4 * MB), leveldb_env::WriteBufferSize(100 * MB * MB));
}

TEST(ChromiumEnv, LockFile) {
  base::FilePath tmp_file_path;
  base::CreateTemporaryFile(&tmp_file_path);
  leveldb::FileLock* lock = nullptr;

  Env* env = Env::Default();
  EXPECT_TRUE(env->LockFile(tmp_file_path.MaybeAsASCII(), &lock).ok());
  EXPECT_NE(nullptr, lock);

  leveldb::FileLock* failed_lock = nullptr;
  EXPECT_FALSE(env->LockFile(tmp_file_path.MaybeAsASCII(), &failed_lock).ok());
  EXPECT_EQ(nullptr, failed_lock);

  EXPECT_TRUE(env->UnlockFile(lock).ok());
  EXPECT_TRUE(env->LockFile(tmp_file_path.MaybeAsASCII(), &lock).ok());
  EXPECT_TRUE(env->UnlockFile(lock).ok());
}

TEST(ChromiumEnvTest, TestOpenOnRead) {
  // Write some test data to a single file that will be opened |n| times.
  base::FilePath tmp_file_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&tmp_file_path));

  FILE* f = fopen(tmp_file_path.AsUTF8Unsafe().c_str(), "w");
  ASSERT_TRUE(f != NULL);
  const char kFileData[] = "abcdefghijklmnopqrstuvwxyz";
  fputs(kFileData, f);
  fclose(f);

  std::unique_ptr<ChromiumEnv> env(new ChromiumEnv());
  env->SetReadOnlyFileLimitForTesting(kReadOnlyFileLimit);

  // Open test file some number greater than kReadOnlyFileLimit to force the
  // open-on-read behavior of POSIX Env leveldb::RandomAccessFile.
  const int kNumFiles = kReadOnlyFileLimit + 5;
  leveldb::RandomAccessFile* files[kNumFiles] = {0};
  for (int i = 0; i < kNumFiles; i++) {
    ASSERT_TRUE(
        env->NewRandomAccessFile(tmp_file_path.AsUTF8Unsafe(), &files[i]).ok());
  }
  char scratch;
  Slice read_result;
  for (int i = 0; i < kNumFiles; i++) {
    ASSERT_TRUE(files[i]->Read(i, 1, &read_result, &scratch).ok());
    ASSERT_EQ(kFileData[i], read_result[0]);
  }
  for (int i = 0; i < kNumFiles; i++) {
    delete files[i];
  }
  ASSERT_TRUE(env->DeleteFile(tmp_file_path.AsUTF8Unsafe()).ok());
}

class ChromiumEnvDBTrackerTest : public ::testing::Test {
 protected:
  ChromiumEnvDBTrackerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}
  void SetUp() override {
    testing::Test::SetUp();
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  }

  const base::FilePath& temp_path() const { return scoped_temp_dir_.GetPath(); }

  using VisitedDBSet = std::set<DBTracker::TrackedDB*>;

  static VisitedDBSet VisitDatabases() {
    VisitedDBSet visited;
    auto db_visitor = [](VisitedDBSet* visited, DBTracker::TrackedDB* db) {
      ASSERT_TRUE(visited->insert(db).second)
          << "Database " << std::hex << db << " visited for the second time";
    };
    DBTracker::GetInstance()->VisitDatabases(
        base::BindRepeating(db_visitor, base::Unretained(&visited)));
    return visited;
  }

  using LiveDBSet = std::vector<std::unique_ptr<DBTracker::TrackedDB>>;

  void AssertEqualSets(const LiveDBSet& live_dbs,
                       const VisitedDBSet& visited_dbs) {
    for (const auto& live_db : live_dbs) {
      ASSERT_EQ(1u, visited_dbs.count(live_db.get()))
          << "Database " << std::hex << live_db.get() << " was not visited";
    }
    ASSERT_EQ(live_dbs.size(), visited_dbs.size())
        << "Extra databases were visited";
  }

 private:
  base::ScopedTempDir scoped_temp_dir_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ChromiumEnvDBTrackerTest, OpenDatabase) {
  struct KeyValue {
    const char* key;
    const char* value;
  };
  constexpr KeyValue db_data[] = {
      {"banana", "yellow"}, {"sky", "blue"}, {"enthusiasm", ""},
  };

  // Open a new database using DBTracker::Open, write some data.
  Options options;
  options.create_if_missing = true;
  std::string name = temp_path().AsUTF8Unsafe();
  DBTracker::TrackedDB* tracked_db;
  Status status =
      DBTracker::GetInstance()->OpenDatabase(options, name, &tracked_db);
  ASSERT_TRUE(status.ok()) << status.ToString();
  for (const auto& kv : db_data) {
    status = tracked_db->Put(WriteOptions(), kv.key, kv.value);
    ASSERT_TRUE(status.ok()) << status.ToString();
  }

  // Close the database.
  delete tracked_db;

  // Open the database again with DB::Open, and check the data.
  options.create_if_missing = false;
  leveldb::DB* plain_db = nullptr;
  status = leveldb::DB::Open(options, name, &plain_db);
  ASSERT_TRUE(status.ok()) << status.ToString();
  for (const auto& kv : db_data) {
    std::string value;
    status = plain_db->Get(ReadOptions(), kv.key, &value);
    ASSERT_TRUE(status.ok()) << status.ToString();
    ASSERT_EQ(value, kv.value);
  }
  delete plain_db;
}

TEST_F(ChromiumEnvDBTrackerTest, TrackedDBInfo) {
  Options options;
  options.create_if_missing = true;
  std::string name = temp_path().AsUTF8Unsafe();
  DBTracker::TrackedDB* db;
  Status status = DBTracker::GetInstance()->OpenDatabase(options, name, &db);
  ASSERT_TRUE(status.ok()) << status.ToString();

  // Check that |db| reports info that was used to open it.
  ASSERT_EQ(name, db->name());

  delete db;
}

TEST_F(ChromiumEnvDBTrackerTest, VisitDatabases) {
  LiveDBSet live_dbs;

  // Open several databases.
  for (const char* tag : {"poets", "movies", "recipes", "novels"}) {
    Options options;
    options.create_if_missing = true;
    std::string name = temp_path().AppendASCII(tag).AsUTF8Unsafe();
    DBTracker::TrackedDB* db;
    Status status = DBTracker::GetInstance()->OpenDatabase(options, name, &db);
    ASSERT_TRUE(status.ok()) << status.ToString();
    live_dbs.emplace_back(db);
  }

  // Check that all live databases are visited.
  AssertEqualSets(live_dbs, VisitDatabases());

  // Close couple of a databases.
  live_dbs.erase(live_dbs.begin());
  live_dbs.erase(live_dbs.begin() + 1);

  // Check that only remaining live databases are visited.
  AssertEqualSets(live_dbs, VisitDatabases());
}

TEST_F(ChromiumEnvDBTrackerTest, OpenDBTracking) {
  Options options;
  options.create_if_missing = true;
  std::unique_ptr<leveldb::DB> db;
  auto status = leveldb_env::OpenDB(options, temp_path().AsUTF8Unsafe(), &db);
  ASSERT_TRUE(status.ok()) << status.ToString();

  auto visited_dbs = VisitDatabases();

  // Databases returned by OpenDB() should be tracked.
  ASSERT_EQ(1u, visited_dbs.size());
  ASSERT_EQ(db.get(), *visited_dbs.begin());
}

TEST_F(ChromiumEnvDBTrackerTest, IsTrackedDB) {
  leveldb_env::Options options;
  options.create_if_missing = true;
  leveldb::DB* untracked_db;
  base::ScopedTempDir untracked_temp_dir;
  ASSERT_TRUE(untracked_temp_dir.CreateUniqueTempDir());
  leveldb::Status s = leveldb::DB::Open(
      options, untracked_temp_dir.GetPath().AsUTF8Unsafe(), &untracked_db);
  ASSERT_TRUE(s.ok());
  EXPECT_FALSE(DBTracker::GetInstance()->IsTrackedDB(untracked_db));

  // Now a tracked db.
  std::unique_ptr<leveldb::DB> tracked_db;
  base::ScopedTempDir tracked_temp_dir;
  ASSERT_TRUE(tracked_temp_dir.CreateUniqueTempDir());
  s = leveldb_env::OpenDB(options, tracked_temp_dir.GetPath().AsUTF8Unsafe(),
                          &tracked_db);
  ASSERT_TRUE(s.ok());
  EXPECT_TRUE(DBTracker::GetInstance()->IsTrackedDB(tracked_db.get()));

  delete untracked_db;
}

TEST_F(ChromiumEnvDBTrackerTest, CheckMemEnv) {
  Env* env = leveldb::Env::Default();
  ASSERT_TRUE(env != nullptr);
  EXPECT_FALSE(leveldb_chrome::IsMemEnv(env));

  std::unique_ptr<leveldb::Env> memenv =
      leveldb_chrome::NewMemEnv("CheckMemEnv", env);
  EXPECT_TRUE(leveldb_chrome::IsMemEnv(memenv.get()));
}

TEST_F(ChromiumEnvDBTrackerTest, MemoryDumpCreation) {
  Options options;
  options.create_if_missing = true;
  leveldb::Cache* web_cache = leveldb_chrome::GetSharedWebBlockCache();
  leveldb::Cache* browser_cache = leveldb_chrome::GetSharedBrowserBlockCache();
  options.block_cache = web_cache;
  std::unique_ptr<leveldb::DB> db1;
  base::ScopedTempDir temp_dir1;
  ASSERT_TRUE(temp_dir1.CreateUniqueTempDir());
  base::ScopedTempDir temp_dir2;
  ASSERT_TRUE(temp_dir2.CreateUniqueTempDir());
  base::ScopedTempDir temp_dir3;
  ASSERT_TRUE(temp_dir3.CreateUniqueTempDir());

  auto status =
      leveldb_env::OpenDB(options, temp_dir1.GetPath().AsUTF8Unsafe(), &db1);
  ASSERT_TRUE(status.ok()) << status.ToString();

  std::unique_ptr<leveldb::DB> db2;
  status =
      leveldb_env::OpenDB(options, temp_dir2.GetPath().AsUTF8Unsafe(), &db2);
  ASSERT_TRUE(status.ok()) << status.ToString();

  std::unique_ptr<leveldb::DB> db3;
  options.block_cache = browser_cache;
  status =
      leveldb_env::OpenDB(options, temp_dir3.GetPath().AsUTF8Unsafe(), &db3);
  ASSERT_TRUE(status.ok()) << status.ToString();

  auto db_visitor = [](DBTracker::TrackedDB* db) {
    leveldb::Cache* db_cache =
        (db->block_cache_type() == DBTracker::SharedReadCacheUse_Browser)
            ? leveldb_chrome::GetSharedBrowserBlockCache()
            : leveldb_chrome::GetSharedWebBlockCache();
    size_t initial_cache_size = db_cache->TotalCharge();
    auto status = db->Put(WriteOptions(), "key", "value");
    EXPECT_TRUE(status.ok()) << status.ToString();
    db->CompactRange(nullptr, nullptr);
    std::string value;
    status = db->Get(ReadOptions(), "key", &value);
    ASSERT_TRUE(status.ok()) << status.ToString();
    EXPECT_GT(db_cache->TotalCharge(), initial_cache_size);
  };
  DBTracker::GetInstance()->VisitDatabases(base::BindRepeating(db_visitor));
  ASSERT_EQ(browser_cache->TotalCharge() * 2, web_cache->TotalCharge());

  MemoryDumpArgs dump_args = {MemoryDumpLevelOfDetail::BACKGROUND};
  base::trace_event::ProcessMemoryDump pmd(dump_args);
  auto* mad1 = DBTracker::GetOrCreateAllocatorDump(&pmd, db1.get());
  auto* mad2 = DBTracker::GetOrCreateAllocatorDump(&pmd, db2.get());
  auto* mad3 = DBTracker::GetOrCreateAllocatorDump(&pmd, db3.get());

  // All databases should have the same size since we made the same changes.
  size_t db_size = mad1->GetSizeInternal();
  EXPECT_GT(db_size, 0ul);
  EXPECT_EQ(db_size, mad2->GetSizeInternal());
  EXPECT_EQ(db_size, mad3->GetSizeInternal());
}

TEST_F(ChromiumEnvDBTrackerTest, MemEnvMemoryDumpCreation) {
  std::unique_ptr<leveldb::Env> memenv = leveldb_chrome::NewMemEnv("test");

  Status s;
  WritableFile* writable_file;
  s = memenv->NewWritableFile("first_file.txt", &writable_file);
  ASSERT_TRUE(s.ok()) << s.ToString();

  const std::string kValue(2048, 'x');
  writable_file->Append(Slice(kValue));
  delete writable_file;

  const MemoryDumpArgs dump_args = {MemoryDumpLevelOfDetail::BACKGROUND};
  base::trace_event::ProcessMemoryDump dump1(dump_args);
  auto* mad = DBTracker::GetOrCreateAllocatorDump(&dump1, memenv.get());

  uint64_t size_with_file = mad->GetSizeInternal();
  EXPECT_GE(size_with_file, kValue.size());

  // Now rename and size should be unchanged.
  s = memenv->RenameFile("first_file.txt", "xxxxx_file.txt");  // same length.
  EXPECT_TRUE(s.ok()) << s.ToString();
  base::trace_event::ProcessMemoryDump dump2(dump_args);
  mad = DBTracker::GetOrCreateAllocatorDump(&dump2, memenv.get());
  EXPECT_EQ(size_with_file, mad->GetSizeInternal());

  // Now delete and size should go down.
  s = memenv->DeleteFile("xxxxx_file.txt");
  EXPECT_TRUE(s.ok()) << s.ToString();

  base::trace_event::ProcessMemoryDump dump3(dump_args);
  mad = DBTracker::GetOrCreateAllocatorDump(&dump3, memenv.get());
  EXPECT_EQ(mad->GetSizeInternal(), 0ul);
}

TEST(ChromiumLevelDB, PossiblyValidDB) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  leveldb::Env* default_env = leveldb::Env::Default();
  const base::FilePath& db_path = scoped_temp_dir.GetPath();
  EXPECT_FALSE(leveldb_chrome::PossiblyValidDB(db_path, default_env));

  {
    base::File current(db_path.Append(FILE_PATH_LITERAL("CURRENT")),
                       base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    ASSERT_TRUE(current.IsValid());
    const char kString[] = "ManifestFile";
    EXPECT_EQ(static_cast<int>(sizeof(kString)),
              current.Write(0, kString, sizeof(kString)));
  }

  EXPECT_TRUE(leveldb_chrome::PossiblyValidDB(db_path, default_env));

  ASSERT_TRUE(scoped_temp_dir.Delete());
  EXPECT_FALSE(leveldb_chrome::PossiblyValidDB(db_path, default_env));
}

TEST(ChromiumLevelDB, DeleteOnDiskDB) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  const base::FilePath db_path = scoped_temp_dir.GetPath().AppendASCII("db");
  leveldb_env::Options on_disk_options;
  on_disk_options.create_if_missing = true;

  // First with no db directory.
  EXPECT_FALSE(base::PathExists(db_path));
  Status s = leveldb_chrome::DeleteDB(db_path, on_disk_options);
  EXPECT_TRUE(s.ok()) << s.ToString();

  // Now an empty directory.
  EXPECT_FALSE(base::PathExists(db_path));
  EXPECT_TRUE(base::CreateDirectory(db_path));
  s = leveldb_chrome::DeleteDB(db_path, on_disk_options);
  EXPECT_TRUE(s.ok()) << s.ToString();
  EXPECT_FALSE(base::PathExists(db_path));

  // Now with a valid leveldb database and an extra file.
  std::unique_ptr<leveldb::DB> db;
  s = OpenDB(on_disk_options, db_path.AsUTF8Unsafe(), &db);
  ASSERT_TRUE(s.ok()) << s.ToString();
  s = db->Put(WriteOptions(), "TheKey", "TheValue");
  EXPECT_TRUE(s.ok()) << s.ToString();
  db.reset();

  base::File test_file(db_path.Append(FILE_PATH_LITERAL("Test file.txt")),
                       base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  ASSERT_TRUE(test_file.IsValid());
  const char kString[] = "Just some text.";
  const int data_len = static_cast<int>(sizeof(kString));
  EXPECT_EQ(data_len, test_file.Write(0, kString, data_len));
  test_file.Close();

  EXPECT_TRUE(leveldb_chrome::PossiblyValidDB(db_path, on_disk_options.env));
  s = leveldb_chrome::DeleteDB(db_path, on_disk_options);
  EXPECT_TRUE(s.ok()) << s.ToString();

  EXPECT_FALSE(base::PathExists(db_path));
}

TEST(ChromiumLevelDB, DeleteInMemoryDB) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  // First create an on-disk db with an extra file.
  const base::FilePath db_path = scoped_temp_dir.GetPath().AppendASCII("db");
  base::FilePath temp_path = db_path.Append(FILE_PATH_LITERAL("Test file.txt"));
  leveldb_env::Options on_disk_options;
  on_disk_options.create_if_missing = true;

  {
    std::unique_ptr<leveldb::DB> db;
    Status s = OpenDB(on_disk_options, db_path.AsUTF8Unsafe(), &db);
    ASSERT_TRUE(s.ok()) << s.ToString();
    s = db->Put(WriteOptions(), "TheKey", "TheValue");
    EXPECT_TRUE(s.ok()) << s.ToString();
    db.reset();

    base::File test_file(
        temp_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    ASSERT_TRUE(test_file.IsValid());
    const char kString[] = "Just some text.";
    const int data_len = static_cast<int>(sizeof(kString));
    EXPECT_EQ(data_len, test_file.Write(0, kString, data_len));
    test_file.Close();
  }

  // Now create an in-memory db.
  std::unique_ptr<leveldb::Env> mem_env = leveldb_chrome::NewMemEnv("testing");
  leveldb_env::Options in_memory_options;
  in_memory_options.create_if_missing = true;
  in_memory_options.env = mem_env.get();

  {
    std::unique_ptr<leveldb::DB> db;
    // The two DB's purposely use the same path even though the in-memory path
    // refers to a temp directory on disk.
    Status s = OpenDB(in_memory_options, db_path.AsUTF8Unsafe(), &db);
    ASSERT_TRUE(s.ok()) << s.ToString();
    s = db->Put(WriteOptions(), "TheKey", "TheValue");
    EXPECT_TRUE(s.ok()) << s.ToString();
    db.reset();

    leveldb::WritableFile* temp_file;
    s = mem_env->NewWritableFile(temp_path.AsUTF8Unsafe(), &temp_file);
    ASSERT_TRUE(s.ok()) << s.ToString();
    s = temp_file->Append("Just some text.");
    EXPECT_TRUE(s.ok()) << s.ToString();
    s = temp_file->Close();
    EXPECT_TRUE(s.ok()) << s.ToString();
    delete temp_file;
  }

  EXPECT_TRUE(leveldb_chrome::PossiblyValidDB(db_path, on_disk_options.env));
  EXPECT_TRUE(mem_env->FileExists(temp_path.AsUTF8Unsafe()));
  Status s = leveldb_chrome::DeleteDB(db_path, in_memory_options);
  EXPECT_TRUE(s.ok()) << s.ToString();
  EXPECT_FALSE(mem_env->FileExists(temp_path.AsUTF8Unsafe()));
  // On disk should be untouched.
  EXPECT_TRUE(leveldb_chrome::PossiblyValidDB(db_path, on_disk_options.env));
}

class ChromiumLevelDBRebuildTest : public ::testing::Test {
 protected:
  ChromiumLevelDBRebuildTest() {
    feature_list_.InitAndEnableFeature(leveldb::kLevelDBRewriteFeature);
  }

  void SetUp() override {
    testing::Test::SetUp();
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  }

  const base::FilePath& temp_path() const { return scoped_temp_dir_.GetPath(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::ScopedAllowBlockingForTesting allow_blocking_;
  base::ScopedTempDir scoped_temp_dir_;
};

TEST_F(ChromiumLevelDBRebuildTest, RebuildDb) {
  std::unique_ptr<leveldb::DB> db;
  base::FilePath db_path = temp_path().AppendASCII("db");
  leveldb_env::Options options;
  options.create_if_missing = true;

  auto s = leveldb_env::OpenDB(options, db_path.AsUTF8Unsafe(), &db);
  ASSERT_TRUE(s.ok());
  db->Put(leveldb::WriteOptions(), "key1", "value1");
  db->Put(leveldb::WriteOptions(), "key2", "value2");
  db->Delete(leveldb::WriteOptions(), "key1");

  leveldb::DB* old_db_ptr = db.get();
  s = leveldb_env::RewriteDB(options, db_path.AsUTF8Unsafe(), &db);
  EXPECT_TRUE(s.ok());
  EXPECT_NE(old_db_ptr, db.get());
  EXPECT_TRUE(db);

  std::string value;
  s = db->Get(leveldb::ReadOptions(), "key1", &value);
  EXPECT_TRUE(s.IsNotFound());
  s = db->Get(leveldb::ReadOptions(), "key2", &value);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ("value2", value);
}

TEST_F(ChromiumLevelDBRebuildTest, RecoverMissingDB) {
  std::unique_ptr<leveldb::DB> db;
  base::FilePath db_path = temp_path().AppendASCII("db");
  base::FilePath tmp_path =
      temp_path().AppendASCII(leveldb_env::DatabaseNameForRewriteDB("db"));
  leveldb_env::Options options;
  options.create_if_missing = true;

  // Write a temporary db to simulate a failed rewrite attempt where only the
  // temporary db exists.
  auto s = leveldb_env::OpenDB(options, tmp_path.AsUTF8Unsafe(), &db);
  ASSERT_TRUE(s.ok());
  db->Put(leveldb::WriteOptions(), "key", "value");
  db.reset();

  EXPECT_FALSE(base::DirectoryExists(db_path));
  EXPECT_TRUE(base::DirectoryExists(tmp_path));

  // Open the regular db and check if the temporary one is recovered.
  s = leveldb_env::OpenDB(options, db_path.AsUTF8Unsafe(), &db);
  ASSERT_TRUE(s.ok()) << s.ToString();
  std::string value;
  s = db->Get(leveldb::ReadOptions(), "key", &value);
  EXPECT_TRUE(s.ok()) << s.ToString();
  EXPECT_EQ("value", value);
  EXPECT_TRUE(base::DirectoryExists(db_path));
  EXPECT_FALSE(base::DirectoryExists(tmp_path));
}

TEST_F(ChromiumLevelDBRebuildTest, RecoverCorruptDB) {
  std::unique_ptr<leveldb::DB> db;
  base::FilePath db_path = temp_path().AppendASCII("db");
  base::FilePath tmp_path =
      temp_path().AppendASCII(leveldb_env::DatabaseNameForRewriteDB("db"));
  leveldb_env::Options options;
  options.create_if_missing = true;

  // Create a corrupt db.
  ASSERT_TRUE(base::CreateDirectory(db_path));
  ASSERT_TRUE(leveldb_chrome::CorruptClosedDBForTesting(db_path));

  // Write a temporary db to simulate a failed rewrite attempt.
  auto s = leveldb_env::OpenDB(options, tmp_path.AsUTF8Unsafe(), &db);
  ASSERT_TRUE(s.ok());
  db->Put(leveldb::WriteOptions(), "key", "value");
  db.reset();

  // Open the regular db and check if the temporary one is recovered.
  s = leveldb_env::OpenDB(options, db_path.AsUTF8Unsafe(), &db);
  ASSERT_TRUE(s.ok()) << s.ToString();
  std::string value;
  s = db->Get(leveldb::ReadOptions(), "key", &value);
  EXPECT_TRUE(s.ok()) << s.ToString();
  EXPECT_EQ("value", value);
  EXPECT_TRUE(base::DirectoryExists(db_path));
  EXPECT_FALSE(base::DirectoryExists(tmp_path));
}

TEST_F(ChromiumLevelDBRebuildTest, FinishCleanup) {
  std::unique_ptr<leveldb::DB> db;
  base::FilePath db_path = temp_path().AppendASCII("db");
  base::FilePath tmp_path =
      temp_path().AppendASCII(leveldb_env::DatabaseNameForRewriteDB("db"));
  leveldb_env::Options options;
  options.create_if_missing = true;

  // Write a regular and a temporary db to simulate a rewrite attempt that
  // crashed before finishing.
  auto s = leveldb_env::OpenDB(options, db_path.AsUTF8Unsafe(), &db);
  ASSERT_TRUE(s.ok());
  db->Put(leveldb::WriteOptions(), "key", "regular");
  db.reset();

  s = leveldb_env::OpenDB(options, tmp_path.AsUTF8Unsafe(), &db);
  ASSERT_TRUE(s.ok());
  db->Put(leveldb::WriteOptions(), "key", "temp");
  db.reset();

  EXPECT_TRUE(base::DirectoryExists(db_path));
  EXPECT_TRUE(base::DirectoryExists(tmp_path));

  // Open the regular db and check that the temporary one was cleaned up.
  s = leveldb_env::OpenDB(options, db_path.AsUTF8Unsafe(), &db);
  ASSERT_TRUE(s.ok()) << s.ToString();
  std::string value;
  s = db->Get(leveldb::ReadOptions(), "key", &value);
  EXPECT_TRUE(s.ok()) << s.ToString();
  EXPECT_EQ("regular", value);
  EXPECT_TRUE(base::DirectoryExists(db_path));
  EXPECT_FALSE(base::DirectoryExists(tmp_path));
}

}  // namespace leveldb_env

int main(int argc, char** argv) { return base::TestSuite(argc, argv).Run(); }
