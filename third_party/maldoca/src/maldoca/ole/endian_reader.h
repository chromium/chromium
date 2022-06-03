/*
 * Copyright 2021 Google LLC
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

// Read or consume 1, 2, 4 or 8 bytes or a string from a StringPieces.
//
// Integral values of size larger than 1 byte are interpreted
// according to a chosen endianness.

#ifndef MALDOCA_OLE_ENDIAN_READER_H_
#define MALDOCA_OLE_ENDIAN_READER_H_

#include <memory>

#include "absl/strings/string_view.h"
#include "maldoca/base/endian.h"
#include "maldoca/base/logging.h"
#include "maldoca/ole/oss_utils.h"

namespace maldoca {

const int kMaxStrParseLength = 1024;

template <typename Endian>
class EndianReader {
 public:
  // Load a 1, 2, 4 or 8 bytes value (that is interpreted according to
  // the endianness in use) from a string piece at a given
  // index. Unaligned reads are allowed. True is returned upon success
  // and the read value placed in the storaged pointed at by the value
  // parameter.
  static bool LoadUInt8At(absl::string_view p, uint32_t at, uint8_t *value) {
    if (at >= p.size()) {
      DLOG(ERROR) << "Can not read uint8_t at " << at;
      return false;
    }
    *value = p[at];
    return true;
  }

  static bool LoadUInt16At(absl::string_view p, uint32_t at, uint16_t *value) {
    if (at >= p.size() - (sizeof(*value) - 1)) {
      DLOG(ERROR) << "Can not read uint16_t at " << at;
      return false;
    }
    *value = Endian::Load16(p.data() + at);
    return true;
  }

  static bool LoadUInt32At(absl::string_view p, uint32_t at, uint32_t *value) {
    if (at >= p.size() - (sizeof(*value) - 1)) {
      DLOG(ERROR) << "Can not read uint32_t at " << at;
      return false;
    }
    *value = Endian::Load32(p.data() + at);
    return true;
  }

  static bool LoadUInt64At(absl::string_view p, uint32_t at, uint64_t *value) {
    if (at >= p.size() - (sizeof(*value) - 1)) {
      DLOG(ERROR) << "Can not read uint64_t at " << at;
      return false;
    }
    *value = Endian::Load64(p.data() + at);
    return true;
  }

  // Consume and return a 1, 2, 4 or 8 bytes value (that is
  // interpreted according to the endianness in use) from a string
  // piece. True is returned upon success and the read value placed in
  // the storaged pointed at by the value parameter. After the
  // invocation, the string piece will point to the character placed
  // right after the number of bytes that were consumed to produce a
  // value of the specified type.
  static bool ConsumeUInt8(absl::string_view *p, uint8_t *value) {
    if (p->size() < sizeof(*value)) {
      DLOG(ERROR) << "Input too short to read uint8";
      return false;
    }
    *value = p->data()[0];
    p->remove_prefix(sizeof(*value));
    return true;
  }
  static bool ConsumeUInt16(absl::string_view *p, uint16_t *value) {
    if (p->size() < sizeof(*value)) {
      DLOG(ERROR) << "Input too short to read uint16";
      return false;
    }
    *value = Endian::Load16(p->data());
    p->remove_prefix(sizeof(*value));
    return true;
  }
  static bool ConsumeUInt32(absl::string_view *p, uint32_t *value) {
    if (p->size() < sizeof(*value)) {
      DLOG(ERROR) << "Input too short to read uint32";
      return false;
    }
    *value = Endian::Load32(p->data());
    p->remove_prefix(sizeof(*value));
    return true;
  }
  static bool ConsumeUInt64(absl::string_view *p, uint64_t *value) {
    if (p->size() < sizeof(*value)) {
      DLOG(ERROR) << "Input too short to read uint64";
      return false;
    }
    *value = Endian::Load64(p->data());
    p->remove_prefix(sizeof(*value));
    return true;
  }

  // Consume and return a string of a given length from a string
  // piece. True is returned upon success. After the invocation, the
  // string piece will point to the character placed right after the
  // last character read to produce the value of the required length.
  static bool ConsumeString(absl::string_view *p, uint32_t length,
                            std::string *value) {
    if (p->size() < length) {
      DLOG(ERROR) << "Input too short to read " << length << " characters";
      return false;
    }
    *value = std::string(p->substr(0, length));
    p->remove_prefix(length);
    return true;
  }

  // Consume and return the first null terminated string from a string
  // piece. True is returned upon success. After the invocation, the
  // string piece will point to the character placed right after the
  // null terminating character.
  static bool ConsumeNullTerminatedString(absl::string_view *p,
                                          std::string *value) {
    size_t length = p->find('\0');
    if (length == absl::string_view::npos || length > p->size()) {
      DLOG(ERROR) << "Can not find null terminated string in input";
      return false;
    }
    if (length > kMaxStrParseLength) {
      DLOG(ERROR) << "Null terminated string of " << length
                  << " characters over kMaxStrParseLength";
      return false;
    }
    *value = std::string(p->substr(0, length));
    p->remove_prefix(length + 1);
    return true;
  }

  // Consume and return the content reinterpreted as the template type.
  template <typename T>
  static StatusOr<const T *> Consume(absl::string_view *p) {
    if (p->size() < sizeof(T)) {
      return absl::OutOfRangeError(absl::StrCat(
          "Stream too small. Need: ", sizeof(T), ", have: ", p->size()));
    }

    const T *ptr = reinterpret_cast<const T *>(p->data());
    p->remove_prefix(sizeof(T));
    return ptr;
  }
};

using LittleEndianReader = EndianReader<utils::LittleEndian>;
using BigEndianReader = EndianReader<utils::BigEndian>;

// Stores a uint16_t in little endian order.
// Only allows retrieval in host endianness.
class LittleEndianUInt16 {
 public:
  // Implicit conversion from host endianness.
  // Usage:
  // uint16_t host = ...;
  // LittleEndianUInt16 little_endian = host;
  LittleEndianUInt16(uint16_t host)
      : little_endian_(little_endian::FromHost16(host)) {}

  // Implicit conversion to host endianness.
  // Usage:
  // LittleEndianUInt16 little_endian = ...;
  // uint16_t host = little_endian;
  // auto this_also_works = uint16_t{little_endian};
  // auto even_this_works = little_endian & 0x1;
  operator uint16_t() const { return little_endian::ToHost16(little_endian_); }

 private:
  uint16_t little_endian_;
} ABSL_ATTRIBUTE_PACKED;

static_assert(sizeof(LittleEndianUInt16) == sizeof(uint16_t),
              "LittleEndianUInt16 and uint16_t must be the same size");

// Stores a uint32_t in little endian order.
// Only allows retrieval in host endianness.
class LittleEndianUInt32 {
 public:
  LittleEndianUInt32(uint32_t host)
      : little_endian_(little_endian::FromHost32(host)) {}

  operator uint32_t() const { return little_endian::ToHost32(little_endian_); }

 private:
  uint32_t little_endian_;
} ABSL_ATTRIBUTE_PACKED;

static_assert(sizeof(LittleEndianUInt32) == sizeof(uint32_t),
              "LittleEndianUInt32 and uint32_t must be the same size");

// Stores a uint64_t in little endian order.
// Only allows retrieval in host endianness.
class LittleEndianUInt64 {
 public:
  LittleEndianUInt64(uint64_t host)
      : little_endian_(little_endian::FromHost64(host)) {}

  operator uint64_t() const { return little_endian::ToHost64(little_endian_); }

 private:
  uint64_t little_endian_;
} ABSL_ATTRIBUTE_PACKED;

static_assert(sizeof(LittleEndianUInt64) == sizeof(uint64_t),
              "LittleEndianUInt64 and uint64_t must be the same size");

// Stores a double in little endian order.
// Only allows retrieval in host endianness.
class LittleEndianDouble {
 public:
  LittleEndianDouble(double host)
      : data_(*reinterpret_cast<uint64_t *>(&host)) {}

  operator double() const {
    uint64_t host_data = data_;
    return *reinterpret_cast<double *>(&host_data);
  }

 private:
  LittleEndianUInt64 data_;
} ABSL_ATTRIBUTE_PACKED;

static_assert(sizeof(double) == sizeof(uint64_t),
              "double and uint64_t must be the same size");

// Decode a UTF-16 string into a UTF-8 string.
void DecodeUTF16(absl::string_view input, std::string *output);

}  // namespace maldoca

#endif  // MALDOCA_OLE_ENDIAN_READER_H_
