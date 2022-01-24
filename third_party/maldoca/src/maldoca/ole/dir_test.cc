// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Unit-tests for the OLE directory reading code.

#include "maldoca/ole/dir.h"

#include "absl/flags/parse.h"
#include "absl/memory/memory.h"
#include "absl/strings/ascii.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "maldoca/base/file.h"
#include "maldoca/base/testing/status_matchers.h"
#include "maldoca/base/testing/test_utils.h"
#include "maldoca/ole/fat.h"

using ::maldoca::DirectoryStorageType;
using ::maldoca::FAT;
using ::maldoca::OLEDirectoryEntry;
using ::maldoca::OLEHeader;

namespace {
std::string TestFilename(absl::string_view filename) {
  return maldoca::testing::OleTestFilename(filename);
}

std::string GetTestContent(absl::string_view filename) {
  std::string content;
  auto status =
      maldoca::testing::GetTestContents(TestFilename(filename), &content);
  MALDOCA_EXPECT_OK(status) << status;
  return content;
}

static constexpr uint8_t clsid[] = {0, 1, 2,  3,  4,  5,  6,  7,
                                    8, 9, 10, 11, 12, 13, 14, 15};

class OLEDirectoryEntryTestFull : public testing::Test {
 protected:
  void SetUp() override {
    content = GetTestContent("vba1_xor_0x42_encoded.bin");
    EXPECT_TRUE(OLEHeader::ParseHeader(content, &header));
    EXPECT_TRUE(header.IsInitialized());
    EXPECT_TRUE(FAT::Read(content, header, &fat));
  }
  std::string content;
  OLEHeader header;
  std::vector<uint32_t> fat;
};

TEST(OLEDirectoryEntryTest, BasicTest) {
  OLEDirectoryEntry entry;

  // From not initialized to initialized.
  EXPECT_FALSE(entry.IsInitialized());
  entry.Initialize("foo", DirectoryStorageType::Storage, 1, 2, 3, 4, 5, 6,
                   clsid, 8, 9, 10);
  EXPECT_TRUE(entry.IsInitialized());

  // Can't initialize twice.
  ASSERT_DEATH(entry.Initialize("foo", DirectoryStorageType::Storage, 1, 2, 3,
                                4, 5, 6, clsid, 8, 9, 10),
               "Check failed: !is_initialized_");
}

TEST(OLEDirectoryEntryTest, TreeTest) {
  // Testing bogus insertions in a tree.
  //
  // We are creating the child instance as a raw pointer because we
  // need, for testing purposes, a non-formal transfer of ownership
  // when child is transferred to its parent.
  OLEDirectoryEntry parent;
  OLEDirectoryEntry *child = new (OLEDirectoryEntry);
  parent.Initialize("parent", DirectoryStorageType::Storage, 1, 2, 3, 4, 5, 6,
                    clsid, 8, 9, 10);
  child->Initialize("child", DirectoryStorageType::Stream, 1, 2, 3, 4, 5, 6,
                    clsid, 8, 9, 10);
  OLEDirectoryEntry not_initialized;

  EXPECT_TRUE(parent.IsInitialized());
  EXPECT_TRUE(child->IsInitialized());
  EXPECT_FALSE(not_initialized.IsInitialized());
  ASSERT_DEATH(parent.AddChild(nullptr), "child");
  ASSERT_DEATH(parent.AddChild(&not_initialized),
               "Check failed: child->IsInitialized");
  // The child is now owned by its parent.
  EXPECT_TRUE(parent.AddChild(child));
  // Can't add a child that already has a parent.
  ASSERT_DEATH(parent.AddChild(child),
               "Check failed: child->parent_ == .*nullptr.*");

  // Can't add a child to a node of type Stream.
  OLEDirectoryEntry node1, node2;
  node1.Initialize("node1", DirectoryStorageType::Stream, 1, 2, 3, 4, 5, 6,
                   clsid, 8, 9, 10);
  node2.Initialize("node2", DirectoryStorageType::Stream, 1, 2, 3, 4, 5, 6,
                   clsid, 8, 9, 10);
  EXPECT_FALSE(node1.AddChild(&node2));

  // Build a simple tree to test node retrieval methods.
  //
  // root
  //  |---- B 'b'
  //  |     |---- B1 'b1'
  //  |     |---- B2 'b2'
  //  |     `---- B3 'b3'
  //  |
  //  `---- C 'c'
  //        |---- C1 'B1'
  //        `---- C2 'c2'
  //
  // Note that node B1 and C2 have the same name 'b1' (after
  // conversion to lower case.)
  OLEDirectoryEntry root;
  root.Initialize("root", DirectoryStorageType::Root, 1, 2, 3, 4, 5, 6, clsid,
                  8, 9, 10);

  // All children need to be pointers so that ownership can be transferred
  // when the tree is built.
  std::unique_ptr<OLEDirectoryEntry> B, C;
  B = absl::make_unique<OLEDirectoryEntry>();
  C = absl::make_unique<OLEDirectoryEntry>();
  B->Initialize("b", DirectoryStorageType::Storage, 1, 2, 3, 4, 5, 6, clsid, 8,
                9, 10);
  C->Initialize("c", DirectoryStorageType::Storage, 1, 2, 3, 4, 5, 6, clsid, 8,
                9, 10);

  std::unique_ptr<OLEDirectoryEntry> B1, B2, B3;
  B1 = absl::make_unique<OLEDirectoryEntry>();
  B2 = absl::make_unique<OLEDirectoryEntry>();
  B3 = absl::make_unique<OLEDirectoryEntry>();
  B1->Initialize("b1", DirectoryStorageType::Stream, 1, 2, 3, 4, 5, 6, clsid, 8,
                 9, 10);
  B2->Initialize("b2", DirectoryStorageType::Stream, 1, 2, 3, 4, 5, 6, clsid, 8,
                 9, 10);
  B3->Initialize("b3", DirectoryStorageType::Stream, 1, 2, 3, 4, 5, 6, clsid, 8,
                 9, 10);

  std::unique_ptr<OLEDirectoryEntry> C1, C2;
  C1 = absl::make_unique<OLEDirectoryEntry>();
  C2 = absl::make_unique<OLEDirectoryEntry>();
  C1->Initialize("B1", DirectoryStorageType::Stream, 1, 2, 3, 4, 5, 6, clsid, 8,
                 9, 10);
  C2->Initialize("c2", DirectoryStorageType::Stream, 1, 2, 3, 4, 5, 6, clsid, 8,
                 9, 10);

  // Get pointer values before ownership is transferred.
  OLEDirectoryEntry *b_ptr = B.get();
  OLEDirectoryEntry *b1_ptr = B1.get();
  OLEDirectoryEntry *c1_ptr = C1.get();

  // Build the tree - child nodes ownership is transferred.
  EXPECT_TRUE(B->AddChild(B1.release()));
  EXPECT_TRUE(B->AddChild(B2.release()));
  EXPECT_TRUE(B->AddChild(B3.release()));
  EXPECT_TRUE(C->AddChild(C1.release()));
  EXPECT_TRUE(C->AddChild(C2.release()));
  EXPECT_TRUE(root.AddChild(B.release()));
  EXPECT_TRUE(root.AddChild(C.release()));

  // Finding a child (or not.)
  EXPECT_EQ(root.FindChildByName("b", DirectoryStorageType::Storage), b_ptr);
  EXPECT_EQ(root.FindChildByName("b", DirectoryStorageType::Stream), nullptr);
  EXPECT_EQ(root.FindChildByName("b1", DirectoryStorageType::Stream), nullptr);
  EXPECT_EQ(b_ptr->FindChildByName("b1", DirectoryStorageType::Stream), b1_ptr);
  EXPECT_EQ(b_ptr->FindChildByName("b1", DirectoryStorageType::Storage),
            nullptr);
  // Using yourself you can not find yourself.
  EXPECT_EQ(b_ptr->FindChildByName("b", DirectoryStorageType::Storage),
            nullptr);

  // Finding descendants (or not.)
  std::vector<OLEDirectoryEntry *> results;
  root.FindAllDescendants("b1", DirectoryStorageType::Stream, &results);
  std::vector<OLEDirectoryEntry *> expected_results = {b1_ptr, c1_ptr};
  EXPECT_EQ(results, expected_results);
  results.clear();
  root.FindAllDescendants("b1", DirectoryStorageType::Storage, &results);
  EXPECT_TRUE(results.empty());

  // Finding roots
  EXPECT_EQ(root.FindRoot(), &root);
  EXPECT_EQ(b1_ptr->FindRoot(), &root);
  EXPECT_EQ(c1_ptr->FindRoot(), b1_ptr->FindRoot());

  // Computing a path. The returned path preserve the case.
  EXPECT_EQ(root.Path(), "/");
  EXPECT_EQ(b1_ptr->Path(), "/b/b1");
  EXPECT_EQ(b_ptr->Path(), "/b");
  EXPECT_EQ(c1_ptr->Path(), "/c/B1");
}

TEST_F(OLEDirectoryEntryTestFull, EarlyErrors) {
  OLEDirectoryEntry root;

  // Can't use an uninitialized header.
  OLEHeader empty;
  std::vector<OLEDirectoryEntry *> dir_entries;
  std::string directory_stream;
  ASSERT_DEATH(OLEDirectoryEntry::ReadDirectory(
                   content, empty, fat, &root, &dir_entries, &directory_stream),
               "Check failed: header\\.IsInitialized");
  CHECK_EQ(dir_entries.size(), 0);

  // Can't use an empty FAT
  std::vector<uint32_t> no_fat;
  dir_entries.clear();
  directory_stream = "";
  ASSERT_DEATH(
      OLEDirectoryEntry::ReadDirectory(content, header, no_fat, &root,
                                       &dir_entries, &directory_stream),
      "Check failed: !fat\\.empty");
  CHECK_EQ(dir_entries.size(), 0);

  // You get a failure if you can't read the input as a stream
  dir_entries.clear();
  directory_stream = "";
  EXPECT_FALSE(OLEDirectoryEntry::ReadDirectory(
      "", header, fat, &root, &dir_entries, &directory_stream));
  CHECK_EQ(dir_entries.size(), 0);
  EXPECT_FALSE(root.IsInitialized());

  // Can't initialize a directory instance twice.
  dir_entries.clear();
  directory_stream = "";
  CHECK(OLEDirectoryEntry::ReadDirectory(content, header, fat, &root,
                                         &dir_entries, &directory_stream));
  CHECK_EQ(dir_entries.size(), 12);

  dir_entries.clear();
  directory_stream = "";
  ASSERT_DEATH(
      OLEDirectoryEntry::ReadDirectory(content, header, fat, &root,
                                       &dir_entries, &directory_stream),
      "Check failed: !directory->IsInitialized");
  CHECK_EQ(dir_entries.size(), 0);
}

TEST_F(OLEDirectoryEntryTestFull, ReadingDir) {
  OLEDirectoryEntry root;
  std::vector<OLEDirectoryEntry *> dir_entries;
  std::string directory_stream;
  CHECK(OLEDirectoryEntry::ReadDirectory(content, header, fat, &root,
                                         &dir_entries, &directory_stream));
  CHECK_EQ(dir_entries.size(), 12);
  // Root entry will be a null pointer since we never use it to check for
  // malformed/orphaned streams.
  CHECK_EQ(dir_entries[0], static_cast<OLEDirectoryEntry *>(nullptr));
  // The last directory entry is also a nullptr because it is either an orphan
  // or unused.
  CHECK_EQ(dir_entries.back(), static_cast<OLEDirectoryEntry *>(nullptr));
  for (int i = 1; i < dir_entries.size() - 1; i++) {
    CHECK_NE(dir_entries[i], static_cast<OLEDirectoryEntry *>(nullptr));
  }
  CHECK(root.IsInitialized());
  LOG(INFO) << root.ToString();

  // What we have read above has the following structure - you can
  // easily verify this by examing the output of root.ToString().
  //
  // Root Entry
  //     |---- VBA/
  //     |      |---- dir
  //     |      |---- __SRP_0
  //     |      |---- __SRP_1
  //     |      |---- __SRP_2
  //     |      |---- __SRP_3
  //     |      |---- _VBA_PROJECT
  //     |      `---- ThisDocument
  //     |
  //     |---- PROJECTwm
  //     `---- PROJECT

  OLEDirectoryEntry *vba =
      root.FindChildByName("vba", DirectoryStorageType::Storage);
  CHECK(vba != nullptr);
  for (auto const &entry : {"__SRP_0", "__SRP_1", "__SRP_2", "__SRP_3", "dir",
                            "_VBA_PROJECT", "ThisDocument"}) {
    EXPECT_EQ(vba->FindChildByName(absl::AsciiStrToLower(entry),
                                   DirectoryStorageType::Stream)
                  ->Name(),
              entry);
  }
  for (auto const &entry : {"PROJECT", "PROJECTwm"}) {
    EXPECT_EQ(root.FindChildByName(absl::AsciiStrToLower(entry),
                                   DirectoryStorageType::Stream)
                  ->Name(),
              entry);
  }

  // We can find VBA content in that tree, starting from root and its
  // root is root.
  EXPECT_EQ(root.FindVBAContentRoot(), &root);
  // But starting from VBA, we don't have enough visibility to find it.
  EXPECT_EQ(vba->FindVBAContentRoot(),
            static_cast<OLEDirectoryEntry *>(nullptr));
}

TEST_F(OLEDirectoryEntryTestFull, ChildrenAccess) {
  OLEDirectoryEntry empty;
  CHECK_EQ(empty.NumberOfChildren(), 0);
  CHECK_EQ(empty.ChildrenAt(-1), static_cast<OLEDirectoryEntry *>(nullptr));
  CHECK_EQ(empty.ChildrenAt(0), static_cast<OLEDirectoryEntry *>(nullptr));
  CHECK_EQ(empty.ChildrenAt(1), static_cast<OLEDirectoryEntry *>(nullptr));

  OLEDirectoryEntry root;
  std::vector<OLEDirectoryEntry *> dir_entries;
  std::string directory_stream;
  CHECK(OLEDirectoryEntry::ReadDirectory(content, header, fat, &root,
                                         &dir_entries, &directory_stream));
  CHECK(root.IsInitialized());

  // We're using the same directory structure that we used in
  // OLEDirectoryEntryTestFull.ReadingDir
  CHECK_EQ(root.NumberOfChildren(), 3);
  CHECK_EQ(root.ChildrenAt(-1), static_cast<OLEDirectoryEntry *>(nullptr));
  int32_t index;
  for (index = 0; index < root.NumberOfChildren(); ++index) {
    CHECK(root.ChildrenAt(index) != static_cast<OLEDirectoryEntry *>(nullptr));
  }
  CHECK_EQ(root.ChildrenAt(index), static_cast<OLEDirectoryEntry *>(nullptr));

  OLEDirectoryEntry *vba =
      root.FindChildByName("vba", DirectoryStorageType::Storage);
  CHECK(vba != nullptr);
  CHECK_EQ(vba->NumberOfChildren(), 7);
  CHECK_EQ(vba->ChildrenAt(-1), static_cast<OLEDirectoryEntry *>(nullptr));
  for (index = 0; index < root.NumberOfChildren(); ++index) {
    CHECK(root.ChildrenAt(index) != static_cast<OLEDirectoryEntry *>(nullptr));
  }
  CHECK_EQ(root.ChildrenAt(index), static_cast<OLEDirectoryEntry *>(nullptr));
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
#ifdef MALDOCA_CHROME
  // mini_chromium needs InitLogging 
  maldoca::InitLogging();
#endif
  return RUN_ALL_TESTS();
}
