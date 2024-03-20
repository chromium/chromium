// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <numeric>
#include <queue>
#include <set>
#include <stack>
#include <unordered_map>
#include <unordered_set>

#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"

#define RAW_PTR_EXCLUSION __attribute__((annotate("raw_ptr_exclusion")))

namespace base {
template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
struct flat_tree {
  // Add dummy functions to ensure flat_tree fields are rewritten.
  std::size_t size() { return 0; }
  void insert() {}
  void erase() {}
};

struct identity {
  template <class T>
  constexpr T&& operator()(T&& t) const noexcept {
    return t;
  }
};

template <class Key,
          class Compare = std::less<>,
          class Container = std::vector<Key>>
using flat_set = flat_tree<Key, identity, Compare, Container>;

template <class T, class Container = std::vector<T>>
using queue = std::queue<T, Container>;

template <class T, class Container = std::vector<T>>
using stack = std::stack<T, Container>;
}  // namespace base

struct S {};

class A {
 public:
  A() : member(init()) {}

  // Expected rewrite: (arg2 is skipped since it's a const char*)
  // A(const std::list<raw_ptr<S>>& arg, const std::list<const char*>& arg2)
  A(const std::list<raw_ptr<S>>& arg, const std::list<const char*>& arg2)
      : member(arg), member2(arg2) {}

  // Expected rewrite:
  // A(const std::list<raw_ptr<S>>* arg)
  A(const std::list<raw_ptr<S>>* arg) : member(*arg) {}

  std::list<raw_ptr<S>> init() { return {}; }

  // Expected rewrite:
  //   std::list<raw_ptr<S>> do_something(std::list<raw_ptr<S>>& a,
  //                                   S* i,
  //                                   std::list<raw_ptr<S>>& b);
  std::list<raw_ptr<S>> do_something(std::list<raw_ptr<S>>& a,
                                     S* i,
                                     std::list<raw_ptr<S>>& b);

  // Expected rewrite:
  // void set(const std::list<raw_ptr<S>> arg);
  void set(const std::list<raw_ptr<S>> arg);

  S* get_stack_top() {
    // Expected rewrite:
    //  auto* top = member8.top().get();
    auto* top = member8.top().get();
    return top;
  }

 private:
  // All the members below are expected to be rewritten except member2
  // since `const char*` is excluded from raw_ptr rewrites for now.
  std::list<raw_ptr<S>> member;
  std::list<const char*> member2;
  std::unordered_set<raw_ptr<S>> member3;
  base::flat_set<raw_ptr<S>> member4;
  base::queue<raw_ptr<S>> member5;
  std::queue<raw_ptr<S>> member6;
  base::stack<raw_ptr<S>> member7;
  std::stack<raw_ptr<S>> member8;
  std::set<raw_ptr<S>> member9;
  // These members are not expected to be rewritten as maps need special
  // handling.
  std::map<S*, S*> member10;
  std::unordered_map<S*, S*> member11;
};

// Expected rewrite:
// std::list<raw_ptr<S>> A::do_something(std::list<raw_ptr<S>>& a,
//                                      S* i,
//                                      std::list<raw_ptr<S>>& b)
std::list<raw_ptr<S>> A::do_something(std::list<raw_ptr<S>>& a,
                                      S* i,
                                      std::list<raw_ptr<S>>& b) {
  a.push_back(i);
  member = b;
  b = a;
  return a;
}

// Expected rewrite:
// void A::set(const std::list<raw_ptr<S>> arg)
void A::set(const std::list<raw_ptr<S>> arg) {
  member = arg;
}

class B {
 public:
  B() = default;

  // Expected rewrite:
  // std::list<raw_ptr<S>> get()
  std::list<raw_ptr<S>> get() { return member; }

  // Expected rewrite:
  // std::list<raw_ptr<S>> get2();
  std::list<raw_ptr<S>> get2();

 private:
  std::list<raw_ptr<S>> member;
};

// Expected rewrite:
// std::list<raw_ptr<S>> B::get2();
std::list<raw_ptr<S>> B::get2() {
  return member;
}

class C {
 public:
  C() = default;

  // Expected rewrite:
  // const std::list<raw_ptr<S>>& get()...
  const std::list<raw_ptr<S>>& get() { return member; }

 private:
  std::list<raw_ptr<S>> member;
};

class D {
 public:
  D() = default;

  // Expected rewrite:
  // const std::list<raw_ptr<S>>& get()...
  std::list<raw_ptr<S>>* get() { return &member; }

 private:
  std::list<raw_ptr<S>> member;
};

class E {
 public:
  E() = default;

  // Expected rewrite:
  // void set(const std::list<raw_ptr<S>>& arg) { member = arg; }
  void set(const std::list<raw_ptr<S>>& arg) { member = arg; }

 private:
  std::list<raw_ptr<S>> member;
};

class F {
 public:
  F() = default;

  void init() {
    // Expected rewrite:
    // std::list<raw_ptr<S>> temp;
    std::list<raw_ptr<S>> temp;
    temp.push_back(nullptr);
    member = temp;

    {
      // Expected rewrite:
      // std::list<raw_ptr<S>>::iterator it;
      std::list<raw_ptr<S>>::iterator it;
      it = temp.begin();
      ++it;
    }

    {
      // Expected rewrite:
      // std::list<raw_ptr<S>>::iterator it = temp.begin();
      std::list<raw_ptr<S>>::iterator it = temp.begin();
      ++it;
    }
  }

 private:
  std::list<raw_ptr<S>> member;
};

class G {
 public:
  G() = default;

  void init() {
    // Expected rewrite:
    // std::list<raw_ptr<S>> temp = member;
    std::list<raw_ptr<S>> temp = member;
    temp.push_back(nullptr);
  }

 private:
  std::list<raw_ptr<S>> member;
};

class H {
 public:
  H() = default;

  std::list<raw_ptr<S>> init() {
    // Expected rewrite:
    // std::list<raw_ptr<S>> temp;
    std::list<raw_ptr<S>> temp;
    temp = member;
    temp.push_back(nullptr);

    // Expected rewrite:
    // std::list<raw_ptr<S>> temp2;
    std::list<raw_ptr<S>> temp2;
    temp2 = temp;
    return temp2;
  }

 private:
  std::list<raw_ptr<S>> member;
};

class I {
 public:
  I() = default;

  std::list<raw_ptr<S>> init() {
    // Expected rewrite:
    // std::list<raw_ptr<S>> temp;
    std::list<raw_ptr<S>> temp;
    temp = std::move(member);
    temp.push_back(nullptr);
    return temp;
  }

 private:
  std::list<raw_ptr<S>> member;
};

class J {
 public:
  J() = default;

  void init() {
    prepare(member);
    prepare2(&member);
    prepare(get());
    prepare(*get_2());
  }

  // Expected rewrite:
  // std::list<raw_ptr<S>>& get() { return member; }
  std::list<raw_ptr<S>>& get() { return member; }

  // Expected rewrite:
  // std::list<raw_ptr<S>>* get_2() { return &member; }
  std::list<raw_ptr<S>>* get_2() { return &member; }

  // Expected rewrite:
  // void prepare(std::list<raw_ptr<S>>& v) { v.push_back(nullptr); }
  void prepare(std::list<raw_ptr<S>>& v) { v.push_back(nullptr); }

  // Expected rewrite:
  // void prepare2(std::list<raw_ptr<S>>* v) { v->push_back(nullptr); }
  void prepare2(std::list<raw_ptr<S>>* v) { v->push_back(nullptr); }

 private:
  std::list<raw_ptr<S>> member;
};

class K {
 public:
  K() = default;

  // Expected rewrite:
  // std::list<raw_ptr<S>> init() {
  std::list<raw_ptr<S>> init() {
    // Expected rewrite:
    // std::list<raw_ptr<S>> temp;
    std::list<raw_ptr<S>> temp;
    temp.swap(member);
    return temp;
  }

  // Expected rewrite:
  // std::list<raw_ptr<S>> init2() {
  std::list<raw_ptr<S>> init2() {
    // Expected rewrite:
    // std::list<raw_ptr<S>> temp;
    std::list<raw_ptr<S>> temp;
    std::swap(temp, member);
    return temp;
  }

 private:
  std::list<raw_ptr<S>> member;
};

class L {
 public:
  L() = default;

  // Expected rewrite:
  // std::list<raw_ptr<S>> init() {
  std::list<raw_ptr<S>> init() {
    // Expected rewrite:
    // std::list<raw_ptr<S>> temp;
    std::list<raw_ptr<S>> temp;
    temp.push_back(nullptr);
    member.swap(temp);
    return temp;
  }

  // Expected rewrite:
  // std::list<raw_ptr<S>> init2() {
  std::list<raw_ptr<S>> init2() {
    // Expected rewrite:
    // std::list<raw_ptr<S>> temp;
    std::list<raw_ptr<S>> temp;
    temp.push_back(nullptr);
    std::swap(member, temp);
    return temp;
  }

 private:
  std::list<raw_ptr<S>> member;
};

class M {
 public:
  M() = default;
  // Expected rewrite:
  // void set(std::list<raw_ptr<S>>* v) { *v = member; }
  void set(std::list<raw_ptr<S>>* v) { *v = member; }

 private:
  std::list<raw_ptr<S>> member;
};

class N {
 public:
  N() : member() {}

  // Expected rewrite:
  // std::list<raw_ptr<S>>* get() {
  std::list<raw_ptr<S>>* get() {
    // Expected rewrite:
    // std::list<raw_ptr<S>>* temp;
    std::list<raw_ptr<S>>* temp;
    temp = &member;
    return temp;
  }

  std::list<raw_ptr<S>>* get_() { return get(); }

  std::list<raw_ptr<S>> get__() { return *get(); }

  std::list<raw_ptr<S>> get2() {
    std::list<raw_ptr<S>>* temp;
    temp = get();

    std::list<raw_ptr<S>>* temp2 = get();
    (void)temp2;

    std::list<raw_ptr<S>> temp3;
    temp3 = *get();
    (void)temp3;

    std::list<raw_ptr<S>> temp4 = *get();
    (void)temp4;
    return *temp;
  }

  const std::list<raw_ptr<S>>* get3() {
    std::list<raw_ptr<S>>* temp = &member;
    return temp;
  }

  std::list<raw_ptr<S>> get4() {
    std::list<raw_ptr<S>>* temp;
    temp = &member;

    std::list<raw_ptr<S>>* temp2 = temp;

    std::list<raw_ptr<S>>** temp3 = &temp2;
    (void)temp3;

    std::list<raw_ptr<S>>& ref = *temp;
    (void)ref;
    return *temp2;
  }

  const std::list<raw_ptr<S>>& get5() {
    std::list<raw_ptr<S>>* temp;
    temp = &member;
    return *temp;
  }

 private:
  std::list<raw_ptr<S>> member;
};

struct obj {
  std::list<raw_ptr<S>> member;
  std::list<raw_ptr<std::map<int, int>>> member2;
};

struct obj2 {
  RAW_PTR_EXCLUSION std::list<S*> member;
};

struct obj3 {
  // No rewrite expected as this is assigned to obj2::member which is annotated
  // with RAW_PTR_EXCLUSION.
  std::list<S*> member;
};

namespace temporary {
std::list<raw_ptr<S>> ge_t() {
  return {};
}
std::list<raw_ptr<S>>* ge_t_ptr() {
  return nullptr;
}
}  // namespace temporary

void fct() {
  std::list<raw_ptr<S>> temp;
  std::list<raw_ptr<S>> temp3;
  std::list<raw_ptr<S>> temp2{temp};
  obj o{temp3};
  std::list<const char*> t;
  A a(temp, t);
  (void)a;

  {
    std::list<raw_ptr<S>> temp;
    A a2(temp, t);
    (void)a2;
  }

  {
    obj p{temporary::ge_t()};
    (void)p;

    obj q{*temporary::ge_t_ptr()};
    (void)q;
  }

  {
    std::list<raw_ptr<S>> temp4;
    std::list<const char*> s;
    std::make_unique<A>(temp4, s);
  }

  {
    std::list<raw_ptr<S>> temp4;
    std::list<const char*> s;
    A* a = new A(temp4, s);
    (void)a;
  }

  {
    std::list<S*> t;
    // creates a link between obj2::member and obj3::member through t;
    // this leads to obj3::member to not be rewritten as it becomes reachable
    // from a RAW_PTR_EXCLUSION annotated field.
    obj3 o3{t};
    obj2 o2{t};
  }
}

class O {
 public:
  O() : member() {}

  std::list<raw_ptr<S>> f() {
    std::list<raw_ptr<S>> temp;
    temp = std::move(member);
    return temp;
  }

  std::list<raw_ptr<S>> f2() {
    std::list<raw_ptr<S>> temp = std::move(member);
    temp.push_back(nullptr);

    // Expected rewrite:
    // for (S* v : temp) {
    for (S* v : temp) {
      (void)v;
    }

    // Expected rewrite:
    // for (S* v : member) {
    for (S* v : member) {
      (void)v;
    }

    // Expected rewrite:
    // for (const S* const v : member) {
    for (const S* const v : member) {
      (void)v;
    }

    auto temp2 = temp;
    // Expected rewrite:
    // for (S* v : temp2) {
    for (S* v : temp2) {
      (void)v;
    }

    // Expected rewrite:
    // for (const S* v : temp2) {
    for (const S* v : temp2) {
      (void)v;
    }

    // Expected rewrite:
    // for (const S* const v : temp2) {
    for (const S* const v : temp2) {
      (void)v;
    }

    // Expected rewrite:
    // auto* ptr2 = temp2.front().get();
    auto* ptr2 = temp2.front().get();
    (void)ptr2;

    // Expected rewrite:
    // auto* ptr2 = temp2.back().get();
    auto* ptr3 = temp2.back().get();
    (void)ptr3;

    return temp2;
  }

  std::list<raw_ptr<S>> g() { return std::move(member); }

  std::list<raw_ptr<S>> g2() {
    std::list<raw_ptr<S>> temp;
    temp.push_back(nullptr);

    auto* var = temp.front().get();
    (void)var;

    auto* var2 = temp.back().get();
    (void)var2;

    return (temp.size() > member.size()) ? std::move(temp) : std::move(member);
  }

 private:
  std::list<raw_ptr<S>> member;
};

class P {
 public:
  P(std::list<raw_ptr<S>> arg) : member(std::move(arg)) {}

  P(std::list<raw_ptr<S>>* arg) : member(*arg) {}

 private:
  std::list<raw_ptr<S>> member;
};

namespace {
std::list<raw_ptr<S>>* get_ptr() {
  return nullptr;
}
void p_fct() {
  {
    std::list<raw_ptr<S>> temp;
    P p(&temp);
    (void)p;
  }

  {
    P p(*get_ptr());
    (void)p;
  }
}
}  // namespace

class Parent {
 public:
  Parent() = default;

  virtual std::list<raw_ptr<S>> get();

 protected:
  std::list<raw_ptr<S>> member;
};

std::list<raw_ptr<S>> Parent::get() {
  return member;
}

class Child : public Parent {
 public:
  Child() = default;

  std::list<raw_ptr<S>> get() override;
};

std::list<raw_ptr<S>> Child::get() {
  return std::list<raw_ptr<S>>{};
}

namespace n {
void do_something(std::list<raw_ptr<int>>& v) {
  v.push_back(nullptr);
}

class BCD {
 public:
  BCD(const std::list<raw_ptr<int>>& arg);

  void dod() {
    do_something(member);
    auto lambda = [this]() -> std::list<raw_ptr<int>> { return member; };
    lambda();

    functor f;
    f(member);

    auto lambda2 = [](const std::list<raw_ptr<int>>& v) {
      for (int* i : v) {
        (void)i;
      }
    };

    lambda2(member);
  }

 private:
  struct functor {
    void operator()(const std::list<raw_ptr<int>>& v) {
      for (int* i : v) {
        (void)i;
      }
    }
  };
  std::list<raw_ptr<int>> member;
};

BCD::BCD(const std::list<raw_ptr<int>>& arg) : member(arg) {}
}  // namespace n

// No change needed here
void any_function(std::list<int*>& v) {
  v.push_back(nullptr);
}

// These should be rewritten but are not for now.
namespace templated_stuff {
template <class T>
void do_something(std::list<T*>& t) {
  t.push_back(nullptr);
}

template <typename T>
class A {
 public:
  A(const std::list<T*>& arg) : v(arg) {}

  virtual const std::list<T*>& get() {
    do_something(v);
    return v;
  }

 protected:
  std::list<T*> v;
};

void fctttttt() {
  A<int> a({});
  std::list<int*> temp = a.get();
  temp.push_back(nullptr);
}

}  // namespace templated_stuff

namespace {
namespace A {
struct SA {
  int count;
};
}  // namespace A

namespace B {
struct S {
  // Expected rewrite: std::list<raw_ptr<const A::SA>> member;
  std::list<raw_ptr<const A::SA>> member;

  bool fct() {
    // This tests whether we properly trim (anonymous namespace):: from the type
    // while conserving constness.
    // Expected rewrite: for(const A::SA* i : member)
    for (const A::SA* i : member) {
      (void)i;
    }

    return std::any_of(
        member.begin(), member.end(),
        // Expected rewrite: [](const A::SA* item) { return item != nullptr; });
        [](const A::SA* item) { return item != nullptr; });
  }

  std::list<raw_ptr<const A::SA>>::iterator fct2() {
    return std::find_if(
        member.begin(), member.end(),
        // Expected rewrite: [](const A::SA* item) { return item == nullptr; });
        [](const A::SA* item) { return item == nullptr; });
  }

  bool fct3() {
    return std::all_of(
        member.begin(), member.end(),
        // Expected rewrite: [](const A::SA* item) { return item != nullptr; });
        [](const A::SA* item) { return item != nullptr; });
  }

  int fct4() {
    return std::accumulate(member.begin(), member.end(), 1,
                           // Expected rewrite: [](int num, const A::SA* item) {
                           [](int num, const A::SA* item) {
                             return (item != nullptr) ? 1 + num : 0;
                           });
  }

  int fct5() {
    return std::count_if(
        member.begin(), member.end(),
        // Expected rewrite: [](const A::SA* item) { return item != nullptr; });
        [](const A::SA* item) { return item != nullptr; });
  }

  void fct6() {
    std::list<int> copy;
    std::transform(
        member.begin(), member.end(), std::back_inserter(copy),
        // Expected rewrite: [](const A::SA* item) { return item->count; });
        [](const A::SA* item) { return item->count; });
  }

  std::list<const A::SA*> fct7() {
    std::list<const A::SA*> copy;
    std::copy_if(
        member.begin(), member.end(), std::back_inserter(copy),
        // Expected rewrite: [](const A::SA* item) { return item != nullptr; });
        [](const A::SA* item) { return item != nullptr; });
    return copy;
  }
};
}  // namespace B
}  // namespace

namespace {
class AA {
 public:
  // No rewrite expected as this is not reachable from a fieldDecl.
  using VECTOR = std::list<S*>;

  // Expected rewrite: using VECTOR2 = std::list<raw_ptr<S>>;
  using VECTOR2 = std::list<raw_ptr<S>>;

  // Expected rewrite: set(std::list<raw_ptr<int>> arg)
  virtual void set(std::list<raw_ptr<int>> arg) = 0;

  // Expected rewrite: get(std::list<raw_ptr<int>>& arg)
  virtual void get(std::list<raw_ptr<int>>& arg) const = 0;

  // Expected rewrite: get2(std::list<raw_ptr<int>>* arg)
  virtual void get2(std::list<raw_ptr<int>>* arg) const = 0;

  virtual void get3(VECTOR& arg) const = 0;

  virtual void get4(VECTOR2& arg) const = 0;

  // No rewrite expected as the argument is not connected/reachable from a
  // rewritten field.
  virtual void GetNotifications(std::list<const int*>* a) const = 0;

  // No rewrite expected as the argument is not connected/reachable from a
  // rewritten field.
  virtual void GetNotifications2(std::list<const int*> a) const = 0;

  // No rewrite expected as the argument is not connected/reachable from a
  // rewritten field.
  virtual void GetNotifications3(std::list<const int*>& a) const = 0;
};

class BB : public AA {
 public:
  // Expected rewrite: set(std::list<raw_ptr<int>> arg)
  void set(std::list<raw_ptr<int>> arg) override { member = arg; }

  // Expected rewrite: get(std::list<raw_ptr<int>>& arg)
  void get(std::list<raw_ptr<int>>& arg) const override { arg = member; }

  // Expected rewrite: get2(std::list<raw_ptr<int>>* arg)
  void get2(std::list<raw_ptr<int>>* arg) const override { *arg = member; }

  void get3(VECTOR& arg) const override {}

  // Expected rewrite: get4(std::list<raw_ptr<S>>& arg)
  void get4(std::list<raw_ptr<S>>& arg) const override { arg = member2; }

  // No rewrite expected as the argument is not connected/reachable from a
  // rewritten field.
  void GetNotifications(std::list<const int*>*) const override {}

  // No rewrite expected as the argument is not connected/reachable from a
  // rewritten field.
  void GetNotifications2(std::list<const int*> a) const override {}

  // No rewrite expected as the argument is not connected/reachable from a
  // rewritten field.
  void GetNotifications3(std::list<const int*>& a) const override {}

 private:
  // Expected rewrite: std::list<raw_ptr<int>> member;
  std::list<raw_ptr<int>> member;
  VECTOR2 member2;
};

class Mocked1 : public AA {
 public:
  // Expected rewrite: void, set, (std::list<raw_ptr<int>>)
  MOCK_METHOD(void, set, (std::list<raw_ptr<int>>));

  // Expected rewrite: get, void(std::list<raw_ptr<int>>&)
  MOCK_CONST_METHOD1(get, void(std::list<raw_ptr<int>>&));

  // Expected rewrite: get2, void(std::list<raw_ptr<int>>*)
  MOCK_CONST_METHOD1(get2, void(std::list<raw_ptr<int>>*));

  // No rewrite expected as the argument is not connected/reachable from a
  // rewritten field.
  MOCK_CONST_METHOD1(get3, void(std::list<S*>&));

  // Expected rewrite: get4, void(std::list<raw_ptr<S>>&)
  MOCK_CONST_METHOD1(get4, void(std::list<raw_ptr<S>>&));

  // No rewrite expected as the argument is not connected/reachable from a
  // rewritten field.
  MOCK_CONST_METHOD1(GetNotifications, void(std::list<const int*>*));

  // No rewrite expected as the argument is not connected/reachable from a
  // rewritten field.
  MOCK_CONST_METHOD1(GetNotifications2, void(std::list<const int*>&));

  // No rewrite expected as the argument is not connected/reachable from a
  // rewritten field.
  MOCK_CONST_METHOD1(GetNotifications3, void(std::list<const int*>));
};

class Mocked2 : public AA {
 public:
  // Expected rewrite: set, void(std::list<raw_ptr<int>> arg)
  MOCK_METHOD1(set, void(std::list<raw_ptr<int>> args));

  // Expected rewrite: get, void(std::list<raw_ptr<int>>&)
  MOCK_CONST_METHOD1(get, void(std::list<raw_ptr<int>>&));

  // Expected rewrite: get2, void(std::list<raw_ptr<int>>*)
  MOCK_CONST_METHOD1(get2, void(std::list<raw_ptr<int>>*));

  // No rewrite expected as the argument is not connected/reachable from a
  // rewritten field.
  MOCK_CONST_METHOD1(get3, void(VECTOR&));

  // Expected rewrite: get4, void(std::list<raw_ptr<S>>&)
  MOCK_CONST_METHOD1(get4, void(std::list<raw_ptr<S>>&));

  // No rewrite expected as the argument is not connected/reachable from a
  // rewritten field.
  MOCK_CONST_METHOD1(GetNotifications, void(std::list<const int*>*));

  // No rewrite expected as the argument is not connected/reachable from a
  // rewritten field.
  MOCK_CONST_METHOD1(GetNotifications2, void(std::list<const int*>&));

  // No rewrite expected as the argument is not connected/reachable from a
  // rewritten field.
  MOCK_CONST_METHOD1(GetNotifications3, void(std::list<const int*>));
};

}  // namespace
