// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <vector>

#include "base/memory/raw_ptr.h"

struct S {};
class A {
 public:
  A() = default;

  // Expected rewrite: std::vector<raw_ptr<const S>> get()
  std::vector<raw_ptr<const S>> get() { return member; }

  // Expected rewrite: std::vector<raw_ptr<const S>> get_()
  std::vector<raw_ptr<const S>> get_() {
    auto temp = member;
    return temp;
  }

  // Expected rewrite: const std::vector<raw_ptr<const S>>& get2()
  const std::vector<raw_ptr<const S>>& get2() { return member; }

  // Expected rewrite: const std::vector<raw_ptr<const S>>& get2_()
  const std::vector<raw_ptr<const S>>& get2_() {
    const auto& temp = member;
    return temp;
  }

  // Expected rewrite: std::vector<raw_ptr<const S>> get3()
  std::vector<raw_ptr<const S>> get3() {
    std::vector<raw_ptr<const S>>* temp = &member;
    return *temp;
  }

  // Expected rewrite: std::vector<raw_ptr<const S>> get3()
  std::vector<raw_ptr<const S>> get3_() {
    auto* temp = &member;
    return *temp;
  }

  // Expected rewrite: std::vector<raw_ptr<const S>> get4()
  std::vector<raw_ptr<const S>> get4() { return std::move(member); }

  // Expected rewrite: std::vector<raw_ptr<const S>> get4_()
  std::vector<raw_ptr<const S>> get4_() {
    auto temp = member;
    return std::move(temp);
  }

  // Expected rewrite: std::vector<raw_ptr<const S>> get5()
  std::vector<raw_ptr<const S>> get5() {
    // Expected rewrite: std::vector<raw_ptr<const S>>* { return &member; };
    auto fct = [&]() -> std::vector<raw_ptr<const S>>* { return &member; };
    return *fct();
  }

  // Expected rewrite: std::vector<raw_ptr<const S>> get6()
  std::vector<raw_ptr<const S>> get6(const std::vector<raw_ptr<const S>>& arg) {
    return (arg.size() > member.size()) ? arg : member;
  }

  // Expected rewrite: std::vector<raw_ptr<const S>>* get_ptr()
  std::vector<raw_ptr<const S>>* get_ptr() { return &member; }

  // Expected rewrite: std::vector<raw_ptr<const S>>* get_ptr2()
  std::vector<raw_ptr<const S>>* get_ptr2() {
    // Expected rewrite: std::vector<raw_ptr<const S>>& { return member; };
    auto fct = [&]() -> std::vector<raw_ptr<const S>>& { return member; };
    return &fct();
  }

  // Expected rewrite: std::vector<raw_ptr<const S>>::iterator get_begin()
  std::vector<raw_ptr<const S>>::iterator get_begin() { return member.begin(); }

  // Expected rewrite: std::vector<raw_ptr<const S>>::iterator get_begin_()
  std::vector<raw_ptr<const S>>::iterator get_begin_() {
    auto it = member.begin();
    return it;
  }

 private:
  // Expected rewrite: std::vector<raw_ptr<const S>> member;
  std::vector<raw_ptr<const S>> member;
};
