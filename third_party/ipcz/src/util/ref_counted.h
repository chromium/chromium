// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_UTIL_REF_COUNTED_H_
#define IPCZ_SRC_UTIL_REF_COUNTED_H_

#include <atomic>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace ipcz {

// Base class for any kind of ref-counted thing whose ownership will be shared
// by one or more Ref<T> objects.
class RefCounted {
 public:
  enum { kAdoptExistingRef };

  RefCounted();
  virtual ~RefCounted();

  void AcquireRef();
  void ReleaseRef();

 private:
  std::atomic_int ref_count_{1};
};

// Base class for every Ref<T>, providing a common implementation for ref count
// management, assignment, etc.
class GenericRef {
 public:
  constexpr GenericRef() = default;

  // Does not increase the ref count, effectively assuming ownership of a
  // previously acquired ref.
  GenericRef(decltype(RefCounted::kAdoptExistingRef), RefCounted* ptr);

  // Constructs a new reference to `ptr`, increasing its ref count by 1.
  explicit GenericRef(RefCounted* ptr);

  GenericRef(GenericRef&& other);
  GenericRef& operator=(GenericRef&& other);
  GenericRef(const GenericRef& other);
  GenericRef& operator=(const GenericRef& other);
  ~GenericRef();

  explicit operator bool() const { return ptr_ != nullptr; }

  void reset();

 protected:
  void* ReleaseImpl();

  RefCounted* ptr_ = nullptr;
};

// A smart pointer which can be used to share ownership of an instance of T,
// where T is any type derived from RefCounted above.
//
// This is used instead of std::shared_ptr so that ipcz can release and
// re-acquire ownership of individual references, as some object references may
// be conceptually owned by the embedding application via an IpczHandle across
// ipcz ABI boundary. std::shared_ptr design does not allow for such manual ref
// manipulation without additional indirection.
template <typename T>
class Ref : public GenericRef {
 public:
  constexpr Ref() = default;
  constexpr Ref(std::nullptr_t) {}
  explicit Ref(T* ptr) : GenericRef(ptr) {}
  Ref(decltype(RefCounted::kAdoptExistingRef), T* ptr)
      : GenericRef(RefCounted::kAdoptExistingRef, ptr) {}

  template <typename U>
  using EnableIfConvertible =
      typename std::enable_if<std::is_convertible<U*, T*>::value>::type;

  template <typename U, typename = EnableIfConvertible<U>>
  Ref(const Ref<U>& other) : Ref(other.get()) {}

  template <typename U, typename = EnableIfConvertible<U>>
  Ref(Ref<U>&& other) noexcept
      : Ref(RefCounted::kAdoptExistingRef, other.release()) {}

  T* get() const { return static_cast<T*>(ptr_); }
  T* operator->() const { return get(); }
  T& operator*() const { return *get(); }

  bool operator==(const T* ptr) const { return ptr_ == ptr; }
  bool operator!=(const T* ptr) const { return ptr_ != ptr; }
  bool operator==(const Ref<T>& other) const { return ptr_ == other.ptr_; }
  bool operator!=(const Ref<T>& other) const { return ptr_ != other.ptr_; }

  T* release() { return static_cast<T*>(ReleaseImpl()); }

  void swap(Ref<T>& other) noexcept { std::swap(ptr_, other.ptr_); }

  template <typename H>
  friend H AbslHashValue(H h, const Ref<T>& ref) {
    return H::combine(std::move(h), ref.get());
  }
};

// Wraps `ptr` as a Ref<T>, increasing the refcount by 1.
template <typename T>
Ref<T> WrapRefCounted(T* ptr) {
  return Ref<T>(ptr);
}

// Wraps `ptr` as a Ref<T>, assuming ownership of an existing reference. Does
// not change the object's refcount.
template <typename T>
Ref<T> AdoptRef(T* ptr) {
  return Ref<T>(RefCounted::kAdoptExistingRef, ptr);
}

template <typename T, typename... Args>
Ref<T> MakeRefCounted(Args&&... args) {
  return AdoptRef(new T(std::forward<Args>(args)...));
}

}  // namespace ipcz

#endif  // IPCZ_SRC_UTIL_REF_COUNTED_H_
