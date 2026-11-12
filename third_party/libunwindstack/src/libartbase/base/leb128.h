/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_LIBARTBASE_BASE_LEB128_H_
#define ART_LIBARTBASE_BASE_LEB128_H_

#include <vector>

#include <android-base/logging.h>

#include "bit_utils.h"
#include "globals.h"
#include "macros.h"

namespace art {

// Reads an unsigned LEB128 value, updating the given pointer to point
// just past the end of the read value. This function tolerates
// non-zero high-order bits in the fifth encoded byte.
static inline uint32_t DecodeUnsignedLeb128(const uint8_t** data) {
  const uint8_t* ptr = *data;
  int result = *(ptr++);
  if (UNLIKELY(result > 0x7f)) {
    int cur = *(ptr++);
    result = (result & 0x7f) | ((cur & 0x7f) << 7);
    if (cur > 0x7f) {
      cur = *(ptr++);
      result |= (cur & 0x7f) << 14;
      if (cur > 0x7f) {
        cur = *(ptr++);
        result |= (cur & 0x7f) << 21;
        if (cur > 0x7f) {
          // Note: We don't check to see if cur is out of range here,
          // meaning we tolerate garbage in the four high-order bits.
          cur = *(ptr++);
          result |= cur << 28;
        }
      }
    }
  }
  *data = ptr;
  return static_cast<uint32_t>(result);
}

static inline uint32_t DecodeUnsignedLeb128WithoutMovingCursor(const uint8_t* data) {
  return DecodeUnsignedLeb128(&data);
}

static inline bool DecodeUnsignedLeb128Checked(const uint8_t** data,
                                               const void* end,
                                               uint32_t* out) {
  const uint8_t* ptr = *data;
  if (ptr >= end) {
    return false;
  }
  int result = *(ptr++);
  if (UNLIKELY(result > 0x7f)) {
    if (ptr >= end) {
      return false;
    }
    int cur = *(ptr++);
    result = (result & 0x7f) | ((cur & 0x7f) << 7);
    if (cur > 0x7f) {
      if (ptr >= end) {
        return false;
      }
      cur = *(ptr++);
      result |= (cur & 0x7f) << 14;
      if (cur > 0x7f) {
        if (ptr >= end) {
          return false;
        }
        cur = *(ptr++);
        result |= (cur & 0x7f) << 21;
        if (cur > 0x7f) {
          if (ptr >= end) {
            return false;
          }
          // Note: We don't check to see if cur is out of range here,
          // meaning we tolerate garbage in the four high-order bits.
          cur = *(ptr++);
          result |= cur << 28;
        }
      }
    }
  }
  *data = ptr;
  *out = static_cast<uint32_t>(result);
  return true;
}

// Reads an unsigned LEB128 + 1 value. updating the given pointer to point
// just past the end of the read value. This function tolerates
// non-zero high-order bits in the fifth encoded byte.
// It is possible for this function to return -1.
static inline int32_t DecodeUnsignedLeb128P1(const uint8_t** data) {
  return DecodeUnsignedLeb128(data) - 1;
}

// Reads a signed LEB128 value, updating the given pointer to point
// just past the end of the read value. This function tolerates
// non-zero high-order bits in the fifth encoded byte.
static inline int32_t DecodeSignedLeb128(const uint8_t** data) {
  const uint8_t* ptr = *data;
  int32_t result = *(ptr++);
  if (result <= 0x7f) {
    result = (result << 25) >> 25;
  } else {
    int cur = *(ptr++);
    result = (result & 0x7f) | ((cur & 0x7f) << 7);
    if (cur <= 0x7f) {
      result = (result << 18) >> 18;
    } else {
      cur = *(ptr++);
      result |= (cur & 0x7f) << 14;
      if (cur <= 0x7f) {
        result = (result << 11) >> 11;
      } else {
        cur = *(ptr++);
        result |= (cur & 0x7f) << 21;
        if (cur <= 0x7f) {
          result = (result << 4) >> 4;
        } else {
          // Note: We don't check to see if cur is out of range here,
          // meaning we tolerate garbage in the four high-order bits.
          cur = *(ptr++);
          result |= cur << 28;
        }
      }
    }
  }
  *data = ptr;
  return result;
}

static inline bool DecodeSignedLeb128Checked(const uint8_t** data,
                                             const void* end,
                                             int32_t* out) {
  const uint8_t* ptr = *data;
  if (ptr >= end) {
    return false;
  }
  int32_t result = *(ptr++);
  if (result <= 0x7f) {
    result = (result << 25) >> 25;
  } else {
    if (ptr >= end) {
      return false;
    }
    int cur = *(ptr++);
    result = (result & 0x7f) | ((cur & 0x7f) << 7);
    if (cur <= 0x7f) {
      result = (result << 18) >> 18;
    } else {
      if (ptr >= end) {
        return false;
      }
      cur = *(ptr++);
      result |= (cur & 0x7f) << 14;
      if (cur <= 0x7f) {
        result = (result << 11) >> 11;
      } else {
        if (ptr >= end) {
          return false;
        }
        cur = *(ptr++);
        result |= (cur & 0x7f) << 21;
        if (cur <= 0x7f) {
          result = (result << 4) >> 4;
        } else {
          if (ptr >= end) {
            return false;
          }
          // Note: We don't check to see if cur is out of range here,
          // meaning we tolerate garbage in the four high-order bits.
          cur = *(ptr++);
          result |= cur << 28;
        }
      }
    }
  }
  *data = ptr;
  *out = static_cast<uint32_t>(result);
  return true;
}

// Returns the number of bytes needed to encode the value in unsigned LEB128.
static inline uint32_t UnsignedLeb128Size(uint32_t data) {
  // bits_to_encode = (data != 0) ? 32 - CLZ(x) : 1  // 32 - CLZ(data | 1)
  // bytes = ceil(bits_to_encode / 7.0);             // (6 + bits_to_encode) / 7
  uint32_t x = 6 + 32 - CLZ(data | 1U);
  // Division by 7 is done by (x * 37) >> 8 where 37 = ceil(256 / 7).
  // This works for 0 <= x < 256 / (7 * 37 - 256), i.e. 0 <= x <= 85.
  return (x * 37) >> 8;
}

static inline bool IsLeb128Terminator(const uint8_t* ptr) {
  return *ptr <= 0x7f;
}

// Returns the first byte of a Leb128 value assuming that:
// (1) `end_ptr` points to the first byte after the Leb128 value, and
// (2) there is another Leb128 value before this one.
template <typename T>
static inline T* ReverseSearchUnsignedLeb128(T* end_ptr) {
  static_assert(std::is_same_v<std::remove_const_t<T>, uint8_t>,
                "T must be a uint8_t");
  T* ptr = end_ptr;

  // Move one byte back, check that this is the terminating byte.
  ptr--;
  DCHECK(IsLeb128Terminator(ptr));

  // Keep moving back while the previous byte is not a terminating byte.
  // Fail after reading five bytes in case there isn't another Leb128 value
  // before this one.
  while (!IsLeb128Terminator(ptr - 1)) {
    ptr--;
    DCHECK_LE(static_cast<ptrdiff_t>(end_ptr - ptr), 5);
  }

  return ptr;
}

// Returns the number of bytes needed to encode the value in unsigned LEB128.
static inline uint32_t SignedLeb128Size(int32_t data) {
  // Like UnsignedLeb128Size(), but we need one bit beyond the highest bit that differs from sign.
  data = data ^ (data >> 31);
  uint32_t x = 1 /* we need to encode the sign bit */ + 6 + 32 - CLZ(data | 1U);
  return (x * 37) >> 8;
}

static inline uint8_t* EncodeUnsignedLeb128(uint8_t* dest, uint32_t value) {
  uint8_t out = value & 0x7f;
  value >>= 7;
  while (value != 0) {
    *dest++ = out | 0x80;
    out = value & 0x7f;
    value >>= 7;
  }
  *dest++ = out;
  return dest;
}

template <typename Vector>
static inline void EncodeUnsignedLeb128(Vector* dest, uint32_t value) {
  static_assert(std::is_same_v<typename Vector::value_type, uint8_t>, "Invalid value type");
  uint8_t out = value & 0x7f;
  value >>= 7;
  while (value != 0) {
    dest->push_back(out | 0x80);
    out = value & 0x7f;
    value >>= 7;
  }
  dest->push_back(out);
}

// Overwrite encoded Leb128 with a new value. The new value must be less than
// or equal to the old value to ensure that it fits the allocated space.
static inline void UpdateUnsignedLeb128(uint8_t* dest, uint32_t value) {
  const uint8_t* old_end = dest;
  uint32_t old_value = DecodeUnsignedLeb128(&old_end);
  DCHECK_LE(UnsignedLeb128Size(value), UnsignedLeb128Size(old_value));
  for (uint8_t* end = EncodeUnsignedLeb128(dest, value); end < old_end; end++) {
    // Use longer encoding than necessary to fill the allocated space.
    end[-1] |= 0x80;
    end[0] = 0;
  }
}

static inline uint8_t* EncodeSignedLeb128(uint8_t* dest, int32_t value) {
  uint32_t extra_bits = static_cast<uint32_t>(value ^ (value >> 31)) >> 6;
  uint8_t out = value & 0x7f;
  while (extra_bits != 0u) {
    *dest++ = out | 0x80;
    value >>= 7;
    out = value & 0x7f;
    extra_bits >>= 7;
  }
  *dest++ = out;
  return dest;
}

template<typename Vector>
static inline void EncodeSignedLeb128(Vector* dest, int32_t value) {
  static_assert(std::is_same_v<typename Vector::value_type, uint8_t>, "Invalid value type");
  uint32_t extra_bits = static_cast<uint32_t>(value ^ (value >> 31)) >> 6;
  uint8_t out = value & 0x7f;
  while (extra_bits != 0u) {
    dest->push_back(out | 0x80);
    value >>= 7;
    out = value & 0x7f;
    extra_bits >>= 7;
  }
  dest->push_back(out);
}

// An encoder that pushes int32_t/uint32_t data onto the given std::vector.
template <typename Vector = std::vector<uint8_t>>
class Leb128Encoder {
  static_assert(std::is_same_v<typename Vector::value_type, uint8_t>, "Invalid value type");

 public:
  explicit Leb128Encoder(Vector* data) : data_(data) {
    DCHECK(data != nullptr);
  }

  void Reserve(uint32_t size) {
    data_->reserve(size);
  }

  void PushBackUnsigned(uint32_t value) {
    EncodeUnsignedLeb128(data_, value);
  }

  template<typename It>
  void InsertBackUnsigned(It cur, It end) {
    for (; cur != end; ++cur) {
      PushBackUnsigned(*cur);
    }
  }

  void PushBackSigned(int32_t value) {
    EncodeSignedLeb128(data_, value);
  }

  template<typename It>
  void InsertBackSigned(It cur, It end) {
    for (; cur != end; ++cur) {
      PushBackSigned(*cur);
    }
  }

  const Vector& GetData() const {
    return *data_;
  }

 protected:
  Vector* const data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Leb128Encoder);
};

// An encoder with an API similar to vector<uint32_t> where the data is captured in ULEB128 format.
template <typename Vector = std::vector<uint8_t>>
class Leb128EncodingVector final : private Vector,
                                   public Leb128Encoder<Vector> {
  static_assert(std::is_same_v<typename Vector::value_type, uint8_t>, "Invalid value type");

 public:
  Leb128EncodingVector() : Leb128Encoder<Vector>(this) { }

  explicit Leb128EncodingVector(const typename Vector::allocator_type& alloc)
    : Vector(alloc),
      Leb128Encoder<Vector>(this) { }

 private:
  DISALLOW_COPY_AND_ASSIGN(Leb128EncodingVector);
};

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_LEB128_H_
