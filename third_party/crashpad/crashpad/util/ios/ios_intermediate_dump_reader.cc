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

#include "util/ios/ios_intermediate_dump_reader.h"

#include <memory>
#include <stack>
#include <vector>

#include "base/logging.h"
#include "util/file/filesystem.h"
#include "util/ios/ios_intermediate_dump_data.h"
#include "util/ios/ios_intermediate_dump_format.h"
#include "util/ios/ios_intermediate_dump_list.h"
#include "util/ios/ios_intermediate_dump_object.h"
#include "util/ios/ios_intermediate_dump_writer.h"

namespace crashpad {
namespace internal {

IOSIntermediateDumpReaderInitializeResult IOSIntermediateDumpReader::Initialize(
    const IOSIntermediateDumpInterface& dump_interface) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  // Don't initialize empty files.
  FileOffset size = dump_interface.Size();
  if (size == 0) {
    return IOSIntermediateDumpReaderInitializeResult::kFailure;
  }

  IOSIntermediateDumpReaderInitializeResult result =
      IOSIntermediateDumpReaderInitializeResult::kSuccess;
  if (!Parse(dump_interface.FileReader(), size)) {
    LOG(ERROR) << "Intermediate dump parsing failed";
    result = IOSIntermediateDumpReaderInitializeResult::kIncomplete;
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return result;
}

const IOSIntermediateDumpMap* IOSIntermediateDumpReader::RootMap() {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &intermediate_dump_;
}

bool IOSIntermediateDumpReader::Parse(FileReaderInterface* reader,
                                      FileOffset file_size) {
  std::stack<IOSIntermediateDumpObject*> stack;
  stack.push(&intermediate_dump_);
  using Command = IOSIntermediateDumpWriter::CommandType;
  using Type = IOSIntermediateDumpObject::Type;

  Command command;
  if (!reader->ReadExactly(&command, sizeof(Command)) ||
      command != Command::kRootMapStart) {
    LOG(ERROR) << "Unexpected start to root map.";
    return false;
  }

  while (reader->ReadExactly(&command, sizeof(Command))) {
    constexpr int kMaxStackDepth = 10;
    if (stack.size() > kMaxStackDepth) {
      LOG(ERROR) << "Unexpected depth of intermediate dump data.";
      return false;
    }

    IOSIntermediateDumpObject* parent = stack.top();
    switch (command) {
      case Command::kMapStart: {
        std::unique_ptr<IOSIntermediateDumpMap> new_map(
            new IOSIntermediateDumpMap());
        if (parent->GetType() == Type::kMap) {
          const auto parent_map = static_cast<IOSIntermediateDumpMap*>(parent);
          stack.push(new_map.get());
          IntermediateDumpKey key;
          if (!reader->ReadExactly(&key, sizeof(key)))
            return false;
          if (key == IntermediateDumpKey::kInvalid)
            return false;
          parent_map->map_[key] = std::move(new_map);
        } else if (parent->GetType() == Type::kList) {
          const auto parent_list =
              static_cast<IOSIntermediateDumpList*>(parent);
          stack.push(new_map.get());
          parent_list->push_back(std::move(new_map));
        } else {
          LOG(ERROR) << "Unexpected parent (not a map or list).";
          return false;
        }
        break;
      }
      case Command::kArrayStart: {
        auto new_list = std::make_unique<IOSIntermediateDumpList>();
        if (parent->GetType() != Type::kMap) {
          LOG(ERROR) << "Attempting to push an array not in a map.";
          return false;
        }

        IntermediateDumpKey key;
        if (!reader->ReadExactly(&key, sizeof(key)))
          return false;
        if (key == IntermediateDumpKey::kInvalid)
          return false;

        auto parent_map = static_cast<IOSIntermediateDumpMap*>(parent);
        stack.push(new_list.get());
        parent_map->map_[key] = std::move(new_list);
        break;
      }
      case Command::kMapEnd:
        if (stack.size() < 2) {
          LOG(ERROR) << "Attempting to pop off main map.";
          return false;
        }

        if (parent->GetType() != Type::kMap) {
          LOG(ERROR) << "Unexpected map end not in a map.";
          return false;
        }
        stack.pop();
        break;
      case Command::kArrayEnd:
        if (stack.size() < 2) {
          LOG(ERROR) << "Attempting to pop off main map.";
          return false;
        }
        if (parent->GetType() != Type::kList) {
          LOG(ERROR) << "Unexpected list end not in a list.";
          return false;
        }
        stack.pop();
        break;
      case Command::kProperty: {
        if (parent->GetType() != Type::kMap) {
          LOG(ERROR) << "Attempting to add a property not in a map.";
          return false;
        }
        IntermediateDumpKey key;
        if (!reader->ReadExactly(&key, sizeof(key)))
          return false;
        if (key == IntermediateDumpKey::kInvalid)
          return false;

        size_t value_length;
        if (!reader->ReadExactly(&value_length, sizeof(value_length))) {
          return false;
        }

        constexpr int kMaximumPropertyLength = 64 * 1024 * 1024;  // 64MB.
        if (value_length > kMaximumPropertyLength) {
          LOG(ERROR) << "Attempting to read a property that's too big: "
                     << value_length;
          return false;
        }

        std::vector<uint8_t> data(value_length);
        if (!reader->ReadExactly(data.data(), value_length)) {
          return false;
        }
        auto parent_map = static_cast<IOSIntermediateDumpMap*>(parent);
        if (parent_map->map_.find(key) != parent_map->map_.end()) {
          LOG(ERROR) << "Inserting duplicate key";
        }
        parent_map->map_[key] =
            std::make_unique<IOSIntermediateDumpData>(std::move(data));
        break;
      }
      case Command::kRootMapEnd: {
        if (stack.size() != 1) {
          LOG(ERROR) << "Unexpected end of root map.";
          return false;
        }

        if (reader->Seek(0, SEEK_CUR) != file_size) {
          LOG(ERROR) << "Root map ended before end of file.";
          return false;
        }
        return true;
      }
      default:
        LOG(ERROR) << "Failed to parse serialized intermediate minidump.";
        return false;
    }
  }

  LOG(ERROR) << "Unexpected end of root map.";
  return false;
}

}  // namespace internal
}  // namespace crashpad
