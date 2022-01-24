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

// Implementation of the OLEDirectoryEntry class and methods to read
// from directory structures from input.

#include "maldoca/ole/dir.h"

#include <iostream>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#ifndef MALDOCA_IN_CHROMIUM
#include "maldoca/base/cleanup.h"
#endif
#include "maldoca/base/utf8/unicodetext.h"
#include "maldoca/ole/endian_reader.h"
#include "maldoca/ole/fat.h"
#include "maldoca/ole/stream.h"

#ifdef MALDOCA_IN_CHROMIUM
#include "base/callback_helpers.h"
#endif

// Directory entry ID constants (from AAF specifications). These are
// constants that we don't need to share so they are defined here.
static const uint32_t kDirectoryMaximumEntry = 0xFFFFFFFA;  // Maximum entry ID
static const uint32_t kDirectoryNoStream = 0xFFFFFFFF;      // Unallocated entry
static const uint32_t kDirectoryEntrySize = 128;

namespace maldoca {

// Read an OLEDirectoryEntry from a stream at a given index,
bool OLEDirectoryEntry::ReadDirectoryEntryFromStream(
    absl::string_view input, uint32_t index, uint32_t sector_size,
    OLEDirectoryEntry *directory_entry) {
  CHECK(!directory_entry->IsInitialized());
  if (index > input.length() / kDirectoryEntrySize) {
    LOG(ERROR) << "Can not read directory past input capacity: "
               << "requested_index=" << index
               << " capacity=" << input.length() / kDirectoryEntrySize;
  }

  // Read the directory entry as a sector
  absl::string_view entry;
  if (!FAT::ReadSectorAt(input, index * kDirectoryEntrySize,
                         kDirectoryEntrySize,
                         /* allow_short_sector_read */ false, &entry)) {
    VLOG(2) << "Failed reading " << kDirectoryEntrySize << " bytes at "
            << index * kDirectoryEntrySize;
    return false;
  }

  // Extract the entry name: at most 31 chars (UTF-16) + null or 64
  // bytes. Consume that space afterward so that we can continue
  // parsing past the first 64 bytes.
  absl::string_view entry_name_piece = entry.substr(0, 64);
  if (entry_name_piece.size() != 64) {
    VLOG(2) << "Byte [0:63]: Can not read entry name";
    return false;
  }
  entry.remove_prefix(64);

  // The length of the buffer above
  uint16_t entry_name_length;
  if (!LittleEndianReader::ConsumeUInt16(&entry, &entry_name_length)) {
    VLOG(2) << "Bytes [64:65] Can not read entry name length";
    return false;
  }
  // We're expecting at least two characters and always a multiple of
  // two characters.
  if (entry_name_length < 2 || entry_name_length % 2) {
    VLOG(2) << "Bytes [64:65] Invalid entry name length: " << entry_name_length;
  }
  // The length is truncated to 64
  if (entry_name_length > 64) {
    LOG(WARNING) << "Bytes [64:65] Entry name length is " << entry_name_length;
    entry_name_length = 64;
  }

  // Having both the entry name data and length allow us to create the
  // entry name data that we decode as UTF-16. The UTF-16 input is
  // built not to contain the terminating character. Note that the
  // following characters are illegal: /, \, : and ! but we do not
  // perform that check here. It should be done upstream when we try
  // to detect some abuse.
  std::string entry_name;
  if (entry_name_length > 2) {
    DecodeUTF16(entry_name_piece.substr(0, entry_name_length - 2), &entry_name);
  }

  uint8_t dir_entry_type_raw;
  if (!(LittleEndianReader::ConsumeUInt8(&entry, &dir_entry_type_raw) &&
        // No need to compare >= DirectoryStorageType::Root, it will
        // be always true.
        dir_entry_type_raw <= DirectoryStorageType::Root)) {
    VLOG(2) << "Byte [66] Unexpected directory entry type "
            << dir_entry_type_raw;
    return false;
  }
  DirectoryStorageType dir_entry_type =
      static_cast<DirectoryStorageType>(dir_entry_type_raw);
  if (dir_entry_type == DirectoryStorageType::Root && index != 0) {
    VLOG(2) << "Byte [66] Directory entry type indicates root but index is "
            << index;
    return false;
  }
  if (dir_entry_type != DirectoryStorageType::Root && index == 0) {
    VLOG(2) << "Byte [66] Directory entry type " << dir_entry_type
            << " at index 0";
    return false;
  }

  // An OLE directory structure is organized as a red/black tree.
  uint8_t node_color;
  if (!(LittleEndianReader::ConsumeUInt8(&entry, &node_color))) {
    VLOG(2) << "Byte [67] Can not read color entry";
    return false;
  }

  uint32_t left_node_index, right_node_index, child_node_index;
  if (!(LittleEndianReader::ConsumeUInt32(&entry, &left_node_index))) {
    VLOG(2) << "Byte [68:71] Can not read left node index";
    return false;
  }
  if (!(LittleEndianReader::ConsumeUInt32(&entry, &right_node_index))) {
    VLOG(2) << "Byte [72:75] Can not read right node index";
    return false;
  }
  if (!(LittleEndianReader::ConsumeUInt32(&entry, &child_node_index))) {
    VLOG(2) << "Byte [76:79] Can not read child node index";
    return false;
  }

  uint64_t clsid_1, clsid_2;
  if (!(LittleEndianReader::ConsumeUInt64(&entry, &clsid_1))) {
    VLOG(2) << "Byte [80:87] Can not read the node identifier "
            << "low 64 bits";
    return false;
  }
  if (!(LittleEndianReader::ConsumeUInt64(&entry, &clsid_2))) {
    VLOG(2) << "Byte [88:95] Can not read the node identifier "
            << "high 64 bits";
    return false;
  }

  uint32_t user_flags;
  if (!(LittleEndianReader::ConsumeUInt32(&entry, &user_flags))) {
    VLOG(2) << "Byte [96:99] Can not read the user flags";
    return false;
  }

  uint64_t creation_timestamp, modification_timestamp;
  if (!(LittleEndianReader::ConsumeUInt64(&entry, &creation_timestamp))) {
    VLOG(2) << "Byte [100:107] Can not read the creation timestamp";
    return false;
  }
  if (!(LittleEndianReader::ConsumeUInt64(&entry, &modification_timestamp))) {
    VLOG(2) << "Byte [108:115] Can not read the modification timestamp";
    return false;
  }

  uint32_t first_sector_index = 0;
  if (!(LittleEndianReader::ConsumeUInt32(&entry, &first_sector_index))) {
    VLOG(2) << "Byte [116:119] Can not read the first sector index";
  }

  uint32_t stream_size_low = 0;
  uint32_t stream_size_high = 0;
  if (!(LittleEndianReader::ConsumeUInt32(&entry, &stream_size_low))) {
    VLOG(2) << "Byte [120:123] Can not read the stream size low 32 bits";
  }
  if (!(LittleEndianReader::ConsumeUInt32(&entry, &stream_size_high))) {
    VLOG(2) << "Byte [124:127] Can not read the stream size high 32 bits";
  }

  if (!entry.empty()) {
    LOG(ERROR) << "Not all bytes from the directory entry input have been "
               << " consumed: " << entry.size() << " left";
    return false;
  }

  // stream_size_high is only used for 4K sectors but it can't be assumed to
  // be 0 for 512 sectors.
  uint64_t size;
  if (sector_size == 512) {
    if (stream_size_high != 0 && stream_size_high != SectorConstant::Free) {
      LOG(WARNING) << "Byte [124:127] High 32 bits of the stream size have "
                   << " unexpected value " << stream_size_high
                   << " for sector size " << sector_size;
    }
    size = stream_size_low;
  } else {
    size = stream_size_low + (static_cast<uint64_t>(stream_size_high) << 32);
  }

  // A storage node is expected to have a size of 0 but it's not
  // always the case (Word 8 for Mac for instance.)
  if (dir_entry_type == DirectoryStorageType::Storage && size != 0) {
    LOG(WARNING) << "Node is of type " << dir_entry_type << " but has size "
                 << size;
  }

  // TODO(somebody): Check that the stream is not already referenced
  // somewhere else.

  OleGuid clsid;
  memcpy(clsid, &clsid_1, sizeof(clsid_1));
  memcpy(clsid + sizeof(clsid_1), &clsid_2, sizeof(clsid_2));
  directory_entry->Initialize(
      entry_name, dir_entry_type, index, left_node_index, right_node_index,
      child_node_index, first_sector_index, size, clsid, user_flags,
      creation_timestamp, modification_timestamp);
  return true;
}

// This forward declaration is required.
static bool BuildDirectoryTree(const std::string &input, uint32_t sector_size,
                               OLEDirectoryEntry *parent,
                               OLEDirectoryEntry *node,
                               std::vector<OLEDirectoryEntry *> *dir_entries);

// Returns true if the sector index is less than the maximum expected number of
// directory entries and the sector has not been visited yet.
bool IsValidSectorIndex(uint32_t sector_index,
                        const std::vector<OLEDirectoryEntry *> &dir_entries) {
  if (sector_index < 0 || sector_index >= dir_entries.size()) {
    LOG(WARNING) << "Sector index is out of range. Sector index: "
                 << sector_index
                 << ", maximum directory entries: " << dir_entries.size();
    return false;
  } else if (dir_entries[sector_index] != nullptr) {
    LOG(WARNING)
        << "Cycle in the directory tree? Skipping duplicate sector index: "
        << sector_index;
    return false;
  }
  return true;
}

void CleanUpDirectory(std::vector<OLEDirectoryEntry *> *dir_entries,
                      uint32_t sector_index) {
  (*dir_entries)[sector_index] = nullptr;
}

// Helper function to read a directory node from a stream, recursively
// build a directory tree from the new node and attach the node to its
// parent.
static bool BuildDirectoryTreeElement(
    const std::string &input, uint32_t sector_index, uint32_t sector_size,
    OLEDirectoryEntry *parent, std::vector<OLEDirectoryEntry *> *dir_entries) {
  if (sector_index == kDirectoryNoStream) {
    return true;
  }
  // Cannot be cycles in the tree, and sector_index must be less than the
  // maximum expected number of directory entries.
  if (!IsValidSectorIndex(sector_index, *dir_entries)) {
    return false;
  }

  auto child = absl::make_unique<OLEDirectoryEntry>();
#ifdef MALDOCA_IN_CHROMIUM
  auto cleanup = base::ScopedClosureRunner(
      base::BindOnce(CleanUpDirectory, dir_entries, sector_index));
#else
  auto cleanup = MakeCleanup(
      [dir_entries, sector_index] { (*dir_entries)[sector_index] = nullptr; });
#endif

  (*dir_entries)[sector_index] = child.get();
  if (!OLEDirectoryEntry::ReadDirectoryEntryFromStream(
          input, sector_index, sector_size, child.get())) {
    return false;
  }
  if (!(BuildDirectoryTree(input, sector_size, parent, child.get(),
                           dir_entries))) {
    return false;
  }
  if (!parent->AddChild(child.get())) {
    LOG(ERROR) << "Can not add node " << child->Name() << " to parent "
               << parent->Name();
    return false;
  }
  // Only release the child now - we can return above with an error
  // without triggering a memory leak.
  child.release();

#ifdef MALDOCA_IN_CHROMIUM
  cleanup.Release();
#else
  std::move(cleanup).Cancel();
#endif
  return true;
}

// From the left/right and child node sector index of a given node,
// its parent and some input, build a directory tree by reading the
// appropriate stream and recursively invoking this method.
static bool BuildDirectoryTree(const std::string &input, uint32_t sector_size,
                               OLEDirectoryEntry *parent,
                               OLEDirectoryEntry *node,
                               std::vector<OLEDirectoryEntry *> *dir_entries) {
  // TODO(b/120545604): Possibly continue to build tree even if parts of it are
  // invalid.
  for (auto const &sector_index :
       {node->LeftNodeSectorIndex(), node->RightNodeSectorIndex()}) {
    if (!BuildDirectoryTreeElement(input, sector_index, sector_size, parent,
                                   dir_entries)) {
      return false;
    }
  }
  return BuildDirectoryTreeElement(input, node->ChildNodeSectorIndex(),
                                   sector_size, node, dir_entries);
}

std::string OLEDirectoryEntry::ToString() {
  std::string output;
  LogNodeWithTab(0, &output);
  return output;
}

void OLEDirectoryEntry::Initialize(
    const std::string &name, DirectoryStorageType entry_type,
    uint32_t node_index, uint32_t left_node_sector_index,
    uint32_t right_node_sector_index, uint32_t child_node_sector_index,
    uint32_t stream_sector_index, uint32_t stream_size, const OleGuid &clsid,
    uint32_t user_flags, uint64_t creation_timestamp,
    uint64_t modification_timestamp) {
  // Do not allow double initialization.
  CHECK(!is_initialized_);

  name_ = name;
  name_lower_ = absl::AsciiStrToLower(name);
  entry_type_ = entry_type;
  node_index_ = node_index;
  left_node_sector_index_ = left_node_sector_index;
  right_node_sector_index_ = right_node_sector_index;
  child_node_sector_index_ = child_node_sector_index;
  memcpy(clsid_, clsid, sizeof(OleGuid));
  user_flags_ = user_flags;
  creation_timestamp_ = creation_timestamp;
  modification_timestamp_ = modification_timestamp;

  stream_sector_index_ = stream_sector_index;
  stream_size_ = stream_size;

  // Note here that an initialized child doesn't yet have a parent.
  is_initialized_ = true;
}

// Helper for LogNodeWithTab: produce a string name for
// DirectoryStorageType entries.
const std::string &EntryTypeToString(DirectoryStorageType entry_type) {
  static const std::map<DirectoryStorageType, std::string> *lookup =
      new std::map<DirectoryStorageType, std::string>(
          {{DirectoryStorageType::Empty, "Empty"},
           {DirectoryStorageType::Storage, "Storage"},
           {DirectoryStorageType::Stream, "Stream"},
           {DirectoryStorageType::LockBytes, "LockBytes"},
           {DirectoryStorageType::Property, "Property"},
           {DirectoryStorageType::Root, "Root"}});
  return lookup->at(entry_type);
}

// Helper for LogNodeWithTab: produce a string name for a sector index.
static std::string SectorIndexToString(uint32_t sector) {
  switch (sector) {
    case kDirectoryMaximumEntry:
      return "N/A";
    case kDirectoryNoStream:
      return "N/A";
    default:
      return absl::StrCat(sector);
  }
}

void OLEDirectoryEntry::LogNodeWithTab(int num_spaces, std::string *output) {
  absl::StrAppend(
      output, std::string(num_spaces, ' '), "name='", name_,
      "' type=", EntryTypeToString(entry_type_), " index=", node_index_,
      " left=", SectorIndexToString(left_node_sector_index_),
      " right=", SectorIndexToString(right_node_sector_index_),
      " child=", SectorIndexToString(child_node_sector_index_),
      " data=", stream_size_, "@", SectorIndexToString(stream_sector_index_),
      " clsid=",
      absl::BytesToHexString({reinterpret_cast<const char *>(clsid_), 16}),
      " flags=", user_flags_, " creation_ts=", creation_timestamp_,
      " modification_ts=", modification_timestamp_, "\n");
  for (const auto &node : children_) {
    node->LogNodeWithTab(num_spaces + 2, output);
  }
}

bool OLEDirectoryEntry::AddChild(OLEDirectoryEntry *child) {
  // These checks aren't here to validate the input, but to make sure
  // that we are using our API correctly.
  CHECK(child != nullptr);
  CHECK(child->IsInitialized());
  CHECK_EQ(child->parent_, static_cast<OLEDirectoryEntry *>(nullptr));

  // This check is validating the input
  if (this->entry_type_ != DirectoryStorageType::Storage &&
      this->entry_type_ != DirectoryStorageType::Root) {
    LOG(ERROR) << "Node is of type " << this->entry_type_
               << " and can not be added a child.";
    return false;
  }
  children_.emplace_back(child);
  child->parent_ = this;
  return true;
}

OLEDirectoryEntry *OLEDirectoryEntry::FindChildByName(
    const std::string &name, DirectoryStorageType type) const {
  for (const auto &node : children_) {
    if (node->name_lower_ == name && node->entry_type_ == type) {
      return node.get();
    }
  }
  return nullptr;
}

OLEDirectoryEntry *OLEDirectoryEntry::FindRoot() const {
  OLEDirectoryEntry *root = const_cast<OLEDirectoryEntry *>(this);
  while (root->parent_) {
    root = root->parent_;
  }
  return root;
}

void OLEDirectoryEntry::FindAllDescendants(
    const std::string &name, DirectoryStorageType type,
    std::vector<OLEDirectoryEntry *> *results) const {
  for (const auto &node : children_) {
    if (node->name_lower_ == name && node->entry_type_ == type) {
      results->push_back(node.get());
    }
    if (node->entry_type_ == DirectoryStorageType::Storage) {
      node->FindAllDescendants(name, type, results);
    }
  }
}

std::string OLEDirectoryEntry::Path() const {
  std::vector<std::string> components;
  // strings::Join is empty if start == end so we generate the result
  // manually for that special case.
  if (!parent_) {
    return "/";
  }
  for (auto current = this; current->parent_; current = current->parent_) {
    components.insert(components.begin(), current->name_);
  }
  components.insert(components.begin(), "");
  return absl::StrJoin(components, "/");
}

// static
bool OLEDirectoryEntry::ReadDirectory(
    absl::string_view input, const OLEHeader &header,
    const std::vector<uint32_t> &fat, OLEDirectoryEntry *directory,
    std::vector<OLEDirectoryEntry *> *dir_entries,
    std::string *directory_stream) {
  CHECK(header.IsInitialized());
  CHECK(!directory->IsInitialized());
  CHECK(!fat.empty());

  // For a directory, the size is unknown.
  if (!(OLEStream::Read(input, header, header.DirectoryFirstSector(),
                        header.SectorSize(), header.SectorSize(), 0,
                        /* expected_stream_size_is_unknown_p */ true,
                        /* root_first_sector */ -1,
                        /* root_stream_size */ -1, fat, directory_stream))) {
    LOG(ERROR) << "Failed to read input as stream:"
               << " first_sector=" << header.DirectoryFirstSector()
               << " first_sector_offset=" << header.SectorSize()
               << " sector_size=" << header.SectorSize();
    return false;
  }
  *dir_entries = std::vector<OLEDirectoryEntry *>(directory_stream->size() /
                                                  kDirectoryEntrySize);

  if (!ReadDirectoryEntryFromStream(*directory_stream, 0, header.SectorSize(),
                                    directory)) {
    LOG(ERROR) << "Can not read directory entry from input";
    return false;
  }
  if (!BuildDirectoryTree(*directory_stream, header.SectorSize(), directory,
                          directory, dir_entries)) {
    LOG(ERROR) << "Can not build directory tree from input";
    return false;
  }
  return true;
}

OLEDirectoryEntry *OLEDirectoryEntry::FindVBAContentRoot() const {
  std::vector<OLEDirectoryEntry *> found;
  this->FindAllDescendants("vba", DirectoryStorageType::Storage, &found);
  // Go over all results, keep only the ones that are showing the node
  // is a storage node with _VBA_PROJECT and dir as direct descendant.
  for (const auto &entry : found) {
    if (entry->Parent() &&
        entry->FindChildByName("_vba_project", DirectoryStorageType::Stream) &&
        entry->FindChildByName("dir", DirectoryStorageType::Stream)) {
      return entry->Parent();
    }
  }
  return nullptr;
}

OLEDirectoryEntry *OLEDirectoryEntry::FindPowerPointDocumentRoot() const {
  return this->FindChildByName("powerpoint document",
                               DirectoryStorageType::Stream);
}

}  // namespace maldoca
