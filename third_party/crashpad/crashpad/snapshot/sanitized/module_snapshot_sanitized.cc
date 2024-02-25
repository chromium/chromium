// Copyright 2018 The Crashpad Authors
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

#include "snapshot/sanitized/module_snapshot_sanitized.h"

#include "base/strings/pattern.h"

namespace crashpad {
namespace internal {

namespace {

bool KeyIsAllowed(const std::string& name,
                  const std::vector<std::string>& allowed_keys) {
  for (const auto& key : allowed_keys) {
    if (base::MatchPattern(name, key)) {
      return true;
    }
  }
  return false;
}

}  // namespace

ModuleSnapshotSanitized::ModuleSnapshotSanitized(
    const ModuleSnapshot* snapshot,
    const std::vector<std::string>* allowed_annotations)
    : snapshot_(snapshot), allowed_annotations_(allowed_annotations) {}

ModuleSnapshotSanitized::~ModuleSnapshotSanitized() = default;

std::string ModuleSnapshotSanitized::Name() const {
  return snapshot_->Name();
}

uint64_t ModuleSnapshotSanitized::Address() const {
  return snapshot_->Address();
}

uint64_t ModuleSnapshotSanitized::Size() const {
  return snapshot_->Size();
}

time_t ModuleSnapshotSanitized::Timestamp() const {
  return snapshot_->Timestamp();
}

void ModuleSnapshotSanitized::FileVersion(uint16_t* version_0,
                                          uint16_t* version_1,
                                          uint16_t* version_2,
                                          uint16_t* version_3) const {
  snapshot_->FileVersion(version_0, version_1, version_2, version_3);
}

void ModuleSnapshotSanitized::SourceVersion(uint16_t* version_0,
                                            uint16_t* version_1,
                                            uint16_t* version_2,
                                            uint16_t* version_3) const {
  snapshot_->SourceVersion(version_0, version_1, version_2, version_3);
}

ModuleSnapshot::ModuleType ModuleSnapshotSanitized::GetModuleType() const {
  return snapshot_->GetModuleType();
}

void ModuleSnapshotSanitized::UUIDAndAge(crashpad::UUID* uuid,
                                         uint32_t* age) const {
  snapshot_->UUIDAndAge(uuid, age);
}

std::string ModuleSnapshotSanitized::DebugFileName() const {
  return snapshot_->DebugFileName();
}

std::vector<uint8_t> ModuleSnapshotSanitized::BuildID() const {
  return snapshot_->BuildID();
}

std::vector<std::string> ModuleSnapshotSanitized::AnnotationsVector() const {
  // TODO(jperaza): If/when AnnotationsVector() begins to be used, determine
  // whether and how the content should be sanitized.
  DCHECK(snapshot_->AnnotationsVector().empty());
  return std::vector<std::string>();
}

std::map<std::string, std::string>
ModuleSnapshotSanitized::AnnotationsSimpleMap() const {
  std::map<std::string, std::string> annotations =
      snapshot_->AnnotationsSimpleMap();
  if (allowed_annotations_) {
    for (auto kv = annotations.begin(); kv != annotations.end();) {
      if (KeyIsAllowed(kv->first, *allowed_annotations_)) {
        ++kv;
      } else {
        kv = annotations.erase(kv);
      }
    }
  }
  return annotations;
}

std::vector<AnnotationSnapshot> ModuleSnapshotSanitized::AnnotationObjects()
    const {
  std::vector<AnnotationSnapshot> annotations = snapshot_->AnnotationObjects();
  if (allowed_annotations_) {
    std::vector<AnnotationSnapshot> allowed;
    for (const auto& anno : annotations) {
      if (KeyIsAllowed(anno.name, *allowed_annotations_)) {
        allowed.push_back(anno);
      }
    }
    annotations.swap(allowed);
  }
  return annotations;
}

std::set<CheckedRange<uint64_t>> ModuleSnapshotSanitized::ExtraMemoryRanges()
    const {
  DCHECK(snapshot_->ExtraMemoryRanges().empty());
  return std::set<CheckedRange<uint64_t>>();
}

std::vector<const UserMinidumpStream*>
ModuleSnapshotSanitized::CustomMinidumpStreams() const {
  return snapshot_->CustomMinidumpStreams();
}

}  // namespace internal
}  // namespace crashpad
