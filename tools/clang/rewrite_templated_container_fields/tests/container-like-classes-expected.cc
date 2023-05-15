// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <vector>

#include "base/memory/raw_ptr.h"

class List {
 public:
  // Expected rewrite: List(const std::vector<raw_ptr<int>>& arg):
  // member_(arg){}
  List(const std::vector<raw_ptr<int>>& arg) : member_(arg) {}

  // Expected rewrite: std::vector<raw_ptr<int>>::iterator begin()
  std::vector<raw_ptr<int>>::iterator begin() { return member_.begin(); }

  // Expected rewrite: std::vector<raw_ptr<int>>::const_iterator end() const
  std::vector<raw_ptr<int>>::iterator end() { return member_.end(); }

  // Expected rewrite: std::vector<raw_ptr<int>>::const_iterator begin() const
  std::vector<raw_ptr<int>>::const_iterator begin() const {
    return member_.begin();
  }

  // Expected rewrite: std::vector<raw_ptr<int>>::const_iterator end() const
  std::vector<raw_ptr<int>>::const_iterator end() const {
    return member_.end();
  }

 private:
  // Expected rewrite: std::vector<raw_ptr<int>> member_;
  std::vector<raw_ptr<int>> member_;
};

List* get_ptr() {
  return nullptr;
}

void fct() {
  // Expected rewrite: std::vector<raw_ptr<int>> temp;
  std::vector<raw_ptr<int>> temp;
  temp.push_back(nullptr);

  List l(temp);

  // Expected rewrite: for (int* i : l)
  for (int* i : l) {
    (void)i;
  }

  List* ptr = &l;
  // Expected rewrite: for (int* i : *ptr)
  for (int* i : *ptr) {
    (void)i;
  }

  // Expected rewrite: for (int* i : *get_ptr())
  for (int* i : *get_ptr()) {
    (void)i;
  }
}
