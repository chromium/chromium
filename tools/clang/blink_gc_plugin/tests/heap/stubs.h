// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEAP_STUBS_H_
#define HEAP_STUBS_H_

#include <stddef.h>
#include <stdint.h>

namespace WTF {

template<typename T> class RefCounted { };

template<typename T> class RawPtr {
public:
    operator T*() const { return 0; }
    T* operator->() const { return 0; }
};

template<typename T> class scoped_refptr {
public:
    ~scoped_refptr() { }
    operator T*() const { return 0; }
    T* operator->() const { return 0; }
};

template<typename T> class WeakPtr {
public:
    ~WeakPtr() { }
    operator T*() const { return 0; }
    T* operator->() const { return 0; }
};

class DefaultAllocator {
public:
    static const bool isGarbageCollected = false;
};

template <typename T,
          size_t inlineCapacity = 0,
          typename Allocator = DefaultAllocator>
class Vector {
 public:
  using iterator = T*;
  using const_iterator = const T*;
  using reverse_iterator = T*;
  using const_reverse_iterator = const T*;

  size_t size();
  T& operator[](size_t);

  ~Vector() {}
};

template <typename T,
          size_t inlineCapacity = 0,
          typename Allocator = DefaultAllocator>
class Deque {
 public:
  using iterator = T*;
  using const_iterator = const T*;
  using reverse_iterator = T*;
  using const_reverse_iterator = const T*;

  ~Deque() {}
};

template <typename ValueArg,
          typename HashArg = void,
          typename TraitsArg = void,
          typename Allocator = DefaultAllocator>
class HashSet {
 public:
  typedef ValueArg* iterator;
  typedef const ValueArg* const_iterator;
  typedef ValueArg* reverse_iterator;
  typedef const ValueArg* const_reverse_iterator;

  ~HashSet() {}
};

template <typename ValueArg,
          typename TraitsArg = void,
          typename Allocator = DefaultAllocator>
class LinkedHashSet {
 public:
  typedef ValueArg* iterator;
  typedef const ValueArg* const_iterator;
  typedef ValueArg* reverse_iterator;
  typedef const ValueArg* const_reverse_iterator;

  ~LinkedHashSet() {}
};

template <typename ValueArg,
          typename HashArg = void,
          typename TraitsArg = void,
          typename Allocator = DefaultAllocator>
class HashCountedSet {
 public:
  ~HashCountedSet() {}
};

template <typename KeyArg,
          typename MappedArg,
          typename HashArg = void,
          typename KeyTraitsArg = void,
          typename MappedTraitsArg = void,
          typename Allocator = DefaultAllocator>
class HashMap {
 public:
  typedef MappedArg* iterator;
  typedef const MappedArg* const_iterator;
  typedef MappedArg* reverse_iterator;
  typedef const MappedArg* const_reverse_iterator;

  ~HashMap() {}
};
}

// Empty namespace declaration to exercise internal
// handling of namespace equality.
namespace std {
  /* empty */
}

namespace std {

template<typename T> class unique_ptr {
public:
    ~unique_ptr() { }
    operator T*() const { return 0; }
    T* operator->() const { return 0; }
};

template <typename T, typename... Args>
unique_ptr<T> make_unique(Args&&... args) {
  return unique_ptr<T>();
}

}  // namespace std

namespace base {

template <typename T>
std::unique_ptr<T> WrapUnique(T* ptr) {
  return std::unique_ptr<T>();
}

}  // namespace base

namespace absl {

template <typename T>
class optional {};

template <class... Ts>
class variant {};

}  // namespace absl

#if defined(USE_V8_OILPAN)

namespace cppgc {

class Visitor {
 public:
  template <typename T, void (T::*method)(Visitor*)>
  void RegisterWeakMembers(const T* obj);

  template <typename T>
  void Trace(const T&);
};

namespace internal {
class StrongMemberTag;
class WeakMemberTag;

class MemberBase {};

template <typename T, typename Tag>
class BasicMember : public MemberBase {
 public:
  operator T*() const { return 0; }
  T* operator->() const { return 0; }
  bool operator!() const { return false; }
};

class StrongPersistentPolicy;
class WeakPersistentPolicy;

class PersistentBase {};

template <typename T, typename Tag>
class BasicPersistent : public PersistentBase {
 public:
  operator T*() const { return 0; }
  T* operator->() const { return 0; }
  bool operator!() const { return false; }
};

class StrongCrossThreadPersistentPolicy;
class WeakCrossThreadPersistentPolicy;

template <typename T, typename Tag>
class BasicCrossThreadPersistent : public PersistentBase {
 public:
  operator T*() const { return 0; }
  T* operator->() const { return 0; }
  bool operator!() const { return false; }
};

}  // namespace internal

template <typename T>
class GarbageCollected {
 public:
  void* operator new(size_t, void* location) { return location; }

 private:
  void* operator new(size_t) = delete;
  void* operator new[](size_t) = delete;
};

template <typename T, typename... Args>
T* MakeGarbageCollected(int, Args&&... args) {
  return new (reinterpret_cast<void*>(0x87654321)) T(args...);
}

class GarbageCollectedMixin {
 public:
  virtual void AdjustAndMark(Visitor*) const = 0;
  virtual bool IsHeapObjectAlive(Visitor*) const = 0;
  virtual void Trace(Visitor*) const {}
};

template <typename T>
using Member = internal::BasicMember<T, internal::StrongMemberTag>;
template <typename T>
using WeakMember = internal::BasicMember<T, internal::WeakMemberTag>;

template <typename T>
using Persistent =
    internal::BasicPersistent<T, internal::StrongPersistentPolicy>;
template <typename T>
using WeakPersistent =
    internal::BasicPersistent<T, internal::WeakPersistentPolicy>;

namespace subtle {

template <typename T>
using CrossThreadPersistent = internal::
    BasicCrossThreadPersistent<T, internal::StrongCrossThreadPersistentPolicy>;
template <typename T>
using CrossThreadWeakPersistent = internal::
    BasicCrossThreadPersistent<T, internal::WeakCrossThreadPersistentPolicy>;

}  // namespace subtle

}  // namespace cppgc

namespace blink {

using Visitor = cppgc::Visitor;

template <typename T>
using GarbageCollected = cppgc::GarbageCollected<T>;
template <typename T, typename... Args>
T* MakeGarbageCollected(Args&&... args) {
  return cppgc::MakeGarbageCollected<T>(0, args...);
}

using GarbageCollectedMixin = cppgc::GarbageCollectedMixin;

template <typename T>
using Member = cppgc::Member<T>;
template <typename T>
using WeakMember = cppgc::WeakMember<T>;
template <typename T>
using Persistent = cppgc::Persistent<T>;
template <typename T>
using WeakPersistent = cppgc::WeakPersistent<T>;
template <typename T>
using CrossThreadPersistent = cppgc::subtle::CrossThreadPersistent<T>;
template <typename T>
using CrossThreadWeakPersistent = cppgc::subtle::CrossThreadWeakPersistent<T>;

#else  // !defined(USE_V8_OILPAN)

namespace blink {

class Visitor {
 public:
  template <typename T, void (T::*method)(Visitor*)>
  void RegisterWeakMembers(const T* obj);

  template <typename T>
  void Trace(const T&);
};

template <typename T>
class GarbageCollected {
 public:
  void* operator new(size_t, void* location) { return location; }

 private:
  void* operator new(size_t) = delete;
  void* operator new[](size_t) = delete;
};

template <typename T, typename... Args>
T* MakeGarbageCollected(Args&&... args) {
  return new (reinterpret_cast<void*>(0x87654321)) T(args...);
}

class GarbageCollectedMixin {
 public:
  virtual void AdjustAndMark(Visitor*) const = 0;
  virtual bool IsHeapObjectAlive(Visitor*) const = 0;
  virtual void Trace(Visitor*) const {}
};

template<typename T> class Member {
public:
    operator T*() const { return 0; }
    T* operator->() const { return 0; }
    bool operator!() const { return false; }

   private:
    uint32_t compressed;
};

template<typename T> class WeakMember {
public:
    operator T*() const { return 0; }
    T* operator->() const { return 0; }
    bool operator!() const { return false; }
};

template<typename T> class Persistent {
public:
    operator T*() const { return 0; }
    T* operator->() const { return 0; }
    bool operator!() const { return false; }
};

template<typename T> class WeakPersistent {
public:
    operator T*() const { return 0; }
    T* operator->() const { return 0; }
    bool operator!() const { return false; }
};

template<typename T> class CrossThreadPersistent {
public:
    operator T*() const { return 0; }
    T* operator->() const { return 0; }
    bool operator!() const { return false; }
};

template<typename T> class CrossThreadWeakPersistent {
public:
    operator T*() const { return 0; }
    T* operator->() const { return 0; }
    bool operator!() const { return false; }
};

#endif  // !defined(USE_V8_OILPAN)

using namespace WTF;

#define DISALLOW_NEW()                                            \
 public:                                                          \
  void* operator new(size_t, void* location) { return location; } \
                                                                  \
 private:                                                         \
  void* operator new(size_t) = delete

#define STACK_ALLOCATED()                                  \
 public:                                                   \
  using IsStackAllocatedTypeMarker [[maybe_unused]] = int; \
                                                           \
 private:                                                  \
  void* operator new(size_t) = delete;                     \
  void* operator new(size_t, void*) = delete

#define GC_PLUGIN_IGNORE(bug) \
  __attribute__((annotate("blink_gc_plugin_ignore")))

template <typename T>
class RefCountedGarbageCollected : public GarbageCollected<T> {};

template <typename T>
class TraceWrapperV8Reference {
 public:
  operator T*() const { return 0; }
  T* operator->() const { return 0; }
  bool operator!() const { return false; }
};

class HeapAllocator {
public:
    static const bool isGarbageCollected = true;
};

template <typename T, size_t inlineCapacity = 0>
class HeapVector : public GarbageCollected<HeapVector<T, inlineCapacity>>,
                   public Vector<T, inlineCapacity, HeapAllocator> {};

template <typename T, size_t inlineCapacity = 0>
class HeapDeque : public GarbageCollected<HeapDeque<T, inlineCapacity>>,
                  public Vector<T, inlineCapacity, HeapAllocator> {};

template <typename T>
class HeapHashSet : public GarbageCollected<HeapHashSet<T>>,
                    public HashSet<T, void, void, HeapAllocator> {};

template <typename T>
class HeapLinkedHashSet : public GarbageCollected<HeapLinkedHashSet<T>>,
                          public LinkedHashSet<T, void, HeapAllocator> {};

template <typename T>
class HeapHashCountedSet : public GarbageCollected<HeapHashCountedSet<T>>,
                           public HashCountedSet<T, void, void, HeapAllocator> {
};

template <typename K, typename V>
class HeapHashMap : public GarbageCollected<HeapHashMap<K, V>>,
                    public HashMap<K, V, void, void, void, HeapAllocator> {};

template<typename T>
struct TraceIfNeeded {
  static void Trace(Visitor*, const T&);
};

}  // namespace blink

#endif
