// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IWYU pragma: private, include "third_party/jni_zero/jni_zero.h"

#ifndef JNI_ZERO_JNI_RAW_PTR_H_
#define JNI_ZERO_JNI_RAW_PTR_H_

#include <cstddef>

namespace jni_zero {

// Corresponds to Java org.jni_zero.JniRawPtr.
//
// A way to pass a raw_ptr<> to Java without losing BackupRefPtr protection.
//
// Mental Model & Ownership:
// - C++ owns the object's lifetime; Java holds a non-owning borrowed reference.
// - Handing a pointer to Java acquires a PartitionAlloc BackupRefPtr (BRP)
//   quarantine reference. Java MUST call `release()` when finished to drop it.
// - BRP quarantine prevents memory slot reuse after C++ deletion; it does NOT
//   extend object lifetime or delay the C++ destructor.
//
// When to Use vs. Alternatives:
// - USE JniRawPtr<T>: In JNI boundary signatures (@NativeMethods return,
//   @CalledByNative params) when handing a borrowed reference to Java that Java
//   stores/retains across multiple calls.
// - DO NOT USE for ephemeral calls: Use JniPtr<T> in Java (which maps to plain
//   T* in C++) for single, stack-scoped JNI invocations where Java does not
//   retain the pointer.
// - DO NOT USE for Java-owned objects: Use jni_zero::JniUniquePtr<T>.
// - DO NOT USE for C++ members: Use base::raw_ptr<T> for C++ class fields.
//
// Example:
//   // C++ export
//   jni_zero::JniRawPtr<MyClass> Bridge::GetMyClass() {
//     return jni_zero::MakeRaw(my_class_.get());
//   }
template <typename T>
class JniRawPtr {
 public:
  constexpr JniRawPtr() : ptr_(nullptr) {}
  constexpr JniRawPtr(std::nullptr_t) : ptr_(nullptr) {}
  explicit JniRawPtr(T* ptr) : ptr_(ptr) {}

  // Allow copy and move.
  JniRawPtr(const JniRawPtr&) = default;
  JniRawPtr& operator=(const JniRawPtr&) = default;
  JniRawPtr(JniRawPtr&&) noexcept = default;
  JniRawPtr& operator=(JniRawPtr&&) noexcept = default;

  T* get() const { return ptr_; }
  T* operator->() const { return ptr_; }
  T& operator*() const { return *ptr_; }
  explicit operator bool() const { return ptr_ != nullptr; }

 private:
  T* ptr_;
};

template <typename T>
inline JniRawPtr<T> MakeRaw(T* ptr) {
  return JniRawPtr<T>(ptr);
}

}  // namespace jni_zero

#endif  // JNI_ZERO_JNI_RAW_PTR_H_
