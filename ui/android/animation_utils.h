// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_ANIMATION_UTILS_H_
#define UI_ANDROID_ANIMATION_UTILS_H_

namespace ui {

template <typename T>
T Lerp(T a, T b, T t) {
  return a + (b - a) * t;
}

template <typename T>
T Clamp(T value, T low, T high) {
  return value < low ? low : (value > high ? high : value);
}

template <typename T>
T Damp(T input, T factor) {
  T result;
  if (factor == 1) {
    result = 1 - (1 - input) * (1 - input);
  } else {
    result = 1 - std::pow(1 - input, 2 * factor);
  }
  return result;
}

}  // namespace ui

#endif  // UI_ANDROID_ANIMATION_UTILS_H_
