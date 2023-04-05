// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <vector>

#define EXPECT_EQ(x, y) x == y
#define ASSERT_EQ(x, y) EXPECT_EQ(x, y)

struct S {};

struct obj {
  // Expected rewrite: const std::vector<raw_ptr<S>>& get();
  const std::vector<S*>& get() { return member; }

  // Expected rewrite: std::vector<raw_ptr<S>> member;
  std::vector<S*> member;
};

// Expected rewrite: std::vector<raw_ptr<S>> get_value();
std::vector<S*> get_value() {
  return {};
}

// Expected rewrite: std::vector<raw_ptr<S>>* get_ptr();
std::vector<S*>* get_ptr() {
  return nullptr;
}

// No rewrite expected.
std::vector<S*> unrelated_fct() {
  return {};
}

void fct() {
  obj o;

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>> a = o.member;
    std::vector<S*> a = o.member;
    // Expected rewrite: std::vector<raw_ptr<S>> b;
    std::vector<S*> b;
    // a = b;
    a = b;

    // Expected rewrite: std::vector<raw_ptr<S>> c;
    std::vector<S*> c;
    c.swap(b);

    // Expected rewrite: std::vector<raw_ptr<S>> d;
    std::vector<S*> d;
    std::swap(c, d);
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>> a = o.member;
    auto a = o.member;
    // Expected rewrite: std::vector<raw_ptr<S>> b;
    std::vector<S*> b;
    // a = b;
    a = b;

    // Expected rewrite: std::vector<raw_ptr<S>> c;
    std::vector<S*> c;
    c.swap(a);

    // Expected rewrite: std::vector<raw_ptr<S>> d;
    std::vector<S*> d;
    std::swap(d, a);
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>> a = o.member;
    std::vector<S*> a = o.member;
    // Expected rewrite: a = std::vector<raw_ptr<S>>();
    a = std::vector<S*>();
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>> a = o.member;
    auto a = o.member;
    // Expected rewrite: a = std::vector<raw_ptr<S>>();
    a = std::vector<S*>();
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>> a = o.member;
    std::vector<S*> a = o.member;
    // Expected rewrite: std::vector<raw_ptr<S>> b;
    std::vector<S*> b;
    // a = std::move(b);
    a = std::move(b);
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>> a = o.member;
    auto a = o.member;
    // Expected rewrite: std::vector<raw_ptr<S>> b;
    std::vector<S*> b;
    // a = std::move(b);
    a = std::move(b);
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>> a = o.member;
    std::vector<S*>* a = &o.member;
    // Expected rewrite: std::vector<raw_ptr<S>> b;
    std::vector<S*> b;
    // a = &b;
    a = &b;
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>> a = o.member;
    auto* a = &o.member;
    // Expected rewrite: std::vector<raw_ptr<S>> b;
    std::vector<S*> b;
    // a = &b;
    a = &b;

    // Expected rewrite: std::vector<raw_ptr<S>> c;
    std::vector<S*> c;
    c.swap(*a);

    // Expected rewrite: std::vector<raw_ptr<S>> d;
    std::vector<S*> d;
    std::swap(d, *a);

    // Expected rewrite: std::vector<raw_ptr<S>> e;
    std::vector<S*> e;
    EXPECT_EQ(e, *a);

    // Expected rewrite: std::vector<raw_ptr<S>> d;
    std::vector<S*> f;
    ASSERT_EQ(f, *a);
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>>* a = &o.member;
    std::vector<S*>* a = &o.member;
    // Expected rewrite: std::vector<raw_ptr<S>> b;
    std::vector<S*> b;
    // *a = b;
    *a = b;
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>>* a = &o.member;
    std::vector<S*>* a = &o.member;
    // Expected rewrite: std::vector<raw_ptr<S>> b;
    std::vector<S*> b;
    // *a = b;
    std::swap(*a, b);
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>>* a = &o.member;
    std::vector<S*>* a = &o.member;
    // Expected rewrite: std::vector<raw_ptr<S>> b;
    std::vector<S*> b;
    // *a = b;
    a->swap(b);
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>>* a = &o.member;
    auto* a = &o.member;
    // Expected rewrite: std::vector<raw_ptr<S>> b;
    std::vector<S*> b;
    // *a = b;
    *a = b;
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>>* a = &o.member;
    auto* a = &o.member;
    // Expected rewrite: std::vector<raw_ptr<S>> b;
    std::vector<S*> b;
    // *a = b;
    std::swap(*a, b);

    // Expected rewrite: std::vector<raw_ptr<S>> d;
    std::vector<S*> d;
    d.swap(*a);
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>>* a = &o.member;
    std::vector<S*>* a = &o.member;
    // Expected rewrite: std::vector<raw_ptr<S>>* b;
    std::vector<S*>* b = nullptr;
    // *a = b;
    *a = *b;

    // Expected rewrite: std::vector<raw_ptr<S>>* d;
    std::vector<S*>* d;
    d->swap(*a);
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>>* a = &o.member;
    std::vector<S*>* a = &o.member;
    // Expected rewrite: std::vector<raw_ptr<S>>* b;
    std::vector<S*>* b = nullptr;
    // *a = b;
    std::swap(*a, *b);
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>>* a = &o.member;
    auto* a = &o.member;
    // Expected rewrite: std::vector<raw_ptr<S>>* b;
    std::vector<S*>* b = nullptr;
    // *a = b;
    *a = *b;
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>>* a = &o.member;
    auto* a = &o.member;
    // Expected rewrite: std::vector<raw_ptr<S>>* b;
    std::vector<S*>* b = nullptr;
    // *a = b;
    std::swap(*a, *b);
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>> a = o.member;
    std::vector<S*> a = o.member;
    // a = fct();
    a = get_value();
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>> a = o.member;
    auto a = o.member;
    // a = fct();
    auto fct_value = []() -> std::vector<S*> { return {}; };
    a = fct_value();
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>>* a = &o.member;
    std::vector<S*>* a = &o.member;
    // a = fct();
    a = get_ptr();
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>>* a = &o.member;
    auto* a = &o.member;
    // a = fct();
    auto fct_ptr = []() -> std::vector<S*>* { return nullptr; };
    a = fct_ptr();
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>>* a = &o.member;
    const std::vector<S*>* a = &o.member;
    // a = &fct();
    a = &o.get();
  }

  {
    // create a link with a member to propagate the rewrite.
    // Expected rewrite: std::vector<raw_ptr<S>> a;
    std::vector<S*> a;
    auto fct = [&]() -> std::vector<S*>* { return &o.member; };
    a = *fct();

    // Expected rewrite: std::vector<raw_ptr<S>> b;
    std::vector<S*> b;
    std::swap(b, *fct());
  }

  {
    // Expected rewrite: std::vector<raw_ptr<S>>::iterator it;
    std::vector<S*>::iterator it;
    it = o.member.begin();
    (void)it;
  }

  {
    auto it = o.member.begin();
    // Expected rewrite: std::vector<raw_ptr<S>>::iterator it2;
    std::vector<S*>::iterator it2;
    it2 = it;
  }

  {
    // create a link with a member to propagate the rewrite.
    std::vector<S*> a = o.member;
    // Expected rewrite: std::vector<raw_ptr<S>> b;
    std::vector<S*> b;
    // a = b;
    // Expected rewrite: a = (true) ? b : std::vector<raw_ptr<S>>();
    a = (true) ? b : std::vector<S*>();
  }

  {
    // create a link with a member to propagate the rewrite.
    auto a = o.member;
    // Expected rewrite: std::vector<raw_ptr<S>> b;
    std::vector<S*> b;
    // a = b;
    // Expected rewrite: a = (true) ? b : std::vector<raw_ptr<S>>();
    a = (true) ? b : std::vector<S*>();
  }
}
