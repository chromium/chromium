// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <algorithm>
#include <map>
#include <memory>
#include <numeric>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"

#define RAW_PTR_EXCLUSION __attribute__((annotate("raw_ptr_exclusion")))

struct S {};

class A {
 public:
  A() : member(init()) {}

  A(const std::vector<S*>& arg, const std::vector<const char*>& arg2)
      : member(arg), member2(arg2) {}

  A(const std::vector<S*>* arg) : member(*arg) {}

  std::vector<S*> init() { return {}; }

  std::vector<S*> do_something(std::vector<S*>& a, S* i, std::vector<S*>& b);

  void set(const std::vector<S*> arg);

 private:
  std::vector<S*> member;
  std::vector<const char*> member2;
};

std::vector<S*> A::do_something(std::vector<S*>& a, S* i, std::vector<S*>& b) {
  a.push_back(i);
  member = b;
  b = a;
  return a;
}

void A::set(const std::vector<S*> arg) {
  member = arg;
}

class B {
 public:
  B() = default;

  std::vector<S*> get() { return member; }

  std::vector<S*> get2();

 private:
  std::vector<S*> member;
};

std::vector<S*> B::get2() {
  return member;
}

class C {
 public:
  C() = default;

  const std::vector<S*>& get() { return member; }

 private:
  std::vector<S*> member;
};

class D {
 public:
  D() = default;

  std::vector<S*>* get() { return &member; }

 private:
  std::vector<S*> member;
};

class E {
 public:
  E() = default;

  void set(const std::vector<S*>& arg) { member = arg; }

 private:
  std::vector<S*> member;
};

class F {
 public:
  F() = default;

  void init() {
    std::vector<S*> temp;
    temp.push_back(nullptr);
    member = temp;

    {
      std::vector<S*>::iterator it;
      it = temp.begin();
      ++it;
    }

    {
      std::vector<S*>::iterator it = temp.begin();
      ++it;
    }
  }

 private:
  std::vector<S*> member;
};

class G {
 public:
  G() = default;

  void init() {
    std::vector<S*> temp = member;
    temp.push_back(nullptr);
  }

 private:
  std::vector<S*> member;
};

class H {
 public:
  H() = default;

  std::vector<S*> init() {
    std::vector<S*> temp;
    temp = member;
    temp.push_back(nullptr);

    std::vector<S*> temp2;
    temp2 = temp;
    return temp2;
  }

 private:
  std::vector<S*> member;
};

class I {
 public:
  I() = default;

  std::vector<S*> init() {
    std::vector<S*> temp;
    temp = std::move(member);
    temp.push_back(nullptr);
    return temp;
  }

 private:
  std::vector<S*> member;
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

  std::vector<S*>& get() { return member; }

  std::vector<S*>* get_2() { return &member; }

  void prepare(std::vector<S*>& v) { v.push_back(nullptr); }

  void prepare2(std::vector<S*>* v) { v->push_back(nullptr); }

 private:
  std::vector<S*> member;
};

class K {
 public:
  K() = default;

  std::vector<S*> init() {
    std::vector<S*> temp;
    temp.swap(member);
    return temp;
  }

  std::vector<S*> init2() {
    std::vector<S*> temp;
    std::swap(temp, member);
    return temp;
  }

 private:
  std::vector<S*> member;
};

class L {
 public:
  L() = default;

  std::vector<S*> init() {
    std::vector<S*> temp;
    temp.push_back(nullptr);
    member.swap(temp);
    return temp;
  }

  std::vector<S*> init2() {
    std::vector<S*> temp;
    temp.push_back(nullptr);
    std::swap(member, temp);
    return temp;
  }

 private:
  std::vector<S*> member;
};

class M {
 public:
  M() = default;
  void set(std::vector<S*>* v) { *v = member; }

 private:
  std::vector<S*> member;
};

class N {
 public:
  N() : member() {}

  std::vector<S*>* get() {
    std::vector<S*>* temp;
    temp = &member;
    return temp;
  }

  std::vector<S*>* get_() { return get(); }

  std::vector<S*> get__() { return *get(); }

  std::vector<S*> get2() {
    std::vector<S*>* temp;
    temp = get();

    std::vector<S*>* temp2 = get();
    (void)temp2;

    std::vector<S*> temp3;
    temp3 = *get();
    (void)temp3;

    std::vector<S*> temp4 = *get();
    (void)temp4;
    return *temp;
  }

  const std::vector<S*>* get3() {
    std::vector<S*>* temp = &member;
    return temp;
  }

  std::vector<S*> get4() {
    std::vector<S*>* temp;
    temp = &member;

    std::vector<S*>* temp2 = temp;

    std::vector<S*>** temp3 = &temp2;
    (void)temp3;

    std::vector<S*>& ref = *temp;
    (void)ref;
    return *temp2;
  }

  const std::vector<S*>& get5() {
    std::vector<S*>* temp;
    temp = &member;
    return *temp;
  }

 private:
  std::vector<S*> member;
};

struct obj {
  std::vector<S*> member;
  std::vector<std::map<int, int>*> member2;
};

struct obj2 {
  RAW_PTR_EXCLUSION std::vector<S*> member;
};

struct obj3 {
  // No rewrite expected as this is assigned to obj2::member which is annotated
  // with RAW_PTR_EXCLUSION.
  std::vector<S*> member;
};

namespace temporary {
std::vector<S*> ge_t() {
  return {};
}
std::vector<S*>* ge_t_ptr() {
  return nullptr;
}
}  // namespace temporary

void fct() {
  std::vector<S*> temp;
  std::vector<S*> temp3;
  std::vector<S*> temp2{temp};
  obj o{temp3};
  std::vector<const char*> t;
  A a(temp, t);
  (void)a;

  {
    std::vector<S*> temp;
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
    std::vector<S*> temp4;
    std::vector<const char*> s;
    std::make_unique<A>(temp4, s);
  }

  {
    std::vector<S*> temp4;
    std::vector<const char*> s;
    A* a = new A(temp4, s);
    (void)a;
  }

  {
    std::vector<S*> t;
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

  std::vector<S*> f() {
    std::vector<S*> temp;
    temp = std::move(member);
    return temp;
  }

  std::vector<S*> f2() {
    std::vector<S*> temp = std::move(member);
    temp.push_back(nullptr);

    for (auto* v : temp) {
      (void)v;
    }

    for (auto* v : member) {
      (void)v;
    }

    for (const auto* const v : member) {
      (void)v;
    }

    auto temp2 = temp;
    for (auto* v : temp2) {
      (void)v;
    }

    for (const auto* v : temp2) {
      (void)v;
    }

    for (const auto* const v : temp2) {
      (void)v;
    }

    auto* ptr1 = temp2[0];
    (void)ptr1;

    auto* ptr2 = temp2.front();
    (void)ptr2;

    auto* ptr3 = temp2.back();
    (void)ptr3;

    int index = 0;
    auto* ptr4 = temp2.at(index);
    (void)ptr4;

    return temp2;
  }

  std::vector<S*> g() { return std::move(member); }

  std::vector<S*> g2() {
    std::vector<S*> temp;
    temp.push_back(nullptr);

    auto* var = temp.front();
    (void)var;

    auto* var2 = temp.back();
    (void)var2;

    int index = 0;
    auto* var3 = temp[index];
    (void)var3;
    return (temp.size() > member.size()) ? std::move(temp) : std::move(member);
  }

 private:
  std::vector<S*> member;
};

class P {
 public:
  P(std::vector<S*> arg) : member(std::move(arg)) {}

  P(std::vector<S*>* arg) : member(*arg) {}

 private:
  std::vector<S*> member;
};

namespace {
std::vector<S*>* get_ptr() {
  return nullptr;
}
void p_fct() {
  {
    std::vector<S*> temp;
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

  virtual std::vector<S*> get();

 protected:
  std::vector<S*> member;
};

std::vector<S*> Parent::get() {
  return member;
}

class Child : public Parent {
 public:
  Child() = default;

  std::vector<S*> get() override;
};

std::vector<S*> Child::get() {
  return std::vector<S*>{};
}

namespace n {
template <class T>
void do_something(std::vector<T*>& v) {
  v.push_back(nullptr);
}

class BCD {
 public:
  BCD(const std::vector<int*>& arg);

  void dod() {
    do_something(member);
    auto lambda = [this]() -> std::vector<int*> { return member; };
    lambda();

    functor f;
    f(member);

    auto lambda2 = [](const std::vector<int*>& v) {
      for (auto* i : v) {
        (void)i;
      }
    };

    lambda2(member);
  }

 private:
  struct functor {
    void operator()(const std::vector<int*>& v) {
      for (auto* i : v) {
        (void)i;
      }
    }
  };
  std::vector<int*> member;
};

BCD::BCD(const std::vector<int*>& arg) : member(arg) {}
}  // namespace n

// No change needed here
void any_function(std::vector<int*>& v) {
  v.push_back(nullptr);
}

namespace templated_stuff {
template <class T>
void do_something(std::vector<T*>& t) {
  t.push_back(nullptr);
}

template <typename T>
class A {
 public:
  A(const std::vector<T*>& arg) : v(arg) {}

  virtual const std::vector<T*>& get() {
    do_something(v);
    return v;
  }

 protected:
  std::vector<T*> v;
};

void fctttttt() {
  A<int> a({});
  std::vector<int*> temp = a.get();
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
  // Expected rewrite: std::vector<raw_ptr<const A::SA>> member;
  std::vector<const A::SA*> member;

  bool fct() {
    // This tests whether we properly trim (anonymous namespace):: from the type
    // while conserving constness.
    // Expected rewrite: for(const A::SA* i : member)
    for (auto* i : member) {
      (void)i;
    }

    return std::any_of(
        member.begin(), member.end(),
        // Expected rewrite: [](const A::SA* item) { return item != nullptr; });
        [](auto* item) { return item != nullptr; });
  }

  std::vector<const A::SA*>::iterator fct2() {
    return std::find_if(
        member.begin(), member.end(),
        // Expected rewrite: [](const A::SA* item) { return item == nullptr; });
        [](const auto* item) { return item == nullptr; });
  }

  bool fct3() {
    return std::all_of(
        member.begin(), member.end(),
        // Expected rewrite: [](const A::SA* item) { return item != nullptr; });
        [](auto* item) { return item != nullptr; });
  }

  int fct4() {
    return std::accumulate(member.begin(), member.end(), 1,
                           // Expected rewrite: [](int num, const A::SA* item) {
                           [](int num, const auto* item) {
                             return (item != nullptr) ? 1 + num : 0;
                           });
  }

  int fct5() {
    return std::count_if(
        member.begin(), member.end(),
        // Expected rewrite: [](const A::SA* item) { return item != nullptr; });
        [](const auto* item) { return item != nullptr; });
  }

  void fct6() {
    std::vector<int> copy;
    std::transform(
        member.begin(), member.end(), std::back_inserter(copy),
        // Expected rewrite: [](const A::SA* item) { return item->count; });
        [](const auto* item) { return item->count; });
  }

  std::vector<const A::SA*> fct7() {
    std::vector<const A::SA*> copy;
    std::copy_if(
        member.begin(), member.end(), std::back_inserter(copy),
        // Expected rewrite: [](const A::SA* item) { return item != nullptr; });
        [](const auto* item) { return item != nullptr; });
    return copy;
  }
};
}  // namespace B
}  // namespace

namespace {
class AA {
 public:
  // No rewrite expected as this is not reachable from a fieldDecl.
  using VECTOR = std::vector<S*>;

  // Expected rewrite: using VECTOR2 = std::vector<raw_ptr<S>>;
  using VECTOR2 = std::vector<S*>;

  // Expected rewrite: set(std::vector<raw_ptr<int>> arg)
  virtual void set(std::vector<int*> arg) = 0;

  // Expected rewrite: get(std::vector<raw_ptr<int>>& arg)
  virtual void get(std::vector<int*>& arg) const = 0;

  // Expected rewrite: get2(std::vector<raw_ptr<int>>* arg)
  virtual void get2(std::vector<int*>* arg) const = 0;

  virtual void get3(VECTOR& arg) const = 0;

  virtual void get4(VECTOR2& arg) const = 0;

  // No rewrite expected as the argument is not connected/reachable from a
  // rewritten field.
  virtual void GetNotifications(std::vector<const int*>* a) const = 0;

  // No rewrite expected as the argument is not connected/reachable from a
  // rewritten field.
  virtual void GetNotifications2(std::vector<const int*> a) const = 0;

  // No rewrite expected as the argument is not connected/reachable from a
  // rewritten field.
  virtual void GetNotifications3(std::vector<const int*>& a) const = 0;
};

class BB : public AA {
 public:
  // Expected rewrite: set(std::vector<raw_ptr<int>> arg)
  void set(std::vector<int*> arg) override { member = arg; }

  // Expected rewrite: get(std::vector<raw_ptr<int>>& arg)
  void get(std::vector<int*>& arg) const override { arg = member; }

  // Expected rewrite: get2(std::vector<raw_ptr<int>>* arg)
  void get2(std::vector<int*>* arg) const override { *arg = member; }

  void get3(VECTOR& arg) const override {}

  // Expected rewrite: get4(std::vector<raw_ptr<S>>& arg)
  void get4(std::vector<S*>& arg) const override { arg = member2; }

  // No rewrite expected as the argument is not connected/reachable from a
  // rewritten field.
  void GetNotifications(std::vector<const int*>*) const override {}

  // No rewrite expected as the argument is not connected/reachable from a
  // rewritten field.
  void GetNotifications2(std::vector<const int*> a) const override {}

  // No rewrite expected as the argument is not connected/reachable from a
  // rewritten field.
  void GetNotifications3(std::vector<const int*>& a) const override {}

 private:
  // Expected rewrite: std::vector<raw_ptr<int>> member;
  std::vector<int*> member;
  VECTOR2 member2;
};

class Mocked1 : public AA {
 public:
  // Expected rewrite: void, set, (std::vector<raw_ptr<int>>)
  MOCK_METHOD(void, set, (std::vector<int*>));

  // Expected rewrite: get, void(std::vector<raw_ptr<int>>&)
  MOCK_CONST_METHOD1(get, void(std::vector<int*>&));

  // Expected rewrite: get2, void(std::vector<raw_ptr<int>>*)
  MOCK_CONST_METHOD1(get2, void(std::vector<int*>*));

  // No rewrite expected as the argument is not connected/reachable from a
  // rewritten field.
  MOCK_CONST_METHOD1(get3, void(std::vector<S*>&));

  // Expected rewrite: get4, void(std::vector<raw_ptr<S>>&)
  MOCK_CONST_METHOD1(get4, void(std::vector<S*>&));

  // No rewrite expected as the argument is not connected/reachable from a
  // rewritten field.
  MOCK_CONST_METHOD1(GetNotifications, void(std::vector<const int*>*));

  // No rewrite expected as the argument is not connected/reachable from a
  // rewritten field.
  MOCK_CONST_METHOD1(GetNotifications2, void(std::vector<const int*>&));

  // No rewrite expected as the argument is not connected/reachable from a
  // rewritten field.
  MOCK_CONST_METHOD1(GetNotifications3, void(std::vector<const int*>));
};

class Mocked2 : public AA {
 public:
  // Expected rewrite: set, void(std::vector<raw_ptr<int>> arg)
  MOCK_METHOD1(set, void(std::vector<int*> args));

  // Expected rewrite: get, void(std::vector<raw_ptr<int>>&)
  MOCK_CONST_METHOD1(get, void(std::vector<int*>&));

  // Expected rewrite: get2, void(std::vector<raw_ptr<int>>*)
  MOCK_CONST_METHOD1(get2, void(std::vector<int*>*));

  // No rewrite expected as the argument is not connected/reachable from a
  // rewritten field.
  MOCK_CONST_METHOD1(get3, void(VECTOR&));

  // Expected rewrite: get4, void(std::vector<raw_ptr<S>>&)
  MOCK_CONST_METHOD1(get4, void(std::vector<S*>&));

  // No rewrite expected as the argument is not connected/reachable from a
  // rewritten field.
  MOCK_CONST_METHOD1(GetNotifications, void(std::vector<const int*>*));

  // No rewrite expected as the argument is not connected/reachable from a
  // rewritten field.
  MOCK_CONST_METHOD1(GetNotifications2, void(std::vector<const int*>&));

  // No rewrite expected as the argument is not connected/reachable from a
  // rewritten field.
  MOCK_CONST_METHOD1(GetNotifications3, void(std::vector<const int*>));
};

}  // namespace
