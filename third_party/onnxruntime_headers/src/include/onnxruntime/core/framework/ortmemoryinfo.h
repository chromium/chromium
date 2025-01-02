// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string_view>

#include "core/common/hash_combine.h"

struct OrtMemoryInfo {
  OrtMemoryInfo() = default;  // to allow default construction of Tensor

  // use string for name, so we could have customized allocator in execution provider.
  const char* name = nullptr;
  int id = -1;
  OrtMemType mem_type = OrtMemTypeDefault;
  OrtAllocatorType alloc_type = OrtInvalidAllocator;
  OrtDevice device;

  constexpr OrtMemoryInfo(const char* name_, OrtAllocatorType type_, OrtDevice device_ = OrtDevice(), int id_ = 0,
                          OrtMemType mem_type_ = OrtMemTypeDefault)
#if ((defined(__GNUC__) && __GNUC__ > 4) || defined(__clang__))
      // this causes a spurious error in CentOS gcc 4.8 build so disable if GCC version < 5
      __attribute__((nonnull))
#endif
      : name(name_),
        id(id_),
        mem_type(mem_type_),
        alloc_type(type_),
        device(device_) {
  }

  // To make OrtMemoryInfo become a valid key in std map
  bool operator<(const OrtMemoryInfo& other) const {
    if (alloc_type != other.alloc_type)
      return alloc_type < other.alloc_type;
    if (mem_type != other.mem_type)
      return mem_type < other.mem_type;
    if (id != other.id)
      return id < other.id;

    return strcmp(name, other.name) < 0;
  }

  // This is to make OrtMemoryInfo a valid key in hash tables
  // we ignore device id
  size_t Hash() const {
    auto h = std::hash<int>()(alloc_type);
    onnxruntime::HashCombine(mem_type, h);
    onnxruntime::HashCombine(id, h);
    onnxruntime::HashCombine<std::string_view>(name, h);
    return h;
  }

  std::string ToString() const {
    std::ostringstream ostr;
    ostr << "OrtMemoryInfo:["
         << "name:" << name
         << " id:" << id
         << " OrtMemType:" << mem_type
         << " OrtAllocatorType:" << alloc_type
         << " " << device.ToString()
         << "]";
    return ostr.str();
  }
};

// Required by hash tables
inline bool operator==(const OrtMemoryInfo& left, const OrtMemoryInfo& other) {
  return left.mem_type == other.mem_type &&
         left.alloc_type == other.alloc_type &&
         left.id == other.id &&
         strcmp(left.name, other.name) == 0;
}

inline bool operator!=(const OrtMemoryInfo& lhs, const OrtMemoryInfo& rhs) { return !(lhs == rhs); }

std::ostream& operator<<(std::ostream& out, const OrtMemoryInfo& info);

namespace std {
template <>
struct hash<OrtMemoryInfo> {
  size_t operator()(const OrtMemoryInfo& i) const {
    return i.Hash();
  }
};
}  // namespace std
