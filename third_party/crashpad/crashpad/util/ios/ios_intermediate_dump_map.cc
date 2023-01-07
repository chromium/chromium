// Copyright 2021 The Crashpad Authors
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

#include "util/ios/ios_intermediate_dump_map.h"

#include "util/ios/ios_intermediate_dump_data.h"
#include "util/ios/ios_intermediate_dump_list.h"
#include "util/ios/ios_intermediate_dump_object.h"

using crashpad::internal::IntermediateDumpKey;

namespace crashpad {
namespace internal {

IOSIntermediateDumpMap::IOSIntermediateDumpMap() : map_() {}

IOSIntermediateDumpMap::~IOSIntermediateDumpMap() {}

IOSIntermediateDumpMap::Type IOSIntermediateDumpMap::GetType() const {
  return Type::kMap;
}

const IOSIntermediateDumpData* IOSIntermediateDumpMap::GetAsData(
    const IntermediateDumpKey& key) const {
  auto object_it = map_.find(key);
  if (object_it != map_.end()) {
    IOSIntermediateDumpObject* object = object_it->second.get();
    if (object->GetType() == Type::kData)
      return static_cast<IOSIntermediateDumpData*>(object);
  }
  return nullptr;
}

const IOSIntermediateDumpList* IOSIntermediateDumpMap::GetAsList(
    const IntermediateDumpKey& key) const {
  auto object_it = map_.find(key);
  if (object_it != map_.end()) {
    IOSIntermediateDumpObject* object = object_it->second.get();
    if (object->GetType() == Type::kList)
      return static_cast<IOSIntermediateDumpList*>(object);
  }
  return nullptr;
}

const IOSIntermediateDumpMap* IOSIntermediateDumpMap::GetAsMap(
    const IntermediateDumpKey& key) const {
  auto object_it = map_.find(key);
  if (object_it != map_.end()) {
    IOSIntermediateDumpObject* object = object_it->second.get();
    if (object->GetType() == Type::kMap)
      return static_cast<IOSIntermediateDumpMap*>(object);
  }
  return nullptr;
}

}  // namespace internal
}  // namespace crashpad
