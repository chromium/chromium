// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_ANDROID_SCOPED_A_NATIVE_WINDOW_H_
#define UI_GL_ANDROID_SCOPED_A_NATIVE_WINDOW_H_

#include <cstddef>

#include "base/memory/raw_ptr_exclusion.h"
#include "ui/gl/gl_export.h"

struct ANativeWindow;

namespace gl {

class ScopedJavaSurface;

class GL_EXPORT ScopedANativeWindow {
 public:
  // Wraps a native window, and increments its reference count.
  static ScopedANativeWindow Wrap(ANativeWindow* a_native_window);

  // Adopts a native window, taking ownership of it without incrementing the
  // reference count.
  static ScopedANativeWindow Adopt(ANativeWindow* a_native_window);

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

  // RAW_PTR_EXCLUSION: This pointer is managed by the android API, and is to
  // a reference-counted object. It can't dangle because the ref counting
  // enforces that it isn't freed until the pointer is released, and we null
  // it immediately afterwards.
  RAW_PTR_EXCLUSION ANativeWindow* a_native_window_ = nullptr;
};

}  // namespace gl

#endif  // UI_GL_ANDROID_SCOPED_A_NATIVE_WINDOW_H_
