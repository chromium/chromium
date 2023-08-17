// Copyright 2020 The Crashpad Authors
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

#include "snapshot/ios/module_snapshot_ios_intermediate_dump.h"

#include <mach-o/loader.h>
#include <mach/mach.h>

#include "base/apple/mach_logging.h"
#include "base/files/file_path.h"
#include "client/annotation.h"
#include "snapshot/ios/intermediate_dump_reader_util.h"
#include "util/ios/ios_intermediate_dump_data.h"
#include "util/ios/ios_intermediate_dump_list.h"
#include "util/misc/from_pointer_cast.h"
#include "util/misc/uuid.h"

namespace crashpad {
namespace internal {

using Key = IntermediateDumpKey;

ModuleSnapshotIOSIntermediateDump::ModuleSnapshotIOSIntermediateDump()
    : ModuleSnapshot(),
      name_(),
      address_(0),
      size_(0),
      timestamp_(0),
      dylib_version_(0),
      source_version_(0),
      filetype_(0),
      initialized_() {}

ModuleSnapshotIOSIntermediateDump::~ModuleSnapshotIOSIntermediateDump() {}

bool ModuleSnapshotIOSIntermediateDump::Initialize(
    const IOSIntermediateDumpMap* image_data) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  GetDataStringFromMap(image_data, Key::kName, &name_);
  GetDataValueFromMap(image_data, Key::kAddress, &address_);
  GetDataValueFromMap(image_data, Key::kSize, &size_);
  GetDataValueFromMap(image_data, Key::kFileType, &filetype_);

  // These keys are often missing.
  GetDataValueFromMap(image_data,
                      Key::kSourceVersion,
                      &source_version_,
                      LogMissingDataValueFromMap::kDontLogIfMissing);
  GetDataValueFromMap(image_data,
                      Key::kTimestamp,
                      &timestamp_,
                      LogMissingDataValueFromMap::kDontLogIfMissing);
  GetDataValueFromMap(image_data,
                      Key::kDylibCurrentVersion,
                      &dylib_version_,
                      LogMissingDataValueFromMap::kDontLogIfMissing);

  const IOSIntermediateDumpData* uuid_dump =
      GetDataFromMap(image_data, IntermediateDumpKey::kUUID);
  if (uuid_dump) {
    const std::vector<uint8_t>& bytes = uuid_dump->bytes();
    if (!bytes.data() || bytes.size() != 16) {
      LOG(ERROR) << "Invalid module uuid.";
    } else {
      uuid_.InitializeFromBytes(bytes.data());
    }
  }

  const IOSIntermediateDumpList* annotation_list =
      image_data->GetAsList(IntermediateDumpKey::kAnnotationObjects);
  if (annotation_list) {
    for (auto& annotation : *annotation_list) {
      std::string name;
      if (!GetDataStringFromMap(
              annotation.get(), Key::kAnnotationName, &name) ||
          name.empty() || name.length() > Annotation::kNameMaxLength) {
        LOG(ERROR) << "Invalid annotation name (" << name
                   << "), size=" << name.size()
                   << ", max size=" << Annotation::kNameMaxLength
                   << ", discarding annotation.";
        continue;
      }

      uint16_t type;
      const IOSIntermediateDumpData* type_dump =
          annotation->GetAsData(IntermediateDumpKey::kAnnotationType);
      const IOSIntermediateDumpData* value_dump =
          annotation->GetAsData(IntermediateDumpKey::kAnnotationValue);
      if (type_dump && value_dump && type_dump->GetValue<uint16_t>(&type)) {
        const std::vector<uint8_t>& bytes = value_dump->bytes();
        uint64_t length = bytes.size();
        if (!bytes.data() || length > Annotation::kValueMaxSize) {
          LOG(ERROR) << "Invalid annotation value, size=" << length
                     << ", max size=" << Annotation::kValueMaxSize
                     << ", discarding annotation.";
          continue;
        }
        annotation_objects_.push_back(AnnotationSnapshot(name, type, bytes));
      }
    }
  }

  const IOSIntermediateDumpList* simple_map_dump =
      image_data->GetAsList(IntermediateDumpKey::kAnnotationsSimpleMap);
  if (simple_map_dump) {
    for (auto& annotation : *simple_map_dump) {
      const IOSIntermediateDumpData* name_dump =
          annotation->GetAsData(IntermediateDumpKey::kAnnotationName);
      const IOSIntermediateDumpData* value_dump =
          annotation->GetAsData(IntermediateDumpKey::kAnnotationValue);
      if (name_dump && value_dump) {
        annotations_simple_map_.insert(
            make_pair(name_dump->GetString(), value_dump->GetString()));
      }
    }
  }

  const IOSIntermediateDumpMap* crash_info_dump =
      image_data->GetAsMap(IntermediateDumpKey::kAnnotationsCrashInfo);
  if (crash_info_dump) {
    const IOSIntermediateDumpData* message1_dump = crash_info_dump->GetAsData(
        IntermediateDumpKey::kAnnotationsCrashInfoMessage1);
    if (message1_dump) {
      std::string message1 = message1_dump->GetString();
      if (!message1.empty())
        annotations_vector_.push_back(message1);
    }
    const IOSIntermediateDumpData* message2_dump = crash_info_dump->GetAsData(
        IntermediateDumpKey::kAnnotationsCrashInfoMessage2);
    if (message2_dump) {
      std::string message2 = message2_dump->GetString();
      if (!message2.empty())
        annotations_vector_.push_back(message2);
    }
  }

  const IOSIntermediateDumpData* dyld_error_dump =
      image_data->GetAsData(IntermediateDumpKey::kAnnotationsDyldErrorString);
  if (dyld_error_dump) {
    std::string dyld_error_string = dyld_error_dump->GetString();
    if (!dyld_error_string.empty())
      annotations_vector_.push_back(dyld_error_string);
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

std::string ModuleSnapshotIOSIntermediateDump::Name() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return name_;
}

uint64_t ModuleSnapshotIOSIntermediateDump::Address() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return address_;
}

uint64_t ModuleSnapshotIOSIntermediateDump::Size() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return size_;
}

time_t ModuleSnapshotIOSIntermediateDump::Timestamp() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return timestamp_;
}

void ModuleSnapshotIOSIntermediateDump::FileVersion(uint16_t* version_0,
                                                    uint16_t* version_1,
                                                    uint16_t* version_2,
                                                    uint16_t* version_3) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  if (filetype_ == MH_DYLIB) {
    *version_0 = (dylib_version_ & 0xffff0000) >> 16;
    *version_1 = (dylib_version_ & 0x0000ff00) >> 8;
    *version_2 = (dylib_version_ & 0x000000ff);
    *version_3 = 0;
  } else {
    *version_0 = 0;
    *version_1 = 0;
    *version_2 = 0;
    *version_3 = 0;
  }
}

void ModuleSnapshotIOSIntermediateDump::SourceVersion(
    uint16_t* version_0,
    uint16_t* version_1,
    uint16_t* version_2,
    uint16_t* version_3) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *version_0 = (source_version_ & 0xffff000000000000u) >> 48;
  *version_1 = (source_version_ & 0x0000ffff00000000u) >> 32;
  *version_2 = (source_version_ & 0x00000000ffff0000u) >> 16;
  *version_3 = source_version_ & 0x000000000000ffffu;
}

ModuleSnapshot::ModuleType ModuleSnapshotIOSIntermediateDump::GetModuleType()
    const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  switch (filetype_) {
    case MH_EXECUTE:
      return kModuleTypeExecutable;
    case MH_DYLIB:
      return kModuleTypeSharedLibrary;
    case MH_DYLINKER:
      return kModuleTypeDynamicLoader;
    case MH_BUNDLE:
      return kModuleTypeLoadableModule;
    default:
      return kModuleTypeUnknown;
  }
}

void ModuleSnapshotIOSIntermediateDump::UUIDAndAge(crashpad::UUID* uuid,
                                                   uint32_t* age) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *uuid = uuid_;
  *age = 0;
}

std::string ModuleSnapshotIOSIntermediateDump::DebugFileName() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return base::FilePath(Name()).BaseName().value();
}

std::vector<uint8_t> ModuleSnapshotIOSIntermediateDump::BuildID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<uint8_t>();
}

std::vector<std::string> ModuleSnapshotIOSIntermediateDump::AnnotationsVector()
    const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return annotations_vector_;
}

std::map<std::string, std::string>
ModuleSnapshotIOSIntermediateDump::AnnotationsSimpleMap() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return annotations_simple_map_;
}

std::vector<AnnotationSnapshot>
ModuleSnapshotIOSIntermediateDump::AnnotationObjects() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return annotation_objects_;
}

std::set<CheckedRange<uint64_t>>
ModuleSnapshotIOSIntermediateDump::ExtraMemoryRanges() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::set<CheckedRange<uint64_t>>();
}

std::vector<const UserMinidumpStream*>
ModuleSnapshotIOSIntermediateDump::CustomMinidumpStreams() const {
  return std::vector<const UserMinidumpStream*>();
}

}  // namespace internal
}  // namespace crashpad
