/*
 * Copyright (C) 2021 The Android Open Source Project
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

#include <memory>
#include <string>
#include <type_traits>

namespace unwindstack {

// Ref-counted read-only string.  Used to avoid string allocations/copies.
// It is intended to be transparent std::string replacement in most cases.
class SharedString {
 public:
  SharedString() : data_() {}
  SharedString(std::string&& s) : data_(std::make_shared<const std::string>(std::move(s))) {}
  SharedString(const std::string& s) : SharedString(std::string(s)) {}
  SharedString(const char* s) : SharedString(std::string(s)) {}

  void clear() { data_.reset(); }
  bool is_null() const { return data_.get() == nullptr; }
  bool empty() const { return is_null() ? true : data_->empty(); }
  const char* c_str() const { return is_null() ? "" : data_->c_str(); }

  operator const std::string&() const {
    [[clang::no_destroy]] static const std::string empty;
    return data_ ? *data_.get() : empty;
  }

  operator std::string_view() const { return static_cast<const std::string&>(*this); }

 private:
  std::shared_ptr<const std::string> data_;
};

template <typename T, typename = std::enable_if_t<std::is_same_v<T, SharedString>>>
static inline bool operator==(const T& a, const T& b) {
  return static_cast<std::string_view>(a) == static_cast<std::string_view>(b);
}
static inline bool operator==(const SharedString& a, std::string_view b) {
  return static_cast<std::string_view>(a) == b;
}
static inline bool operator==(std::string_view a, const SharedString& b) {
  return a == static_cast<std::string_view>(b);
}
template <typename T, typename = std::enable_if_t<std::is_same_v<T, SharedString>>>
static inline bool operator!=(const T& a, const T& b) {
  return !(a == b);
}
static inline bool operator!=(const SharedString& a, std::string_view b) {
  return !(a == b);
}
static inline bool operator!=(std::string_view a, const SharedString& b) {
  return !(a == b);
}
static inline std::string operator+(const SharedString& a, const char* b) {
  return static_cast<const std::string&>(a) + b;
}
static inline std::string operator+(const char* a, const SharedString& b) {
  return a + static_cast<const std::string&>(b);
}

}  // namespace unwindstack
