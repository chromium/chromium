// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <map>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
template <typename Signature>
class RepeatingCallback;

template <typename R, typename... Args>
class RepeatingCallback<R(Args...)> {
 public:
  RepeatingCallback() {}

  RepeatingCallback(const RepeatingCallback&) = default;
  RepeatingCallback& operator=(const RepeatingCallback&) = default;

  RepeatingCallback(RepeatingCallback&&) = default;
  RepeatingCallback& operator=(RepeatingCallback&&) = default;

  R Run(Args... args) const& { return R(); }
  R Run(Args... args) && { return R(); }
};
}  // namespace base

struct S {};

// Expected rewrite: using VECTOR = std::vector<raw_ptr<S>>;
using VECTOR = std::vector<raw_ptr<S>>;

struct obj {
  // Expected rewrite: const std::vector<raw_ptr<S>>& get();
  const std::vector<raw_ptr<S>>& get() { return member; }

  // Expected rewrite: std::vector<raw_ptr<S>> member;
  std::vector<raw_ptr<S>> member;

  typedef std::map<int, VECTOR> MAP;
  MAP member2;

  // No rewrite expected.
  base::RepeatingCallback<void(std::vector<S*>&)> callback1_;

  // No rewrite expected.
  using RB = base::RepeatingCallback<void(std::vector<int*>)>;

  RB callback2_;
};

// Expected rewrite: std::vector<raw_ptr<S>> get_value();
std::vector<raw_ptr<S>> get_value() {
  return {};
}

// Expected rewrite: std::vector<raw_ptr<S>>* get_ptr();
std::vector<raw_ptr<S>>* get_ptr() {
  return nullptr;
}

// No rewrite expected.
std::vector<S*> unrelated_fct() {
  return {};
}

// Expected rewrite: void call_on_vector(std::vector<raw_ptr<S>>* v)
void call_on_vector(std::vector<raw_ptr<S>>* v) {}

void fct() {
  obj o;

  {
    // Expected rewrite: std::map<int, std::vector<raw_ptr<S>>> m;
    std::map<int, std::vector<raw_ptr<S>>> m;
    m[0] = o.member;
  }

  {
    // Expected rewrite: o.member2.emplace(0, std::vector<raw_ptr<S>>{});
    o.member2.emplace(0, std::vector<raw_ptr<S>>{});
  }

  {
    // Expected rewrite: std::vector<raw_ptr<S>> temp;
    std::vector<raw_ptr<S>> temp;
    temp = (*o.member2.begin()).second;
  }

  {
    // Expected rewrite: std::map<int, std::vector<raw_ptr<S>>>::iterator it;
    std::map<int, std::vector<raw_ptr<S>>>::iterator it;
    it = o.member2.begin();
  }

  {
    // Expected rewrite: std::map<int, std::vector<raw_ptr<S>>>::iterator it;
    std::map<int, std::vector<raw_ptr<S>>>::iterator it;
    it = o.member2.find(0);
  }

  {
    for (auto& p : o.member2) {
      // call_on_vector signature will be rewritten due to this call.
      call_on_vector(&(p.second));
    }
  }

  {
    std::vector<raw_ptr<S>> temp;
    // Expected rewrite: std::vector<raw_ptr<S>> temp;
    temp = o.member2.find(0)->second;
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>> a = o.member;
    std::vector<raw_ptr<S>> a = o.member;
    // Expected rewrite: std::vector<raw_ptr<S>> b;
    std::vector<raw_ptr<S>> b;
    // a = b;
    a = b;

    // Expected rewrite: std::vector<raw_ptr<S>> c;
    std::vector<raw_ptr<S>> c;
    c.swap(b);

    // Expected rewrite: std::vector<raw_ptr<S>> d;
    std::vector<raw_ptr<S>> d;
    std::swap(c, d);
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>> a = o.member;
    auto a = o.member;
    // Expected rewrite: std::vector<raw_ptr<S>> b;
    std::vector<raw_ptr<S>> b;
    // a = b;
    a = b;

    // Expected rewrite: std::vector<raw_ptr<S>> c;
    std::vector<raw_ptr<S>> c;
    c.swap(a);

    // Expected rewrite: std::vector<raw_ptr<S>> d;
    std::vector<raw_ptr<S>> d;
    std::swap(d, a);
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>> a = o.member;
    std::vector<raw_ptr<S>> a = o.member;
    // Expected rewrite: a = std::vector<raw_ptr<S>>();
    a = std::vector<raw_ptr<S>>();
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>> a = o.member;
    auto a = o.member;
    // Expected rewrite: a = std::vector<raw_ptr<S>>();
    a = std::vector<raw_ptr<S>>();
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>> a = o.member;
    std::vector<raw_ptr<S>> a = o.member;
    // Expected rewrite: std::vector<raw_ptr<S>> b;
    std::vector<raw_ptr<S>> b;
    // a = std::move(b);
    a = std::move(b);
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>> a = o.member;
    auto a = o.member;
    // Expected rewrite: std::vector<raw_ptr<S>> b;
    std::vector<raw_ptr<S>> b;
    // a = std::move(b);
    a = std::move(b);
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>> a = o.member;
    std::vector<raw_ptr<S>>* a = &o.member;
    // Expected rewrite: std::vector<raw_ptr<S>> b;
    std::vector<raw_ptr<S>> b;
    // a = &b;
    a = &b;
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>> a = o.member;
    auto* a = &o.member;
    // Expected rewrite: std::vector<raw_ptr<S>> b;
    std::vector<raw_ptr<S>> b;
    // a = &b;
    a = &b;

    // Expected rewrite: std::vector<raw_ptr<S>> c;
    std::vector<raw_ptr<S>> c;
    c.swap(*a);

    // Expected rewrite: std::vector<raw_ptr<S>> d;
    std::vector<raw_ptr<S>> d;
    std::swap(d, *a);

    // Expected rewrite: std::vector<raw_ptr<S>> e;
    std::vector<raw_ptr<S>> e;
    EXPECT_EQ(e, *a);

    // Expected rewrite: std::vector<raw_ptr<S>> d;
    std::vector<raw_ptr<S>> f;
    ASSERT_EQ(f, *a);
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>>* a = &o.member;
    std::vector<raw_ptr<S>>* a = &o.member;
    // Expected rewrite: std::vector<raw_ptr<S>> b;
    std::vector<raw_ptr<S>> b;
    // *a = b;
    *a = b;
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>>* a = &o.member;
    std::vector<raw_ptr<S>>* a = &o.member;
    // Expected rewrite: std::vector<raw_ptr<S>> b;
    std::vector<raw_ptr<S>> b;
    // *a = b;
    std::swap(*a, b);
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>>* a = &o.member;
    std::vector<raw_ptr<S>>* a = &o.member;
    // Expected rewrite: std::vector<raw_ptr<S>> b;
    std::vector<raw_ptr<S>> b;
    // *a = b;
    a->swap(b);
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>>* a = &o.member;
    auto* a = &o.member;
    // Expected rewrite: std::vector<raw_ptr<S>> b;
    std::vector<raw_ptr<S>> b;
    // *a = b;
    *a = b;
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>>* a = &o.member;
    auto* a = &o.member;
    // Expected rewrite: std::vector<raw_ptr<S>> b;
    std::vector<raw_ptr<S>> b;
    // *a = b;
    std::swap(*a, b);

    // Expected rewrite: std::vector<raw_ptr<S>> d;
    std::vector<raw_ptr<S>> d;
    d.swap(*a);
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>>* a = &o.member;
    std::vector<raw_ptr<S>>* a = &o.member;
    // Expected rewrite: std::vector<raw_ptr<S>>* b;
    std::vector<raw_ptr<S>>* b = nullptr;
    // *a = b;
    *a = *b;

    // Expected rewrite: std::vector<raw_ptr<S>>* d;
    std::vector<raw_ptr<S>>* d;
    d->swap(*a);
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>>* a = &o.member;
    std::vector<raw_ptr<S>>* a = &o.member;
    // Expected rewrite: std::vector<raw_ptr<S>>* b;
    std::vector<raw_ptr<S>>* b = nullptr;
    // *a = b;
    std::swap(*a, *b);
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>>* a = &o.member;
    auto* a = &o.member;
    // Expected rewrite: std::vector<raw_ptr<S>>* b;
    std::vector<raw_ptr<S>>* b = nullptr;
    // *a = b;
    *a = *b;
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>>* a = &o.member;
    auto* a = &o.member;
    // Expected rewrite: std::vector<raw_ptr<S>>* b;
    std::vector<raw_ptr<S>>* b = nullptr;
    // *a = b;
    std::swap(*a, *b);
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>> a = o.member;
    std::vector<raw_ptr<S>> a = o.member;
    // a = fct();
    a = get_value();
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>> a = o.member;
    auto a = o.member;
    // a = fct();
    auto fct_value = []() -> std::vector<raw_ptr<S>> { return {}; };
    a = fct_value();
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>>* a = &o.member;
    std::vector<raw_ptr<S>>* a = &o.member;
    // a = fct();
    a = get_ptr();
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>>* a = &o.member;
    auto* a = &o.member;
    // a = fct();
    auto fct_ptr = []() -> std::vector<raw_ptr<S>>* { return nullptr; };
    a = fct_ptr();
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>>* a = &o.member;
    const std::vector<raw_ptr<S>>* a = &o.member;
    // a = &fct();
    a = &o.get();
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>> a;
    std::vector<raw_ptr<S>> a;
    auto fct = [&]() -> std::vector<raw_ptr<S>>* { return &o.member; };
    a = *fct();

    // Expected rewrite: std::vector<raw_ptr<S>> b;
    std::vector<raw_ptr<S>> b;
    std::swap(b, *fct());
  }

  {
    // Expected rewrite: std::vector<raw_ptr<S>>::iterator it;
    std::vector<raw_ptr<S>>::iterator it;
    it = o.member.begin();
    (void)it;
  }

  {
    auto it = o.member.begin();
    // Expected rewrite: std::vector<raw_ptr<S>>::iterator it2;
    std::vector<raw_ptr<S>>::iterator it2;
    it2 = it;
  }

  {
    // create a link with a member to propagate the rewrite.
    std::vector<raw_ptr<S>> a = o.member;
    // Expected rewrite: std::vector<raw_ptr<S>> b;
    std::vector<raw_ptr<S>> b;
    // a = b;
    // Expected rewrite: a = (true) ? b : std::vector<raw_ptr<S>>();
    a = (true) ? b : std::vector<raw_ptr<S>>();
  }

  {
    // create a link with a member to propagate the rewrite.
    auto a = o.member;
    // Expected rewrite: std::vector<raw_ptr<S>> b;
    std::vector<raw_ptr<S>> b;
    // a = b;
    // Expected rewrite: a = (true) ? b : std::vector<raw_ptr<S>>();
    a = (true) ? b : std::vector<raw_ptr<S>>();
  }
}
