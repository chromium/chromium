// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include "ui/qt/qt_interface.h"

#include <cstdlib>
#include <cstring>

namespace qt {

String::String() = default;

String::String(const char* str) {
  if (str)
    str_ = strdup(str);
}

String::String(String&& other) {
  str_ = other.str_;
  other.str_ = nullptr;
}

String& String::operator=(String&& other) {
  free(str_);
  str_ = other.str_;
  other.str_ = nullptr;
  return *this;
}

String::~String() {
  free(str_);
}

Buffer::Buffer() = default;

Buffer::Buffer(const uint8_t* data, size_t size)
    : data_(static_cast<uint8_t*>(malloc(size))), size_(size) {
  memcpy(data_, data, size);
}

Buffer::Buffer(Buffer&& other) {
  data_ = other.data_;
  size_ = other.size_;
  other.data_ = nullptr;
  other.size_ = 0;
}

Buffer& Buffer::operator=(Buffer&& other) {
  free(data_);
  data_ = other.data_;
  size_ = other.size_;
  other.data_ = nullptr;
  other.size_ = 0;
  return *this;
}

Buffer::~Buffer() {
  free(data_);
}

uint8_t* Buffer::Take() {
  uint8_t* data = data_;
  data_ = nullptr;
  size_ = 0;
  return data;
}

}  // namespace qt
