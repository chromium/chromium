// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_ANDROID_SCOPED_A_NATIVE_WINDOW_H_
#define UI_GL_ANDROID_SCOPED_A_NATIVE_WINDOW_H_

#include <cstddef>

#include "ui/gl/gl_export.h"

struct ANativeWindow;

namespace gl {

class ScopedJavaSurface;

class GL_EXPORT ScopedANativeWindow {
 public:
  static ScopedANativeWindow Wrap(ANativeWindow* a_native_window);
  constexpr ScopedANativeWindow() = default;
  constexpr ScopedANativeWindow(std::nullptr_t) {}
  explicit ScopedANativeWindow(const ScopedJavaSurface& surface);
  ~ScopedANativeWindow();

  ScopedANativeWindow(ScopedANativeWindow&& other);
  ScopedANativeWindow& operator=(ScopedANativeWindow&& other);

  // Move only type.
  ScopedANativeWindow(const ScopedANativeWindow&) = delete;
  ScopedANativeWindow& operator=(const ScopedANativeWindow&) = delete;

  explicit operator bool() const { return !!a_native_window_; }

  ANativeWindow* a_native_window() const { return a_native_window_; }

 private:
  explicit ScopedANativeWindow(ANativeWindow* a_native_window);

  void DestroyIfNeeded();

  ANativeWindow* a_native_window_ = nullptr;
};

}  // namespace gl

#endif  // UI_GL_ANDROID_SCOPED_A_NATIVE_WINDOW_H_
