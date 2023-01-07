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

#include "minidump/test/minidump_string_writer_test_util.h"

#include <sys/types.h>

#include "gtest/gtest.h"
#include "minidump/minidump_extensions.h"
#include "minidump/test/minidump_writable_test_util.h"

namespace crashpad {
namespace test {

namespace {

template <
    typename T,
    typename RVAType = RVA,
    typename MinidumpLocationDescriptorType = MINIDUMP_LOCATION_DESCRIPTOR>
const T* TMinidumpStringAtRVA(const std::string& file_contents, RVAType rva) {
  const T* string_base = MinidumpWritableAtRVA<T>(file_contents, rva);
  if (!string_base) {
    return nullptr;
  }

  // |Length| must indicate the ability to store an integral number of code
  // units.
  const size_t kCodeUnitSize = sizeof(string_base->Buffer[0]);
  if (string_base->Length % kCodeUnitSize != 0) {
    EXPECT_EQ(string_base->Length % kCodeUnitSize, 0u);
    return nullptr;
  }

  // |Length| does not include space for the required NUL terminator.
  MinidumpLocationDescriptorType location;
  location.DataSize =
      sizeof(*string_base) + string_base->Length + kCodeUnitSize;
  location.Rva = rva;
  const T* string =
      MinidumpWritableAtLocationDescriptor<T>(file_contents, location);
  if (!string) {
    return nullptr;
  }

  EXPECT_EQ(string, string_base);

  // Require the NUL terminator to be NUL.
  if (string->Buffer[string->Length / kCodeUnitSize] != '\0') {
    EXPECT_EQ(string->Buffer[string->Length / kCodeUnitSize], '\0');
    return nullptr;
  }

  return string;
}

template <typename StringType,
          typename MinidumpStringType,
          typename RVAType,
          typename MinidumpLocationDescriptorType>
StringType TMinidumpStringAtRVAAsString(const std::string& file_contents,
                                        RVAType rva) {
  const MinidumpStringType* minidump_string =
      TMinidumpStringAtRVA<MinidumpStringType,
                           RVAType,
                           MinidumpLocationDescriptorType>(file_contents, rva);
  if (!minidump_string) {
    return StringType();
  }

  StringType minidump_string_data(
      reinterpret_cast<const typename StringType::value_type*>(
          &minidump_string->Buffer[0]),
      minidump_string->Length / sizeof(minidump_string->Buffer[0]));
  return minidump_string_data;
}

}  // namespace

const MINIDUMP_STRING* MinidumpStringAtRVA(const std::string& file_contents,
                                           RVA rva) {
  return TMinidumpStringAtRVA<MINIDUMP_STRING,
                              RVA,
                              MINIDUMP_LOCATION_DESCRIPTOR>(file_contents, rva);
}

const MINIDUMP_STRING* MinidumpStringAtRVA(const std::string& file_contents,
                                           RVA64 rva) {
  return TMinidumpStringAtRVA<MINIDUMP_STRING,
                              RVA64,
                              MINIDUMP_LOCATION_DESCRIPTOR64>(file_contents,
                                                              rva);
}

const MinidumpUTF8String* MinidumpUTF8StringAtRVA(
    const std::string& file_contents,
    RVA rva) {
  return TMinidumpStringAtRVA<MinidumpUTF8String,
                              RVA,
                              MINIDUMP_LOCATION_DESCRIPTOR>(file_contents, rva);
}

const MinidumpUTF8String* MinidumpUTF8StringAtRVA(
    const std::string& file_contents,
    RVA64 rva) {
  return TMinidumpStringAtRVA<MinidumpUTF8String,
                              RVA64,
                              MINIDUMP_LOCATION_DESCRIPTOR64>(file_contents,
                                                              rva);
}

std::u16string MinidumpStringAtRVAAsString(const std::string& file_contents,
                                           RVA rva) {
  return TMinidumpStringAtRVAAsString<std::u16string,
                                      MINIDUMP_STRING,
                                      RVA,
                                      MINIDUMP_LOCATION_DESCRIPTOR>(
      file_contents, rva);
}

std::u16string MinidumpStringAtRVAAsString(const std::string& file_contents,
                                           RVA64 rva) {
  return TMinidumpStringAtRVAAsString<std::u16string,
                                      MINIDUMP_STRING,
                                      RVA64,
                                      MINIDUMP_LOCATION_DESCRIPTOR64>(
      file_contents, rva);
}

std::string MinidumpUTF8StringAtRVAAsString(const std::string& file_contents,
                                            RVA rva) {
  return TMinidumpStringAtRVAAsString<std::string,
                                      MinidumpUTF8String,
                                      RVA,
                                      MINIDUMP_LOCATION_DESCRIPTOR>(
      file_contents, rva);
}

std::string MinidumpUTF8StringAtRVAAsString(const std::string& file_contents,
                                            RVA64 rva) {
  return TMinidumpStringAtRVAAsString<std::string,
                                      MinidumpUTF8String,
                                      RVA64,
                                      MINIDUMP_LOCATION_DESCRIPTOR64>(
      file_contents, rva);
}

}  // namespace test
}  // namespace crashpad
