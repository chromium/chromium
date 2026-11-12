/*
 * Copyright (C) 2018 The Android Open Source Project
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

#pragma once

#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>

#include <functional>
#include <string>
#include <vector>

#include <android-base/file.h>

namespace android {
namespace procinfo {

struct MapInfo {
  uint64_t start;
  uint64_t end;
  uint16_t flags;
  uint64_t pgoff;
  ino_t inode;
  std::string name;
  bool shared;

  MapInfo(uint64_t start, uint64_t end, uint16_t flags, uint64_t pgoff, ino_t inode,
          const char* name, bool shared)
      : start(start),
        end(end),
        flags(flags),
        pgoff(pgoff),
        inode(inode),
        name(name),
        shared(shared) {}

  MapInfo(const MapInfo& params)
      : start(params.start),
        end(params.end),
        flags(params.flags),
        pgoff(params.pgoff),
        inode(params.inode),
        name(params.name),
        shared(params.shared) {}
};

typedef std::function<void(const MapInfo&)> MapInfoCallback;
typedef std::function<void(uint64_t start, uint64_t end, uint16_t flags, uint64_t pgoff,
                      ino_t inode, const char* name, bool shared)> MapInfoParamsCallback;

static inline bool PassSpace(char** p) {
  if (**p != ' ') {
    return false;
  }
  while (**p == ' ') {
    (*p)++;
  }
  return true;
}

static inline bool PassXdigit(char** p) {
  if (!isxdigit(**p)) {
    return false;
  }
  do {
    (*p)++;
  } while (isxdigit(**p));
  return true;
}

// Parses the given line p pointing at proc/<pid>/maps content buffer and returns true on success
// and false on failure parsing. The first new line character of line will be replaced by the
// null character and *next_line will point to the character after the null.
//
// Example of how a parsed line look line:
// 00400000-00409000 r-xp 00000000 fc:00 426998  /usr/lib/gvfs/gvfsd-http
static inline bool ParseMapsFileLine(char* p, uint64_t& start_addr, uint64_t& end_addr, uint16_t& flags,
                      uint64_t& pgoff, ino_t& inode, char** name, bool& shared, char** next_line) {
  // Make the first new line character null.
  *next_line = strchr(p, '\n');
  if (*next_line != nullptr) {
    **next_line = '\0';
    (*next_line)++;
  }

  char* end;
  // start_addr
  start_addr = strtoull(p, &end, 16);
  if (end == p || *end != '-') {
    return false;
  }
  p = end + 1;
  // end_addr
  end_addr = strtoull(p, &end, 16);
  if (end == p) {
    return false;
  }
  p = end;
  if (!PassSpace(&p)) {
    return false;
  }
  // flags
  flags = 0;
  if (*p == 'r') {
    flags |= PROT_READ;
  } else if (*p != '-') {
    return false;
  }
  p++;
  if (*p == 'w') {
    flags |= PROT_WRITE;
  } else if (*p != '-') {
    return false;
  }
  p++;
  if (*p == 'x') {
    flags |= PROT_EXEC;
  } else if (*p != '-') {
    return false;
  }
  p++;
  if (*p != 'p' && *p != 's') {
    return false;
  }
  shared = *p == 's';

  p++;
  if (!PassSpace(&p)) {
    return false;
  }
  // pgoff
  pgoff = strtoull(p, &end, 16);
  if (end == p) {
    return false;
  }
  p = end;
  if (!PassSpace(&p)) {
    return false;
  }
  // major:minor
  if (!PassXdigit(&p) || *p++ != ':' || !PassXdigit(&p) || !PassSpace(&p)) {
    return false;
  }
  // inode
  inode = strtoull(p, &end, 10);
  if (end == p) {
    return false;
  }
  p = end;

  if (*p != '\0' && !PassSpace(&p)) {
    return false;
  }

  // Assumes that the first new character was replaced with null.
  *name = p;

  return true;
}

inline bool ReadMapFileContent(char* content, const MapInfoParamsCallback& callback) {
  uint64_t start_addr;
  uint64_t end_addr;
  uint16_t flags;
  uint64_t pgoff;
  ino_t inode;
  char* line_start = content;
  char* next_line;
  char* name;
  bool shared;

  while (line_start != nullptr && *line_start != '\0') {
    bool parsed = ParseMapsFileLine(line_start, start_addr, end_addr, flags, pgoff,
                                    inode, &name, shared, &next_line);
    if (!parsed) {
      return false;
    }

    line_start = next_line;
    callback(start_addr, end_addr, flags, pgoff, inode, name, shared);
  }
  return true;
}

inline bool ReadMapFileContent(char* content, const MapInfoCallback& callback) {
  uint64_t start_addr;
  uint64_t end_addr;
  uint16_t flags;
  uint64_t pgoff;
  ino_t inode;
  char* line_start = content;
  char* next_line;
  char* name;
  bool shared;

  while (line_start != nullptr && *line_start != '\0') {
    bool parsed = ParseMapsFileLine(line_start, start_addr, end_addr, flags, pgoff,
                                    inode, &name, shared, &next_line);
    if (!parsed) {
      return false;
    }

    line_start = next_line;
    callback(MapInfo(start_addr, end_addr, flags, pgoff, inode, name, shared));
  }
  return true;
}

inline bool ReadMapFile(const std::string& map_file,
                const MapInfoCallback& callback) {
  std::string content;
  if (!android::base::ReadFileToString(map_file, &content)) {
    return false;
  }
  return ReadMapFileContent(&content[0], callback);
}


inline bool ReadMapFile(const std::string& map_file, const MapInfoParamsCallback& callback,
                        std::string& mapsBuffer) {
  if (!android::base::ReadFileToString(map_file, &mapsBuffer)) {
    return false;
  }
  return ReadMapFileContent(&mapsBuffer[0], callback);
}

inline bool ReadMapFile(const std::string& map_file,
                const MapInfoParamsCallback& callback) {
  std::string content;
  return ReadMapFile(map_file, callback, content);
}

inline bool ReadProcessMaps(pid_t pid, const MapInfoCallback& callback) {
  return ReadMapFile("/proc/" + std::to_string(pid) + "/maps", callback);
}

inline bool ReadProcessMaps(pid_t pid, const MapInfoParamsCallback& callback,
                            std::string& mapsBuffer) {
  return ReadMapFile("/proc/" + std::to_string(pid) + "/maps", callback, mapsBuffer);
}

inline bool ReadProcessMaps(pid_t pid, const MapInfoParamsCallback& callback) {
  std::string content;
  return ReadProcessMaps(pid, callback, content);
}

inline bool ReadProcessMaps(pid_t pid, std::vector<MapInfo>* maps) {
  return ReadProcessMaps(pid, [&](const MapInfo& mapinfo) { maps->emplace_back(mapinfo); });
}

// Reads maps file and executes given callback for each mapping
// Warning: buffer should not be modified asynchronously while this function executes
template <class CallbackType>
inline bool ReadMapFileAsyncSafe(const char* map_file, void* buffer, size_t buffer_size,
                                 const CallbackType& callback) {
  if (buffer == nullptr || buffer_size == 0) {
    return false;
  }

  int fd = open(map_file, O_RDONLY | O_CLOEXEC);
  if (fd == -1) {
    return false;
  }

  char* char_buffer = reinterpret_cast<char*>(buffer);
  size_t start = 0;
  size_t read_bytes = 0;
  char* line = nullptr;
  bool read_complete = false;
  while (true) {
    ssize_t bytes =
        TEMP_FAILURE_RETRY(read(fd, char_buffer + read_bytes, buffer_size - read_bytes - 1));
    if (bytes <= 0) {
      if (read_bytes == 0) {
        close(fd);
        return bytes == 0;
      }
      // Treat the last piece of data as the last line.
      char_buffer[start + read_bytes] = '\n';
      bytes = 1;
      read_complete = true;
    }
    read_bytes += bytes;

    while (read_bytes > 0) {
      char* newline = reinterpret_cast<char*>(memchr(&char_buffer[start], '\n', read_bytes));
      if (newline == nullptr) {
        break;
      }
      *newline = '\0';
      line = &char_buffer[start];
      start = newline - char_buffer + 1;
      read_bytes -= newline - line + 1;

      // Ignore the return code, errors are okay.
      ReadMapFileContent(line, callback);
    }

    if (read_complete) {
      close(fd);
      return true;
    }

    if (start == 0 && read_bytes == buffer_size - 1) {
      // The buffer provided is too small to contain this line, give up
      // and indicate failure.
      close(fd);
      return false;
    }

    // Copy any leftover data to the front  of the buffer.
    if (start > 0) {
      if (read_bytes > 0) {
        memmove(char_buffer, &char_buffer[start], read_bytes);
      }
      start = 0;
    }
  }
}

} /* namespace procinfo */
} /* namespace android */
