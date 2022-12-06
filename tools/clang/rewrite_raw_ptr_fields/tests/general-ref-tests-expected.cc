// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "base/memory/raw_ref.h"

struct MySubStruct {
  MySubStruct(const int& num);
  int member;
  virtual int get_member() const { return member; }
  virtual ~MySubStruct() = default;

  struct inner_s {
    inner_s(const inner_s&);
    // Expected rewrite: const raw_ref<int> inner_int;
    const raw_ref<int> inner_int;
  };
};

// This was added due to a bug in the matcher that was doing erroneous rewrites
// here. No rewrite expected.
MySubStruct::inner_s::inner_s(const inner_s&) = default;

struct MyStruct {
  MyStruct(MySubStruct& s) : ref(s) {}

  const MySubStruct& get() const;
  // Expected rewrite: const raw_ref<MySubStruct> ref;
  const raw_ref<MySubStruct> ref;
};

template <class T>
struct MyTemplatedStruct {
  MyTemplatedStruct(T& t) : ref(t) {}

  // Expected rewrite: ref->member
  void setSubmember(int n) { ref->member = n; }

  // Expected rewrite: return *ref;
  T& get() { return *ref; }

  const T& get() const;
  // Expected rewrite: return *ref;
  T get_by_value() const { return *ref; }

  // Expected rewrite: ref->get_member();
  int getInt() { return ref->get_member(); }

  // Expected rewrite: const raw_ref<T> ref;
  const raw_ref<T> ref;
};

template <class T>
void func(T& ref, const T& ref2) {}

int main() {
  MySubStruct sub{1};
  MyStruct s(sub);
  // Expected rewrite: s.ref->member;
  s.ref->member = 11;
  // Expected rewrite: s.ref->get_member();
  s.ref->get_member();

  // Expected rewrite: MyStruct* s2 = new MyStruct(*s.ref);
  MyStruct* s2 = new MyStruct(*s.ref);

  MyTemplatedStruct<MySubStruct> my_template_inst(sub);
  my_template_inst.setSubmember(1234);

  // Expected rewrites:
  // func(*my_template_inst.ref, my_template_inst.get());
  func(*my_template_inst.ref, my_template_inst.get());
  // func(my_template_inst.get(), *my_template_inst.ref);
  func(my_template_inst.get(), *my_template_inst.ref);
  // func(*my_template_inst.ref, *my_template_inst.ref);
  func(*my_template_inst.ref, *my_template_inst.ref);

  // Expected rewrite: MySubStruct* ptr = &*s.ref;
  MySubStruct* ptr = &*s.ref;

  // Expected rewrite:
  //  auto &ref = *s.ref, &ref2 = *s.ref;
  auto &ref = *s.ref, &ref2 = *s.ref;

  int a = 0;
  int b = 0;

  // No rewrite for anonymous struct.
  struct {
    int& in;
    int& out;
  } report_lists[]{{a, b}, {a, b}};
}

template <typename T>
const T& MyTemplatedStruct<T>::get() const {
  // Expected rewrite: return *ref;
  return *ref;
}

const MySubStruct& MyStruct::get() const {
  // Expected rewrite: return *ref;
  return *ref;
}

struct key_compare {
  bool operator()(const int& a, const int& b) const { return a < b; }
};

struct KeyValueCompare {
  // The key comparison object must outlive this class.
  explicit KeyValueCompare(const key_compare& comp) : comp_(comp) {}

  bool operator()(const int& lhs, const int& rhs) const {
    // Expected rewrite: return (*comp_)(lhs, rhs);
    return (*comp_)(lhs, rhs);
  }

  template <typename T, typename U>
  bool operator()(const T& lhs, const U& rhs) const {
    // Expected rewrite: return (*comp_)(lhs, rhs);
    return (*comp_)(lhs, rhs);
  }

 private:
  const raw_ref<const key_compare> comp_;
};

struct KeyValueCompare2 {
  // The key comparison object must outlive this class.
  explicit KeyValueCompare2(const key_compare& comp) : comp_(comp) {}

  template <typename T, typename U>
  bool operator()(const T& lhs, const U& rhs) const {
    // Expected rewrite: return (*comp_)(lhs, rhs);
    return (*comp_)(lhs, rhs);
  }

 private:
  const raw_ref<const key_compare> comp_;
};

template <typename T>
void doSomething(T& t) {
  (void)t;
}

template <typename T>
struct VectorMemberRef {
  void iterate() {
    for (T& t : *v) {
      doSomething(t);
    }
  }

  T get_first() { return (*v)[0]; }

  const raw_ref<std::vector<T>> v;
};