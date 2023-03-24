// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <map>
#include <memory>
#include <vector>

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
struct SA {};
}  // namespace A

namespace B {
struct S {
  // Expected rewrite: std::vector<raw_ptr<const A::SA>> member;
  std::vector<const A::SA*> member;

  void fct() {
    // This tests whether we properly trim (anonymous namespace):: from the type
    // while conserving constness.
    // Expected rewrite: for(const A::SA* i : member)
    for (auto* i : member) {
      (void)i;
    }
  }
};
}  // namespace B
}  // namespace
