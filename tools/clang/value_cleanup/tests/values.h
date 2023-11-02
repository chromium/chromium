// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VALUES_H_
#define VALUES_H_

#include <memory>
#include <string>
#include <vector>

namespace base {

class Value {
 public:
  using ListStorage = std::vector<Value>;

  ListStorage& GetList();
  const ListStorage& GetList() const;

 protected:
  ListStorage list_;
};

class ListValue : public Value {
 public:
  void Clear();
  size_t GetSize() const;
  bool empty() const;
  void Reserve(size_t);

  void AppendBoolean(bool);
  void AppendInteger(int);
  void AppendDouble(double);
  void AppendString(std::string);

  void Append(std::unique_ptr<Value> in_value);
  void AppendStrings(const std::vector<std::string>& in_values);
  bool AppendIfNotPresent(std::unique_ptr<Value> in_value);
};

}  // namespace base

#endif  // VALUES_H_
