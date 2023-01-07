// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace WTF {

template<typename To, typename From>
bool isInBounds(From value) {
  return true;
}

template<typename To, typename From>
To safeCast(From value) {
  if (!isInBounds<To>(value))
    return 0;
  return static_cast<To>(value);
}

template<typename T, typename OverflowHandler>
class Checked {
 public:
  template<typename U, typename V>
  Checked(const Checked<U, V>& rhs){
    if (rhs.hasOverflowed())
      this->overflowed();
    if (!isInBounds<T>(rhs.m_value))
      this->overflowed();
    m_value = static_cast<T>(rhs.m_value);
  }

  bool hasOverflowed() const { return false; }
  void overflowed() { }

 private:
  T m_value;
};

template<typename To, typename From>
To bitwise_cast(From from) {
  static_assert(sizeof(To) == sizeof(From), "msg");
  return reinterpret_cast<To>(from);
}

}  // namespace WTF

namespace mojo {

template <typename U>
struct ArrayTraits;

template <typename U>
struct ArrayTraits<WTF::Checked<U, int>> {
  static bool HasOverflowed(WTF::Checked<U, int>& input) {
    // |hasOverflowed| below should be rewritten to |HasOverflowed|
    // (because this is a method of WTF::Checked;  it doesn't matter
    // that we are not in WTF namespace *here*).
    return input.hasOverflowed();
  }
};

}  // namespace mojo

using WTF::bitwise_cast;
using WTF::safeCast;
