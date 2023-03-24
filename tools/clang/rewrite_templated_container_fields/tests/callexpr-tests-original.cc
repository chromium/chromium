// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <vector>

struct S {};

struct obj {
  // Expected rewrite: std::vector<raw_ptr<S>> member;
  std::vector<S*> member;
};

namespace n1 {
// Expected rewrite: fct(std::vector<raw_ptr<S>>& arg);
void fct(const std::vector<S*>& arg) {}
void fct2() {
  obj o;
  fct(o.member);
}
}  // namespace n1

namespace n2 {
// Expected rewrite: fct(std::vector<raw_ptr<S>>& arg);
void fct(const std::vector<S*>& arg) {}
// Expected rewrite: fct2(std::vector<raw_ptr<S>>& arg);
void fct2(const std::vector<S*>& arg) {}
void fct3() {
  obj o;
  // Expected rewrite: std::vector<raw_ptr<S>> temp = o.member;
  std::vector<S*> temp = o.member;
  fct(temp);

  const auto& temp2 = o.member;
  fct2(temp2);
}

}  // namespace n2

namespace n3 {
// Expected rewrite: fct1(std::vector<raw_ptr<S>>& arg);
void fct1(const std::vector<S*>& arg) {}

// Expected rewrite: fct2(std::vector<raw_ptr<S>>& arg);
void fct2(const std::vector<S*>& arg) {
  fct1(arg);
}

void fct3() {
  obj o;
  // Expected rewrite: std::vector<raw_ptr<S>> temp = o.member;
  std::vector<S*> temp = o.member;
  fct2(temp);
}
}  // namespace n3

namespace n4 {
// Expected rewrite: fct(std::vector<raw_ptr<S>>* arg);
void fct(std::vector<S*>* arg) {}
// Expected rewrite: fct2(std::vector<raw_ptr<S>>* arg);
void fct2(std::vector<S*>* arg) {}

void fct3() {
  obj o;
  fct(&o.member);

  auto temp = o.member;
  fct2(&temp);
}
}  // namespace n4

namespace n5 {
// Expected rewrite: fct(std::vector<raw_ptr<S>>* arg);
void fct(std::vector<S*>* arg) {}
void fct2() {
  obj o;
  // Expected rewrite: std::vector<raw_ptr<S>> temp = o.member;
  std::vector<S*> temp = o.member;
  fct(&temp);
}
}  // namespace n5

namespace n6 {
// Expected rewrite: fct1(std::vector<raw_ptr<S>>* arg);
void fct1(std::vector<S*>* arg) {}

// Expected rewrite: fct2(std::vector<raw_ptr<S>>* arg);
void fct2(std::vector<S*>* arg) {
  fct1(arg);
}

void fct3() {
  obj o;
  // Expected rewrite: std::vector<raw_ptr<S>> temp = o.member;
  std::vector<S*> temp = o.member;
  fct2(&temp);
}
}  // namespace n6

namespace n7 {
// Expected rewrite: fct(std::vector<raw_ptr<S>> arg);
void fct(std::vector<S*> arg) {}
// Expected rewrite: fct2(std::vector<raw_ptr<S>> arg);
void fct2(std::vector<S*> arg) {}
void fct2() {
  obj o;
  // Expected rewrite: std::vector<raw_ptr<S>>* temp = &o.member;
  std::vector<S*>* temp = &o.member;
  fct(*temp);

  auto* temp2 = &o.member;
  fct2(*temp2);
}
}  // namespace n7

namespace n8 {
// Expected rewrite: fct1(std::vector<raw_ptr<S>> arg);
void fct1(std::vector<S*> arg) {}

// Expected rewrite: fct2(std::vector<raw_ptr<S>>* arg);
void fct2(std::vector<S*>* arg) {
  fct1(*arg);
}

void fct3() {
  obj o;
  // Expected rewrite: std::vector<raw_ptr<S>> temp = o.member;
  std::vector<S*> temp = o.member;
  fct2(&temp);
}
}  // namespace n8

namespace n9 {
// Expected rewrite: std::vector<raw_ptr<S>> get()
std::vector<S*> get() {
  return {};
}

// Expected rewrite: fct(std::vector<raw_ptr<S>> arg);
void fct1(std::vector<S*> arg) {}

void fct() {
  obj o;
  o.member = get();
  fct1(get());
}
}  // namespace n9

namespace n10 {
// Expected rewrite: std::vector<raw_ptr<S>>& get()
std::vector<S*>& get() {
  // Expected rewrite: static std::vector<raw_ptr<S>> v;
  static std::vector<S*> v;
  return v;
}

// Expected rewrite: fct1(std::vector<raw_ptr<S>>* arg);
void fct1(std::vector<S*>* arg) {}

void fct() {
  obj o;
  o.member = get();
  fct1(&get());
}
}  // namespace n10
