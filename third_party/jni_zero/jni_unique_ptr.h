// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IWYU pragma: private, include "third_party/jni_zero/jni_zero.h"

#ifndef JNI_ZERO_JNI_UNIQUE_PTR_H_
#define JNI_ZERO_JNI_UNIQUE_PTR_H_

#include <jni.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

namespace jni_zero {

struct DeleterBase {
  virtual ~DeleterBase() = default;
  virtual void Destroy(void* ptr) const = 0;
};

template <typename T>
struct TemplatedDeleter : public DeleterBase {
  void Destroy(void* ptr) const override { delete static_cast<T*>(ptr); }
};

template <typename T>
inline constexpr TemplatedDeleter<T> kTemplatedDeleter;

template <typename T>
inline jlong GetDeleterAddress() {
  return static_cast<jlong>(reinterpret_cast<uintptr_t>(
      static_cast<const DeleterBase*>(&kTemplatedDeleter<T>)));
}

// Corresponds to java JniUniquePtr.
// Represents a unique pointer that is owned by Java and will be deleted by Java
// calling its registered deleter via JNI.
//
// Lifecycle and Ownership Model:
// In C++, JniUniquePtr is an ephemeral transfer object constructed via
// MakeUnique<T>() immediately before crossing the JNI boundary. The generated
// CalledByNative glue code reads the deleter address and calls .release() on
// the pointer to pass both raw uintptr_t addresses (as jlong) to Java.
//
// If a JniUniquePtr is destroyed before release() has been called (e.g. when
// Java object creation fails before ownership transfer), the held object is
// deleted, as with std::unique_ptr. After release(), Java owns the object and
// destroys it via the registered deleter. In normal JNI dispatch paths,
// release() is always invoked by the generated glue code.
template <typename T>
class JniUniquePtr {
 public:
  JniUniquePtr() : ptr_(nullptr) {}
  JniUniquePtr(std::nullptr_t) : ptr_(nullptr) {}
  explicit JniUniquePtr(std::unique_ptr<T> ptr) : ptr_(std::move(ptr)) {}
  explicit JniUniquePtr(T* ptr) : ptr_(ptr) {}

  ~JniUniquePtr() = default;

  // Move-only semantics.
  JniUniquePtr(const JniUniquePtr&) = delete;
  JniUniquePtr& operator=(const JniUniquePtr&) = delete;

  JniUniquePtr(JniUniquePtr&& other) noexcept = default;
  JniUniquePtr& operator=(JniUniquePtr&& other) noexcept = default;

  T* get() const { return ptr_.get(); }
  T* release() { return ptr_.release(); }
  jlong deleter_address() const { return ptr_ ? GetDeleterAddress<T>() : 0; }

  explicit operator bool() const { return static_cast<bool>(ptr_); }
  T* operator->() const { return ptr_.get(); }
  T& operator*() const { return *ptr_; }

 private:
  std::unique_ptr<T> ptr_;
};

template <typename T>
inline JniUniquePtr<T> MakeUnique(std::unique_ptr<T> ptr) {
  return JniUniquePtr<T>(std::move(ptr));
}

template <typename T>
inline JniUniquePtr<T> MakeUnique(T* ptr) {
  return JniUniquePtr<T>(ptr);
}

}  // namespace jni_zero

#endif  // JNI_ZERO_JNI_UNIQUE_PTR_H_
