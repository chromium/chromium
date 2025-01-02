// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

namespace onnxruntime {

// Combine hash value `seed` with hash value `h`, updating `seed` in place.
// TODO(edgchen1) find a better implementation? e.g., see a more recent version of boost::hash_combine()
inline void HashCombineWithHashValue(size_t h, size_t& seed) {
  seed ^= h + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

// Combine hash value `seed` with the hash value of `value`, updating `seed` in place.
// The hash value computation is specified by the `Hash` template parameter.
template <typename T, typename Hash = std::hash<T>>
inline void HashCombine(const T& value, size_t& seed) {
  HashCombineWithHashValue(Hash{}(value), seed);
}

}  // namespace onnxruntime
