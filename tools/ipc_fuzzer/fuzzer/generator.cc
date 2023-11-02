// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/string_util.h"
#include "tools/ipc_fuzzer/fuzzer/generator.h"
#include "tools/ipc_fuzzer/fuzzer/rand_util.h"

namespace ipc_fuzzer {

template <typename T>
void GenerateIntegralType(T* value) {
  switch (RandInRange(16)) {
    case 0:
      *value = static_cast<T>(0);
      break;
    case 1:
      *value = static_cast<T>(1);
      break;
    case 2:
      *value = static_cast<T>(-1);
      break;
    case 3:
      *value = static_cast<T>(2);
      break;
    default:
      *value = static_cast<T>(RandU64());
      break;
  }
}

template <typename T>
void GenerateFloatingType(T* value) {
  *value = RandDouble();
}

template <typename T>
void GenerateStringType(T* value) {
  T temp_string;
  size_t length = RandInRange(300);
  for (size_t i = 0; i < length; ++i)
    temp_string += RandInRange(256);
  *value = temp_string;
}

void Generator::FuzzBool(bool* value) {
  *value = RandInRange(2) ? true: false;
}

void Generator::FuzzInt(int* value) {
  GenerateIntegralType<int>(value);
}

void Generator::FuzzLong(long* value) {
  GenerateIntegralType<long>(value);
}

void Generator::FuzzSize(size_t* value) {
  GenerateIntegralType<size_t>(value);
}

void Generator::FuzzUChar(unsigned char* value) {
  GenerateIntegralType<unsigned char>(value);
}

void Generator::FuzzWChar(wchar_t* value) {
  GenerateIntegralType<wchar_t>(value);
}

void Generator::FuzzUInt16(uint16_t* value) {
  GenerateIntegralType<uint16_t>(value);
}

void Generator::FuzzUInt32(uint32_t* value) {
  GenerateIntegralType<uint32_t>(value);
}

void Generator::FuzzInt64(int64_t* value) {
  GenerateIntegralType<int64_t>(value);
}

void Generator::FuzzUInt64(uint64_t* value) {
  GenerateIntegralType<uint64_t>(value);
}

void Generator::FuzzFloat(float* value) {
  GenerateFloatingType<float>(value);
}

void Generator::FuzzDouble(double* value) {
  GenerateFloatingType<double>(value);
}

void Generator::FuzzString(std::string* value) {
  GenerateStringType<std::string>(value);
}

void Generator::FuzzString16(std::u16string* value) {
  GenerateStringType<std::u16string>(value);
}

void Generator::FuzzData(char* data, int length) {
  for (int i = 0; i < length; ++i) {
    GenerateIntegralType<char>(&data[i]);
  }
}

void Generator::FuzzBytes(void* data, int data_len) {
  FuzzData(static_cast<char*>(data), data_len);
}

bool Generator::ShouldGenerate() {
  // The generator fuzzer should always generate new values.
  return true;
}

}  // namespace ipc_fuzzer
