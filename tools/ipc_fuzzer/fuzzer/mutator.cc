// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "tools/ipc_fuzzer/fuzzer/mutator.h"
#include "tools/ipc_fuzzer/fuzzer/rand_util.h"

namespace ipc_fuzzer {

template <typename T>
void FuzzIntegralType(T* value, unsigned int frequency) {
  if (RandEvent(frequency)) {
    switch (RandInRange(4)) {
      case 0: (*value) = 0; break;
      case 1: (*value)--; break;
      case 2: (*value)++; break;
      case 3: (*value) = RandU64(); break;
    }
  }
}

template <typename T>
void FuzzStringType(T* value, unsigned int frequency,
                    const T& literal1, const T& literal2) {
  if (RandEvent(frequency)) {
    switch (RandInRange(5)) {
      case 4: (*value) = (*value) + (*value); [[fallthrough]];
      case 3: (*value) = (*value) + (*value); [[fallthrough]];
      case 2: (*value) = (*value) + (*value); break;
      case 1: (*value) += literal1; break;
      case 0: (*value) = literal2; break;
    }
  }
}

void Mutator::FuzzBool(bool* value) {
  if (RandEvent(frequency_))
    (*value) = !(*value);
}

void Mutator::FuzzInt(int* value) {
  FuzzIntegralType<int>(value, frequency_);
}

void Mutator::FuzzLong(long* value) {
  FuzzIntegralType<long>(value, frequency_);
}

void Mutator::FuzzSize(size_t* value) {
  FuzzIntegralType<size_t>(value, frequency_);
}

void Mutator::FuzzUChar(unsigned char* value) {
  FuzzIntegralType<unsigned char>(value, frequency_);
}

void Mutator::FuzzWChar(wchar_t* value) {
  FuzzIntegralType<wchar_t>(value, frequency_);
}

void Mutator::FuzzUInt16(uint16_t* value) {
  FuzzIntegralType<uint16_t>(value, frequency_);
}

void Mutator::FuzzUInt32(uint32_t* value) {
  FuzzIntegralType<uint32_t>(value, frequency_);
}

void Mutator::FuzzInt64(int64_t* value) {
  FuzzIntegralType<int64_t>(value, frequency_);
}

void Mutator::FuzzUInt64(uint64_t* value) {
  FuzzIntegralType<uint64_t>(value, frequency_);
}

void Mutator::FuzzFloat(float* value) {
  if (RandEvent(frequency_))
    *value = RandDouble();
}

void Mutator::FuzzDouble(double* value) {
  if (RandEvent(frequency_))
    *value = RandDouble();
}

void Mutator:: FuzzString(std::string* value) {
  FuzzStringType<std::string>(value, frequency_, "BORKED", std::string());
}

void Mutator::FuzzString16(std::u16string* value) {
  FuzzStringType<std::u16string>(value, frequency_, u"BORKED", u"");
}

void Mutator::FuzzData(char* data, int length) {
  if (RandEvent(frequency_)) {
    for (int i = 0; i < length; ++i) {
      FuzzIntegralType<char>(&data[i], frequency_);
    }
  }
}

void Mutator::FuzzBytes(void* data, int data_len) {
  FuzzData(static_cast<char*>(data), data_len);
}

bool Mutator::ShouldGenerate() {
  // TODO(mbarbella): With a low probability, allow something to be fully
  // rewritten while mutating instead of always changing the existing value.
  return false;
}

}  // namespace ipc_fuzzer
