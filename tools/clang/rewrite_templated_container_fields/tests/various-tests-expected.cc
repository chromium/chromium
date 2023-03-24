// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <map>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"

#define RAW_PTR_EXCLUSION __attribute__((annotate("raw_ptr_exclusion")))

struct S {};

class A {
 public:
  A() : member(init()) {}

  A(const std::vector<raw_ptr<S>>& arg, const std::vector<const char*>& arg2)
      : member(arg), member2(arg2) {}

  A(const std::vector<raw_ptr<S>>* arg) : member(*arg) {}

  std::vector<raw_ptr<S>> init() { return {}; }

  std::vector<raw_ptr<S>> do_something(std::vector<raw_ptr<S>>& a,
                                       S* i,
                                       std::vector<raw_ptr<S>>& b);

  void set(const std::vector<raw_ptr<S>> arg);

 private:
  std::vector<raw_ptr<S>> member;
  std::vector<const char*> member2;
};

std::vector<raw_ptr<S>> A::do_something(std::vector<raw_ptr<S>>& a,
                                        S* i,
                                        std::vector<raw_ptr<S>>& b) {
  a.push_back(i);
  member = b;
  b = a;
  return a;
}

void A::set(const std::vector<raw_ptr<S>> arg) {
  member = arg;
}

class B {
 public:
  B() = default;

  std::vector<raw_ptr<S>> get() { return member; }

  std::vector<raw_ptr<S>> get2();

 private:
  std::vector<raw_ptr<S>> member;
};

std::vector<raw_ptr<S>> B::get2() {
  return member;
}

class C {
 public:
  C() = default;

  const std::vector<raw_ptr<S>>& get() { return member; }

 private:
  std::vector<raw_ptr<S>> member;
};

class D {
 public:
  D() = default;

  std::vector<raw_ptr<S>>* get() { return &member; }

 private:
  std::vector<raw_ptr<S>> member;
};

class E {
 public:
  E() = default;

  void set(const std::vector<raw_ptr<S>>& arg) { member = arg; }

 private:
  std::vector<raw_ptr<S>> member;
};

class F {
 public:
  F() = default;

  void init() {
    std::vector<raw_ptr<S>> temp;
    temp.push_back(nullptr);
    member = temp;

    {
      std::vector<raw_ptr<S>>::iterator it;
      it = temp.begin();
      ++it;
    }

    {
      std::vector<raw_ptr<S>>::iterator it = temp.begin();
      ++it;
    }
  }

 private:
  std::vector<raw_ptr<S>> member;
};

class G {
 public:
  G() = default;

  void init() {
    std::vector<raw_ptr<S>> temp = member;
    temp.push_back(nullptr);
  }

 private:
  std::vector<raw_ptr<S>> member;
};

class H {
 public:
  H() = default;

  std::vector<raw_ptr<S>> init() {
    std::vector<raw_ptr<S>> temp;
    temp = member;
    temp.push_back(nullptr);

    std::vector<raw_ptr<S>> temp2;
    temp2 = temp;
    return temp2;
  }

 private:
  std::vector<raw_ptr<S>> member;
};

class I {
 public:
  I() = default;

  std::vector<raw_ptr<S>> init() {
    std::vector<raw_ptr<S>> temp;
    temp = std::move(member);
    temp.push_back(nullptr);
    return temp;
  }

 private:
  std::vector<raw_ptr<S>> member;
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

  std::vector<raw_ptr<S>>& get() { return member; }

  std::vector<raw_ptr<S>>* get_2() { return &member; }

  void prepare(std::vector<raw_ptr<S>>& v) { v.push_back(nullptr); }

  void prepare2(std::vector<raw_ptr<S>>* v) { v->push_back(nullptr); }

 private:
  std::vector<raw_ptr<S>> member;
};

class K {
 public:
  K() = default;

  std::vector<raw_ptr<S>> init() {
    std::vector<raw_ptr<S>> temp;
    temp.swap(member);
    return temp;
  }

  std::vector<raw_ptr<S>> init2() {
    std::vector<raw_ptr<S>> temp;
    std::swap(temp, member);
    return temp;
  }

 private:
  std::vector<raw_ptr<S>> member;
};

class L {
 public:
  L() = default;

  std::vector<raw_ptr<S>> init() {
    std::vector<raw_ptr<S>> temp;
    temp.push_back(nullptr);
    member.swap(temp);
    return temp;
  }

  std::vector<raw_ptr<S>> init2() {
    std::vector<raw_ptr<S>> temp;
    temp.push_back(nullptr);
    std::swap(member, temp);
    return temp;
  }

 private:
  std::vector<raw_ptr<S>> member;
};

class M {
 public:
  M() = default;
  void set(std::vector<raw_ptr<S>>* v) { *v = member; }

 private:
  std::vector<raw_ptr<S>> member;
};

class N {
 public:
  N() : member() {}

  std::vector<raw_ptr<S>>* get() {
    std::vector<raw_ptr<S>>* temp;
    temp = &member;
    return temp;
  }

  std::vector<raw_ptr<S>>* get_() { return get(); }

  std::vector<raw_ptr<S>> get__() { return *get(); }

  std::vector<raw_ptr<S>> get2() {
    std::vector<raw_ptr<S>>* temp;
    temp = get();

    std::vector<raw_ptr<S>>* temp2 = get();
    (void)temp2;

    std::vector<raw_ptr<S>> temp3;
    temp3 = *get();
    (void)temp3;

    std::vector<raw_ptr<S>> temp4 = *get();
    (void)temp4;
    return *temp;
  }

  const std::vector<raw_ptr<S>>* get3() {
    std::vector<raw_ptr<S>>* temp = &member;
    return temp;
  }

  std::vector<raw_ptr<S>> get4() {
    std::vector<raw_ptr<S>>* temp;
    temp = &member;

    std::vector<raw_ptr<S>>* temp2 = temp;

    std::vector<raw_ptr<S>>** temp3 = &temp2;
    (void)temp3;

    std::vector<raw_ptr<S>>& ref = *temp;
    (void)ref;
    return *temp2;
  }

  const std::vector<raw_ptr<S>>& get5() {
    std::vector<raw_ptr<S>>* temp;
    temp = &member;
    return *temp;
  }

 private:
  std::vector<raw_ptr<S>> member;
};

struct obj {
  std::vector<raw_ptr<S>> member;
  std::vector<raw_ptr<std::map<int, int>>> member2;
};

struct obj2 {
  RAW_PTR_EXCLUSION std::vector<S*> member;
};

namespace temporary {
std::vector<raw_ptr<S>> ge_t() {
  return {};
}
std::vector<raw_ptr<S>>* ge_t_ptr() {
  return nullptr;
}
}  // namespace temporary

void fct() {
  std::vector<raw_ptr<S>> temp;
  std::vector<raw_ptr<S>> temp3;
  std::vector<raw_ptr<S>> temp2{temp};
  obj o{temp3};
  std::vector<const char*> t;
  A a(temp, t);
  (void)a;

  {
    std::vector<raw_ptr<S>> temp;
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
    std::vector<raw_ptr<S>> temp4;
    std::vector<const char*> s;
    std::make_unique<A>(temp4, s);
  }

  {
    std::vector<raw_ptr<S>> temp4;
    std::vector<const char*> s;
    A* a = new A(temp4, s);
    (void)a;
  }
}

class O {
 public:
  O() : member() {}

  std::vector<raw_ptr<S>> f() {
    std::vector<raw_ptr<S>> temp;
    temp = std::move(member);
    return temp;
  }

  std::vector<raw_ptr<S>> f2() {
    std::vector<raw_ptr<S>> temp = std::move(member);
    temp.push_back(nullptr);

    for (S* v : temp) {
      (void)v;
    }

    for (S* v : member) {
      (void)v;
    }

    for (const S* const v : member) {
      (void)v;
    }

    auto temp2 = temp;
    for (S* v : temp2) {
      (void)v;
    }

    for (const S* v : temp2) {
      (void)v;
    }

    for (const S* const v : temp2) {
      (void)v;
    }

    auto* ptr1 = temp2[0].get();
    (void)ptr1;

    auto* ptr2 = temp2.front().get();
    (void)ptr2;

    auto* ptr3 = temp2.back().get();
    (void)ptr3;

    return temp2;
  }

  std::vector<raw_ptr<S>> g() { return std::move(member); }

  std::vector<raw_ptr<S>> g2() {
    std::vector<raw_ptr<S>> temp;
    temp.push_back(nullptr);

    auto* var = temp.front().get();
    (void)var;

    auto* var2 = temp.back().get();
    (void)var2;

    int index = 0;
    auto* var3 = temp[index].get();
    (void)var3;
    return (temp.size() > member.size()) ? std::move(temp) : std::move(member);
  }

 private:
  std::vector<raw_ptr<S>> member;
};

class P {
 public:
  P(std::vector<raw_ptr<S>> arg) : member(std::move(arg)) {}

  P(std::vector<raw_ptr<S>>* arg) : member(*arg) {}

 private:
  std::vector<raw_ptr<S>> member;
};

namespace {
std::vector<raw_ptr<S>>* get_ptr() {
  return nullptr;
}
void p_fct() {
  {
    std::vector<raw_ptr<S>> temp;
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

  virtual std::vector<raw_ptr<S>> get();

 protected:
  std::vector<raw_ptr<S>> member;
};

std::vector<raw_ptr<S>> Parent::get() {
  return member;
}

class Child : public Parent {
 public:
  Child() = default;

  std::vector<raw_ptr<S>> get() override;
};

std::vector<raw_ptr<S>> Child::get() {
  return std::vector<raw_ptr<S>>{};
}

namespace n {
template <class T>
void do_something(std::vector<raw_ptr<T>>& v) {
  v.push_back(nullptr);
}

class BCD {
 public:
  BCD(const std::vector<raw_ptr<int>>& arg);

  void dod() {
    do_something(member);
    auto lambda = [this]() -> std::vector<raw_ptr<int>> { return member; };
    lambda();

    functor f;
    f(member);

    auto lambda2 = [](const std::vector<raw_ptr<int>>& v) {
      for (int* i : v) {
        (void)i;
      }
    };

    lambda2(member);
  }

 private:
  struct functor {
    void operator()(const std::vector<raw_ptr<int>>& v) {
      for (int* i : v) {
        (void)i;
      }
    }
  };
  std::vector<raw_ptr<int>> member;
};

BCD::BCD(const std::vector<raw_ptr<int>>& arg) : member(arg) {}
}  // namespace n

// No change needed here
void any_function(std::vector<int*>& v) {
  v.push_back(nullptr);
}

// These should be rewritten but are not for now.
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
struct SA {};
}  // namespace A

namespace B {
struct S {
  // Expected rewrite: std::vector<raw_ptr<const A::SA>> member;
  std::vector<raw_ptr<const A::SA>> member;

  void fct() {
    // This tests whether we properly trim (anonymous namespace):: from the type
    // while conserving constness.
    // Expected rewrite: for(const A::SA* i : member)
    for (const A::SA* i : member) {
      (void)i;
    }
  }
};
}  // namespace B
}  // namespace
