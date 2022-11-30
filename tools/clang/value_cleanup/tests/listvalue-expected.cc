// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "values.h"

// All of these should be renamed to |GetList().emplace_back()|.
void F() {
  base::ListValue value;
  value.GetList().emplace_back(false);
  value.GetList().emplace_back(0);
  value.GetList().emplace_back(0.0);
  value.GetList().emplace_back("");
}

// All of these should be renamed to GetList() + their std::vector equivalent.
void G() {
  base::ListValue value;
  value.GetList().clear();
  value.GetList().size();
  value.GetList().empty();
  value.GetList().reserve(0);
}

// None of these should be renamed, as these methods require different handling.
void H() {
  base::ListValue value;
  value.Append(std::unique_ptr<base::Value>(new base::Value()));
  value.AppendStrings({"foo", "bar"});
  value.AppendIfNotPresent(std::unique_ptr<base::Value>(new base::Value()));
}
