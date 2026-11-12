/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <android-base/unique_fd.h>
#include <procinfo/process_map.h>

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <vector>

#include <unwindstack/Elf.h>
#include <unwindstack/Maps.h>
#include <unwindstack/Memory.h>

namespace unwindstack {

std::shared_ptr<MapInfo> Maps::Find(uint64_t pc) {
  if (maps_.empty()) {
    return nullptr;
  }
  size_t first = 0;
  size_t last = maps_.size();
  while (first < last) {
    size_t index = (first + last) / 2;
    const auto& cur = maps_[index];
    if (pc >= cur->start() && pc < cur->end()) {
      return cur;
    } else if (pc < cur->start()) {
      last = index;
    } else {
      first = index + 1;
    }
  }
  return nullptr;
}

bool Maps::Parse() {
  std::shared_ptr<MapInfo> prev_map;
  return android::procinfo::ReadMapFile(GetMapsFile(),
                      [&](const android::procinfo::MapInfo& mapinfo) {
    // Mark a device map in /dev/ and not in /dev/ashmem/ specially.
    auto flags = mapinfo.flags;
    if (strncmp(mapinfo.name.c_str(), "/dev/", 5) == 0 &&
        strncmp(mapinfo.name.c_str() + 5, "ashmem/", 7) != 0) {
      flags |= unwindstack::MAPS_FLAGS_DEVICE_MAP;
    }
    maps_.emplace_back(
        MapInfo::Create(prev_map, mapinfo.start, mapinfo.end, mapinfo.pgoff, flags, mapinfo.name));
    prev_map = maps_.back();
  });
}

void Maps::Add(uint64_t start, uint64_t end, uint64_t offset, uint64_t flags,
               const std::string& name) {
  std::shared_ptr<MapInfo> prev_map(maps_.empty() ? nullptr : maps_.back());
  auto map_info = MapInfo::Create(prev_map, start, end, offset, flags, name);
  maps_.emplace_back(std::move(map_info));
}

void Maps::Add(uint64_t start, uint64_t end, uint64_t offset, uint64_t flags,
               const std::string& name, uint64_t load_bias) {
  std::shared_ptr<MapInfo> prev_map(maps_.empty() ? nullptr : maps_.back());
  auto map_info = MapInfo::Create(prev_map, start, end, offset, flags, name);
  map_info->set_load_bias(load_bias);
  maps_.emplace_back(std::move(map_info));
}

void Maps::Sort() {
  if (maps_.empty()) {
    return;
  }

  std::sort(maps_.begin(), maps_.end(),
            [](const std::shared_ptr<MapInfo>& a, const std::shared_ptr<MapInfo>& b) {
              return a->start() < b->start();
            });

  // Set prev_map and next_map on the info objects.
  std::shared_ptr<MapInfo> prev_map;
  // Set the last next_map to nullptr.
  maps_.back()->set_next_map(prev_map);
  for (auto& map_info : maps_) {
    map_info->set_prev_map(prev_map);
    if (prev_map) {
      prev_map->set_next_map(map_info);
    }
    prev_map = map_info;
  }
}

bool BufferMaps::Parse() {
  std::string content(buffer_);
  std::shared_ptr<MapInfo> prev_map;
  return android::procinfo::ReadMapFileContent(
      &content[0], [&](const android::procinfo::MapInfo& mapinfo) {
        // Mark a device map in /dev/ and not in /dev/ashmem/ specially.
        auto flags = mapinfo.flags;
        if (strncmp(mapinfo.name.c_str(), "/dev/", 5) == 0 &&
            strncmp(mapinfo.name.c_str() + 5, "ashmem/", 7) != 0) {
          flags |= unwindstack::MAPS_FLAGS_DEVICE_MAP;
        }
        maps_.emplace_back(MapInfo::Create(prev_map, mapinfo.start, mapinfo.end, mapinfo.pgoff,
                                           flags, mapinfo.name));
        prev_map = maps_.back();
      });
}

const std::string RemoteMaps::GetMapsFile() const {
  return "/proc/" + std::to_string(pid_) + "/maps";
}

const std::string LocalUpdatableMaps::GetMapsFile() const {
  return "/proc/self/maps";
}

LocalUpdatableMaps::LocalUpdatableMaps() : Maps() {
  pthread_rwlock_init(&maps_rwlock_, nullptr);
}

std::shared_ptr<MapInfo> LocalUpdatableMaps::Find(uint64_t pc) {
  pthread_rwlock_rdlock(&maps_rwlock_);
  std::shared_ptr<MapInfo> map_info = Maps::Find(pc);
  pthread_rwlock_unlock(&maps_rwlock_);

  if (map_info == nullptr) {
    pthread_rwlock_wrlock(&maps_rwlock_);
    // This is guaranteed not to invalidate any previous MapInfo objects so
    // we don't need to worry about any MapInfo* values already in use.
    if (Reparse()) {
      map_info = Maps::Find(pc);
    }
    pthread_rwlock_unlock(&maps_rwlock_);
  }

  return map_info;
}

bool LocalUpdatableMaps::Parse() {
  pthread_rwlock_wrlock(&maps_rwlock_);
  bool parsed = Maps::Parse();
  pthread_rwlock_unlock(&maps_rwlock_);
  return parsed;
}

bool LocalUpdatableMaps::Reparse(/*out*/ bool* any_changed) {
  // New maps will be added at the end without deleting the old ones.
  size_t last_map_idx = maps_.size();
  if (!Maps::Parse()) {
    maps_.resize(last_map_idx);
    return false;
  }

  size_t search_map_idx = 0;
  size_t num_deleted_old_entries = 0;
  size_t num_deleted_new_entries = 0;
  for (size_t new_map_idx = last_map_idx; new_map_idx < maps_.size(); new_map_idx++) {
    auto& new_map_info = maps_[new_map_idx];
    uint64_t start = new_map_info->start();
    uint64_t end = new_map_info->end();
    uint64_t flags = new_map_info->flags();
    const SharedString& name = new_map_info->name();
    for (size_t old_map_idx = search_map_idx; old_map_idx < last_map_idx; old_map_idx++) {
      auto& info = maps_[old_map_idx];
      if (start == info->start() && end == info->end() && flags == info->flags() &&
          name == info->name()) {
        search_map_idx = old_map_idx + 1;
        // Since we are throwing away a map from the new list, need to
        // adjust the next/prev pointers in the old map entry.
        auto prev = new_map_info->prev_map();
        auto next = new_map_info->next_map();
        info->set_prev_map(prev);
        info->set_next_map(next);

        // Fix up the pointers in the prev and next entries.
        if (prev != nullptr) {
          prev->set_next_map(info);
        }
        if (next != nullptr) {
          next->set_prev_map(info);
        }

        maps_[new_map_idx] = nullptr;
        num_deleted_new_entries++;
        break;
      } else if (info->start() > start) {
        // Stop, there isn't going to be a match.
        search_map_idx = old_map_idx;
        break;
      }

      // Never delete these maps, they may be in use. The assumption is
      // that there will only every be a handful of these so waiting
      // to destroy them is not too expensive.
      // Since these are all shared_ptrs, we can just remove the references.
      // Any code still holding on to the pointer, will still have a
      // valid pointer after this.
      search_map_idx = old_map_idx + 1;
      maps_[old_map_idx] = nullptr;
      num_deleted_old_entries++;
    }
    if (search_map_idx >= last_map_idx) {
      break;
    }
  }

  for (size_t i = search_map_idx; i < last_map_idx; i++) {
    maps_[i] = nullptr;
    num_deleted_old_entries++;
  }

  // Sort all of the values such that the nullptrs wind up at the end, then
  // resize them away.
  std::sort(maps_.begin(), maps_.end(), [](const auto& a, const auto& b) {
    if (a == nullptr) {
      return false;
    } else if (b == nullptr) {
      return true;
    }
    return a->start() < b->start();
  });
  maps_.resize(maps_.size() - num_deleted_old_entries - num_deleted_new_entries);

  if (any_changed != nullptr) {
    *any_changed = num_deleted_old_entries != 0 || maps_.size() != last_map_idx;
  }

  return true;
}

}  // namespace unwindstack
