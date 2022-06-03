/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// A class to represent a OLE2 directory entry.
//
// Instances of this class can be assembled into a tree and elements
// of that tree searched. A class static method is provided to extract
// an entire directory tree from an OLE2 input, provided that a header
// and a FAT have already been extracted:
//
//   OLEHeader header;
//   CHECK(OLEHeader::ParseHeader(input, &header));
//   CHECK(header.IsInitialized());
//   std::vector<uint32_t> fat;
//   CHECK(FAT::Read(input, header, &fat));
//   CHECK(!fat.empty());
//   OLEDirectoryEntry root;
//   CHECK(OLEDirectoryEntry::ReadDirectory(input, header, fat, &root));
//   CHECK(root.IsInitialized());

#ifndef MALDOCA_OLE_DIR_H_
#define MALDOCA_OLE_DIR_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "maldoca/base/logging.h"
#include "maldoca/ole/header.h"

namespace maldoca {

// Object type in directory storage (from AAF specifications).These
// constants need to be shared outside of the implementation for the
// OLEDirectoryEntry class so they are defined as enum members here.
typedef enum : uint8_t {
  Empty = 0,
  Storage = 1,
  Stream = 2,
  LockBytes = 3,
  Property = 4,
  Root = 5,
} DirectoryStorageType;

// This is standard windows GUID, or more general UUID:
// https://en.wikipedia.org/wiki/Universally_unique_identifier
// Since we just need to store the data and don't plan to interpret it as
// a GUID anywhere, let's define it as a simple uint8_t array
using OleGuid = uint8_t[16];

class OLEDirectoryEntry {
 public:
  OLEDirectoryEntry()
      : is_initialized_(false),
        entry_type_(DirectoryStorageType::Empty),
        node_index_(0),
        left_node_sector_index_(0),
        right_node_sector_index_(0),
        child_node_sector_index_(0),
        stream_sector_index_(0),
        stream_size_(0),
        user_flags_(0),
        creation_timestamp_(0),
        modification_timestamp_(0),
        parent_(nullptr) {}

  virtual ~OLEDirectoryEntry() {}

  // Disallow copy and assign.
  OLEDirectoryEntry(const OLEDirectoryEntry &) = delete;
  OLEDirectoryEntry &operator=(const OLEDirectoryEntry &) = delete;

  // Recursively dump the content of a tree from a root to a
  // string. Each level is tabulated 2 extra spaces.
  std::string ToString();

  // Set a new internal state for the instance and mark it as fully
  // initialized.
  void Initialize(const std::string &name, DirectoryStorageType entry_type,
                  uint32_t node_index, uint32_t left_node_sector_index,
                  uint32_t right_node_sector_index,
                  uint32_t child_node_sector_index, uint32_t stream_sector_index,
                  uint32_t stream_size, const OleGuid &clsid, uint32_t user_flags,
                  uint64_t creation_timestamp, uint64_t modification_timestamp);

  // Add a direct child to a node. The parent will assume ownership of
  // the child. We are checking that the child is initialized and
  // doesn't already have a parent. If the parent isn't a node that
  // have children, we return false.
  bool AddChild(OLEDirectoryEntry *child);

  // Find a child given its name and type. Stop at the first found
  // child and return a pointer to it or return a null pointer if
  // nothing is found. The search is case insensitive but it's up to
  // the caller to provide a lowercase name parameter as input.
  OLEDirectoryEntry *FindChildByName(const std::string &name,
                                     DirectoryStorageType type) const;

  // Find the root of a directory entry. This implementations verifies
  // that root->FindRoot() == root.
  OLEDirectoryEntry *FindRoot() const;

  // Find all descendant given a name and a type and add them to the
  // results argument. The search is case insensitive but it's up to
  // the caller to provide a lowercase name parameter as input.
  void FindAllDescendants(const std::string &name, DirectoryStorageType type,
                          std::vector<OLEDirectoryEntry *> *results) const;

  // Return a forward slash separated path from the root of the node
  // to the node itself, using each node name along the way as path
  // component.
  std::string Path() const;

  // We are looking for this subtree the current node and if we find
  // it we will return its parent. If that subtree isn't found, we
  // return nullptr.
  //
  //  |- VBA/
  //  |   |- _VBA_PROJECT
  //  |   `- dir
  //  `- PROJECT
  OLEDirectoryEntry *FindVBAContentRoot() const;

  // Find a PowerPoint 97 document stream root.
  // Reference:
  // https://msdn.microsoft.com/en-us/library/dd921564%28v=office.12%29.aspx
  OLEDirectoryEntry *FindPowerPointDocumentRoot() const;

  // Getters.
  bool IsInitialized() const { return is_initialized_; }
  std::string Name() const { return name_; }
  uint32_t NodeIndex() const { return node_index_; }
  uint32_t LeftNodeSectorIndex() const { return left_node_sector_index_; }
  uint32_t RightNodeSectorIndex() const { return right_node_sector_index_; }
  uint32_t ChildNodeSectorIndex() const { return child_node_sector_index_; }
  DirectoryStorageType EntryType() const { return entry_type_; }
  uint32_t StreamFirstSectorIndex() const { return stream_sector_index_; }
  uint32_t StreamSize() const { return stream_size_; }
  OLEDirectoryEntry *Parent() const { return parent_; }
  const OleGuid &Clsid() const { return clsid_; }
  uint32_t UserFlags() const { return user_flags_; }
  uint64_t ModificationTimestamp() const { return modification_timestamp_; }
  uint64_t CreationTimestamp() const { return creation_timestamp_; }

  // Children access methods.
  const uint32_t NumberOfChildren() const { return children_.size(); }
  // Return a raw children pointer at a given index, nullptr if no children
  // exists or the index is out of bounds. There is no transfer of ownership.
  const OLEDirectoryEntry *ChildrenAt(int32_t index) const {
    if (children_.empty()) {
      return nullptr;
    }
    if (index < 0 || static_cast<size_t>(index) > children_.size() - 1) {
      return nullptr;
    }
    return children_[index].get();
  }

  // Initialize a OLEDirectoryEntry instance given an OLE file content,
  // a parsed OLEHeader instance and a FAT. Return true upon success.
  static bool ReadDirectory(absl::string_view input, const OLEHeader &header,
                            const std::vector<uint32_t> &fat,
                            OLEDirectoryEntry *directory,
                            std::vector<OLEDirectoryEntry *> *dir_entries,
                            std::string *directory_stream);

  static bool ReadDirectoryEntryFromStream(absl::string_view input,
                                           uint32_t index, uint32_t sector_size,
                                           OLEDirectoryEntry *directory_entry);

 private:
  void LogNodeWithTab(int num_spaces, std::string *output);

  bool is_initialized_;

  // The name of the node. We keep both the original and the lower
  // case value. The latter is used for comparisons.
  std::string name_;
  std::string name_lower_;

  DirectoryStorageType entry_type_;
  uint32_t node_index_;
  // This information is required in nodes when a directory tree is
  // being built.
  uint32_t left_node_sector_index_;
  uint32_t right_node_sector_index_;
  uint32_t child_node_sector_index_;

  // Information that makes it possible to access data held by a node.
  uint32_t stream_sector_index_;
  uint32_t stream_size_;

  OleGuid clsid_;
  uint32_t user_flags_;
  uint64_t creation_timestamp_;
  uint64_t modification_timestamp_;

  // Keeping the tree structure.
  OLEDirectoryEntry *parent_;
  std::vector<std::unique_ptr<OLEDirectoryEntry>> children_;
};

const std::string &EntryTypeToString(DirectoryStorageType entry_type);

}  // namespace maldoca

#endif  // MALDOCA_OLE_DIR_H_
