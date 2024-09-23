// Copyright 2014 The Crashpad Authors
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

#if !defined(__STDC_FORMAT_MACROS)
#define __STDC_FORMAT_MACROS
#endif

#include "util/misc/uuid.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <string_view>
#include <type_traits>

#include "base/containers/span.h"
#include "base/numerics/byte_conversions.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE)
#include <uuid/uuid.h>
#endif  // BUILDFLAG(IS_APPLE)

namespace crashpad {

static_assert(sizeof(UUID) == 16, "UUID must be 16 bytes");
static_assert(std::is_standard_layout<UUID>::value,
              "UUID must be a standard-layout type");
static_assert(std::is_trivial<UUID>::value, "UUID must be a trivial type");

bool UUID::operator==(const UUID& that) const {
  return memcmp(this, &that, sizeof(*this)) == 0;
}

bool UUID::operator<(const UUID& that) const {
  return memcmp(this, &that, sizeof(*this)) < 0;
}

void UUID::InitializeToZero() {
  memset(this, 0, sizeof(*this));
}

void UUID::InitializeFromBytes(const uint8_t* bytes_ptr) {
  // TODO(crbug.com/40284755): This span construction is unsound. The caller
  // should provide a span instead of an unbounded pointer.
  base::span<const uint8_t, sizeof(UUID)> bytes(bytes_ptr, sizeof(UUID));
  data_1 = base::numerics::U32FromBigEndian(bytes.subspan<0u, 4u>());
  data_2 = base::numerics::U16FromBigEndian(bytes.subspan<4u, 2u>());
  data_3 = base::numerics::U16FromBigEndian(bytes.subspan<6u, 2u>());
  std::ranges::copy(bytes.subspan<8u, 2u>(), data_4);
  std::ranges::copy(bytes.subspan<10u, 6u>(), data_5);
}

bool UUID::InitializeFromString(std::string_view string) {
  if (string.length() != 36)
    return false;

  UUID temp;
  static constexpr char kScanFormat[] =
      "%08" SCNx32 "-%04" SCNx16 "-%04" SCNx16
      "-%02" SCNx8 "%02" SCNx8
      "-%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "%02" SCNx8;
  int rv = sscanf(string.data(),
                  kScanFormat,
                  &temp.data_1,
                  &temp.data_2,
                  &temp.data_3,
                  &temp.data_4[0],
                  &temp.data_4[1],
                  &temp.data_5[0],
                  &temp.data_5[1],
                  &temp.data_5[2],
                  &temp.data_5[3],
                  &temp.data_5[4],
                  &temp.data_5[5]);
  if (rv != 11)
    return false;

  *this = temp;
  return true;
}

#if BUILDFLAG(IS_WIN)
bool UUID::InitializeFromString(std::wstring_view string) {
  return InitializeFromString(base::WideToUTF8(string));
}
#endif

bool UUID::InitializeWithNew() {
#if BUILDFLAG(IS_APPLE)
  uuid_t uuid;
  uuid_generate(uuid);
  InitializeFromBytes(uuid);
  return true;
#elif BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
  // Linux, Android, and Fuchsia do not provide a UUID generator in a
  // widely-available system library. On Linux and Android, uuid_generate()
  // from libuuid is not available everywhere.
  // On Windows, do not use UuidCreate() to avoid a dependency on rpcrt4, so
  // that this function is usable early in DllMain().
  base::RandBytes(base::byte_span_from_ref(*this));

  // Set six bits per RFC 4122 §4.4 to identify this as a pseudo-random UUID.
  data_3 = (4 << 12) | (data_3 & 0x0fff);  // §4.1.3
  data_4[0] = 0x80 | (data_4[0] & 0x3f);  // §4.1.1

  return true;
#else
#error Port.
#endif  // BUILDFLAG(IS_APPLE)
}

#if BUILDFLAG(IS_WIN)
void UUID::InitializeFromSystemUUID(const ::UUID* system_uuid) {
  static_assert(sizeof(::UUID) == sizeof(UUID),
                "unexpected system uuid size");
  static_assert(offsetof(::UUID, Data1) == offsetof(UUID, data_1),
                "unexpected system uuid layout");
  memcpy(this, system_uuid, sizeof(*this));
}
#endif  // BUILDFLAG(IS_WIN)

std::string UUID::ToString() const {
  return base::StringPrintf("%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                            data_1,
                            data_2,
                            data_3,
                            data_4[0],
                            data_4[1],
                            data_5[0],
                            data_5[1],
                            data_5[2],
                            data_5[3],
                            data_5[4],
                            data_5[5]);
}

#if BUILDFLAG(IS_WIN)
std::wstring UUID::ToWString() const {
  return base::UTF8ToWide(ToString());
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace crashpad
