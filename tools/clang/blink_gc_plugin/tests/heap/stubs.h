// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEAP_STUBS_H_
#define HEAP_STUBS_H_

#include "stddef.h"

#define WTF_MAKE_FAST_ALLOCATED                 \
    public:                                     \
    void* operator new(size_t, void* p);        \
    void* operator new[](size_t, void* p);      \
    void* operator new(size_t size);            \
    private:                                    \
    typedef int __thisIsHereToForceASemicolonAfterThisMacro

namespace WTF {

template<typename T> class RefCounted { };

template<typename T> class RawPtr {
public:
    operator T*() const { return 0; }
    T* operator->() { return 0; }
};

template<typename T> class RefPtr {
public:
    ~RefPtr() { }
    operator T*() const { return 0; }
    T* operator->() { return 0; }
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
          typename HashArg = void,
          typename TraitsArg = void,
          typename Allocator = DefaultAllocator>
class ListHashSet {
 public:
  typedef ValueArg* iterator;
  typedef const ValueArg* const_iterator;
  typedef ValueArg* reverse_iterator;
  typedef const ValueArg* const_reverse_iterator;

  ~ListHashSet() {}
};

template <typename ValueArg,
          typename HashArg = void,
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
    T* operator->() { return 0; }
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

template <typename T>
class Optional {};

}  // namespace base

namespace blink {

using namespace WTF;

#define DISALLOW_NEW()                   \
    private:                                    \
    void* operator new(size_t) = delete;        \
    void* operator new(size_t, void*) = delete;

#define STACK_ALLOCATED()                                   \
    private:                                                \
    __attribute__((annotate("blink_stack_allocated")))      \
    void* operator new(size_t) = delete;                    \
    void* operator new(size_t, void*) = delete;

#define DISALLOW_NEW_EXCEPT_PLACEMENT_NEW() \
    public:                                 \
    void* operator new(size_t, void*);      \
    private:                                \
    void* operator new(size_t) = delete;

#define GC_PLUGIN_IGNORE(bug)                           \
    __attribute__((annotate("blink_gc_plugin_ignore")))

#define USING_GARBAGE_COLLECTED_MIXIN(type)                             \
 public:                                                                \
  virtual void AdjustAndMark(Visitor*) const override {}                \
  virtual bool IsHeapObjectAlive(Visitor*) const override { return 0; } \
  void* mixin_constructor_marker_

#define USING_GARBAGE_COLLECTED_MIXIN_NEW(type)                         \
 public:                                                                \
  virtual void AdjustAndMark(Visitor*) const override {}                \
  virtual bool IsHeapObjectAlive(Visitor*) const override { return 0; } \
  typedef int HasUsingGarbageCollectedMixinMacro

template<typename T> class GarbageCollected { };

template <typename T>
class RefCountedGarbageCollected : public GarbageCollected<T> {};

template<typename T> class Member {
public:
    operator T*() const { return 0; }
    T* operator->() { return 0; }
    bool operator!() const { return false; }
};

template<typename T> class WeakMember {
public:
    operator T*() const { return 0; }
    T* operator->() { return 0; }
    bool operator!() const { return false; }
};

template<typename T> class Persistent {
public:
    operator T*() const { return 0; }
    T* operator->() { return 0; }
    bool operator!() const { return false; }
};

template<typename T> class WeakPersistent {
public:
    operator T*() const { return 0; }
    T* operator->() { return 0; }
    bool operator!() const { return false; }
};

template<typename T> class CrossThreadPersistent {
public:
    operator T*() const { return 0; }
    T* operator->() { return 0; }
    bool operator!() const { return false; }
};

template<typename T> class CrossThreadWeakPersistent {
public:
    operator T*() const { return 0; }
    T* operator->() { return 0; }
    bool operator!() const { return false; }
};

template <typename T>
class TraceWrapperV8Reference {
 public:
  operator T*() const { return 0; }
  T* operator->() { return 0; }
  bool operator!() const { return false; }
};

class HeapAllocator {
public:
    static const bool isGarbageCollected = true;
};

template<typename T, size_t inlineCapacity = 0>
class HeapVector : public Vector<T, inlineCapacity, HeapAllocator> { };

template<typename T, size_t inlineCapacity = 0>
class HeapDeque : public Vector<T, inlineCapacity, HeapAllocator> { };

template<typename T>
class HeapHashSet : public HashSet<T, void, void, HeapAllocator> { };

template<typename T>
class HeapListHashSet : public ListHashSet<T, void, void, HeapAllocator> { };

template<typename T>
class HeapLinkedHashSet : public LinkedHashSet<T, void, void, HeapAllocator> {
};

template<typename T>
class HeapHashCountedSet : public HashCountedSet<T, void, void, HeapAllocator> {
};

template<typename K, typename V>
class HeapHashMap : public HashMap<K, V, void, void, void, HeapAllocator> { };

class Visitor {
 public:
  template <typename T, void (T::*method)(Visitor*)>
  void RegisterWeakMembers(const T* obj);

  template <typename T>
  void Trace(const T&);
};

class GarbageCollectedMixin {
public:
    virtual void AdjustAndMark(Visitor*) const = 0;
    virtual bool IsHeapObjectAlive(Visitor*) const = 0;
    virtual void Trace(Visitor*) { }
};

template<typename T>
struct TraceIfNeeded {
    static void Trace(Visitor*, T*);
};

}

#endif
