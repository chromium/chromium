// Copyright 2015 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "snapshot/win/module_snapshot_win.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "client/crashpad_info.h"
#include "client/simple_address_range_bag.h"
#include "snapshot/memory_snapshot_generic.h"
#include "snapshot/win/pe_image_annotations_reader.h"
#include "snapshot/win/pe_image_reader.h"
#include "util/misc/tri_state.h"
#include "util/misc/uuid.h"

namespace crashpad {
namespace internal {

ModuleSnapshotWin::ModuleSnapshotWin()
    : ModuleSnapshot(),
      name_(),
      pdb_name_(),
      uuid_(),
      memory_range_(),
      streams_(),
      vs_fixed_file_info_(),
      initialized_vs_fixed_file_info_(),
      process_reader_(nullptr),
      pe_image_reader_(),
      crashpad_info_(),
      timestamp_(0),
      age_(0),
      initialized_() {}

ModuleSnapshotWin::~ModuleSnapshotWin() {}

bool ModuleSnapshotWin::Initialize(
    ProcessReaderWin* process_reader,
    const ProcessInfo::Module& process_reader_module) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  process_reader_ = process_reader;
  name_ = process_reader_module.name;
  timestamp_ = process_reader_module.timestamp;
  pe_image_reader_.reset(new PEImageReader());
  if (!pe_image_reader_->Initialize(process_reader_,
                                    process_reader_module.dll_base,
                                    process_reader_module.size,
                                    base::UTF16ToUTF8(name_))) {
    return false;
  }

  DWORD age_dword;
  if (pe_image_reader_->DebugDirectoryInformation(
          &uuid_, &age_dword, &pdb_name_)) {
    static_assert(sizeof(DWORD) == sizeof(uint32_t), "unexpected age size");
    age_ = age_dword;
  } else {
    // If we fully supported all old debugging formats, we would want to extract
    // and emit a different type of CodeView record here (as old Microsoft tools
    // would do). As we don't expect to ever encounter a module that wouldn't be
    // using .PDB that we actually have symbols for, we simply set a plausible
    // name here, but this will never correspond to symbols that we have.
    pdb_name_ = base::UTF16ToUTF8(name_);
  }

  if (!memory_range_.Initialize(process_reader_->Memory(),
                                process_reader_->Is64Bit())) {
    return false;
  }

  WinVMAddress crashpad_info_address;
  WinVMSize crashpad_info_size;
  if (pe_image_reader_->GetCrashpadInfoSection(&crashpad_info_address,
                                               &crashpad_info_size)) {
    ProcessMemoryRange info_range;
    info_range.Initialize(memory_range_);
    info_range.RestrictRange(crashpad_info_address,
                             crashpad_info_address + crashpad_info_size);

    auto info = std::make_unique<CrashpadInfoReader>();
    if (info->Initialize(&info_range, crashpad_info_address)) {
      crashpad_info_ = std::move(info);
    }
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

void ModuleSnapshotWin::GetCrashpadOptions(CrashpadInfoClientOptions* options) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  if (process_reader_->Is64Bit())
    GetCrashpadOptionsInternal<process_types::internal::Traits64>(options);
  else
    GetCrashpadOptionsInternal<process_types::internal::Traits32>(options);
}

std::string ModuleSnapshotWin::Name() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return base::UTF16ToUTF8(name_);
}

uint64_t ModuleSnapshotWin::Address() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return pe_image_reader_->Address();
}

uint64_t ModuleSnapshotWin::Size() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return pe_image_reader_->Size();
}

time_t ModuleSnapshotWin::Timestamp() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return timestamp_;
}

void ModuleSnapshotWin::FileVersion(uint16_t* version_0,
                                    uint16_t* version_1,
                                    uint16_t* version_2,
                                    uint16_t* version_3) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  const VS_FIXEDFILEINFO* ffi = VSFixedFileInfo();
  if (ffi) {
    *version_0 = ffi->dwFileVersionMS >> 16;
    *version_1 = ffi->dwFileVersionMS & 0xffff;
    *version_2 = ffi->dwFileVersionLS >> 16;
    *version_3 = ffi->dwFileVersionLS & 0xffff;
  } else {
    *version_0 = 0;
    *version_1 = 0;
    *version_2 = 0;
    *version_3 = 0;
  }
}

void ModuleSnapshotWin::SourceVersion(uint16_t* version_0,
                                      uint16_t* version_1,
                                      uint16_t* version_2,
                                      uint16_t* version_3) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  const VS_FIXEDFILEINFO* ffi = VSFixedFileInfo();
  if (ffi) {
    *version_0 = ffi->dwProductVersionMS >> 16;
    *version_1 = ffi->dwProductVersionMS & 0xffff;
    *version_2 = ffi->dwProductVersionLS >> 16;
    *version_3 = ffi->dwProductVersionLS & 0xffff;
  } else {
    *version_0 = 0;
    *version_1 = 0;
    *version_2 = 0;
    *version_3 = 0;
  }
}

ModuleSnapshot::ModuleType ModuleSnapshotWin::GetModuleType() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  const VS_FIXEDFILEINFO* ffi = VSFixedFileInfo();
  if (ffi) {
    switch (ffi->dwFileType) {
      case VFT_APP:
        return ModuleSnapshot::kModuleTypeExecutable;
      case VFT_DLL:
        return ModuleSnapshot::kModuleTypeSharedLibrary;
      case VFT_DRV:
      case VFT_VXD:
        return ModuleSnapshot::kModuleTypeLoadableModule;
    }
  }
  return ModuleSnapshot::kModuleTypeUnknown;
}

void ModuleSnapshotWin::UUIDAndAge(crashpad::UUID* uuid, uint32_t* age) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *uuid = uuid_;
  *age = age_;
}

std::string ModuleSnapshotWin::DebugFileName() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return pdb_name_;
}

std::vector<uint8_t> ModuleSnapshotWin::BuildID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<uint8_t>();
}

std::vector<std::string> ModuleSnapshotWin::AnnotationsVector() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // These correspond to system-logged things on Mac. We don't currently track
  // any of these on Windows, but could in the future. See
  // https://crashpad.chromium.org/bug/38.
  return std::vector<std::string>();
}

std::map<std::string, std::string> ModuleSnapshotWin::AnnotationsSimpleMap()
    const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  PEImageAnnotationsReader annotations_reader(
      process_reader_, pe_image_reader_.get(), name_);
  return annotations_reader.SimpleMap();
}

std::vector<AnnotationSnapshot> ModuleSnapshotWin::AnnotationObjects() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  PEImageAnnotationsReader annotations_reader(
      process_reader_, pe_image_reader_.get(), name_);
  return annotations_reader.AnnotationsList();
}

std::set<CheckedRange<uint64_t>> ModuleSnapshotWin::ExtraMemoryRanges() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  std::set<CheckedRange<uint64_t>> ranges;
  if (process_reader_->Is64Bit())
    GetCrashpadExtraMemoryRanges<process_types::internal::Traits64>(&ranges);
  else
    GetCrashpadExtraMemoryRanges<process_types::internal::Traits32>(&ranges);
  return ranges;
}

std::vector<const UserMinidumpStream*>
ModuleSnapshotWin::CustomMinidumpStreams() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  streams_.clear();
  if (process_reader_->Is64Bit()) {
    GetCrashpadUserMinidumpStreams<process_types::internal::Traits64>(
        &streams_);
  } else {
    GetCrashpadUserMinidumpStreams<process_types::internal::Traits32>(
        &streams_);
  }

  std::vector<const UserMinidumpStream*> result;
  for (const auto& stream : streams_) {
    result.push_back(stream.get());
  }
  return result;
}

template <class Traits>
void ModuleSnapshotWin::GetCrashpadOptionsInternal(
    CrashpadInfoClientOptions* options) {
  if (!crashpad_info_) {
    options->crashpad_handler_behavior = TriState::kUnset;
    options->system_crash_reporter_forwarding = TriState::kUnset;
    options->gather_indirectly_referenced_memory = TriState::kUnset;
    options->indirectly_referenced_memory_cap = 0;
    return;
  }

  options->crashpad_handler_behavior =
      crashpad_info_->CrashpadHandlerBehavior();
  options->system_crash_reporter_forwarding =
      crashpad_info_->SystemCrashReporterForwarding();
  options->gather_indirectly_referenced_memory =
      crashpad_info_->GatherIndirectlyReferencedMemory();
  options->indirectly_referenced_memory_cap =
      crashpad_info_->IndirectlyReferencedMemoryCap();
}

const VS_FIXEDFILEINFO* ModuleSnapshotWin::VSFixedFileInfo() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  if (initialized_vs_fixed_file_info_.is_uninitialized()) {
    initialized_vs_fixed_file_info_.set_invalid();
    if (pe_image_reader_->VSFixedFileInfo(&vs_fixed_file_info_)) {
      initialized_vs_fixed_file_info_.set_valid();
    }
  }

  return initialized_vs_fixed_file_info_.is_valid() ? &vs_fixed_file_info_
                                                    : nullptr;
}

template <class Traits>
void ModuleSnapshotWin::GetCrashpadExtraMemoryRanges(
    std::set<CheckedRange<uint64_t>>* ranges) const {
  if (!crashpad_info_ || !crashpad_info_->ExtraMemoryRanges())
    return;

  std::vector<SimpleAddressRangeBag::Entry> simple_ranges(
      SimpleAddressRangeBag::num_entries);
  if (!process_reader_->Memory()->Read(
          crashpad_info_->ExtraMemoryRanges(),
          simple_ranges.size() * sizeof(simple_ranges[0]),
          &simple_ranges[0])) {
    LOG(WARNING) << "could not read simple address_ranges from "
                 << base::UTF16ToUTF8(name_);
    return;
  }

  for (const auto& entry : simple_ranges) {
    if (entry.base != 0 || entry.size != 0) {
      // Deduplication here is fine.
      ranges->insert(CheckedRange<uint64_t>(entry.base, entry.size));
    }
  }
}

template <class Traits>
void ModuleSnapshotWin::GetCrashpadUserMinidumpStreams(
    std::vector<std::unique_ptr<const UserMinidumpStream>>* streams) const {
  if (!crashpad_info_)
    return;

  for (uint64_t cur = crashpad_info_->UserDataMinidumpStreamHead(); cur;) {
    internal::UserDataMinidumpStreamListEntry list_entry;
    if (!process_reader_->Memory()->Read(
            cur, sizeof(list_entry), &list_entry)) {
      LOG(WARNING) << "could not read user data stream entry from "
                   << base::UTF16ToUTF8(name_);
      return;
    }

    if (list_entry.size != 0) {
      std::unique_ptr<internal::MemorySnapshotGeneric> memory(
          new internal::MemorySnapshotGeneric());
      memory->Initialize(
          process_reader_->Memory(), list_entry.base_address, list_entry.size);
      streams->push_back(std::make_unique<UserMinidumpStream>(
          list_entry.stream_type, memory.release()));
    }

    cur = list_entry.next;
  }
}

}  // namespace internal
}  // namespace crashpad
