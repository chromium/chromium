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

#include "snapshot/minidump/module_snapshot_minidump.h"

#include <stddef.h>
#include <string.h>

#include "base/logging.h"
#include "base/notreached.h"
#include "minidump/minidump_extensions.h"
#include "snapshot/minidump/minidump_annotation_reader.h"
#include "snapshot/minidump/minidump_simple_string_dictionary_reader.h"
#include "snapshot/minidump/minidump_string_list_reader.h"
#include "snapshot/minidump/minidump_string_reader.h"
#include "util/misc/pdb_structures.h"

namespace crashpad {
namespace internal {

ModuleSnapshotMinidump::ModuleSnapshotMinidump()
    : ModuleSnapshot(),
      minidump_module_(),
      annotations_vector_(),
      annotations_simple_map_(),
      annotation_objects_(),
      uuid_(),
      build_id_(),
      name_(),
      debug_file_name_(),
      age_(0),
      initialized_() {}

ModuleSnapshotMinidump::~ModuleSnapshotMinidump() {}

bool ModuleSnapshotMinidump::Initialize(
    FileReaderInterface* file_reader,
    RVA minidump_module_rva,
    const MINIDUMP_LOCATION_DESCRIPTOR*
        minidump_module_crashpad_info_location) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  if (!file_reader->SeekSet(minidump_module_rva)) {
    return false;
  }

  if (!file_reader->ReadExactly(&minidump_module_, sizeof(minidump_module_))) {
    return false;
  }

  if (!InitializeModuleCrashpadInfo(file_reader,
                                    minidump_module_crashpad_info_location)) {
    return false;
  }

  ReadMinidumpUTF16String(file_reader, minidump_module_.ModuleNameRva, &name_);

  if (minidump_module_.CvRecord.Rva != 0 &&
      !InitializeModuleCodeView(file_reader)) {
    return false;
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

bool ModuleSnapshotMinidump::InitializeModuleCodeView(
    FileReaderInterface* file_reader) {
  uint32_t signature;

  DCHECK_NE(minidump_module_.CvRecord.Rva, 0u);

  if (minidump_module_.CvRecord.DataSize < sizeof(signature)) {
    LOG(ERROR) << "CodeView record in module too small to contain signature";
    return false;
  }

  if (!file_reader->SeekSet(minidump_module_.CvRecord.Rva)) {
    return false;
  }

  std::vector<uint8_t> cv_record;
  cv_record.resize(minidump_module_.CvRecord.DataSize);

  if (!file_reader->ReadExactly(cv_record.data(), cv_record.size())) {
    return false;
  }

  signature = *reinterpret_cast<uint32_t*>(cv_record.data());

  if (signature == CodeViewRecordPDB70::kSignature) {
    if (cv_record.size() < offsetof(CodeViewRecordPDB70, pdb_name)) {
      LOG(ERROR) << "CodeView record in module marked as PDB70 but too small";
      return false;
    }

    auto cv_record_pdb70 =
        reinterpret_cast<CodeViewRecordPDB70*>(cv_record.data());

    age_ = cv_record_pdb70->age;
    uuid_ = cv_record_pdb70->uuid;

    std::copy(cv_record.begin() + offsetof(CodeViewRecordPDB70, pdb_name),
              cv_record.end(),
              std::back_inserter(debug_file_name_));
    return true;
  }

  if (signature == CodeViewRecordBuildID::kSignature) {
    std::copy(cv_record.begin() + offsetof(CodeViewRecordBuildID, build_id),
              cv_record.end(),
              std::back_inserter(build_id_));
    return true;
  }

  LOG(ERROR) << "Bad CodeView signature in module";
  return false;
}

std::string ModuleSnapshotMinidump::Name() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return name_;
}

uint64_t ModuleSnapshotMinidump::Address() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return minidump_module_.BaseOfImage;
}

uint64_t ModuleSnapshotMinidump::Size() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return minidump_module_.SizeOfImage;
}

time_t ModuleSnapshotMinidump::Timestamp() const {
  return minidump_module_.TimeDateStamp;
}

void ModuleSnapshotMinidump::FileVersion(uint16_t* version_0,
                                         uint16_t* version_1,
                                         uint16_t* version_2,
                                         uint16_t* version_3) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  uint32_t version_01 = minidump_module_.VersionInfo.dwFileVersionMS;
  uint32_t version_23 = minidump_module_.VersionInfo.dwFileVersionLS;
  *version_0 = static_cast<uint16_t>(version_01 >> 16);
  *version_1 = static_cast<uint16_t>(version_01 & 0xFFFF);
  *version_2 = static_cast<uint16_t>(version_23 >> 16);
  *version_3 = static_cast<uint16_t>(version_23 & 0xFFFF);
}

void ModuleSnapshotMinidump::SourceVersion(uint16_t* version_0,
                                           uint16_t* version_1,
                                           uint16_t* version_2,
                                           uint16_t* version_3) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  uint32_t version_01 = minidump_module_.VersionInfo.dwProductVersionMS;
  uint32_t version_23 = minidump_module_.VersionInfo.dwProductVersionLS;
  *version_0 = static_cast<uint16_t>(version_01 >> 16);
  *version_1 = static_cast<uint16_t>(version_01 & 0xFFFF);
  *version_2 = static_cast<uint16_t>(version_23 >> 16);
  *version_3 = static_cast<uint16_t>(version_23 & 0xFFFF);
}

ModuleSnapshot::ModuleType ModuleSnapshotMinidump::GetModuleType() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  switch (minidump_module_.VersionInfo.dwFileType) {
    case VFT_APP:
      return kModuleTypeExecutable;
    case VFT_DLL:
      return kModuleTypeSharedLibrary;
  }
  return kModuleTypeUnknown;
}

void ModuleSnapshotMinidump::UUIDAndAge(crashpad::UUID* uuid,
                                        uint32_t* age) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *uuid = uuid_;
  *age = age_;
}

std::string ModuleSnapshotMinidump::DebugFileName() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return debug_file_name_;
}

std::vector<uint8_t> ModuleSnapshotMinidump::BuildID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return build_id_;
}

std::vector<std::string> ModuleSnapshotMinidump::AnnotationsVector() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return annotations_vector_;
}

std::map<std::string, std::string>
ModuleSnapshotMinidump::AnnotationsSimpleMap() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return annotations_simple_map_;
}

std::vector<AnnotationSnapshot> ModuleSnapshotMinidump::AnnotationObjects()
    const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return annotation_objects_;
}

std::set<CheckedRange<uint64_t>> ModuleSnapshotMinidump::ExtraMemoryRanges()
    const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  NOTREACHED();  // https://crashpad.chromium.org/bug/10
  return std::set<CheckedRange<uint64_t>>();
}

std::vector<const UserMinidumpStream*>
ModuleSnapshotMinidump::CustomMinidumpStreams() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  NOTREACHED();  // https://crashpad.chromium.org/bug/10
  return std::vector<const UserMinidumpStream*>();
}

bool ModuleSnapshotMinidump::InitializeModuleCrashpadInfo(
    FileReaderInterface* file_reader,
    const MINIDUMP_LOCATION_DESCRIPTOR*
        minidump_module_crashpad_info_location) {
  if (!minidump_module_crashpad_info_location ||
      minidump_module_crashpad_info_location->Rva == 0) {
    return true;
  }

  MinidumpModuleCrashpadInfo minidump_module_crashpad_info;
  if (minidump_module_crashpad_info_location->DataSize <
      sizeof(minidump_module_crashpad_info)) {
    LOG(ERROR) << "minidump_module_crashpad_info size mismatch";
    return false;
  }

  if (!file_reader->SeekSet(minidump_module_crashpad_info_location->Rva)) {
    return false;
  }

  if (!file_reader->ReadExactly(&minidump_module_crashpad_info,
                                sizeof(minidump_module_crashpad_info))) {
    return false;
  }

  if (minidump_module_crashpad_info.version !=
      MinidumpModuleCrashpadInfo::kVersion) {
    LOG(ERROR) << "minidump_module_crashpad_info version mismatch";
    return false;
  }

  if (!ReadMinidumpStringList(file_reader,
                              minidump_module_crashpad_info.list_annotations,
                              &annotations_vector_)) {
    return false;
  }

  if (!ReadMinidumpSimpleStringDictionary(
          file_reader,
          minidump_module_crashpad_info.simple_annotations,
          &annotations_simple_map_)) {
    return false;
  }

  if (!ReadMinidumpAnnotationList(
          file_reader,
          minidump_module_crashpad_info.annotation_objects,
          &annotation_objects_)) {
    return false;
  }

  return true;
}

}  // namespace internal
}  // namespace crashpad
